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
	struct Event { virtual ~Event() = default; };
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
		s.state(STATE_LOCKED)
			.on_entry([](Machine&) { printf("[Locked] on entry\n"); })
			.handle([](Machine& sm, const Event& ev) {
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
				s.state(STATE_CLOSED)
					.on_entry([](Machine&) { printf("  [Closed] on entry\n"); })
					.handle([](Machine& sm, const Event& ev) {
						return hsm::match(sm, ev).template on<Open>([](Machine& sm, const Open&) {
							printf("  [Closed] handle Open -> transition to Open\n");
							sm.transition(STATE_OPEN);
							return hsm::Result::Done;
						});
					});

				s.state(STATE_OPEN)
					.on_entry([](Machine&) { printf("  [Open] on entry\n"); })
					.handle([](Machine& sm, const Event& ev) {
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
