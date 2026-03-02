#include <string>
#include <vector>

#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

enum class CallType {
	Entry,
	Run,
	Exit,
};

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

enum StateID {
	ID_Idle,
	ID_Active,
	ID_Busy,
};

struct Event {};
struct Traits {
	struct Context {
		std::vector<CallRecord> calls;
		void                    log(CallType type, const char* name) { calls.push_back({type, name}); }
		void                    clear() { calls.clear(); }
	};
	using Event   = ::Event;
	using StateID = ::StateID;
};

using TestState   = hsm::State<Traits>;
using TestMachine = hsm::Machine<Traits>;
using TestScope   = hsm::Scope<Traits>;

class IdleState : public TestState {
public:
	const char* name() const override { return "Idle"; }

	void        on_entry(TestMachine& sm) override { sm->log(CallType::Entry, name()); }
	void        on_exit(TestMachine& sm) override { sm->log(CallType::Exit, name()); }
	hsm::Result handle(TestMachine& sm, const Event&) override {
		sm->log(CallType::Run, name());
		return hsm::Result::Done;
	}
};

class ActiveState : public TestState {
public:
	const char* name() const override { return "Active"; }

	void        on_entry(TestMachine& sm) override { sm->log(CallType::Entry, name()); }
	void        on_exit(TestMachine& sm) override { sm->log(CallType::Exit, name()); }
	hsm::Result handle(TestMachine& sm, const Event&) override {
		sm->log(CallType::Run, name());
		return hsm::Result::Done;
	}
};

}  // namespace

TEST_CASE("Basic State Machine Operations", "[basic]") {
	TestMachine sm;
	sm.start(ID_Idle, [](TestScope& root) {
		root.state<IdleState>(ID_Idle);
		root.state<ActiveState>(ID_Active);
	});

	SECTION("Initialization") {
		REQUIRE(sm.context().calls.size() == 1);
		REQUIRE(sm->calls.size() == 1);
		CHECK(sm.context().calls[0] == CallRecord{CallType::Entry, "Idle"});
		CHECK(sm->calls[0] == CallRecord{CallType::Entry, "Idle"});
	}

	SECTION("Simple Transition") {
		sm->clear();

		sm.transition(ID_Active);

		REQUIRE(sm->calls.size() == 2);
		// 1. Exit Idle
		// 2. Entry Active
		CHECK(sm->calls[0] == CallRecord{CallType::Exit, "Idle"});
		CHECK(sm->calls[1] == CallRecord{CallType::Entry, "Active"});
	}

	SECTION("Entry Run Exit Order") {
		sm.transition(ID_Active);
		sm->clear();

		sm.dispatch();  // Transitions to Active (Idle Run -> Exit Idle -> Entry Active)

		sm->clear();
		sm.dispatch();  // Run Active

		REQUIRE(sm->calls.size() == 1);
		CHECK(sm->calls[0] == CallRecord{CallType::Run, "Active"});
	}

	SECTION("Self Transition via transition") {
		sm->clear();

		sm.transition(ID_Idle);

		REQUIRE(sm->calls.size() == 2);
		CHECK(sm->calls[0] == CallRecord{CallType::Exit, "Idle"});
		CHECK(sm->calls[1] == CallRecord{CallType::Entry, "Idle"});
	}

	SECTION("Self transition triggers exit and entry") {
		// Equivalent to previous test_self_transition.cpp
		sm->clear();
		sm.transition(ID_Idle);  // Assuming Idle works similarly to 'NormalState' in the old test

		REQUIRE(sm->calls.size() == 2);
		CHECK(sm->calls[0] == CallRecord{CallType::Exit, "Idle"});
		CHECK(sm->calls[1] == CallRecord{CallType::Entry, "Idle"});
	}

	SECTION("Start with transition_to()") {
		// Replicating basic behavior from test_start_with_transition.cpp
		TestMachine sm_start;
		REQUIRE(sm_start.started() == false);

		sm_start.start(ID_Idle, [](TestScope& root) {
			root.state<IdleState>(ID_Idle);
			root.state<ActiveState>(ID_Active);
		});

		REQUIRE(sm_start.current_state_id() == ID_Idle);
		REQUIRE(sm_start.context().calls.size() == 1);
		CHECK(sm_start.context().calls[0] == CallRecord{CallType::Entry, "Idle"});

		sm_start->clear();

		sm_start.transition(ID_Active);

		REQUIRE(sm_start.current_state_id() == ID_Active);
		REQUIRE(sm_start->calls.size() == 2);
		CHECK(sm_start->calls[0] == CallRecord{CallType::Exit, "Idle"});
		CHECK(sm_start->calls[1] == CallRecord{CallType::Entry, "Active"});
	}
}

TEST_CASE("Exception Handling", "[exception]") {
	TestMachine sm;

	SECTION("start() throws if already started") {
		sm.start(ID_Idle, [](TestScope& root) { root.state<IdleState>(ID_Idle); });
		REQUIRE_THROWS_AS(sm.start(ID_Idle, [](TestScope& root) { root.state<IdleState>(ID_Idle); }), std::logic_error);
	}

	SECTION("start() throws if initial state ID not found") {
		REQUIRE_THROWS_AS(sm.start(ID_Active, [](TestScope& root) { root.state<IdleState>(ID_Idle); }), std::invalid_argument);
	}

	SECTION("transition() throws if target state ID not found") {
		sm.start(ID_Active, [](TestScope& root) { root.state<ActiveState>(ID_Active); });
		REQUIRE_THROWS_AS(sm.transition(ID_Idle), std::runtime_error);
	}

	SECTION("transition() throws during Exit phase") {
		TestMachine sm;
		sm.start(ID_Idle, [](TestScope& root) {
			root.state(ID_Idle).on_exit([](TestMachine& sm) { sm.transition(ID_Active); });
			root.state<ActiveState>(ID_Active);
		});
		REQUIRE_THROWS_AS(sm.transition(ID_Active), std::runtime_error);
	}

	SECTION("Duplicate StateID throws 1") {
		REQUIRE_THROWS_AS(sm.start(ID_Idle,
								   [](TestScope& root) {
									   root.state<ActiveState>(ID_Active);
									   root.state<IdleState>(ID_Idle);
									   root.state<ActiveState>(ID_Active);
								   }),
						  std::invalid_argument);
	}

	SECTION("Duplicate StateID throws 2") {
		REQUIRE_THROWS_AS(sm.start(ID_Idle,
								   [](TestScope& root) {
									   root.state(ID_Idle);
									   root.state(ID_Active);
									   root.state(ID_Idle);
								   }),
						  std::invalid_argument);
	}

	SECTION("Duplicate StateID throws 3") {
		REQUIRE_THROWS_AS(sm.start(ID_Idle,
								   [](TestScope& root) {
									   root.state(ID_Idle);
									   root.state<IdleState>(ID_Idle);
								   }),
						  std::invalid_argument);
	}
}
