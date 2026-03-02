/*
 * HSM Version 0.1.4
 *
 * MIT License
 *
 * Copyright (c) 2026 tayne3
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef HSM_HSM_HPP
#define HSM_HSM_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
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

	Result handle(Machine<Traits> &sm, const typename Traits::Event &ev) override { return handle_ ? handle_(sm, ev) : Result::Pass; }
	void   on_entry(Machine<Traits> &sm) override {
        if (entry_) { entry_(sm); }
	}
	void on_exit(Machine<Traits> &sm) override {
		if (exit_) { exit_(sm); }
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
	Context                                                         ctx_;
	LambdaState<Traits>                                             root_ = {"Root"};
	std::vector<std::pair<StateID, std::unique_ptr<State<Traits>>>> registry_;

	State<Traits> *active_state_    = nullptr;
	State<Traits> *executing_state_ = nullptr;
	State<Traits> *pending_state_   = nullptr;

	Phase phase_ = Phase::Idle;

	struct EventWrapperBase {
		virtual ~EventWrapperBase()      = default;
		virtual const Event &get() const = 0;
	};

	template <typename T>
	struct EventWrapper : EventWrapperBase {
		T payload;
		EventWrapper(const T &t) : payload(t) {}
		const Event &get() const override { return payload; }
	};

	std::queue<std::unique_ptr<EventWrapperBase>> event_queue_;

	bool has_pending_    = false;
	bool is_started_     = false;
	bool is_terminated_  = false;
	bool is_handled_     = false;
	bool is_dispatching_ = false;

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
	bool started() const { return is_started_; }

	/// @brief Indicates whether the machine has been requested to terminate
	/// @return True if `stop()` was called or termination was triggered internally
	bool terminated() const { return is_terminated_; }

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
		if (is_started_ && !is_terminated_) { throw std::logic_error("Cannot already started"); }

		registry_.clear();
		is_started_    = false;
		is_terminated_ = false;
		has_pending_   = false;
		phase_         = Phase::Idle;
		active_state_  = nullptr;
		pending_state_ = nullptr;

		root_.handle_ = root_handler ? std::move(root_handler) : nullptr;
		Scope<Traits> root_scope(this, &root_);
		fn(root_scope);

		std::sort(registry_.begin(), registry_.end(),
				  [](const std::pair<StateID, std::unique_ptr<State<Traits>>> &a, const std::pair<StateID, std::unique_ptr<State<Traits>>> &b) {
					  return a.first < b.first;
				  });

		auto *init = get_state(initial_id);
		if (!init) throw std::invalid_argument("Initial state ID not found");

		is_started_   = true;
		active_state_ = &root_;

		do_transition(init);
		process_pending();
	}

	/// @brief Request termination; subsequent events and transitions are ignored
	void stop() { is_terminated_ = true; }

	/// @brief Schedule a transition to the target state (deferred execution during dispatch/entries, immediate if idle)
	/// @param target_id Identifier of the destination state
	/// @throws std::runtime_error If called during Exit phase or target not found
	void transition(StateID target_id) {
		if (phase_ == Phase::Exit) { throw std::runtime_error("Cannot transition during Exit phase"); }
		auto *dest = get_state(target_id);
		if (!dest) { throw std::runtime_error("Target state ID not found"); }

		pending_state_ = dest;
		has_pending_   = true;

		if (phase_ == Phase::Idle && !is_dispatching_) { process_pending(); }
	}

	/// @brief Dispatch an event, propagating from the active state up the parent chain
	/// @param evt Event object; default-constructed indicates an empty event
	/// @note No-op if not started or already terminated; if a pending transition or termination occurs, propagation stops and pending transitions are processed
	template <typename E>
	void dispatch(const E &evt) {
		if (!is_started_ || is_terminated_) { return; }

		if (is_dispatching_) {
			event_queue_.push(std::unique_ptr<EventWrapperBase>(new EventWrapper<E>(evt)));
			return;
		}

		is_dispatching_ = true;
		is_handled_     = false;
		phase_          = Phase::Run;

		for (auto *s = active_state_; s; s = s->parent_) {
			executing_state_ = s;
			if (s->handle(*this, evt) == Result::Done) {
				is_handled_ = true;
				break;
			}
			if (has_pending_ || is_terminated_) { break; }
		}

		phase_ = Phase::Idle;
		process_pending();

		while (!event_queue_.empty() && !is_terminated_) {
			auto wrapper = std::move(event_queue_.front());
			event_queue_.pop();

			is_handled_ = false;
			phase_      = Phase::Run;

			for (auto *s = active_state_; s; s = s->parent_) {
				executing_state_ = s;
				if (s->handle(*this, wrapper->get()) == Result::Done) {
					is_handled_ = true;
					break;
				}
				if (has_pending_ || is_terminated_) { break; }
			}

			phase_ = Phase::Idle;
			process_pending();
		}

		is_dispatching_ = false;
	}

	/// @brief Dispatch with brace-initialization fallback
	void dispatch(const Event &evt) { dispatch<Event>(evt); }

	/// @brief Dispatch an empty default event
	void dispatch() { dispatch<Event>(Event{}); }

private:
	State<Traits> *get_state(StateID id) {
		auto it = std::lower_bound(registry_.begin(), registry_.end(), id,
								   [](const std::pair<StateID, std::unique_ptr<State<Traits>>> &pair, const StateID &val) { return pair.first < val; });

		if (it != registry_.end() && it->first == id) { return it->second.get(); }
		return nullptr;
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
		while (has_pending_ && !is_terminated_) {
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
			if (is_terminated_) { return; }
			executing_state_ = dest;
			dest->on_entry(*this);
			return;
		}

		auto *common = lca(source, dest);

		phase_ = Phase::Exit;
		for (auto *s = source; s != common; s = s->parent_) {
			executing_state_ = s;
			s->on_exit(*this);
			if (is_terminated_) {
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

				if (is_terminated_ || has_pending_) {
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
		ScopeProxy(Machine<Traits> *sm, State<Traits> *s) : sub_scope_(sm, s) {}

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
		LambdaProxy(Machine<Traits> *sm, LambdaState<Traits> *s) : ScopeProxy(sm, s), target_state_(s) {}

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

	Scope(Machine<Traits> *sm, State<Traits> *s) : machine_(sm), parent_(s) {}

	bool has_state(typename Traits::StateID id) const {
		for (const auto &pair : machine_->registry_) {
			if (pair.first == id) { return true; }
		}
		return false;
	}

	// Helper to register state pointer
	void register_ptr(typename Traits::StateID id, State<Traits> *s) {
		s->parent_ = parent_;
		s->depth_  = parent_->depth_ + 1;
		s->id_     = id;
		machine_->registry_.emplace_back(id, std::unique_ptr<State<Traits>>(s));
	}

public:
	// Class-based (Template) -> ScopeProxy
	template <typename S, typename... Args>
	ScopeProxy state(typename Traits::StateID id, Args &&...args) {
		static_assert(std::is_base_of<State<Traits>, S>::value, "Must derive from State");
		if (has_state(id)) { throw std::invalid_argument("Duplicate StateID detected"); }

		auto *s = new S(std::forward<Args>(args)...);
		register_ptr(id, s);
		return ScopeProxy(machine_, s);
	}

	// Lambda-based (No Template) -> LambdaProxy
	LambdaProxy state(typename Traits::StateID id) {
		if (has_state(id)) { throw std::invalid_argument("Duplicate StateID detected"); }

		auto *s = new LambdaState<Traits>();
		register_ptr(id, s);
		return LambdaProxy(machine_, s);
	}
};

// ============================================================================
// Event Dispatcher Helper
// ============================================================================

struct FastCastPolicy {
	template <typename Specific, typename Base>
	static const Specific *apply(const Base &b) {
		if (typeid(b) == typeid(Specific)) { return static_cast<const Specific *>(&b); }
		return nullptr;
	}
};

struct DynamicCastPolicy {
	template <typename Specific, typename Base>
	static const Specific *apply(const Base &b) {
		return dynamic_cast<const Specific *>(&b);
	}
};

struct TagCastPolicy {
	template <typename Specific, typename Base>
	static const Specific *apply(const Base &b) {
		if (b.id == Specific::ID) { return static_cast<const Specific *>(&b); }
		return nullptr;
	}
};

template <typename CastPolicy, typename Traits>
class Matcher {
	Machine<Traits>              &sm_;
	const typename Traits::Event &ev_;
	Result                        result_ = Result::Pass;
	bool                          done_   = false;

public:
	Matcher(Machine<Traits> &sm, const typename Traits::Event &ev) : sm_(sm), ev_(ev) {}

	// Match a specific event type
	template <typename TargetEvent, typename Handler>
	Matcher &on(Handler &&h) {
		if (!done_) {
			if (const auto *ptr = CastPolicy::template apply<TargetEvent>(ev_)) {
				result_ = h(sm_, *ptr);
				done_   = true;
			}
		}
		return *this;
	}

	// Fallback handler
	template <typename Handler>
	Matcher &otherwise(Handler &&h) {
		if (!done_) {
			result_ = h(sm_, ev_);
			done_   = true;
		}
		return *this;
	}

	Result result() const { return result_; }

	operator Result() const { return result_; }
};

template <typename CastPolicy = FastCastPolicy, typename Traits>
Matcher<CastPolicy, Traits> match(Machine<Traits> &sm, const typename Traits::Event &ev) {
	return Matcher<CastPolicy, Traits>(sm, ev);
}

}  // namespace hsm

#endif  // HSM_HSM_HPP
