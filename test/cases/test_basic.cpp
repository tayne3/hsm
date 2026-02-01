#include <catch.hpp>
#include <hsm/hsm.hpp>
#include <string>
#include <vector>

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

struct TestContext {
	std::vector<CallRecord> calls;
	void                    log(CallType type, const char* name) { calls.push_back({type, name}); }
	void                    clear() { calls.clear(); }
};

struct Event {};
enum StateID { ID_Idle, ID_Active };

struct TestTraits {
	using Context = TestContext;
	using Event   = Event;
	using StateID = StateID;
};

using AppState = hsm::State<TestTraits>;

class IdleState : public AppState {
public:
	const char* name() const override { return "Idle"; }

	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		return hsm::Result::Done;
	}
};

class ActiveState : public AppState {
public:
	const char* name() const override { return "Active"; }

	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		return hsm::Result::Done;
	}
};

}  // namespace

TEST_CASE("Basic State Machine Operations", "[basic]") {
	hsm::Machine<TestTraits> sm;

	auto config = [](hsm::Scope<TestTraits>& root) {
		root.state<IdleState>(ID_Idle);
		root.state<ActiveState>(ID_Active);
	};

	SECTION("Initialization") {
		sm.start(ID_Idle, config);
		REQUIRE(sm.context().calls.size() == 1);
		CHECK(sm.context().calls[0] == CallRecord{CallType::Entry, "Idle"});
	}

	SECTION("Simple Transition") {
		sm.start(ID_Idle, config);
		sm.context().clear();

		sm.transition(ID_Active);
		sm.handle();

		REQUIRE(sm.context().calls.size() == 3);
		// 1. Run Idle
		// 2. Exit Idle
		// 3. Entry Active
		CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "Idle"});
		CHECK(sm.context().calls[1] == CallRecord{CallType::Exit, "Idle"});
		CHECK(sm.context().calls[2] == CallRecord{CallType::Entry, "Active"});
	}

	SECTION("Entry Run Exit Order") {
		sm.start(ID_Idle, config);
		sm.transition(ID_Active);
		sm.context().clear();

		sm.handle();  // Transitions to Active (Idle Run -> Exit Idle -> Entry Active)

		sm.context().clear();
		sm.handle();  // Run Active

		REQUIRE(sm.context().calls.size() == 1);
		CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "Active"});
	}

	SECTION("Self Transition via transition") {
		sm.start(ID_Idle, config);
		sm.context().clear();

		sm.transition(ID_Idle);
		sm.handle();  // Run Idle -> Exit Idle -> Entry Idle

		REQUIRE(sm.context().calls.size() == 3);
		CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "Idle"});
		CHECK(sm.context().calls[1] == CallRecord{CallType::Exit, "Idle"});
		CHECK(sm.context().calls[2] == CallRecord{CallType::Entry, "Idle"});
	}
}
