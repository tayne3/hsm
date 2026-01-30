#include <cstdio>

#include "hsm/hsm.hpp"

struct AppTraits {
	enum class StateID { Off, On, Idle, Working };
	struct Context {
		int tick_count = 0;
	};
	using Event = int;
};
using AppState   = hsm::State<AppTraits>;
using AppMachine = hsm::Machine<AppTraits>;

struct OffState : public AppState {
	void        on_entry(AppMachine&) { printf("Entering OFF state\n"); }
	void        on_exit(AppMachine&) { printf("Leaving OFF state\n"); }
	hsm::Result handle(AppMachine&, const AppTraits::Event&) { return hsm::Result::Done; }
};

struct OnState : public AppState {
	void        on_entry(AppMachine&) { printf("Entering ON state\n"); }
	void        on_exit(AppMachine&) { printf("Leaving ON state\n"); }
	hsm::Result handle(AppMachine& sm, const AppTraits::Event&) {
		if (sm.context().tick_count >= 8) {
			printf("Battery low, shutting down\n");
			sm.transition(AppTraits::StateID::Off);
			return hsm::Result::Done;
		}
		return hsm::Result::Pass;
	}
};

struct IdleState : public AppState {
	void        on_entry(AppMachine&) { printf("Entering IDLE state\n"); }
	void        on_exit(AppMachine&) { printf("Leaving IDLE state\n"); }
	hsm::Result handle(AppMachine& sm, const AppTraits::Event&) {
		if (sm.context().tick_count == 3) {
			printf("Work requested\n");
			sm.transition(AppTraits::StateID::Working);
			return hsm::Result::Done;
		}
		return hsm::Result::Pass;
	}
};

struct WorkingState : public AppState {
	void        on_entry(AppMachine&) { printf("Entering WORKING state\n"); }
	void        on_exit(AppMachine&) { printf("Leaving WORKING state\n"); }
	hsm::Result handle(AppMachine& sm, const AppTraits::Event&) {
		if (sm.context().tick_count == 5) {
			printf("Work done\n");
			sm.transition(AppTraits::StateID::Idle);
			return hsm::Result::Done;
		}
		return hsm::Result::Pass;
	}
};

int main() {
	AppMachine machine;
	machine.start(AppTraits::StateID::Idle, [](hsm::Scope<AppTraits>& s) {
		s.state<OffState>(AppTraits::StateID::Off);
		s.state<OnState>(AppTraits::StateID::On).with([](hsm::Scope<AppTraits>& s) {
			s.state<IdleState>(AppTraits::StateID::Idle);
			s.state<WorkingState>(AppTraits::StateID::Working);
		});
	});

	for (int i = 0; i < 10; i++) {
		printf("-- Tick %d --\n", machine.context().tick_count);
		machine.handle(AppTraits::Event{});
		machine.context().tick_count++;
	}

	return 0;
}
