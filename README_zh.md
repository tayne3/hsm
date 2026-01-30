# HSM (C++11)

![CMake](https://img.shields.io/badge/CMake-3.14%2B-brightgreen?logo=cmake&logoColor=white)
[![Release](https://img.shields.io/github/v/release/tayne3/HSM?include_prereleases&label=release&logo=github&logoColor=white)](https://github.com/tayne3/HSM/releases)
[![Tag](https://img.shields.io/github/v/tag/tayne3/HSM?color=%23ff8936&style=flat-square&logo=git&logoColor=white)](https://github.com/tayne3/HSM/tags)

[English](README.md) | **中文**

HSM 是一个轻量级、仅头文件的 C++11 **分层状态机 (HSM)** 库。它支持嵌套状态、事件传播和类型安全的上下文管理，适用于嵌入式系统和复杂的状态逻辑。

## 特性

- **仅头文件 (Header-only)**: 易于集成，只需包含 `hsm/hsm.hpp`。
- **分层 (Hierarchical)**: 支持嵌套状态（父/子关系）。
- **类型安全 (Type-Safe)**: 使用 C++ 模板自定义 `Context`、`Event` 和 `StateID` 类型。
- **定义灵活 (Flexible Definition)**: 可以使用类或内联 Lambda 表达式定义状态。
- **LCA 转换 (LCA Transitions)**: 基于最近公共祖先 (LCA) 逻辑自动处理 `exit` 和 `entry` 链。
- **事件传播 (Event Propagation)**: 未处理的事件会从子状态向上传播到父状态。

## 集成

由于这是一个仅头文件的库，您只需将 `include` 目录添加到项目的包含路径中即可。

```cmake
target_include_directories(my_target PRIVATE path/to/hsm/include)
```

## 完整示例

```cpp
#include "hsm/hsm.hpp"
#include <cstdio>

// 1. 定义事件和状态 ID
enum class Event { Tick, WorkReq, WorkDone, LowBattery };
enum class StateID { Off, On, Idle, Working };

// 2. 定义上下文 (用户数据)
struct MyContext {
    int ticks = 0;
    void log(const char* msg) { printf("[LOG] %s\n", msg); }
};

// 3. 定义 Traits
struct MyTraits {
    using Context = MyContext;
    using Event = Event;
    using StateID = StateID;
};

// 为了方便，定义别名
using MyMachine = hsm::Machine<MyTraits>;

int main() {
    // 初始化状态机，可传入 Context 构造参数 (如果有)
    MyMachine sm;

    // 配置并启动状态机
    // 我们在 start() 配置 lambda 中定义层次结构
    sm.start(StateID::Idle, [](hsm::Scope<MyTraits>& s) {

        // 定义 'Off' 状态 (叶子节点)
        s.state(StateID::Off,
            [](MyMachine& m, const Event& e) { return hsm::Result::Pass; }, // Handle (处理)
            [](MyMachine& m) { m->log("Enter Off"); },                      // Entry (进入)
            [](MyMachine& m) { m->log("Exit Off"); },                       // Exit (退出)
            "Off"                                                           // Name (名称)
        );

        // 定义 'On' 状态 (父节点)
        s.state(StateID::On,
            [](MyMachine& m, const Event& e) {
                // 父状态为所有子状态处理 LowBattery 事件
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
        // 定义 'On' 的子状态
        .with([](hsm::Scope<MyTraits>& s) {

            // 'Idle' 状态
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

            // 'Working' 状态
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

    // 运行事件
    printf("--- Sending Tick ---\n");
    sm.handle(Event::Tick);

    printf("--- Sending WorkReq ---\n");
    sm.handle(Event::WorkReq);  // 转换: Idle -> Working

    printf("--- Sending WorkDone ---\n");
    sm.handle(Event::WorkDone); // 转换: Working -> Idle

    printf("--- Sending LowBattery ---\n");
    sm.handle(Event::LowBattery); // 转换: Idle -> Off (由 On 处理)

    return 0;
}
```

## API 参考

### `hsm::Machine<Traits>`

核心状态机引擎。

- **模板参数**: `Traits` 结构体，包含 `Context`, `Event`, 和 `StateID` 类型。
- **构造函数**: `Machine(Args&&... args)` - 将参数转发给 `Context` 的构造函数。
- **`start(StateID initial, config_fn)`**: 配置状态层次结构并进入初始状态。
- **`handle(const Event&)`**: 向活动状态分发事件。如果未处理 (`Result::Pass`)，则传播到父状态。
- **`transition(StateID target)`**: 调度转换到目标状态。
- **`context()`**: 返回用户上下文的引用。

### `hsm::State<Traits>`

状态的基类。您可以继承此类来创建复杂状态。

- **`handle(Machine&, const Event&)`**: 如果已处理返回 `Result::Done`，如果要传播返回 `Result::Pass`。
- **`on_entry(Machine&)`**: 进入状态时调用。
- **`on_exit(Machine&)`**: 退出状态时调用。

### `hsm::Result`

- `Result::Done`: 事件已处理，停止传播。
- `Result::Pass`: 事件未处理，继续传播到父状态。
