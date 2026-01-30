# HSM

![CMake](https://img.shields.io/badge/CMake-3.14%2B-brightgreen?logo=cmake&logoColor=white)
[![Release](https://img.shields.io/github/v/release/tayne3/HSM?include_prereleases&label=release&logo=github&logoColor=white)](https://github.com/tayne3/HSM/releases)
[![Tag](https://img.shields.io/github/v/tag/tayne3/HSM?color=%23ff8936&style=flat-square&logo=git&logoColor=white)](https://github.com/tayne3/HSM/tags)

**English** | [中文](README_zh.md)

HSM is a lightweight, header-only **Hierarchical State Machine (HSM)** library for C++11. It supports nested states, event propagation, and type-safe context management, making it suitable for embedded systems and complex state logic.

## Features

- **Header-only**: Easy to integrate, just include `hsm/hsm.hpp`.
- **Hierarchical**: Supports nested states (Parent/Child relationships).
- **Type-Safe**: Uses C++ templates for custom `Context`, `Event`, and `StateID` types.
- **Flexible Definition**: Define states using classes or inline lambdas.
- **LCA Transitions**: Automatically handles `exit` and `entry` chains based on Least Common Ancestor logic.
- **Event Propagation**: Unhandled events bubble up from child to parent states.

## Integration

Since this is a header-only library, you only need to add the `include` directory to your project's include path.

```cmake
target_include_directories(my_target PRIVATE path/to/hsm/include)
```

## Complete Example

```cpp
#include "hsm/hsm.hpp"
#include <cstdio>

// 1. Define Event and State ID
enum class Event { Tick, WorkReq, WorkDone, LowBattery };
enum class StateID { Off, On, Idle, Working };

// 2. Define Context (User Data)
struct MyContext {
    int ticks = 0;
    void log(const char* msg) { printf("[LOG] %s\n", msg); }
};

// 3. Define Traits
struct MyTraits {
    using Context = MyContext;
    using Event = Event;
    using StateID = StateID;
};

// Alias for convenience
using MyMachine = hsm::Machine<MyTraits>;

int main() {
    // Initialize Machine with Context arguments (if any)
    MyMachine sm;

    // Configure and Start the State Machine
    // We define the hierarchy inside the start() configuration lambda
    sm.start(StateID::Idle, [](hsm::Scope<MyTraits>& s) {

        // Define 'Off' State (Leaf)
        s.state(StateID::Off,
            [](MyMachine& m, const Event& e) { return hsm::Result::Pass; }, // Handle
            [](MyMachine& m) { m->log("Enter Off"); },                      // Entry
            [](MyMachine& m) { m->log("Exit Off"); },                       // Exit
            "Off"                                                           // Name
        );

        // Define 'On' State (Parent)
        s.state(StateID::On,
            [](MyMachine& m, const Event& e) {
                // Parent handles LowBattery for all children
                if (e == Event::LowBattery) {
                    m.transition(StateID::Off);
                    return hsm::Result::Done;
                }
                return hsm::Result::Pass;
            },
            [](MyMachine& m) { m->log("Enter On"); },
            [](MyMachine& m) { m->log("Exit On"); },
            "On"
        )
        // Define children of 'On'
        .with([](hsm::Scope<MyTraits>& s) {

            // 'Idle' State
            s.state(StateID::Idle,
                [](MyMachine& m, const Event& e) {
                    if (e == Event::WorkReq) {
                        m.transition(StateID::Working);
                        return hsm::Result::Done;
                    }
                    return hsm::Result::Pass;
                },
                [](MyMachine& m) { m->log("Enter Idle"); },
                nullptr,
                "Idle"
            );

            // 'Working' State
            s.state(StateID::Working,
                [](MyMachine& m, const Event& e) {
                    if (e == Event::WorkDone) {
                        m.transition(StateID::Idle);
                        return hsm::Result::Done;
                    }
                    return hsm::Result::Pass;
                },
                [](MyMachine& m) { m->log("Enter Working"); },
                nullptr,
                "Working"
            );
        });
    });

    // Run Events
    printf("--- Sending Tick ---\n");
    sm.handle(Event::Tick);

    printf("--- Sending WorkReq ---\n");
    sm.handle(Event::WorkReq);  // Transitions Idle -> Working

    printf("--- Sending WorkDone ---\n");
    sm.handle(Event::WorkDone); // Transitions Working -> Idle

    printf("--- Sending LowBattery ---\n");
    sm.handle(Event::LowBattery); // Transitions Idle -> Off (handled by On)

    return 0;
}
```

## API Reference

### `hsm::Machine<Traits>`

The core state machine engine.

- **Template Parameter**: `Traits` struct containing `Context`, `Event`, and `StateID` types.
- **Constructor**: `Machine(Args&&... args)` - Forwards arguments to the `Context` constructor.
- **`start(StateID initial, config_fn)`**: Configures the state hierarchy and enters the initial state.
- **`handle(const Event&)`**: Dispatches an event to the active state. If unhandled (`Result::Pass`), it propagates to the parent.
- **`transition(StateID target)`**: Schedules a transition to the target state.
- **`context()`**: Returns reference to the user context.

### `hsm::State<Traits>`

Base class for states. You can inherit from this to create complex states.

- **`handle(Machine&, const Event&)`**: Return `Result::Done` if handled, `Result::Pass` to propagate.
- **`on_entry(Machine&)`**: Called when entering the state.
- **`on_exit(Machine&)`**: Called when exiting the state.

### `hsm::Result`

- `Result::Done`: Event handled, stop propagation.
- `Result::Pass`: Event not handled, continue to parent.
