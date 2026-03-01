#include <string>
#include <vector>

#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

enum StateID {
	ID_A,
	ID_B,
};

struct TestTraits {
	struct Context {
		std::vector<std::string> calls;
		void                     log(const std::string& msg) { calls.push_back(msg); }
		void                     clear() { calls.clear(); }
	};
	struct Event {};
	using StateID = StateID;
};

using TestState   = hsm::State<TestTraits>;
using TestMachine = hsm::Machine<TestTraits>;

class StateA : public TestState {
public:
	const char* name() const override { return "StateA"; }
	void        on_entry(TestMachine& sm) override { sm->log("Entry A"); }
	void        on_exit(TestMachine& sm) override { sm->log("Exit A"); }
};

class StateB : public TestState {
public:
	const char* name() const override { return "StateB"; }
	void        on_entry(TestMachine& sm) override { sm->log("Entry B"); }
	void        on_exit(TestMachine& sm) override { sm->log("Exit B"); }
};

}  // namespace

TEST_CASE("Start with transition_to", "[hsm]") {
	TestMachine sm;

	SECTION("Use start() as start") {
		REQUIRE(sm.started() == false);

		// Start the machine
		sm.start(ID_A, [](hsm::Scope<TestTraits>& root) {
			root.state<StateA>(ID_A);
			root.state<StateB>(ID_B);
		});

		// Check if StateA is entered
		REQUIRE(sm.current_state_id() == ID_A);
		REQUIRE(sm->calls.size() == 1);
		CHECK(sm->calls[0] == "Entry A");

		sm->clear();

		// Normal transition should still work
		sm.transition(ID_B);
		// Need run() to process transition
		sm.dispatch();

		REQUIRE(sm.current_state_id() == ID_B);
		// Run A (no-op, but happens) -> Exit A -> Entry B
		REQUIRE(sm->calls.size() == 2);
		CHECK(sm->calls[0] == "Exit A");
		CHECK(sm->calls[1] == "Entry B");
	}
}
