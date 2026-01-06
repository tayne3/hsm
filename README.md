# SMF

![CMake](https://img.shields.io/badge/CMake-3.14%2B-brightgreen?logo=cmake&logoColor=white)
[![Release](https://img.shields.io/github/v/release/tayne3/SMF?include_prereleases&label=release&logo=github&logoColor=white)](https://github.com/tayne3/SMF/releases)
[![Tag](https://img.shields.io/github/v/tag/tayne3/SMF?color=%23ff8936&style=flat-square&logo=git&logoColor=white)](https://github.com/tayne3/SMF/tags)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/tayne3/SMF)

**English** | [中文](README_zh.md)

SMF is a lightweight hierarchical state machine framework that supports state nesting, event propagation, and initial transitions. It is suitable for embedded systems and applications requiring state management.

## Project Background

This project is adapted from the [SMF (State Machine Framework)](https://docs.zephyrproject.org/latest/services/smf/index.html) module of [Zephyr RTOS](https://github.com/zephyrproject-rtos/zephyr). Zephyr's SMF is an excellent hierarchical state machine implementation, but it is tightly coupled with the Zephyr ecosystem. This project extracts it into a **standalone, portable C library** that can be used in any C/C++ project without depending on Zephyr.

> [!NOTE]
> The `zephyr/` directory retains the original Zephyr code as a reference and is not included in the build.

## Key Differences from Zephyr SMF

| Aspect                      | Zephyr Original                                            | This Project                                           |
| --------------------------- | ---------------------------------------------------------- | ------------------------------------------------------ |
| **Dependencies**            | `<zephyr/kernel.h>`, `<zephyr/sys/util.h>`, etc.           | Only `<stddef.h>`, `<stdint.h>`, fully standalone      |
| **Conditional Compilation** | Features controlled by `CONFIG_SMF_ANCESTOR_SUPPORT`, etc. | Hierarchical SM and initial transitions always enabled |
| **Context Structure**       | `internal` field accessed via pointer casting              | Direct `flags` struct member, clearer design           |
| **User Data**               | Requires embedding `smf_ctx` in custom struct              | Added `userdata` field with accessor functions         |
| **API Signature**           | `smf_set_initial(ctx, state)` returns `void`               | `smf_set_initial(ctx, state, userdata)` returns `int`  |
| **Error Handling**          | Uses logging macros, no return value                       | Returns `-1` on failure                                |
| **Function Pointers**       | Parameter is `void *obj`                                   | Parameter is `smf_ctx_t *ctx`, more type-safe          |

### User Data Management Improvements

**Zephyr original** requires embedding `smf_ctx` into a custom struct:

```c
// Zephyr approach
struct my_ctx {
    struct smf_ctx ctx;  // Must be the first member
    int my_data;
};
struct my_ctx my = { ... };
smf_set_initial(&my.ctx, ...);

void my_entry(void *obj) {
    struct my_ctx *ctx = (struct my_ctx *)obj;  // Requires casting
}
```

**This project** uses a separate `userdata` pointer:

```c
// This project's approach
app_data_t data = {0};
smf_set_initial(&ctx, &states[STATE_IDLE], &data);

void idle_entry(smf_ctx_t *ctx) {
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);  // Type-safe access
}
```

> [!IMPORTANT]
> This project's API is **not compatible** with Zephyr SMF. You cannot directly copy Zephyr SMF code to use with this library.

## Complete Example

```c
#include "smf/smf.h"
#include <stdio.h>

// Application data
typedef struct {
    int tick_count;  // Run counter
} app_data_t;

// F(code, name, description, entry, run, exit, parent, initial)
#define STATE_FOREACH(F)                                                                           \
    F(0, OFF, "Off State", off_entry, off_run, SMF_NONE_EXIT, SMF_NONE_PARENT, SMF_NONE_INITIAL)   \
    F(1, ON, "On State", on_entry, on_run, on_exit, SMF_NONE_PARENT, &states[STATE_IDLE])          \
    F(2, IDLE, "Idle State", idle_entry, idle_run, idle_exit, &states[STATE_ON], SMF_NONE_INITIAL) \
    F(3, WORKING, "Working State", working_entry, working_run, working_exit, &states[STATE_ON], SMF_NONE_INITIAL)

static const smf_state_t states[];

static void               off_entry(smf_ctx_t *ctx);
static smf_state_result_t off_run(smf_ctx_t *ctx);

static void               on_entry(smf_ctx_t *ctx);
static smf_state_result_t on_run(smf_ctx_t *ctx);
static void               on_exit(smf_ctx_t *ctx);

static void               idle_entry(smf_ctx_t *ctx);
static smf_state_result_t idle_run(smf_ctx_t *ctx);
static void               idle_exit(smf_ctx_t *ctx);

static void               working_entry(smf_ctx_t *ctx);
static smf_state_result_t working_run(smf_ctx_t *ctx);
static void               working_exit(smf_ctx_t *ctx);

// State enum
enum state {
#define F(code, name, ...) STATE_##name = code,
    STATE_FOREACH(F)
#undef F
};

static const smf_state_t states[] = {
#define F(code, name, desc, entry, run, exit, parent, initial) \
    [code] = SMF_CREATE_STATE(entry, run, exit, parent, initial),
    STATE_FOREACH(F)
#undef F
};

// ========== State Function Implementations ==========

// OFF state
static void off_entry(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Entering OFF state\n");
}

static smf_state_result_t off_run(smf_ctx_t *ctx) {
    (void)ctx;
    return SMF_EVENT_HANDLED;
}

// ON state (parent)
static void on_entry(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Entering ON state\n");
}

static smf_state_result_t on_run(smf_ctx_t *ctx) {
    // Parent state handles common logic for all child states
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

    // Shutdown after 8 cycles
    if (data->tick_count >= 8) {
        printf("Battery low, shutting down\n");
        smf_set_state(ctx, &states[STATE_OFF]);
        return SMF_EVENT_HANDLED;
    }

    return SMF_EVENT_PROPAGATE;
}

static void on_exit(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Leaving ON state\n");
}

// IDLE state (child)
static void idle_entry(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Entering IDLE state\n");
}

static smf_state_result_t idle_run(smf_ctx_t *ctx) {
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

    // Start working after 3 cycles
    if (data->tick_count == 3) {
        printf("Work requested\n");
        smf_set_state(ctx, &states[STATE_WORKING]);  // ✅ Transition in run
        return SMF_EVENT_HANDLED;
    }

    return SMF_EVENT_PROPAGATE;  // Propagate to parent (ON) for handling
}

static void idle_exit(smf_ctx_t *ctx) {
    (void)ctx;
    // ⚠️ Don't call smf_set_state() here
    printf("Leaving IDLE state\n");
}

// WORKING state (child)
static void working_entry(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Entering WORKING state\n");
}

static smf_state_result_t working_run(smf_ctx_t *ctx) {
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

    // Finish after working for 2 cycles
    if (data->tick_count == 5) {
        printf("Work done\n");
        smf_set_state(ctx, &states[STATE_IDLE]);
        return SMF_EVENT_HANDLED;
    }

    return SMF_EVENT_PROPAGATE;
}

static void working_exit(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Leaving WORKING state\n");
}

// ========== Main Function ==========

int main(void) {
    smf_ctx_t ctx;
    app_data_t data = {0};

    // Initialize state machine to ON state
    // Since ON's initial is IDLE, it will automatically enter IDLE
    smf_set_initial(&ctx, &states[STATE_ON], &data);

    // Run the state machine
    for (int i = 0; i < 10 && !ctx.terminate_val; i++) {
        printf("--- Tick %d ---\n", data.tick_count);
        smf_run_state(&ctx);
        data.tick_count++;
    }

    return 0;
}
```

**Output**:

```text
Entering ON state
Entering IDLE state
--- Tick 0 ---
--- Tick 1 ---
--- Tick 2 ---
--- Tick 3 ---
Work requested
Leaving IDLE state
Entering WORKING state
--- Tick 4 ---
--- Tick 5 ---
Work done
Leaving WORKING state
Entering IDLE state
--- Tick 6 ---
--- Tick 7 ---
--- Tick 8 ---
Battery low, shutting down
Leaving IDLE state
Leaving ON state
Entering OFF state
--- Tick 9 ---
```

**State Hierarchy**:

```bash
OFF
ON (parent)
├── IDLE (initial)
└── WORKING
```

**X-Macro Explanation**:

- The `STATE_FOREACH` macro defines all state information in one place
- Through different macro expansions, it auto-generates the `enum state` and `states[]` table
- When adding new states, you only need to modify the `STATE_FOREACH` macro; the enum and state table update automatically
- Function declarations are kept handwritten for readability

**Key Concepts**:

- **State Callbacks**:

  - `entry`: Called once when entering a state, for initialization
  - `run`: Called repeatedly while in the state, for event handling and transitions
  - `exit`: Called once when leaving a state, for cleanup
  - All callbacks can be `NULL`

- **Hierarchical State Machine**:

  - `parent`: Parent state pointer, `NULL` for root states
  - `initial`: Initial child state pointer, auto-entered when entering parent, `NULL` for leaf states

- **Execution Order** (Hierarchical SM):

  - Entry: Parent → Child (`ON.entry` → `IDLE.entry`)
  - Run: Child → Parent (`IDLE.run` → `ON.run`)
  - Exit: Child → Parent (`IDLE.exit` → `ON.exit`)

- **Event Propagation**:

  - `SMF_EVENT_HANDLED`: Event handled, stop propagation
  - `SMF_EVENT_PROPAGATE`: Propagate to parent for further handling

- **⚠️ Important Restrictions**:
  - **Only call `smf_set_state()` in `run` functions**
  - Calling in `exit` returns `-1` (SM is mid-transition, nested transitions cause inconsistent state)
  - Calling in `entry` may cause infinite recursion (use `initial` field for initial child states)

## API Reference

### smf_set_initial(ctx, state, userdata)

Initializes the state machine and sets its initial state. If `state` has an `initial` field, it automatically enters the initial child state.

**Parameters**:

- `ctx` - State machine context
- `state` - Initial state (cannot be `NULL`)
- `userdata` - User data pointer (optional, for passing app data between states)

**Returns**: `0` on success, `-1` on failure

---

### smf_set_state(ctx, state)

Transitions to a new state. Automatically calls the current state's `exit` and the new state's `entry`.

**Parameters**:

- `ctx` - State machine context
- `state` - Target state (cannot be `NULL`)

**Returns**: `0` on success, `-1` on failure

**⚠️ Restrictions**:

- ❌ Cannot be called in `exit` functions
- ❌ Not recommended in `entry` functions
- ✅ Only call in `run` functions

---

### smf_run_state(ctx)

Runs one iteration of the state machine. Calls the `run` function of the current state and all its parent states.

**Parameters**:

- `ctx` - State machine context

**Returns**:

- `0` - Normal execution
- Non-zero - State machine terminated (value set by `smf_set_terminate()`)

---

### smf_set_terminate(ctx, val)

Terminates the state machine. `val` will be returned by `smf_run_state()`.

**Parameters**:

- `ctx` - State machine context
- `val` - Termination value (non-zero)

---

### smf_get_userdata(ctx) / smf_set_userdata(ctx, data)

Get/set user data. Used for passing application-specific data between states.

**Example**:

```c
typedef struct {
    int counter;
    bool flag;
} app_data_t;

app_data_t data = {0};
smf_set_initial(&ctx, &states[STATE_IDLE], &data);

// Access in state function
static void idle_run(smf_ctx_t *ctx) {
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);
    data->counter++;
}
```

---

### smf_get_current_leaf_state(ctx)

Gets the current leaf state (deepest state).

---

### smf_get_current_executing_state(ctx)

Gets the currently executing state (may be a parent state).

---

### SMF_CREATE_STATE(entry, run, exit, parent, initial)

Macro for defining states.

**Parameters**:

- `entry` - Function called when entering the state (can be `NULL`)
- `run` - Function called while in the state (can be `NULL`)
- `exit` - Function called when leaving the state (can be `NULL`)
- `parent` - Parent state pointer (`NULL` for root states)
- `initial` - Initial child state pointer (`NULL` for leaf states)
