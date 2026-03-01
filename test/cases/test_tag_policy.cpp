#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

enum class StateID { ROOT, S1, S2 };
enum class EventID { A, B };

struct Event {
	const EventID id;
	Event(EventID id) : id(id) {}
	virtual ~Event() = default;
};

struct EventA : Event {
	static constexpr EventID ID = EventID::A;
	EventA() : Event(ID) {}
};

struct EventB : Event {
	static constexpr EventID ID = EventID::B;
	EventB() : Event(ID) {}
};

struct Traits {
	using StateID = StateID;
	using Event   = Event;
	struct Context {
		int a_count = 0;
		int b_count = 0;
	};
};

using Machine = hsm::Machine<Traits>;

}  // namespace

TEST_CASE("TagCastPolicy dispatching", "[tag_policy]") {
	Machine sm;

	sm.start(StateID::S1, [](hsm::Scope<Traits>& s) {
		s.state(StateID::S1).handle([](Machine& m, const Event& ev) {
			return hsm::match<hsm::TagCastPolicy>(m, ev)
				.on<EventA>([](Machine& m, const EventA&) {
					m.context().a_count++;
					m.transition(StateID::S2);
					return hsm::Result::Done;
				})
				.otherwise([](Machine&, const Event&) { return hsm::Result::Pass; });
		});

		s.state(StateID::S2).handle([](Machine& m, const Event& ev) {
			return hsm::match<hsm::TagCastPolicy>(m, ev)
				.on<EventB>([](Machine& m, const EventB&) {
					m.context().b_count++;
					m.transition(StateID::S1);
					return hsm::Result::Done;
				})
				.otherwise([](Machine&, const Event&) { return hsm::Result::Pass; });
		});
	});

	REQUIRE(sm.current_state_id() == StateID::S1);
	REQUIRE(sm.context().a_count == 0);
	REQUIRE(sm.context().b_count == 0);

	sm.dispatch(EventA{});
	REQUIRE(sm.current_state_id() == StateID::S2);
	REQUIRE(sm.context().a_count == 1);
	REQUIRE(sm.context().b_count == 0);

	sm.dispatch(EventB{});
	REQUIRE(sm.current_state_id() == StateID::S1);
	REQUIRE(sm.context().a_count == 1);
	REQUIRE(sm.context().b_count == 1);
}
