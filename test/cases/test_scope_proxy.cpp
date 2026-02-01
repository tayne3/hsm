#include <catch.hpp>
#include <hsm/hsm.hpp>
#include <string>
#include <vector>

namespace {

struct Event {};
enum StateID { ID_Root, ID_Child, ID_GrandChild };

struct TestTraits {
	using Context = std::vector<std::string>;
	using Event   = Event;
	using StateID = StateID;
};

using Machine = hsm::Machine<TestTraits>;
using Scope   = hsm::Scope<TestTraits>;

// Class-based State for Track 1
struct ClassState : hsm::State<TestTraits> {
	const char* name_val;
	ClassState(const char* n) : name_val(n) {}

	const char* name() const override { return name_val; }
	void        on_entry(Machine& sm) override { sm.context().push_back(std::string(name()) + "_Entry"); }
};

}  // namespace

TEST_CASE("Scope Proxy Dual-Track API", "[scope]") {
	Machine sm;

	SECTION("Track 1: Class State Hierarchy") {
		// config: Root(Class) -> Child(Class)
		auto config = [](Scope& root) { root.state<ClassState>(ID_Root, "Root").with([](Scope& s) { s.state<ClassState>(ID_Child, "Child"); }); };

		sm.start(ID_Root, config);

		// Check Entry logs: Root_Entry
		REQUIRE(sm.context().size() == 1);
		CHECK(sm.context()[0] == "Root_Entry");

		// Transition to Child
		sm.transition(ID_Child);
		sm.handle();

		// Check logs: Root_Entry, Child_Entry
		REQUIRE(sm.context().size() == 2);
		CHECK(sm.context()[1] == "Child_Entry");
	}

	SECTION("Track 2: Lambda State Fluent Config") {
		// config: Root(Lambda) -> Child(Lambda)
		// Verify fluent API order independence
		auto config = [](Scope& root) {
			root.state(ID_Root).name("RootLambda").on_entry([](Machine& m) { m.context().push_back("Root_Entry"); }).with([](Scope& s) {
				s.state(ID_Child)
					// Calling name() last shouldn't break anything
					.on_entry([](Machine& m) { m.context().push_back("Child_Entry"); })
					.name("ChildLambda");
			});
		};

		sm.start(ID_Root, config);

		REQUIRE(sm.context().size() == 1);
		CHECK(sm.context()[0] == "Root_Entry");
		CHECK(sm.current_state_id() == ID_Root);

		sm.transition(ID_Child);
		sm.handle();

		REQUIRE(sm.context().size() == 2);
		CHECK(sm.context()[1] == "Child_Entry");
		CHECK(sm.current_state_id() == ID_Child);
	}

	SECTION("Mixed Hierarchy") {
		// Root(Class) -> Child(Lambda) -> GrandChild(Class)
		auto config = [](Scope& root) {
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
		};

		sm.start(ID_Root, config);
		CHECK(sm.context().back() == "Root_Entry");

		sm.transition(ID_Child);
		sm.handle();
		CHECK(sm.context().back() == "Child_Entry");

		// Dispatch event handled by Lambda Child to transition to Class GrandChild
		sm.handle(Event{});
		CHECK(sm.context().back() == "GrandChild_Entry");
		CHECK(sm.current_state_id() == ID_GrandChild);
	}
}
