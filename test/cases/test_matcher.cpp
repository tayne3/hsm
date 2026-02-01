#include <catch.hpp>
#include <hsm/hsm.hpp>
#include <iostream>
#include <string>

// ============================================================================
// Standard Polymorphic Events (DefaultCastPolicy)
// ============================================================================

struct BaseEvent {
	virtual ~BaseEvent() = default;
};

struct ClickEvent : BaseEvent {
	int x, y;
	ClickEvent(int x, int y) : x(x), y(y) {}
};

struct KeyEvent : BaseEvent {
	int key_code;
	KeyEvent(int k) : key_code(k) {}
};

struct UnknownEvent : BaseEvent {};

struct DispatchContext {
	std::string log;
};

struct DispatchTraits {
	using StateID = int;
	using Event   = BaseEvent;
	using Context = DispatchContext;
};

struct DispatchState : hsm::State<DispatchTraits> {
	hsm::Result handle(hsm::Machine<DispatchTraits> &m, const Event &e) override {
		// Use the new match helper with default policy
		return hsm::match(m, e)
			.on<ClickEvent>([](hsm::Machine<DispatchTraits> &m, const ClickEvent &click) {
				m.context().log += "Click(" + std::to_string(click.x) + "," + std::to_string(click.y) + ");";
				return hsm::Result::Done;
			})
			.on<KeyEvent>([](hsm::Machine<DispatchTraits> &m, const KeyEvent &key) {
				m.context().log += "Key(" + std::to_string(key.key_code) + ");";
				return hsm::Result::Done;
			})
			.otherwise([](hsm::Machine<DispatchTraits> &m, const Event &) {
				m.context().log += "Unhandled;";
				return hsm::Result::Pass;
			});
	}
};

TEST_CASE("Event Matcher Helper - Default Policy", "[hsm][match]") {
	hsm::Machine<DispatchTraits> machine;

	machine.start(0, [](hsm::Scope<DispatchTraits> &scope) { scope.state<DispatchState>(0); });

	SECTION("Match ClickEvent") {
		ClickEvent e(10, 20);
		machine.handle(e);
		REQUIRE(machine.context().log == "Click(10,20);");
	}

	SECTION("Match KeyEvent") {
		KeyEvent e(65);
		machine.handle(e);
		REQUIRE(machine.context().log == "Key(65);");
	}

	SECTION("Match Unhandled Event") {
		UnknownEvent e;
		machine.handle(e);
		REQUIRE(machine.context().log == "Unhandled;");
	}
}

// ============================================================================
// Enum-based Events (Custom CastPolicy)
// ============================================================================

enum class EventType { MOUSE, KEYBOARD };

struct MyEventBase {
	EventType type;
	MyEventBase(EventType t) : type(t) {}
};

struct MouseEvent : MyEventBase {
	MouseEvent() : MyEventBase(EventType::MOUSE) {}
};

struct KeyboardEvent : MyEventBase {
	KeyboardEvent() : MyEventBase(EventType::KEYBOARD) {}
};

// Custom Policy: checks 'type' field instead of dynamic_cast
struct StaticTypePolicy {
	template <typename Specific, typename Base>
	static const Specific *apply(const Base &b) {
		// In a real scenario, you'd probably check b.type against a static constant in Specific
		// Here specifically for the test:
		if (std::is_same<Specific, MouseEvent>::value && b.type == EventType::MOUSE) { return static_cast<const Specific *>(&b); }
		if (std::is_same<Specific, KeyboardEvent>::value && b.type == EventType::KEYBOARD) { return static_cast<const Specific *>(&b); }
		return nullptr;
	}
};

struct CustomTraits {
	using StateID = int;
	using Event   = MyEventBase;
	using Context = DispatchContext;
};

struct CustomState : hsm::State<CustomTraits> {
	hsm::Result handle(hsm::Machine<CustomTraits> &m, const Event &e) override {
		// Use custom policy!
		return hsm::match<StaticTypePolicy>(m, e)
			.on<MouseEvent>([](hsm::Machine<CustomTraits> &m, const MouseEvent &) {
				m.context().log += "Mouse;";
				return hsm::Result::Done;
			})
			.on<KeyboardEvent>([](hsm::Machine<CustomTraits> &m, const KeyboardEvent &) {
				m.context().log += "Keyboard;";
				return hsm::Result::Done;
			});
	}
};

TEST_CASE("Event Matcher Helper - Custom Policy", "[hsm][match]") {
	hsm::Machine<CustomTraits> machine;

	machine.start(0, [](hsm::Scope<CustomTraits> &scope) { scope.state<CustomState>(0); });

	SECTION("Match MouseEvent via Custom Policy") {
		MouseEvent e;
		machine.handle(e);
		REQUIRE(machine.context().log == "Mouse;");
	}

	SECTION("Match KeyboardEvent via Custom Policy") {
		KeyboardEvent e;
		machine.handle(e);
		REQUIRE(machine.context().log == "Keyboard;");
	}
}
