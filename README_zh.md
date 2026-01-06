# SMF

![CMake](https://img.shields.io/badge/CMake-3.14%2B-brightgreen?logo=cmake&logoColor=white)
[![Release](https://img.shields.io/github/v/release/tayne3/SMF?include_prereleases&label=release&logo=github&logoColor=white)](https://github.com/tayne3/SMF/releases)
[![Tag](https://img.shields.io/github/v/tag/tayne3/SMF?color=%23ff8936&style=flat-square&logo=git&logoColor=white)](https://github.com/tayne3/SMF/tags)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/tayne3/SMF)

[English](README.md) | **中文**

SMF 是一个轻量级的层次状态机框架，支持状态嵌套、事件传播和初始转换。适用于嵌入式系统和需要状态管理的应用。

## 项目背景

本项目基于 [Zephyr RTOS](https://github.com/zephyrproject-rtos/zephyr) 的 [SMF (State Machine Framework)](https://docs.zephyrproject.org/latest/services/smf/index.html) 模块改编而来。Zephyr 的 SMF 是一个优秀的层次状态机实现，但它与 Zephyr 生态紧密耦合。本项目将其剥离为一个**独立的、可移植的 C 库**，可在任何 C/C++ 项目中使用，无需依赖 Zephyr。

## 与 Zephyr SMF 的主要区别

| 方面           | Zephyr 原版                                     | 本项目                                             |
| -------------- | ----------------------------------------------- | -------------------------------------------------- |
| **依赖**       | `<zephyr/kernel.h>`, `<zephyr/sys/util.h>` 等   | 仅 `<stddef.h>`, `<stdint.h>`，完全独立            |
| **条件编译**   | 通过 `CONFIG_SMF_ANCESTOR_SUPPORT` 等宏控制特性 | 始终启用层次状态机和初始转换支持                   |
| **上下文结构** | `internal` 字段通过指针强转访问                 | 直接使用 `flags` 结构体成员，更清晰                |
| **用户数据**   | 需嵌入 `smf_ctx` 到自定义结构体                 | 新增 `userdata` 字段和访问函数                     |
| **API 签名**   | `smf_set_initial(ctx, state)` 返回 `void`       | `smf_set_initial(ctx, state, userdata)` 返回 `int` |
| **错误处理**   | 使用日志宏，无返回值                            | 返回 `-1` 表示失败                                 |
| **函数指针**   | 参数为 `void *obj`                              | 参数为 `smf_ctx_t *ctx`，类型更明确                |

### 用户数据管理改进

**Zephyr 原版**需要将 `smf_ctx` 嵌入自定义结构体：

```c
// Zephyr 方式
struct my_ctx {
    struct smf_ctx ctx;  // 必须是第一个成员
    int my_data;
};
struct my_ctx my = { ... };
smf_set_initial(&my.ctx, ...);

void my_entry(void *obj) {
    struct my_ctx *ctx = (struct my_ctx *)obj;  // 需要强制转换
}
```

**本项目**使用独立的 `userdata` 指针：

```c
// 本项目方式
app_data_t data = {0};
smf_set_initial(&ctx, &states[STATE_IDLE], &data);

void idle_entry(smf_ctx_t *ctx) {
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);  // 类型安全访问
}
```

> [!IMPORTANT]
> 本项目的 API 与 Zephyr SMF **不兼容**，不能直接将 Zephyr 的 SMF 代码复制过来使用。

## 完整示例

```c
#include "smf/smf.h"
#include <stdio.h>

// 应用数据
typedef struct {
    int tick_count;  // 运行计数
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

// 状态枚举
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

// ========== 状态函数实现 ==========

// OFF 状态
static void off_entry(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Entering OFF state\n");
}

static smf_state_result_t off_run(smf_ctx_t *ctx) {
    (void)ctx;
    return SMF_EVENT_HANDLED;
}

// ON 状态（父状态）
static void on_entry(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Entering ON state\n");
}

static smf_state_result_t on_run(smf_ctx_t *ctx) {
    // 父状态处理所有子状态的公共逻辑
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

    // 运行 8 个周期后关机
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

// IDLE 状态（子状态）
static void idle_entry(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Entering IDLE state\n");
}

static smf_state_result_t idle_run(smf_ctx_t *ctx) {
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

    // 运行 3 个周期后开始工作
    if (data->tick_count == 3) {
        printf("Work requested\n");
        smf_set_state(ctx, &states[STATE_WORKING]);  // ✅ 在 run 中转换状态
        return SMF_EVENT_HANDLED;
    }

    return SMF_EVENT_PROPAGATE;  // 传播给父状态（ON）处理
}

static void idle_exit(smf_ctx_t *ctx) {
    (void)ctx;
    // ⚠️ 不要在这里调用 smf_set_state()
    printf("Leaving IDLE state\n");
}

// WORKING 状态（子状态）
static void working_entry(smf_ctx_t *ctx) {
    (void)ctx;
    printf("Entering WORKING state\n");
}

static smf_state_result_t working_run(smf_ctx_t *ctx) {
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

    // 工作 2 个周期后完成
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

// ========== 主函数 ==========

int main(void) {
    smf_ctx_t ctx;
    app_data_t data = {0};

    // 初始化状态机到 ON 状态
    // 因为 ON 的 initial 是 IDLE，所以会自动进入 IDLE
    smf_set_initial(&ctx, &states[STATE_ON], &data);

    // 运行状态机
    for (int i = 0; i < 10 && !ctx.terminate_val; i++) {
        printf("--- Tick %d ---\n", data.tick_count);
        smf_run_state(&ctx);
        data.tick_count++;
    }

    return 0;
}
```

**输出**：

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

**状态层次结构**：

```bash
OFF
ON (parent)
├── IDLE (initial)
└── WORKING
```

**X-Macro 说明**：

- `STATE_FOREACH` 宏定义了所有状态的信息（一处定义）
- 通过不同的宏展开，自动生成枚举 `enum state` 和状态表 `states[]`
- 添加新状态时只需修改 `STATE_FOREACH` 宏，枚举和状态表会自动更新
- 函数声明保持手写，便于阅读和理解

**关键概念**：

- **状态回调**：

  - `entry`：进入状态时调用一次，用于初始化
  - `run`：状态运行时重复调用，用于处理事件和状态转换
  - `exit`：离开状态时调用一次，用于清理资源
  - 所有回调都可以为 `NULL`

- **层次状态机**：

  - `parent`：父状态指针，`NULL` 表示根状态
  - `initial`：初始子状态指针，进入父状态时自动进入这个子状态，`NULL` 表示叶状态

- **执行顺序**（层次状态机）：

  - Entry: 父 → 子（`ON.entry` → `IDLE.entry`）
  - Run: 子 → 父（`IDLE.run` → `ON.run`）
  - Exit: 子 → 父（`IDLE.exit` → `ON.exit`）

- **事件传播**：

  - `SMF_EVENT_HANDLED`：事件已处理，停止传播
  - `SMF_EVENT_PROPAGATE`：传播给父状态继续处理

- **⚠️ 重要限制**：
  - **只能在 `run` 函数中调用 `smf_set_state()`**
  - 在 `exit` 中调用会返回 `-1`（状态机正在转换中，嵌套转换会导致状态不一致）
  - 在 `entry` 中调用可能导致无限递归（应该使用 `initial` 字段定义初始子状态）

## API 参考

### smf_set_initial(ctx, state, userdata)

初始化状态机并设置初始状态。如果 `state` 有 `initial` 字段，会自动进入初始子状态。

**参数**：

- `ctx` - 状态机上下文
- `state` - 初始状态（不能为 `NULL`）
- `userdata` - 用户数据指针（可选，用于在状态间传递应用数据）

**返回值**：成功返回 `0`，失败返回 `-1`

---

### smf_set_state(ctx, state)

转换到新状态。会自动调用当前状态的 `exit` 和新状态的 `entry`。

**参数**：

- `ctx` - 状态机上下文
- `state` - 目标状态（不能为 `NULL`）

**返回值**：成功返回 `0`，失败返回 `-1`

**⚠️ 限制**：

- ❌ 不能在 `exit` 函数中调用
- ❌ 不建议在 `entry` 函数中调用
- ✅ 只能在 `run` 函数中调用

---

### smf_run_state(ctx)

运行状态机的一次迭代。会调用当前状态及其所有父状态的 `run` 函数。

**参数**：

- `ctx` - 状态机上下文

**返回值**：

- `0` - 正常运行
- 非零值 - 状态机已终止（由 `smf_set_terminate()` 设置的值）

---

### smf_set_terminate(ctx, val)

终止状态机。`val` 会被 `smf_run_state()` 返回。

**参数**：

- `ctx` - 状态机上下文
- `val` - 终止值（非零）

---

### smf_get_userdata(ctx) / smf_set_userdata(ctx, data)

获取/设置用户数据。用于在状态间传递应用特定的数据。

**示例**：

```c
typedef struct {
    int counter;
    bool flag;
} app_data_t;

app_data_t data = {0};
smf_set_initial(&ctx, &states[STATE_IDLE], &data);

// 在状态函数中访问
static void idle_run(smf_ctx_t *ctx) {
    app_data_t *data = (app_data_t *)smf_get_userdata(ctx);
    data->counter++;
}
```

---

### smf_get_current_leaf_state(ctx)

获取当前的叶状态（最深层的状态）。

---

### smf_get_current_executing_state(ctx)

获取当前正在执行的状态（可能是父状态）。

---

### SMF_CREATE_STATE(entry, run, exit, parent, initial)

定义状态的宏。

**参数**：

- `entry` - 进入状态时调用的函数（可为 `NULL`）
- `run` - 状态运行时调用的函数（可为 `NULL`）
- `exit` - 离开状态时调用的函数（可为 `NULL`）
- `parent` - 父状态指针（根状态为 `NULL`）
- `initial` - 初始子状态指针（叶状态为 `NULL`）
