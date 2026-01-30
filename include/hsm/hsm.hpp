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
#include <vector>

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

	State<Traits> *parent() const { return parent_; }
	int            depth() const { return depth_; }
	StateID        id() const { return id_; }

protected:
	State<Traits> *parent_ = nullptr;
	int            depth_  = 0;
	StateID        id_     = StateID{};
};

// ============================================================================
// Lambda State
// ============================================================================

template <typename Traits>
class LambdaState : public State<Traits> {
	friend class Machine<Traits>;

public:
	using Context = typename Traits::Context;
	using Event   = typename Traits::Event;
	using StateID = typename Traits::StateID;

	using HandleFn = std::function<Result(Machine<Traits> &, const Event &)>;
	using EntryFn  = std::function<void(Machine<Traits> &)>;
	using ExitFn   = std::function<void(Machine<Traits> &)>;

	LambdaState(HandleFn handle, EntryFn entry, ExitFn exit, const char *name)
		: handle_(std::move(handle)), entry_(std::move(entry)), exit_(std::move(exit)), name_(name ? name : "Lambda") {}

	Result handle(Machine<Traits> &m, const Event &e) override { return handle_ ? handle_(m, e) : Result::Pass; }
	void   on_entry(Machine<Traits> &m) override {
        if (entry_) { entry_(m); }
	}
	void on_exit(Machine<Traits> &m) override {
		if (exit_) { exit_(m); }
	}
	const char *name() const override { return name_.c_str(); }

private:
	HandleFn    handle_;
	EntryFn     entry_;
	ExitFn      exit_;
	std::string name_;
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
	LambdaState<Traits>                               root_;
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
	explicit Machine(Args &&...args) : ctx_(std::forward<Args>(args)...), root_(nullptr, nullptr, nullptr, "Root") {}

	Machine(const Machine &)            = delete;
	Machine &operator=(const Machine &) = delete;

	Context       &context() { return ctx_; }
	const Context &context() const { return ctx_; }
	Context       *operator->() { return &ctx_; }
	const Context *operator->() const { return &ctx_; }

	bool    started() const { return started_; }
	bool    terminated() const { return terminated_; }
	StateID current_state_id() const { return active_state_ ? active_state_->id() : StateID{}; }

	void start(StateID initial_id, const std::function<void(Scope<Traits> &)> &config, typename LambdaState<Traits>::HandleFn root_handler = nullptr) {
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
		config(root_scope);

		auto *init = get_state(initial_id);
		if (!init) throw std::invalid_argument("Initial state ID not found");

		started_      = true;
		active_state_ = &root_;

		do_transition(init);
		process_pending();
	}

	void stop() { terminated_ = true; }

	void transition(StateID target_id) {
		if (phase_ == Phase::Exit) { throw std::runtime_error("Cannot transition during Exit phase"); }
		auto *dest = get_state(target_id);
		if (!dest) { throw std::runtime_error("Target state ID not found"); }

		pending_state_ = dest;
		has_pending_   = true;
	}

	void handle(const Event &evt = Event{}) {
		if (!started_ || terminated_) { return; }

		handled_ = false;
		phase_   = Phase::Run;

		for (auto *s = active_state_; s; s = s->parent()) {
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
		while (a->depth() > b->depth()) { a = a->parent(); }
		while (b->depth() > a->depth()) { b = b->parent(); }
		while (a != b) {
			a = a->parent();
			b = b->parent();
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
		for (auto *s = source; s != common; s = s->parent()) {
			executing_state_ = s;
			s->on_exit(*this);
			if (terminated_) {
				phase_ = Phase::Idle;
				return;
			}
			active_state_ = s->parent();
		}

		std::vector<State<Traits> *> path;
		for (auto *s = dest; s != common; s = s->parent()) { path.push_back(s); }

		phase_ = Phase::Entry;
		for (auto it = path.rbegin(); it != path.rend(); ++it) {
			auto *s          = *it;
			executing_state_ = s;
			s->on_entry(*this);
			active_state_ = s;

			if (terminated_ || has_pending_) {
				phase_ = Phase::Idle;
				return;
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

	using Context = typename Traits::Context;
	using Event   = typename Traits::Event;
	using StateID = typename Traits::StateID;

	// Proxy: The key to "state(...).with(...)" syntax
	class ScopeProxy {
	public:
		ScopeProxy(Machine<Traits> *m, State<Traits> *s) : machine_(m), target_state_(s) {}

		// The method to open a new scope for children
		void with(const std::function<void(Scope<Traits> &)> &children_config) {
			Scope<Traits> sub_scope(machine_, target_state_);
			children_config(sub_scope);
		}

	private:
		Machine<Traits> *machine_;
		State<Traits>   *target_state_;
	};

private:
	Machine<Traits> *machine_;
	State<Traits>   *parent_;

	Scope(Machine<Traits> *m, State<Traits> *p) : machine_(m), parent_(p) {}

public:
	// Class-based State
	template <typename S, typename... Args>
	ScopeProxy state(StateID id, Args &&...args) {
		static_assert(std::is_base_of<State<Traits>, S>::value, "Must derive from State");

		auto s = new S(std::forward<Args>(args)...);
		machine_->registry_.emplace(id, std::unique_ptr<S>(s));

		s->parent_ = parent_;
		s->depth_  = parent_->depth_ + 1;
		s->id_     = id;

		return ScopeProxy{machine_, s};
	}

	// Lambda-based State
	ScopeProxy state(StateID id, typename LambdaState<Traits>::HandleFn handle = nullptr, typename LambdaState<Traits>::EntryFn entry = nullptr,
					 typename LambdaState<Traits>::ExitFn exit = nullptr, const char *name = nullptr) {
		auto s = new LambdaState<Traits>(std::move(handle), std::move(entry), std::move(exit), name);
		machine_->registry_.emplace(id, std::unique_ptr<LambdaState<Traits>>(s));

		s->parent_ = parent_;
		s->depth_  = parent_->depth_ + 1;
		s->id_     = id;

		return ScopeProxy{machine_, s};
	}
};

}  // namespace hsm

#endif  // HSM_HSM_HPP
