#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

enum class StateID { ROOT, S1, S2 };

struct Event {
	virtual ~Event() = default;
};

// Base event for group match
struct GroupEvent : Event {};

struct EventA : GroupEvent {};
struct EventB : GroupEvent {};
struct EventC : Event {};

struct Traits {
	using StateID = StateID;
	using Event   = Event;
	struct Context {
		int group_count = 0;
		int c_count     = 0;
	};
};

// Explicitly use DynamicCastPolicy
using Machine = hsm::Machine<Traits>;

}  // namespace

TEST_CASE("DynamicCastPolicy dispatching (polymorphic match)", "[dynamic_policy]") {
	Machine sm;

	sm.start(StateID::S1, [](hsm::Scope<Traits>& s) {
		s.state(StateID::S1).handle([](Machine& m, const Event& ev) {
			return hsm::match<hsm::DynamicCastPolicy>(m, ev)
				// Match ANY event derived from GroupEvent
				.on<GroupEvent>([](Machine& m, const GroupEvent&) {
					m.context().group_count++;
					return hsm::Result::Done;
				})
				// Match specific EventC
				.on<EventC>([](Machine& m, const EventC&) {
					m.context().c_count++;
					return hsm::Result::Done;
				})
				.otherwise([](Machine&, const Event&) { return hsm::Result::Pass; });
		});
	});

	REQUIRE(sm.current_state_id() == StateID::S1);
	REQUIRE(sm.context().group_count == 0);
	REQUIRE(sm.context().c_count == 0);

	// EventA derives from GroupEvent -> should map to GroupEvent
	sm.dispatch(EventA{});
	REQUIRE(sm.context().group_count == 1);
	REQUIRE(sm.context().c_count == 0);

	// EventB derives from GroupEvent -> should map to GroupEvent
	sm.dispatch(EventB{});
	REQUIRE(sm.context().group_count == 2);
	REQUIRE(sm.context().c_count == 0);

	// EventC is standalone -> should map to EventC
	sm.dispatch(EventC{});
	REQUIRE(sm.context().group_count == 2);
	REQUIRE(sm.context().c_count == 1);
}
