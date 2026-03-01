#include <iostream>
#include <string>

#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

enum class EventId {
	CLICK   = 1,
	KEY     = 2,
	UNKNOWN = 3,
};

struct Event {
	const EventId id;
	Event(EventId i) : id(i) {}
	virtual ~Event() = default;
};

struct Traits {
	using StateID = int;
	using Event   = Event;
	struct Context {
		std::string log;
	};
};
using Machine = hsm::Machine<Traits>;
using State   = hsm::State<Traits>;
using Scope   = hsm::Scope<Traits>;

struct ClickEvent : public Event {
	int x, y;
	ClickEvent(int x, int y) : Event(EventId::CLICK), x(x), y(y) {}
};

struct KeyEvent : public Event {
	int key_code;
	KeyEvent(int k) : Event(EventId::KEY), key_code(k) {}
};

struct UnknownEvent : public Event {
	UnknownEvent() : Event(EventId::UNKNOWN) {}
};

struct State0 : public State {
	hsm::Result handle(Machine &m, const Event &e) override {
		switch (e.id) {
			case EventId::CLICK: {
				const auto &click = static_cast<const ClickEvent &>(e);
				m.context().log += "Click(" + std::to_string(click.x) + "," + std::to_string(click.y) + ");";
				return hsm::Result::Done;
			}
			case EventId::KEY: {
				const auto &key = static_cast<const KeyEvent &>(e);
				m.context().log += "Key(" + std::to_string(key.key_code) + ");";
				return hsm::Result::Done;
			}
			default: {
				m.context().log += "Unhandled;";
				return hsm::Result::Pass;
			}
		}
	}
};

}  // namespace

TEST_CASE("Event Dispatching", "[hsm][dispatch]") {
	Machine machine;
	machine.start(0, [](Scope &scope) { scope.state<State0>(0); });

	SECTION("Match ClickEvent") {
		ClickEvent e(10, 20);
		machine.dispatch(e);
		REQUIRE(machine.context().log == "Click(10,20);");
	}

	SECTION("Match KeyEvent") {
		KeyEvent e(65);
		machine.dispatch(e);
		REQUIRE(machine.context().log == "Key(65);");
	}

	SECTION("Match Unhandled Event") {
		UnknownEvent e;
		machine.dispatch(e);
		REQUIRE(machine.context().log == "Unhandled;");
	}
}
