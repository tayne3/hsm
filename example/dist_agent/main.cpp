/*
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
#include <cstdio>
#include <string>
#include <vector>

#include "hsm/hsm.hpp"

namespace {

enum class StateID { PENDING, ACTIVE, SUSPENDED, REMOVED };

enum class EventID { APPROVE, REJECT, SUSPEND, RESUME, REMOVE };

struct Event {
	const EventID id;

	Event(EventID id) : id(id) {}
	virtual ~Event() = default;
};
struct Traits {
	using StateID = StateID;
	using Event   = Event;
	struct Context {
		std::string              agent_name;
		std::vector<std::string> audit_log;
	};
};

using Machine = hsm::Machine<Traits>;
using State   = hsm::State<Traits>;

struct Approve : Event {
	Approve() : Event(EventID::APPROVE) {}
};
struct Reject : Event {
	Reject() : Event(EventID::REJECT) {}
};
struct Suspend : Event {
	Suspend() : Event(EventID::SUSPEND) {}
};
struct Resume : Event {
	Resume() : Event(EventID::RESUME) {}
};
struct Remove : Event {
	Remove() : Event(EventID::REMOVE) {}
};

void log_audit(Machine& sm, const std::string& action) {
	sm->audit_log.push_back("[" + sm->agent_name + "] " + action);
	printf("[AUDIT] %s: %s\n", sm->agent_name.c_str(), action.c_str());
}

struct BaseState : public State {
	BaseState(const char* n) : name_(n ? n : "Base") {}

	const char* get_name() const { return name_.c_str(); }

	void on_entry(Machine&) override { printf("[%s] on entry\n", get_name()); }

	void on_exit(Machine& sm) override {
		printf("[%s] on exit\n", get_name());
		log_audit(sm, std::string("exit ") + get_name());
	}

	hsm::Result handle(Machine& sm, const Event&) override {
		log_audit(sm, std::string(get_name()) + " received event");
		return hsm::Result::Pass;
	}

private:
	std::string name_;
};

struct PendingState : public BaseState {
	PendingState() : BaseState("Pending") {}

	hsm::Result handle(Machine& sm, const Event& ev) override {
		BaseState::handle(sm, ev);

		switch (ev.id) {
			case EventID::APPROVE:
				log_audit(sm, "Approved! -> Active");
				sm.transition(StateID::ACTIVE);
				return hsm::Result::Done;
			case EventID::REJECT:
				log_audit(sm, "Rejected! -> Removed");
				sm.transition(StateID::REMOVED);
				return hsm::Result::Done;
			default: return hsm::Result::Pass;
		}
	}
};

struct ActiveState : public BaseState {
	ActiveState() : BaseState("Active") {}

	hsm::Result handle(Machine& sm, const Event& ev) override {
		BaseState::handle(sm, ev);

		switch (ev.id) {
			case EventID::SUSPEND:
				log_audit(sm, "Suspended! -> Suspended");
				sm.transition(StateID::SUSPENDED);
				return hsm::Result::Done;
			case EventID::REMOVE:
				log_audit(sm, "Removed! -> Removed");
				sm.transition(StateID::REMOVED);
				return hsm::Result::Done;
			default: return hsm::Result::Pass;
		}
	}
};

struct SuspendedState : public BaseState {
	SuspendedState() : BaseState("Suspended") {}

	hsm::Result handle(Machine& sm, const Event& ev) override {
		BaseState::handle(sm, ev);

		switch (ev.id) {
			case EventID::RESUME:
				log_audit(sm, "Resumed! -> Active");
				sm.transition(StateID::ACTIVE);
				return hsm::Result::Done;
			case EventID::REMOVE:
				log_audit(sm, "Removed! -> Removed");
				sm.transition(StateID::REMOVED);
				return hsm::Result::Done;
			default: return hsm::Result::Pass;
		}
	}
};

struct RemovedState : public BaseState {
	RemovedState() : BaseState("Removed") {}
};

}  // namespace

int main() {
	Machine sm;
	sm.context().agent_name = "Agent-001";

	sm.start(StateID::PENDING, [](hsm::Scope<Traits>& s) {
		s.state<PendingState>(StateID::PENDING);
		s.state<ActiveState>(StateID::ACTIVE);
		s.state<SuspendedState>(StateID::SUSPENDED);
		s.state<RemovedState>(StateID::REMOVED);
	});

	printf("\n--- Approve ---\n");
	sm.dispatch(Approve{});

	printf("\n--- Suspend ---\n");
	sm.dispatch(Suspend{});

	printf("\n--- Resume ---\n");
	sm.dispatch(Resume{});

	printf("\n--- Remove ---\n");
	sm.dispatch(Remove{});

	printf("\n=== Audit Log ===\n");
	for (const auto& log : sm.context().audit_log) { printf("  %s\n", log.c_str()); }
}
