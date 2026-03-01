#include "catch.hpp"
#include "hsm/hsm.hpp"

namespace {

enum StateID {
	ID_Normal,
};

struct TestTraits {
	struct Context {
		int entry_count = 0;
		int exit_count  = 0;
		int run_count   = 0;
	};
	struct Event {};
	using StateID = ::StateID;
};

using TestState   = hsm::State<TestTraits>;
using TestMachine = hsm::Machine<TestTraits>;

class BaseState : public TestState {
public:
	const char* name() const override { return "Base"; }
};

class NormalState : public BaseState {
public:
	const char* name() const override final { return "Normal"; }
	void        on_entry(TestMachine& sm) override { sm->entry_count++; }
	void        on_exit(TestMachine& sm) override { sm->exit_count++; }
	hsm::Result handle(TestMachine& sm, const Event&) override {
		sm->run_count++;
		if (sm->run_count == 1) {
			sm.transition(ID_Normal);
			return hsm::Result::Done;
		}
		return hsm::Result::Done;
	}
};

}  // namespace

TEST_CASE("Self Transition", "[hsm]") {
	TestMachine sm;
	sm.start(ID_Normal, [](hsm::Scope<TestTraits>& root) { root.state<NormalState>(ID_Normal); });

	SECTION("Self transition triggers exit and entry") {
		REQUIRE(sm->entry_count == 1);

		sm.dispatch();  // run() called -> transition(self) -> exit() -> entry()

		CHECK(sm->exit_count == 1);
		CHECK(sm->entry_count == 2);
	}
}
