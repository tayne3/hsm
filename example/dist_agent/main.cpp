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
	using StateID = ::StateID;
	using Event   = ::Event;
	struct Context {
		std::string              agent_name;
		std::vector<std::string> audit_log;
	};
};

using Machine = hsm::Machine<Traits>;
using State   = hsm::State<Traits>;
using Scope   = hsm::Scope<Traits>;

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
			.on<Approve>([](Machine& sm, const Approve&) {
				log_audit(sm, "Approved! -> Active");
				sm.transition(StateID::ACTIVE);
				return hsm::Result::Done;
			})
			.on<Reject>([](Machine& sm, const Reject&) {
				log_audit(sm, "Rejected! -> Removed");
				sm.transition(StateID::REMOVED);
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
			.on<Suspend>([](Machine& sm, const Suspend&) {
				log_audit(sm, "Suspended! -> Suspended");
				sm.transition(StateID::SUSPENDED);
				return hsm::Result::Done;
			})
			.on<Remove>([](Machine& sm, const Remove&) {
				log_audit(sm, "Removed! -> Removed");
				sm.transition(StateID::REMOVED);
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
			.on<Resume>([](Machine& sm, const Resume&) {
				log_audit(sm, "Resumed! -> Active");
				sm.transition(StateID::ACTIVE);
				return hsm::Result::Done;
			})
			.on<Remove>([](Machine& sm, const Remove&) {
				log_audit(sm, "Removed! -> Removed");
				sm.transition(StateID::REMOVED);
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

	sm.start(StateID::PENDING, [](Scope& s) {
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
