#include <string>
#include <vector>

#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

enum StateID {
	ID_Root,
	ID_Child,
	ID_GrandChild,
};

struct Event {};
struct Traits {
	using Context = std::vector<std::string>;
	using Event   = Event;
	using StateID = StateID;
};
using Machine = hsm::Machine<Traits>;
using State   = hsm::State<Traits>;
using Scope   = hsm::Scope<Traits>;

// Class-based State for Track 1
struct ClassState : State {
	const char* name_val;
	ClassState(const char* n) : name_val(n) {}

	const char* name() const override { return name_val; }
	void        on_entry(Machine& sm) override { sm->push_back(std::string(name()) + "_Entry"); }
};

}  // namespace

TEST_CASE("Scope Proxy Dual-Track API", "[scope]") {
	Machine sm;

	SECTION("Track 1: Class State Hierarchy") {
		// config: Root(Class) -> Child(Class)
		sm.start(ID_Root, [](Scope& root) { root.state<ClassState>(ID_Root, "Root").with([](Scope& s) { s.state<ClassState>(ID_Child, "Child"); }); });

		// Check Entry logs: Root_Entry
		REQUIRE(sm->size() == 1);
		CHECK(sm.context()[0] == "Root_Entry");

		// Transition to Child
		sm.transition(ID_Child);
		sm.dispatch();

		// Check logs: Root_Entry, Child_Entry
		REQUIRE(sm->size() == 2);
		CHECK(sm.context()[1] == "Child_Entry");
	}

	SECTION("Track 2: Lambda State Fluent Config") {
		// config: Root(Lambda) -> Child(Lambda)
		// Verify fluent API order independence
		sm.start(ID_Root, [](Scope& root) {
			root.state(ID_Root).name("RootLambda").on_entry([](Machine& m) { m.context().push_back("Root_Entry"); }).with([](Scope& s) {
				s.state(ID_Child)
					// Calling name() last shouldn't break anything
					.on_entry([](Machine& m) { m.context().push_back("Child_Entry"); })
					.name("ChildLambda");
			});
		});

		REQUIRE(sm->size() == 1);
		CHECK(sm.context()[0] == "Root_Entry");
		CHECK(sm.current_state_id() == ID_Root);

		sm.transition(ID_Child);
		sm.dispatch();

		REQUIRE(sm->size() == 2);
		CHECK(sm.context()[1] == "Child_Entry");
		CHECK(sm.current_state_id() == ID_Child);
	}

	SECTION("Mixed Hierarchy") {
		// Root(Class) -> Child(Lambda) -> GrandChild(Class)
		sm.start(ID_Root, [](Scope& root) {
			root.state<ClassState>(ID_Root, "Root").with([](Scope& s) {
				s.state(ID_Child)
					.name("ChildLambda")
					.on_entry([](Machine& m) { m.context().push_back("Child_Entry"); })
					.handle([](Machine& m, const Event&) {
						m.transition(ID_GrandChild);
						return hsm::Result::Done;
					})
					.with([](Scope& s) { s.state<ClassState>(ID_GrandChild, "GrandChild"); });
			});
		});
		CHECK(sm->back() == "Root_Entry");

		sm.transition(ID_Child);
		sm.dispatch();
		CHECK(sm->back() == "Child_Entry");

		// Dispatch event handled by Lambda Child to transition to Class GrandChild
		sm.dispatch();
		CHECK(sm->back() == "GrandChild_Entry");
		CHECK(sm.current_state_id() == ID_GrandChild);
	}
}
