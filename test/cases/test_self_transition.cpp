#include <catch.hpp>
#include <hsm/hsm.hpp>

namespace {

struct TestContext {
	int entry_count = 0;
	int exit_count  = 0;
	int run_count   = 0;
};

struct Event {};
enum StateID { ID_Normal };

struct TestTraits {
	using Context = TestContext;
	using Event   = Event;
	using StateID = StateID;
};

using AppState = hsm::State<TestTraits>;

class BaseState : public AppState {
public:
	const char* name() const override { return "Base"; }
};

class NormalState : public BaseState {
public:
	const char* name() const override final { return "Normal"; }

	void on_entry(hsm::Machine<TestTraits>& sm) override { sm.context().entry_count++; }

	void on_exit(hsm::Machine<TestTraits>& sm) override { sm.context().exit_count++; }

	hsm::Result handle(hsm::Machine<TestTraits>& sm, const Event&) override {
		sm.context().run_count++;
		if (sm.context().run_count == 1) {
			sm.transition(ID_Normal);
			return hsm::Result::Done;
		}
		return hsm::Result::Done;
	}
};

}  // namespace

TEST_CASE("Self Transition", "[hsm]") {
	hsm::Machine<TestTraits> sm;

	auto config = [](hsm::Scope<TestTraits>& root) { root.state<NormalState>(ID_Normal); };

	SECTION("Self transition triggers exit and entry") {
		sm.start(ID_Normal, config);
		REQUIRE(sm.context().entry_count == 1);

		sm.handle(Event{});
		// run() called -> transition(self) -> exit() -> entry()

		CHECK(sm.context().exit_count == 1);
		CHECK(sm.context().entry_count == 2);
	}
}
