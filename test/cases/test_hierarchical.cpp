#include <string>
#include <vector>

#include "../catch.hpp"
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

struct TestContext {
	std::vector<CallRecord> calls;
	void                    log(CallType type, const char* name) { calls.push_back({type, name}); }
	void                    clear() { calls.clear(); }
};

struct Event {};
enum StateID { ID_PAB, ID_PC, ID_A, ID_B, ID_C, ID_D };

struct TestTraits {
	using Context = TestContext;
	using Event   = Event;
	using StateID = StateID;
};

using AppState   = hsm::State<TestTraits>;
using AppMachine = hsm::Machine<TestTraits>;

class ParentAB : public AppState {
public:
	const char* name() const override { return "ParentAB"; }
	void        on_entry(AppMachine& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(AppMachine& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		return hsm::Result::Done;
	}
};

class ParentC : public AppState {
public:
	const char* name() const override { return "ParentC"; }
	void        on_entry(AppMachine& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm.context().log(CallType::Exit, name()); }
};

class StateA : public AppState {
public:
	const char* name() const override { return "StateA"; }

	void        on_entry(AppMachine& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(AppMachine& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		sm.transition(ID_B);
		return hsm::Result::Done;
	}
};

class StateB : public AppState {
public:
	const char* name() const override { return "StateB"; }

	void        on_entry(AppMachine& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(AppMachine& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		sm.transition(ID_C);
		return hsm::Result::Done;
	}
};

class StateC : public AppState {
public:
	const char* name() const override { return "StateC"; }

	void        on_entry(AppMachine& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(AppMachine& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(AppMachine& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		sm.transition(ID_D);
		return hsm::Result::Done;
	}
};

class StateD : public AppState {
public:
	const char* name() const override { return "StateD"; }

	void on_entry(AppMachine& sm) override { sm.context().log(CallType::Entry, name()); }
};

}  // namespace

TEST_CASE("Hierarchical State Machine", "[hierarchical]") {
	AppMachine sm;

	auto config = [](hsm::Scope<TestTraits>& root) {
		root.state<ParentAB>(ID_PAB).with([](hsm::Scope<TestTraits>& pab) {
			pab.state<StateA>(ID_A);
			pab.state<StateB>(ID_B);
		});

		root.state<ParentC>(ID_PC).with([](hsm::Scope<TestTraits>& pc) { pc.state<StateC>(ID_C); });

		root.state<StateD>(ID_D);
	};

	// Initial: ParentAB -> StateA
	sm.start(ID_A, config);

	REQUIRE(sm.context().calls.size() == 2);
	CHECK(sm.context().calls[0] == CallRecord{CallType::Entry, "ParentAB"});
	CHECK(sm.context().calls[1] == CallRecord{CallType::Entry, "StateA"});

	sm.context().clear();

	// Update 1: StateA run -> transition to StateB
	// A and B share ParentAB.
	// Expected: StateA Exit, StateB Entry. (ParentAB stays)
	sm.handle(Event{});

	REQUIRE(sm.context().calls.size() == 3);
	CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "StateA"});
	CHECK(sm.context().calls[1] == CallRecord{CallType::Exit, "StateA"});
	CHECK(sm.context().calls[2] == CallRecord{CallType::Entry, "StateB"});

	sm.context().clear();

	// Update 2: StateB run -> transition to StateC
	// B (ParentAB) -> C (ParentC)
	// Expected: StateB Exit, ParentAB Exit, ParentC Entry, StateC Entry.
	sm.handle(Event{});

	REQUIRE(sm.context().calls.size() == 5);
	CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "StateB"});
	CHECK(sm.context().calls[1] == CallRecord{CallType::Exit, "StateB"});
	CHECK(sm.context().calls[2] == CallRecord{CallType::Exit, "ParentAB"});
	CHECK(sm.context().calls[3] == CallRecord{CallType::Entry, "ParentC"});
	CHECK(sm.context().calls[4] == CallRecord{CallType::Entry, "StateC"});

	sm.context().clear();

	// Update 3: StateC run -> transition to StateD
	// C (ParentC) -> D (No parent)
	// Expected: StateC Exit, ParentC Exit, StateD Entry.
	sm.handle(Event{});

	REQUIRE(sm.context().calls.size() == 4);
	CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "StateC"});
	CHECK(sm.context().calls[1] == CallRecord{CallType::Exit, "StateC"});
	CHECK(sm.context().calls[2] == CallRecord{CallType::Exit, "ParentC"});
	CHECK(sm.context().calls[3] == CallRecord{CallType::Entry, "StateD"});
}
