/*
 * MIT License
 *
 * Copyright (c) 2026 tayne3
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <cstdio>
#include <hsm/hsm.hpp>

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
