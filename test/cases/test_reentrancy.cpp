#include <string>
#include <vector>

#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

struct EventA {
	int payload = 1;
};
struct EventB {
	int payload = 2;
};
struct EventC {
	int payload = 3;
};

struct ReentrancyContext {
	std::vector<std::string> log;
};

struct ReentrancyTraits {
	using StateID = int;
	using Event   = hsm::Matcher<hsm::FastCastPolicy, ReentrancyTraits> /* placeholders */;
	using Context = ReentrancyContext;
};

// Mocking a Base Event for dynamic traits since we need different event types.
struct BaseEvent {
	virtual ~BaseEvent() = default;
};

struct PolyEventA : BaseEvent {
	int a        = 1;
	PolyEventA() = default;
	PolyEventA(int a) : a(a) {}
};
struct PolyEventB : BaseEvent {
	int b        = 2;
	PolyEventB() = default;
	PolyEventB(int b) : b(b) {}
};
struct PolyEventC : BaseEvent {
	int c        = 3;
	PolyEventC() = default;
	PolyEventC(int c) : c(c) {}
};

struct PolyTraits {
	using StateID = int;
	using Event   = BaseEvent;
	using Context = ReentrancyContext;
};

}  // namespace

TEST_CASE("Re-entrancy Queue Scheduling", "[hsm][reentrancy]") {
	hsm::Machine<PolyTraits> sm;

	sm.start(0, [](hsm::Scope<PolyTraits> &scope) {
		scope.state(0)
			.name("State0")
			.on_entry([](hsm::Machine<PolyTraits> &sm) { sm.context().log.push_back("State0:Entry"); })
			.handle([](hsm::Machine<PolyTraits> &sm, const PolyTraits::Event &ev) {
				return hsm::match(sm, ev)
					.on<PolyEventA>([](hsm::Machine<PolyTraits> &sm, const PolyEventA &a) {
						sm.context().log.push_back("State0:A_begin(" + std::to_string(a.a) + ")");
						// Dispatch while handling! This should queue it up.
						sm.dispatch(PolyEventB{20});
						sm.context().log.push_back("State0:A_end");
						return hsm::Result::Done;
					})
					.on<PolyEventB>([](hsm::Machine<PolyTraits> &sm, const PolyEventB &b) {
						sm.context().log.push_back("State0:B(" + std::to_string(b.b) + ")");
						// Queue another one to see chained deferral
						sm.dispatch(PolyEventC{300});
						return hsm::Result::Done;
					})
					.on<PolyEventC>([](hsm::Machine<PolyTraits> &sm, const PolyEventC &c) {
						sm.context().log.push_back("State0:C(" + std::to_string(c.c) + ")");
						return hsm::Result::Done;
					})
					.otherwise([](hsm::Machine<PolyTraits> &, const PolyTraits::Event &) { return hsm::Result::Pass; });
			});
	});

	SECTION("Direct Reentrancy Appends Events Without Interrupting Active Execution") {
		sm.context().log.clear();
		sm.dispatch(PolyEventA{10});

		std::vector<std::string> expected = {
			"State0:A_begin(10)",
			"State0:A_end",  // B was queued, not executed inline
			"State0:B(20)",  // Queue popped B and ran it
			"State0:C(300)"  // Queue popped C (which was queued by B)
		};
		REQUIRE(sm.context().log == expected);
	}
}

TEST_CASE("Transition with Concurrent Dispatch", "[hsm][reentrancy][transition]") {
	hsm::Machine<PolyTraits> sm;

	sm.start(0, [](hsm::Scope<PolyTraits> &scope) {
		scope.state(0)
			.name("State0")
			.on_entry([](hsm::Machine<PolyTraits> &sm) { sm.context().log.push_back("State0:Entry"); })
			.on_exit([](hsm::Machine<PolyTraits> &sm) { sm.context().log.push_back("State0:Exit"); })
			.handle([](hsm::Machine<PolyTraits> &sm, const PolyTraits::Event &ev) {
				return hsm::match(sm, ev)
					.on<PolyEventA>([](hsm::Machine<PolyTraits> &sm, const PolyEventA &) {
						sm.context().log.push_back("State0:A");
						// Transition out and dispatch simultaneously
						sm.transition(1);
						sm.dispatch(PolyEventB{5});
						return hsm::Result::Done;
					})
					.otherwise([](hsm::Machine<PolyTraits> &, const PolyTraits::Event &) { return hsm::Result::Pass; });
			});

		scope.state(1)
			.name("State1")
			.on_entry([](hsm::Machine<PolyTraits> &sm) { sm.context().log.push_back("State1:Entry"); })
			.handle([](hsm::Machine<PolyTraits> &sm, const PolyTraits::Event &ev) {
				return hsm::match(sm, ev)
					.on<PolyEventB>([](hsm::Machine<PolyTraits> &sm, const PolyEventB &b) {
						sm.context().log.push_back("State1:B(" + std::to_string(b.b) + ")");
						return hsm::Result::Done;
					})
					.otherwise([](hsm::Machine<PolyTraits> &, const PolyTraits::Event &) { return hsm::Result::Pass; });
			});
	});

	SECTION("Queued Events are Evaluated After Transitions") {
		sm.context().log.clear();
		sm.dispatch(PolyEventA{-1});

		std::vector<std::string> expected = {
			"State0:A", "State0:Exit", "State1:Entry",
			"State1:B(5)"  // B was fired when in State0, but evaluated only after State1 became active
		};
		REQUIRE(sm.context().log == expected);
	}
}
