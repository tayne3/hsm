#include "catch.hpp"
#include "hsm/hsm.hpp"

// ============================================================================
// Standard Polymorphic Events (DefaultCastPolicy)
// ============================================================================

namespace {

struct BaseEvent {
	virtual ~BaseEvent() = default;
};

struct ClickEvent : BaseEvent {
	int x, y;
	ClickEvent(int x, int y) : x(x), y(y) {}
};

struct KeyEvent : public BaseEvent {
	int key_code;
	KeyEvent(int k) : key_code(k) {}
};

struct UnknownEvent : public BaseEvent {};

struct DispatchContext {
	std::string log;
};

struct DispatchTraits {
	using StateID = int;
	using Event   = BaseEvent;
	using Context = DispatchContext;
};

struct DispatchState : public hsm::State<DispatchTraits> {
	hsm::Result handle(hsm::Machine<DispatchTraits> &sm, const Event &ev) override {
		// Use the new match helper with default policy
		return hsm::match(sm, ev)
			.on<ClickEvent>([](hsm::Machine<DispatchTraits> &sm, const ClickEvent &click) {
				sm.context().log += "Click(" + std::to_string(click.x) + "," + std::to_string(click.y) + ");";
				return hsm::Result::Done;
			})
			.on<KeyEvent>([](hsm::Machine<DispatchTraits> &sm, const KeyEvent &key) {
				sm.context().log += "Key(" + std::to_string(key.key_code) + ");";
				return hsm::Result::Done;
			})
			.otherwise([](hsm::Machine<DispatchTraits> &sm, const Event &) {
				sm.context().log += "Unhandled;";
				return hsm::Result::Pass;
			});
	}
};

}  // namespace

TEST_CASE("Event Matcher Helper - Default Policy", "[hsm][match]") {
	hsm::Machine<DispatchTraits> machine;

	machine.start(0, [](hsm::Scope<DispatchTraits> &scope) { scope.state<DispatchState>(0); });

	SECTION("Match ClickEvent") {
		ClickEvent ev(10, 20);
		machine.dispatch(ev);
		REQUIRE(machine.context().log == "Click(10,20);");
	}

	SECTION("Match KeyEvent") {
		KeyEvent ev(65);
		machine.dispatch(ev);
		REQUIRE(machine.context().log == "Key(65);");
	}

	SECTION("Match Unhandled Event") {
		UnknownEvent ev;
		machine.dispatch(ev);
		REQUIRE(machine.context().log == "Unhandled;");
	}
}

// ============================================================================
// Enum-based Events (Custom CastPolicy)
// ============================================================================

namespace {

enum class EventType {
	MOUSE,
	KEYBOARD,
};

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
	hsm::Result handle(hsm::Machine<CustomTraits> &sm, const Event &ev) override {
		// Use custom policy!
		return hsm::match<StaticTypePolicy>(sm, ev)
			.on<MouseEvent>([](hsm::Machine<CustomTraits> &sm, const MouseEvent &) {
				sm.context().log += "Mouse;";
				return hsm::Result::Done;
			})
			.on<KeyboardEvent>([](hsm::Machine<CustomTraits> &sm, const KeyboardEvent &) {
				sm.context().log += "Keyboard;";
				return hsm::Result::Done;
			});
	}
};

}  // namespace

TEST_CASE("Event Matcher Helper - Custom Policy", "[hsm][match]") {
	hsm::Machine<CustomTraits> machine;

	machine.start(0, [](hsm::Scope<CustomTraits> &scope) { scope.state<CustomState>(0); });

	SECTION("Match MouseEvent via Custom Policy") {
		MouseEvent ev;
		machine.dispatch(ev);
		REQUIRE(machine.context().log == "Mouse;");
	}

	SECTION("Match KeyboardEvent via Custom Policy") {
		KeyboardEvent ev;
		machine.dispatch(ev);
		REQUIRE(machine.context().log == "Keyboard;");
	}
}

// ============================================================================
// FastCastPolicy (typeid strict match)
// ============================================================================

namespace {

enum class FastStateID { ROOT, S1, S2 };

struct FastEvent {
	virtual ~FastEvent() = default;
};

struct FastEventA : FastEvent {};
struct FastEventB : FastEvent {};

struct FastTraits {
	using StateID = FastStateID;
	using Event   = FastEvent;
	struct Context {
		int a_count = 0;
		int b_count = 0;
	};
};

}  // namespace

TEST_CASE("FastCastPolicy dispatching (typeid strict match)", "[match][fast_policy]") {
	hsm::Machine<FastTraits> sm;

	sm.start(FastStateID::S1, [](hsm::Scope<FastTraits> &s) {
		s.state(FastStateID::S1).handle([](hsm::Machine<FastTraits> &sm, const FastEvent &ev) {
			return hsm::match<hsm::FastCastPolicy>(sm, ev)
				.on<FastEventA>([](hsm::Machine<FastTraits> &sm, const FastEventA &) {
					sm.context().a_count++;
					sm.transition(FastStateID::S2);
					return hsm::Result::Done;
				})
				.otherwise([](hsm::Machine<FastTraits> &, const FastEvent &) { return hsm::Result::Pass; });
		});

		s.state(FastStateID::S2).handle([](hsm::Machine<FastTraits> &sm, const FastEvent &ev) {
			return hsm::match<hsm::FastCastPolicy>(sm, ev)
				.on<FastEventB>([](hsm::Machine<FastTraits> &sm, const FastEventB &) {
					sm.context().b_count++;
					sm.transition(FastStateID::S1);
					return hsm::Result::Done;
				})
				.otherwise([](hsm::Machine<FastTraits> &, const FastEvent &) { return hsm::Result::Pass; });
		});
	});

	REQUIRE(sm.current_state_id() == FastStateID::S1);
	REQUIRE(sm.context().a_count == 0);
	REQUIRE(sm.context().b_count == 0);

	sm.dispatch(FastEventA{});
	REQUIRE(sm.current_state_id() == FastStateID::S2);
	REQUIRE(sm.context().a_count == 1);
	REQUIRE(sm.context().b_count == 0);

	sm.dispatch(FastEventB{});
	REQUIRE(sm.current_state_id() == FastStateID::S1);
	REQUIRE(sm.context().a_count == 1);
	REQUIRE(sm.context().b_count == 1);
}

