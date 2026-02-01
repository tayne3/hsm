#include <cstdio>
#include <hsm/hsm.hpp>

// 1. Define Events
struct Event {
	virtual ~Event() = default;
};
struct Click : Event {};
struct Reset : Event {};

// 2. Define State Machine Traits
struct SwitchTraits {
	using StateID = int;
	using Event   = Event;
	using Context = void*;
};

using Machine = hsm::Machine<SwitchTraits>;
using State   = hsm::State<SwitchTraits>;
using Scope   = hsm::Scope<SwitchTraits>;

// 3. Define States
constexpr int OFF = 0;
constexpr int ON  = 1;

int main() {
	Machine sm;

	sm.start(OFF, [](Scope& s) {
		// State: OFF
		s.state(OFF).on_entry([](Machine&) { printf("State: OFF\n"); }).handle([](Machine& m, const Event& e) {
			return hsm::match(m, e).template on<Click>([](Machine& m, const Click&) {
				printf("  --> Switch ON\n");
				m.transition(ON);
				return hsm::Result::Done;
			});
		});

		// State: ON
		s.state(ON).on_entry([](Machine&) { printf("State: ON\n"); }).handle([](Machine& m, const Event& e) {
			return hsm::match(m, e)
				.template on<Click>([](Machine& m, const Click&) {
					printf("  --> Switch OFF\n");
					m.transition(OFF);
					return hsm::Result::Done;
				})
				.template on<Reset>([](Machine& m, const Reset&) {
					printf("  --> Reset\n");
					m.transition(OFF);
					return hsm::Result::Done;
				});
		});
	});

	// 4. Run
	printf("Dispatching Click...\n");
	sm.handle(Click{});  // OFF -> ON

	printf("Dispatching Click...\n");
	sm.handle(Click{});  // ON -> OFF
}
