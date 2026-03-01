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
	static constexpr EventID ID = EventID::APPROVE;
	Approve() : Event(ID) {}
};
struct Reject : Event {
	static constexpr EventID ID = EventID::REJECT;
	Reject() : Event(ID) {}
};
struct Suspend : Event {
	static constexpr EventID ID = EventID::SUSPEND;
	Suspend() : Event(ID) {}
};
struct Resume : Event {
	static constexpr EventID ID = EventID::RESUME;
	Resume() : Event(ID) {}
};
struct Remove : Event {
	static constexpr EventID ID = EventID::REMOVE;
	Remove() : Event(ID) {}
};

void log_audit(Machine& sm, const std::string& action) {
	sm->audit_log.push_back("[" + sm->agent_name + "] " + action);
	printf("[AUDIT] %s: %s\n", sm->agent_name.c_str(), action.c_str());
}

struct BaseState : public State {
	BaseState(const char* n) : name_(n ? n : "Base") {}
	const char* get_name() const { return name_.c_str(); }
	void        on_entry(Machine&) override { printf("[%s] on entry\n", get_name()); }
	void        on_exit(Machine& sm) override {
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

		return hsm::match<hsm::TagCastPolicy>(sm, ev)
			.on<Approve>([](Machine& m, const Approve&) {
				log_audit(m, "Approved! -> Active");
				m.transition(StateID::ACTIVE);
				return hsm::Result::Done;
			})
			.on<Reject>([](Machine& m, const Reject&) {
				log_audit(m, "Rejected! -> Removed");
				m.transition(StateID::REMOVED);
				return hsm::Result::Done;
			})
			.otherwise([](Machine&, const Event&) { return hsm::Result::Pass; });
	}
};

struct ActiveState : public BaseState {
	ActiveState() : BaseState("Active") {}

	hsm::Result handle(Machine& sm, const Event& ev) override {
		BaseState::handle(sm, ev);

		return hsm::match<hsm::TagCastPolicy>(sm, ev)
			.on<Suspend>([](Machine& m, const Suspend&) {
				log_audit(m, "Suspended! -> Suspended");
				m.transition(StateID::SUSPENDED);
				return hsm::Result::Done;
			})
			.on<Remove>([](Machine& m, const Remove&) {
				log_audit(m, "Removed! -> Removed");
				m.transition(StateID::REMOVED);
				return hsm::Result::Done;
			})
			.otherwise([](Machine&, const Event&) { return hsm::Result::Pass; });
	}
};

struct SuspendedState : public BaseState {
	SuspendedState() : BaseState("Suspended") {}

	hsm::Result handle(Machine& sm, const Event& ev) override {
		BaseState::handle(sm, ev);

		return hsm::match<hsm::TagCastPolicy>(sm, ev)
			.on<Resume>([](Machine& m, const Resume&) {
				log_audit(m, "Resumed! -> Active");
				m.transition(StateID::ACTIVE);
				return hsm::Result::Done;
			})
			.on<Remove>([](Machine& m, const Remove&) {
				log_audit(m, "Removed! -> Removed");
				m.transition(StateID::REMOVED);
				return hsm::Result::Done;
			})
			.otherwise([](Machine&, const Event&) { return hsm::Result::Pass; });
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