// ============================================================================
// DynamicCastPolicy (polymorphic match)
// ============================================================================

namespace {

enum class DynStateID { ROOT, S1, S2 };

struct DynEvent {
	virtual ~DynEvent() = default;
};

struct DynGroupEvent : DynEvent {};

struct DynEventA : DynGroupEvent {};
struct DynEventB : DynGroupEvent {};
struct DynEventC : DynEvent {};

struct DynTraits {
	using StateID = DynStateID;
	using Event   = DynEvent;
	struct Context {
		int group_count = 0;
		int c_count     = 0;
	};
};

}  // namespace

TEST_CASE("DynamicCastPolicy dispatching (polymorphic match)", "[match][dynamic_policy]") {
	hsm::Machine<DynTraits> sm;

	sm.start(DynStateID::S1, [](hsm::Scope<DynTraits> &s) {
		s.state(DynStateID::S1).handle([](hsm::Machine<DynTraits> &sm, const DynEvent &ev) {
			return hsm::match<hsm::DynamicCastPolicy>(sm, ev)
				// Match ANY event derived from GroupEvent
				.on<DynGroupEvent>([](hsm::Machine<DynTraits> &sm, const DynGroupEvent &) {
					sm.context().group_count++;
					return hsm::Result::Done;
				})
				// Match specific EventC
				.on<DynEventC>([](hsm::Machine<DynTraits> &sm, const DynEventC &) {
					sm.context().c_count++;
					return hsm::Result::Done;
				})
				.otherwise([](hsm::Machine<DynTraits> &, const DynEvent &) { return hsm::Result::Pass; });
		});
	});

	REQUIRE(sm.current_state_id() == DynStateID::S1);
	REQUIRE(sm.context().group_count == 0);
	REQUIRE(sm.context().c_count == 0);

	// EventA derives from GroupEvent -> should map to GroupEvent
	sm.dispatch(DynEventA{});
	REQUIRE(sm.context().group_count == 1);
	REQUIRE(sm.context().c_count == 0);

	// EventB derives from GroupEvent -> should map to GroupEvent
	sm.dispatch(DynEventB{});
	REQUIRE(sm.context().group_count == 2);
	REQUIRE(sm.context().c_count == 0);

	// EventC is standalone -> should map to EventC
	sm.dispatch(DynEventC{});
	REQUIRE(sm.context().group_count == 2);
	REQUIRE(sm.context().c_count == 1);
}

// ============================================================================
// TagCastPolicy (static tag match)
// ============================================================================

namespace {

enum class TagStateID { ROOT, S1, S2 };
enum class TagEventID { A, B };

struct TagEvent {
	const TagEventID id;
	TagEvent(TagEventID id) : id(id) {}
	virtual ~TagEvent() = default;
};

struct TagEventExtA : TagEvent {
	static constexpr TagEventID ID = TagEventID::A;
	TagEventExtA() : TagEvent(ID) {}
};

struct TagEventExtB : TagEvent {
	static constexpr TagEventID ID = TagEventID::B;
	TagEventExtB() : TagEvent(ID) {}
};

struct TagTraits {
	using StateID = TagStateID;
	using Event   = TagEvent;
	struct Context {
		int a_count = 0;
		int b_count = 0;
	};
};

}  // namespace

TEST_CASE("TagCastPolicy dispatching", "[match][tag_policy]") {
	hsm::Machine<TagTraits> sm;

	sm.start(TagStateID::S1, [](hsm::Scope<TagTraits> &s) {
		s.state(TagStateID::S1).handle([](hsm::Machine<TagTraits> &sm, const TagEvent &ev) {
			return hsm::match<hsm::TagCastPolicy>(sm, ev)
				.on<TagEventExtA>([](hsm::Machine<TagTraits> &sm, const TagEventExtA &) {
					sm.context().a_count++;
					sm.transition(TagStateID::S2);
					return hsm::Result::Done;
				})
				.otherwise([](hsm::Machine<TagTraits> &, const TagEvent &) { return hsm::Result::Pass; });
		});

		s.state(TagStateID::S2).handle([](hsm::Machine<TagTraits> &sm, const TagEvent &ev) {
			return hsm::match<hsm::TagCastPolicy>(sm, ev)
				.on<TagEventExtB>([](hsm::Machine<TagTraits> &sm, const TagEventExtB &) {
					sm.context().b_count++;
					sm.transition(TagStateID::S1);
					return hsm::Result::Done;
				})
				.otherwise([](hsm::Machine<TagTraits> &, const TagEvent &) { return hsm::Result::Pass; });
		});
	});

	REQUIRE(sm.current_state_id() == TagStateID::S1);
	REQUIRE(sm.context().a_count == 0);
	REQUIRE(sm.context().b_count == 0);

	sm.dispatch(TagEventExtA{});
	REQUIRE(sm.current_state_id() == TagStateID::S2);
	REQUIRE(sm.context().a_count == 1);
	REQUIRE(sm.context().b_count == 0);

	sm.dispatch(TagEventExtB{});
	REQUIRE(sm.current_state_id() == TagStateID::S1);
	REQUIRE(sm.context().a_count == 1);
	REQUIRE(sm.context().b_count == 1);
}
