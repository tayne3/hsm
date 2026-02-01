#ifndef HSM_HSM_HPP
#define HSM_HSM_HPP

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace hsm {

enum class Result {
	Done,  // Event handled, stop propagation
	Pass,  // Event not handled, continue to parent
};

template <typename Traits>
class Machine;
template <typename Traits>
class Scope;

// ============================================================================
// State Base Class
// ============================================================================

template <typename Traits>
class State {
	friend class Machine<Traits>;
	friend class Scope<Traits>;

public:
	using Context = typename Traits::Context;
	using Event   = typename Traits::Event;
	using StateID = typename Traits::StateID;

	virtual ~State() = default;

	virtual Result      handle(Machine<Traits> &, const Event &) { return Result::Pass; }
	virtual void        on_entry(Machine<Traits> &) {}
	virtual void        on_exit(Machine<Traits> &) {}
	virtual const char *name() const { return "State"; }

	StateID id() const { return id_; }

private:
	StateID        id_        = StateID{};
	std::size_t    depth_     = 0;
	State<Traits> *parent_    = nullptr;
	State<Traits> *path_next_ = nullptr;
};

// ============================================================================
// Lambda State
// ============================================================================

template <typename Traits>
class LambdaState : public State<Traits> {
	friend class Machine<Traits>;
	friend class Scope<Traits>;

public:
	using HandleFn = std::function<Result(Machine<Traits> &, const typename Traits::Event &)>;
	using EntryFn  = std::function<void(Machine<Traits> &)>;
	using ExitFn   = std::function<void(Machine<Traits> &)>;

	LambdaState() = default;
	LambdaState(const char *name) : name_(name ? name : "Lambda") {}

	Result handle(Machine<Traits> &m, const typename Traits::Event &e) override { return handle_ ? handle_(m, e) : Result::Pass; }
	void   on_entry(Machine<Traits> &m) override {
        if (entry_) { entry_(m); }
	}
	void on_exit(Machine<Traits> &m) override {
		if (exit_) { exit_(m); }
	}
	const char *name() const override { return name_.c_str(); }

private:
	HandleFn    handle_ = nullptr;
	EntryFn     entry_  = nullptr;
	ExitFn      exit_   = nullptr;
	std::string name_   = "Lambda";
};

// ============================================================================
// Machine
// ============================================================================

template <typename Traits>
class Machine {
	friend class Scope<Traits>;

	using Context = typename Traits::Context;
	using Event   = typename Traits::Event;
	using StateID = typename Traits::StateID;

	enum class Phase { Idle, Run, Entry, Exit };

private:
	Context                                           ctx_;
	LambdaState<Traits>                               root_ = {"Root"};
	std::map<StateID, std::unique_ptr<State<Traits>>> registry_;

	State<Traits> *active_state_    = nullptr;
	State<Traits> *executing_state_ = nullptr;
	State<Traits> *pending_state_   = nullptr;

	Phase phase_       = Phase::Idle;
	bool  has_pending_ = false;
	bool  started_     = false;
	bool  terminated_  = false;
	bool  handled_     = false;

public:
	template <typename... Args>
	explicit Machine(Args &&...args) : ctx_(std::forward<Args>(args)...) {}

	Machine(const Machine &)            = delete;
	Machine &operator=(const Machine &) = delete;

	Context       &context() { return ctx_; }
	const Context &context() const { return ctx_; }
	Context       *operator->() { return &ctx_; }
	const Context *operator->() const { return &ctx_; }

	/// @brief Indicates whether the state machine has been started
	/// @return True if `start()` was called and the machine is not terminated
	bool started() const { return started_; }

	/// @brief Indicates whether the machine has been requested to terminate
	/// @return True if `stop()` was called or termination was triggered internally
	bool terminated() const { return terminated_; }

	/// @brief Get the identifier of the current active state
	/// @return The active state's `StateID`, or default-constructed `StateID{}` if none
	StateID current_state_id() const { return active_state_ ? active_state_->id_ : StateID{}; }

	/// @brief Build the state tree and start the machine at the given initial state
	/// @tparam F Callable type with signature `void(Scope<Traits>&)`
	/// @param initial_id Identifier of the initial state to enter
	/// @param fn Callback to declare states and hierarchy under the root scope
	/// @param root_handler Optional root event handler for top-level match
	/// @throws std::logic_error If called while already started and not terminated
	/// @throws std::invalid_argument If the initial state ID is not found
	template <class F>
	void start(StateID initial_id, F &&fn, typename LambdaState<Traits>::HandleFn root_handler = nullptr) {
		static_assert(std::is_same<void, decltype(fn(std::declval<Scope<Traits> &>()))>::value, "F must be callable as void(Scope<Traits>&)");
		if (started_ && !terminated_) { throw std::logic_error("Cannot already started"); }

		registry_.clear();
		started_       = false;
		terminated_    = false;
		has_pending_   = false;
		phase_         = Phase::Idle;
		active_state_  = nullptr;
		pending_state_ = nullptr;

		root_.handle_ = root_handler ? std::move(root_handler) : nullptr;
		Scope<Traits> root_scope(this, &root_);
		fn(root_scope);

		auto *init = get_state(initial_id);
		if (!init) throw std::invalid_argument("Initial state ID not found");

		started_      = true;
		active_state_ = &root_;

		do_transition(init);
		process_pending();
	}

	/// @brief Request termination; subsequent events and transitions are ignored
	void stop() { terminated_ = true; }

	/// @brief Schedule a transition to the target state (deferred execution)
	/// @param target_id Identifier of the destination state
	/// @throws std::runtime_error If called during Exit phase or target not found
	void transition(StateID target_id) {
		if (phase_ == Phase::Exit) { throw std::runtime_error("Cannot transition during Exit phase"); }
		auto *dest = get_state(target_id);
		if (!dest) { throw std::runtime_error("Target state ID not found"); }

		pending_state_ = dest;
		has_pending_   = true;
	}

	/// @brief Dispatch an event, propagating from the active state up the parent chain
	/// @param evt Event object; default-constructed indicates an empty event
	/// @note No-op if not started or already terminated; if a pending transition or termination occurs, propagation stops and pending transitions are processed
	void handle(const Event &evt = Event{}) {
		if (!started_ || terminated_) { return; }

		handled_ = false;
		phase_   = Phase::Run;

		for (auto *s = active_state_; s; s = s->parent_) {
			executing_state_ = s;
			if (s->handle(*this, evt) == Result::Done) {
				handled_ = true;
				break;
			}
			if (has_pending_ || terminated_) { break; }
		}

		phase_ = Phase::Idle;
		process_pending();
	}

private:
	State<Traits> *get_state(StateID id) {
		auto it = registry_.find(id);
		return (it == registry_.end()) ? nullptr : it->second.get();
	}

	static State<Traits> *lca(State<Traits> *a, State<Traits> *b) {
		while (a->depth_ > b->depth_) { a = a->parent_; }
		while (b->depth_ > a->depth_) { b = b->parent_; }
		while (a != b) {
			a = a->parent_;
			b = b->parent_;
		}
		return a;
	}

	void process_pending() {
		static constexpr int MAX_TRANSITIONS = 100;

		int count = 0;
		while (has_pending_ && !terminated_) {
			if (++count > MAX_TRANSITIONS) {
				stop();
				throw std::runtime_error("Infinite transition loop detected");
			}
			auto *dest     = pending_state_;
			has_pending_   = false;
			pending_state_ = nullptr;
			do_transition(dest);
		}
	}

	void do_transition(State<Traits> *dest) {
		auto *source = (phase_ == Phase::Entry && executing_state_) ? executing_state_ : active_state_;
		if (!source) { source = &root_; }

		if (source == dest) {
			executing_state_ = source;
			source->on_exit(*this);
			if (terminated_) { return; }
			executing_state_ = dest;
			dest->on_entry(*this);
			return;
		}

		auto *common = lca(source, dest);

		phase_ = Phase::Exit;
		for (auto *s = source; s != common; s = s->parent_) {
			executing_state_ = s;
			s->on_exit(*this);
			if (terminated_) {
				phase_ = Phase::Idle;
				return;
			}
			active_state_ = s->parent_;
		}

		if (dest != common) {
			for (auto *s = dest; s != common; s = s->parent_) { s->parent_->path_next_ = s; }

			phase_  = Phase::Entry;
			auto *s = common->path_next_;
			while (s) {
				executing_state_ = s;
				s->on_entry(*this);
				active_state_ = s;

				if (terminated_ || has_pending_) {
					phase_ = Phase::Idle;
					return;
				}
				if (s == dest) { break; }
				s = s->path_next_;
			}
		}
		phase_ = Phase::Idle;
	}
};

// ============================================================================
// Scope
// ============================================================================

template <typename Traits>
class Scope {
	friend class Machine<Traits>;

	// Proxy: The key to "state(...).with(...)" syntax
	class ScopeProxy {
	protected:
		Scope<Traits> sub_scope_;

	public:
		ScopeProxy(Machine<Traits> *m, State<Traits> *s) : sub_scope_(m, s) {}

		template <class F>
		void with(F &&fn) {
			static_assert(std::is_same<void, decltype(fn(std::declval<Scope<Traits> &>()))>::value, "F must be callable as void(Scope<Traits>&)");
			fn(sub_scope_);
		}
	};

	// Lambda Proxy
	class LambdaProxy : public ScopeProxy {
	protected:
		LambdaState<Traits> *target_state_;

	public:
		LambdaProxy(Machine<Traits> *m, LambdaState<Traits> *s) : ScopeProxy(m, s), target_state_(s) {}

		LambdaProxy &handle(typename LambdaState<Traits>::HandleFn fn) {
			target_state_->handle_ = std::move(fn);
			return *this;
		}
		LambdaProxy &on_entry(typename LambdaState<Traits>::EntryFn fn) {
			target_state_->entry_ = std::move(fn);
			return *this;
		}
		LambdaProxy &on_exit(typename LambdaState<Traits>::ExitFn fn) {
			target_state_->exit_ = std::move(fn);
			return *this;
		}
		LambdaProxy &name(const char *name) {
			if (name) { target_state_->name_ = name; }
			return *this;
		}
	};

private:
	Machine<Traits> *machine_;
	State<Traits>   *parent_;

	Scope(Machine<Traits> *m, State<Traits> *s) : machine_(m), parent_(s) {}

	// Helper to register state pointer
	void register_ptr(typename Traits::StateID id, State<Traits> *s) {
		s->parent_ = parent_;
		s->depth_  = parent_->depth_ + 1;
		s->id_     = id;
		machine_->registry_.emplace(id, std::unique_ptr<State<Traits>>(s));
	}

public:
	// Class-based (Template) -> ScopeProxy
	template <typename S, typename... Args>
	ScopeProxy state(typename Traits::StateID id, Args &&...args) {
		static_assert(std::is_base_of<State<Traits>, S>::value, "Must derive from State");
		if (machine_->registry_.find(id) != machine_->registry_.end()) { throw std::invalid_argument("Duplicate StateID detected"); }

		auto *s = new S(std::forward<Args>(args)...);
		register_ptr(id, s);
		return ScopeProxy(machine_, s);
	}

	// Lambda-based (No Template) -> LambdaProxy
	LambdaProxy state(typename Traits::StateID id) {
		if (machine_->registry_.find(id) != machine_->registry_.end()) { throw std::invalid_argument("Duplicate StateID detected"); }

		auto *s = new LambdaState<Traits>();
		register_ptr(id, s);
		return LambdaProxy(machine_, s);
	}
};

// ============================================================================
// Event Dispatcher Helper
// ============================================================================

struct DefaultCastPolicy {
	template <typename Specific, typename Base>
	static const Specific *apply(const Base &b) {
		return dynamic_cast<const Specific *>(&b);
	}
};

template <typename CastPolicy, typename Traits>
class Matcher {
	using MachineType = Machine<Traits>;
	using Event       = typename Traits::Event;

	MachineType &m_;
	const Event &e_;
	Result       result_ = Result::Pass;
	bool         done_   = false;

public:
	Matcher(MachineType &m, const Event &e) : m_(m), e_(e) {}

	// Match a specific event type
	template <typename TargetEvent, typename Handler>
	Matcher &on(Handler &&h) {
		if (!done_) {
			if (const auto *ptr = CastPolicy::template apply<TargetEvent>(e_)) {
				result_ = h(m_, *ptr);
				done_   = true;
			}
		}
		return *this;
	}

	// Fallback handler
	template <typename Handler>
	Matcher &otherwise(Handler &&h) {
		if (!done_) {
			result_ = h(m_, e_);
			done_   = true;
		}
		return *this;
	}

	Result result() const { return result_; }
	operator Result() const { return result_; }
};

template <typename CastPolicy = DefaultCastPolicy, typename Traits>
Matcher<CastPolicy, Traits> match(Machine<Traits> &m, const typename Traits::Event &e) {
	return Matcher<CastPolicy, Traits>(m, e);
}

}  // namespace hsm

#endif  // HSM_HSM_HPP
