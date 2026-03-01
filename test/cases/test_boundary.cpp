#include <string>
#include <vector>

#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

struct EmptyEvent {};

struct Traits {
	using StateID = int;
	using Event   = EmptyEvent;
	struct Context {
		int  counter = 0;
		bool failed  = false;
	};
};

using Machine = hsm::Machine<Traits>;
using Scope   = hsm::Scope<Traits>;

}  // namespace

TEST_CASE("Infinite Loop Protection", "[hsm][boundary]") {
	Machine sm;

	// The `start` command executes initial transition to 0, which triggers 1, which triggers 0...
	SECTION("Infinite jumping throws runtime_error to break execution") {
		REQUIRE_THROWS_AS(sm.start(0,
								   [](Scope &scope) {
									   scope.state(0).name("JumpTo1").on_entry([](Machine &sm) { sm.transition(1); });
									   scope.state(1).name("JumpTo0").on_entry([](Machine &sm) { sm.transition(0); });
								   }),
						  std::runtime_error);
	}

	SECTION("The machine is correctly flagged as terminated after loop breaking") {
		try {
			sm.start(0, [](Scope &scope) {
				scope.state(0).name("JumpTo1").on_entry([](Machine &sm) { sm.transition(1); });
				scope.state(1).name("JumpTo0").on_entry([](Machine &sm) { sm.transition(0); });
			});
		} catch (...) {}
		REQUIRE(sm.terminated() == true);
	}
}

TEST_CASE("Machine Termination Handled Robustly", "[hsm][boundary]") {
	Machine sm;

	sm.start(0, [](Scope &scope) {
		scope.state(0).name("Terminator").handle([](Machine &sm, const EmptyEvent &) {
			sm.context().counter++;
			sm.stop();                  // Hard stop requested
			sm.dispatch(EmptyEvent{});  // Try to stuff the queue
			sm.transition(1);           // Try to initiate a transition
			return hsm::Result::Done;
		});

		scope.state(1).name("Unreachable").on_entry([](Machine &sm) { sm.context().failed = true; });
	});

	SECTION("Stop immediately blackholes transitions and clears active event queue") {
		sm.dispatch();

		REQUIRE(sm.terminated() == true);
		REQUIRE(sm.context().counter == 1);
		REQUIRE(sm.context().failed == false);  // The queued event and the transition were swallowed!
		REQUIRE(sm.current_state_id() == 0);    // Stuck on 0 since transition didn't finalize.
	}
}
