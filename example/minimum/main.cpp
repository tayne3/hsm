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
