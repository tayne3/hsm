#include <cstdio>

#include "hsm/hsm.hpp"

namespace {

enum StateID { STATE_OFF, STATE_ON };

struct Traits {
	using StateID = StateID;
	struct Event {
		virtual ~Event() = default;
	};
	struct Context {};
};

using Event   = Traits::Event;
using Machine = hsm::Machine<Traits>;
using Scope   = hsm::Scope<Traits>;

struct Click : Event {};

}  // namespace

int main() {
	Machine sm;
	sm.start(STATE_OFF, [](Scope& s) {
		s.state(STATE_OFF).on_entry([](Machine&) { printf("[OFF] on entry\n"); }).handle([](Machine& sm, const Event& ev) {
			return hsm::match(sm, ev).template on<Click>([](Machine& sm, const Click&) {
				printf("[OFF] handle Click -> transition to ON\n");
				sm.transition(STATE_ON);
				return hsm::Result::Done;
			});
		});

		s.state(STATE_ON).on_entry([](Machine&) { printf("[ON] on entry\n"); }).handle([](Machine& sm, const Event& ev) {
			return hsm::match(sm, ev).template on<Click>([](Machine& sm, const Click&) {
				printf("[ON] handle Click -> transition to OFF\n");
				sm.transition(STATE_OFF);
				return hsm::Result::Done;
			});
		});
	});

	printf("--- Click ---\n");
	sm.dispatch(Click{});

	printf("--- Click ---\n");
	sm.dispatch(Click{});
}
