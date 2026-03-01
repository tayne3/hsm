#include <cstdio>

#include "hsm/hsm.hpp"

namespace {

enum StateID {
	STATE_LOCKED,
	STATE_UNLOCKED,
	STATE_CLOSED,
	STATE_OPEN,
};

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

struct Unlock : Event {};
struct Lock : Event {};
struct Open : Event {};
struct Close : Event {};

}  // namespace

int main() {
	Machine sm;
	sm.start(STATE_LOCKED, [](Scope& s) {
		s.state(STATE_LOCKED).on_entry([](Machine&) { printf("[Locked] on entry\n"); }).handle([](Machine& sm, const Event& ev) {
			return hsm::match(sm, ev).template on<Unlock>([](Machine& sm, const Unlock&) {
				printf("[Locked] handle Unlock -> transition to Closed\n");
				sm.transition(STATE_CLOSED);
				return hsm::Result::Done;
			});
		});

		s.state(STATE_UNLOCKED)
			.on_entry([](Machine&) { printf("[Unlocked] on entry\n"); })
			.on_exit([](Machine&) { printf("[Unlocked] on exit\n"); })
			.handle([](Machine& sm, const Event& ev) {
				return hsm::match(sm, ev).template on<Lock>([](Machine& sm, const Lock&) {
					printf("[Unlocked] handle Lock -> transition to Locked\n");
					sm.transition(STATE_LOCKED);
					return hsm::Result::Done;
				});
			})
			.with([](Scope& s) {
				s.state(STATE_CLOSED).on_entry([](Machine&) { printf("  [Closed] on entry\n"); }).handle([](Machine& sm, const Event& ev) {
					return hsm::match(sm, ev).template on<Open>([](Machine& sm, const Open&) {
						printf("  [Closed] handle Open -> transition to Open\n");
						sm.transition(STATE_OPEN);
						return hsm::Result::Done;
					});
				});

				s.state(STATE_OPEN).on_entry([](Machine&) { printf("  [Open] on entry\n"); }).handle([](Machine& sm, const Event& ev) {
					return hsm::match(sm, ev).template on<Close>([](Machine& sm, const Close&) {
						printf("  [Open] handle Close -> transition to Closed\n");
						sm.transition(STATE_CLOSED);
						return hsm::Result::Done;
					});
				});
			});
	});

	printf("--- Unlock ---\n");
	sm.dispatch(Unlock{});

	printf("--- Open ---\n");
	sm.dispatch(Open{});

	printf("--- Close ---\n");
	sm.dispatch(Close{});

	printf("--- Lock ---\n");
	sm.dispatch(Lock{});
}
