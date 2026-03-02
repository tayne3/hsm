#include <string>
#include <vector>

#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

enum class CallType { Entry, Run, Exit };

struct CallRecord {
	CallType    type;
	std::string state_name;

	bool operator==(const CallRecord& other) const { return type == other.type && state_name == other.state_name; }
};

std::ostream& operator<<(std::ostream& os, const CallRecord& record) {
	os << "{ ";
	switch (record.type) {
		case CallType::Entry: os << "Entry"; break;
		case CallType::Run: os << "Run"; break;
		case CallType::Exit: os << "Exit"; break;
	}
	os << ", " << record.state_name << " }";
	return os;
}

struct Event {};
enum StateID { ID_PAB, ID_PC, ID_A, ID_B, ID_C, ID_D };

struct TestTraits {
	struct Context {
		std::vector<CallRecord> calls;
		void                    log(CallType type, const char* name) { calls.push_back({type, name}); }
		void                    clear() { calls.clear(); }
	};
	using Event   = ::Event;
	using StateID = ::StateID;
};

using TestState  = hsm::State<TestTraits>;
using AppMachine = hsm::Machine<TestTraits>;

class ParentAB : public TestState {
public:
	const char* name() const override { return "ParentAB"; }
	void        on_entry(AppMachine& sm) override { sm->log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm->log(CallType::Exit, name()); }
	hsm::Result handle(AppMachine& sm, const Event&) override {
		sm->log(CallType::Run, name());
		return hsm::Result::Done;
	}
};

class ParentC : public TestState {
public:
	const char* name() const override { return "ParentC"; }
	void        on_entry(AppMachine& sm) override { sm->log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm->log(CallType::Exit, name()); }
};

class StateA : public TestState {
public:
	const char* name() const override { return "StateA"; }

	void        on_entry(AppMachine& sm) override { sm->log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm->log(CallType::Exit, name()); }
	hsm::Result handle(AppMachine& sm, const Event&) override {
		sm->log(CallType::Run, name());
		sm.transition(ID_B);
		return hsm::Result::Done;
	}
};

class StateB : public TestState {
public:
	const char* name() const override { return "StateB"; }

	void        on_entry(AppMachine& sm) override { sm->log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm->log(CallType::Exit, name()); }
	hsm::Result handle(AppMachine& sm, const Event&) override {
		sm->log(CallType::Run, name());
		sm.transition(ID_C);
		return hsm::Result::Done;
	}
};

class StateC : public TestState {
public:
	const char* name() const override { return "StateC"; }

	void        on_entry(AppMachine& sm) override { sm->log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm->log(CallType::Exit, name()); }
	hsm::Result handle(AppMachine& sm, const Event&) override {
		sm->log(CallType::Run, name());
		sm.transition(ID_D);
		return hsm::Result::Done;
	}
};

class StateD : public TestState {
public:
	const char* name() const override { return "StateD"; }

	void on_entry(AppMachine& sm) override { sm->log(CallType::Entry, name()); }
};

}  // namespace

TEST_CASE("Hierarchical State Machine", "[hierarchical]") {
	AppMachine sm;

	// Initial: ParentAB -> StateA
	sm.start(ID_A, [](hsm::Scope<TestTraits>& root) {
		root.state<ParentAB>(ID_PAB).with([](hsm::Scope<TestTraits>& pab) {
			pab.state<StateA>(ID_A);
			pab.state<StateB>(ID_B);
		});

		root.state<ParentC>(ID_PC).with([](hsm::Scope<TestTraits>& pc) { pc.state<StateC>(ID_C); });

		root.state<StateD>(ID_D);
	});

	REQUIRE(sm->calls.size() == 2);
	CHECK(sm->calls[0] == CallRecord{CallType::Entry, "ParentAB"});
	CHECK(sm->calls[1] == CallRecord{CallType::Entry, "StateA"});

	sm->clear();

	// Update 1: StateA run -> transition to StateB
	// A and B share ParentAB.
	// Expected: StateA Exit, StateB Entry. (ParentAB stays)
	sm.dispatch();

	REQUIRE(sm->calls.size() == 3);
	CHECK(sm->calls[0] == CallRecord{CallType::Run, "StateA"});
	CHECK(sm->calls[1] == CallRecord{CallType::Exit, "StateA"});
	CHECK(sm->calls[2] == CallRecord{CallType::Entry, "StateB"});

	sm->clear();

	// Update 2: StateB run -> transition to StateC
	// B (ParentAB) -> C (ParentC)
	// Expected: StateB Exit, ParentAB Exit, ParentC Entry, StateC Entry.
	sm.dispatch();

	REQUIRE(sm->calls.size() == 5);
	CHECK(sm->calls[0] == CallRecord{CallType::Run, "StateB"});
	CHECK(sm->calls[1] == CallRecord{CallType::Exit, "StateB"});
	CHECK(sm->calls[2] == CallRecord{CallType::Exit, "ParentAB"});
	CHECK(sm->calls[3] == CallRecord{CallType::Entry, "ParentC"});
	CHECK(sm->calls[4] == CallRecord{CallType::Entry, "StateC"});

	sm->clear();

	// Update 3: StateC run -> transition to StateD
	// C (ParentC) -> D (No parent)
	// Expected: StateC Exit, ParentC Exit, StateD Entry.
	sm.dispatch();

	REQUIRE(sm->calls.size() == 4);
	CHECK(sm->calls[0] == CallRecord{CallType::Run, "StateC"});
	CHECK(sm->calls[1] == CallRecord{CallType::Exit, "StateC"});
	CHECK(sm->calls[2] == CallRecord{CallType::Exit, "ParentC"});
	CHECK(sm->calls[3] == CallRecord{CallType::Entry, "StateD"});
}
