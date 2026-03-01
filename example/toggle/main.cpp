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
