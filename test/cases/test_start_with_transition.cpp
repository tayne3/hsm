#include <catch.hpp>
#include <hsm/hsm.hpp>
#include <string>
#include <vector>

namespace {

struct TestContext {
	std::vector<std::string> calls;
	void                     log(const std::string& msg) { calls.push_back(msg); }
	void                     clear() { calls.clear(); }
};

struct Event {};
enum StateID { ID_A, ID_B };

struct TestTraits {
	using Context = TestContext;
	using Event   = Event;
	using StateID = StateID;
};

using AppState = hsm::State<TestTraits>;

class StateA : public AppState {
public:
	const char* name() const override { return "StateA"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log("Entry A"); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log("Exit A"); }
};

class StateB : public AppState {
public:
	const char* name() const override { return "StateB"; }
	void        on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().log("Entry B"); }
	void        on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().log("Exit B"); }
};

}  // namespace

TEST_CASE("Start with transition_to", "[hsm]") {
	hsm::Machine<TestTraits> sm;

	auto config = [](hsm::Scope<TestTraits>& root) {
		root.state<StateA>(ID_A);
		root.state<StateB>(ID_B);
	};

	SECTION("Use start() as start") {
		REQUIRE(sm.started() == false);

		// Start the machine
		sm.start(ID_A, config);

		// Check if StateA is entered
		REQUIRE(sm.current_state_id() == ID_A);
		REQUIRE(sm.context().calls.size() == 1);
		CHECK(sm.context().calls[0] == "Entry A");

		sm.context().clear();

		// Normal transition should still work
		sm.transition(ID_B);
		// Need run() to process transition
		sm.handle(Event{});

		REQUIRE(sm.current_state_id() == ID_B);
		// Run A (no-op, but happens) -> Exit A -> Entry B
		REQUIRE(sm.context().calls.size() == 2);
		CHECK(sm.context().calls[0] == "Exit A");
		CHECK(sm.context().calls[1] == "Entry B");
	}
}
