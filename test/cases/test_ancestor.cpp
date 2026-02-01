#include <catch.hpp>
#include <hsm/hsm.hpp>
#include <string>
#include <vector>

namespace {

// Define call types and records for verification
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
enum StateID { ID_P05, ID_P04, ID_P03, ID_P02, ID_P01, ID_StateA, ID_StateB, ID_StateC, ID_StateD };

struct TestTraits {
	using Context = TestContext;
	using Event   = Event;
	using StateID = StateID;
};

using AppState = hsm::State<TestTraits>;

// Forward declarations
class P05;
class P04;
class P03;
class P02;
class P01;
class StateA;
class StateB;
class StateC;
class StateD;

// Root
class P05 : public AppState {
public:
	const char* name() const override { return "P05"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override;
};

class P04 : public AppState {
public:
	const char* name() const override { return "P04"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		return hsm::Result::Pass;
	}
};

class P03 : public AppState {
public:
	const char* name() const override { return "P03"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		return hsm::Result::Pass;
	}
};

class P02 : public AppState {
public:
	const char* name() const override { return "P02"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		return hsm::Result::Pass;
	}
};

class P01 : public AppState {
public:
	const char* name() const override { return "P01"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		return hsm::Result::Pass;
	}
};

class StateA : public AppState {
public:
	const char* name() const override { return "StateA"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override;
};

class StateB : public AppState {
public:
	const char* name() const override { return "StateB"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override {
		sm.context().log(CallType::Run, name());
		return hsm::Result::Pass;
	}
};

class StateC : public AppState {
public:
	const char* name() const override { return "StateC"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Exit, name()); }
	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override;
};

class StateD : public AppState {
public:
	const char* name() const override { return "StateD"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log(CallType::Entry, name()); }
};

// Implementations requiring forward declared classes
hsm::Result P05::handle(hsm::Machine<TestTraits>& sm, const Event&) {
	sm.context().log(CallType::Run, name());
	sm.transition(ID_StateC);
	return hsm::Result::Pass;
}

hsm::Result StateA::handle(hsm::Machine<TestTraits>& sm, const Event&) {
	sm.context().log(CallType::Run, name());
	sm.transition(ID_StateB);
	return hsm::Result::Pass;
}

hsm::Result StateC::handle(hsm::Machine<TestTraits>& sm, const Event&) {
	sm.context().log(CallType::Run, name());
	sm.transition(ID_StateD);
	return hsm::Result::Done;
}

}  // namespace

TEST_CASE("Ancestor Propagation and Transition", "[ancestor]") {
	hsm::Machine<TestTraits> sm;

	auto config = [](hsm::Scope<TestTraits>& root) {
		root.state<P05>(ID_P05).with([](hsm::Scope<TestTraits>& p05) {
			p05.state<P04>(ID_P04).with([](hsm::Scope<TestTraits>& p04) {
				p04.state<P03>(ID_P03).with([](hsm::Scope<TestTraits>& p03) {
					p03.state<P02>(ID_P02).with([](hsm::Scope<TestTraits>& p02) {
						p02.state<P01>(ID_P01).with([](hsm::Scope<TestTraits>& p01) {
							p01.state<StateA>(ID_StateA);
							p01.state<StateB>(ID_StateB);
						});
					});
				});
			});
		});

		root.state<StateC>(ID_StateC);
		root.state<StateD>(ID_StateD);
	};

	// 1. Start A
	sm.start(ID_StateA, config);

	// Check entry order: P05 -> P04 -> P03 -> P02 -> P01 -> A
	REQUIRE(sm.context().calls.size() == 6);
	CHECK(sm.context().calls[0] == CallRecord{CallType::Entry, "P05"});
	CHECK(sm.context().calls[1] == CallRecord{CallType::Entry, "P04"});
	CHECK(sm.context().calls[2] == CallRecord{CallType::Entry, "P03"});
	CHECK(sm.context().calls[3] == CallRecord{CallType::Entry, "P02"});
	CHECK(sm.context().calls[4] == CallRecord{CallType::Entry, "P01"});
	CHECK(sm.context().calls[5] == CallRecord{CallType::Entry, "StateA"});

	sm.context().clear();

	// 2. Run A -> Transition to B
	// A run (Propagate but transition stops it) -> Exit A -> Entry B
	sm.handle(Event{});

	REQUIRE(sm.context().calls.size() == 3);
	CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "StateA"});
	CHECK(sm.context().calls[1] == CallRecord{CallType::Exit, "StateA"});
	CHECK(sm.context().calls[2] == CallRecord{CallType::Entry, "StateB"});

	sm.context().clear();

	// 3. Run B -> Propagate up to P05 -> Transition to C
	sm.handle(Event{});

	REQUIRE(sm.context().calls.size() == 13);
	CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "StateB"});
	CHECK(sm.context().calls[1] == CallRecord{CallType::Run, "P01"});
	CHECK(sm.context().calls[2] == CallRecord{CallType::Run, "P02"});
	CHECK(sm.context().calls[3] == CallRecord{CallType::Run, "P03"});
	CHECK(sm.context().calls[4] == CallRecord{CallType::Run, "P04"});
	CHECK(sm.context().calls[5] == CallRecord{CallType::Run, "P05"});
	CHECK(sm.context().calls[6] == CallRecord{CallType::Exit, "StateB"});
	CHECK(sm.context().calls[7] == CallRecord{CallType::Exit, "P01"});
	CHECK(sm.context().calls[8] == CallRecord{CallType::Exit, "P02"});
	CHECK(sm.context().calls[9] == CallRecord{CallType::Exit, "P03"});
	CHECK(sm.context().calls[10] == CallRecord{CallType::Exit, "P04"});
	CHECK(sm.context().calls[11] == CallRecord{CallType::Exit, "P05"});
	CHECK(sm.context().calls[12] == CallRecord{CallType::Entry, "StateC"});

	sm.context().clear();

	// 4. Run C -> Transition to D
	sm.handle(Event{});
	REQUIRE(sm.context().calls.size() == 3);
	CHECK(sm.context().calls[0] == CallRecord{CallType::Run, "StateC"});
	CHECK(sm.context().calls[1] == CallRecord{CallType::Exit, "StateC"});
	CHECK(sm.context().calls[2] == CallRecord{CallType::Entry, "StateD"});
}
