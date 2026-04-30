# STM32 UART Shell/CLI 项目

> 物联网工程应届生嵌入式开发简历项目全套材料

## 项目概述

在STM32F103单片机上实现类似Linux的命令行终端，通过串口与电脑交互，提供直观的硬件调试和控制界面。

**核心特点：**
- 三层架构设计（应用/Shell核心/HAL适配）
- 动态命令注册机制，扩展无需修改核心
- 完整行编辑：Tab补全、历史记录、彩色输出
- 硬件无关设计，可轻松移植到其他MCU

## 文件结构

```
stm32_uart_shell/
├── Core/                    # STM32项目核心代码
│   ├── Inc/                # 头文件
│   │   ├── uart_shell.h    # Shell框架头文件
│   │   ├── cmd_system.h    # 系统命令头文件
│   │   ├── cmd_led.h       # LED命令头文件
│   │   └── cmd_sensor.h    # 传感器命令头文件
│   └── Src/                # 源文件
│       ├── uart_shell.c    # Shell核心实现
│       ├── cmd_system.c    # 系统命令实现
│       ├── cmd_led.c       # LED命令实现
│       ├── cmd_sensor.c    # 传感器命令实现
│       ├── hal_uart.c      # STM32 HAL适配
│       └── main.c          # STM32主程序
├── pc_sim_main.c           # PC模拟版本（标准C）
├── python_simulator.py     # Python演示脚本（完整功能）
├── python_simulator_compatible.py  # Python演示脚本（兼容版）
├── 简历项目描述_UART_Shell.md      # 可直接用于简历的项目描述
├── 面试准备_UART_Shell项目.md      # 详细面试准备指南
├── 项目亮点卡_UART_Shell.md        # 面试小抄/快速回顾
└── README.md               # 本文件
```

## 快速开始

### 硬件版本（STM32F103）
1. 使用STM32CubeIDE或Keil打开项目
2. 配置USART1为115200-8-N-1
3. 连接开发板串口到电脑
4. 使用串口终端（PuTTY/SecureCRT）连接
5. 输入`help`查看可用命令

### PC模拟版本
```bash
# 需要C编译器（gcc/clang）
cd stm32_uart_shell
gcc pc_sim_main.c -o uart_shell_sim
./uart_shell_sim
```

### Python演示版本

**完整功能版**（需要readline支持，推荐Linux/macOS）：
```bash
cd stm32_uart_shell
python python_simulator.py
# 或
python3 python_simulator.py
```

**兼容版**（无需额外依赖，Windows/Linux/macOS通用）：
```bash
cd stm32_uart_shell
python python_simulator_compatible.py
# 或
python3 python_simulator_compatible.py
```

## 面试准备材料

### 1. 简历内容
使用 **[简历项目描述_UART_Shell.md](./简历项目描述_UART_Shell.md)** 中的内容：
- **精简版**：适合简历空间有限的情况
- **完整版**：推荐用于详细项目列表

### 2. 面试指南
详细阅读 **[面试准备_UART_Shell项目.md](./面试准备_UART_Shell项目.md)**，包含：
- 不同面试官类型的应对策略
- 常见问题及标准答案
- 项目演示技巧
- 技术深度讲解要点

### 3. 快速回顾
打印 **[项目亮点卡_UART_Shell.md](./项目亮点卡_UART_Shell.md)** 作为面试小抄，包含：
- 核心亮点总结
- 技术架构要点
- 量化成果数据
- 常见问题关键词

## 核心功能展示

### 内置命令
| 类别 | 命令 | 功能 |
|------|------|------|
| 系统 | `help`, `version`, `reboot`, `uptime`, `free`, `ps`, `date`, `echo`, `clear`, `history` | 系统信息与管理 |
| LED | `led on`, `led off`, `led toggle`, `led status`, `led blink` | LED控制 |
| 传感器 | `read_temp`, `read_adc`, `read_gpio`, `read_dht` | 传感器数据读取 |

### 交互功能
- **Tab补全**：输入部分命令后按Tab自动补全
- **历史记录**：上下键浏览历史命令（环形缓冲区存储16条）
- **行编辑**：Backspace删除、光标左右移动
- **彩色输出**：ANSI转义码实现的彩色终端
- **参数解析**：支持引号包裹的参数和`#`注释

## 技术架构详解

### 三层架构设计
1. **应用层** (`cmd_*.c`)：具体命令实现，专注业务逻辑
2. **Shell核心层** (`uart_shell.c`)：命令解析、注册表、行编辑器
3. **HAL适配层** (`hal_uart.c`)：硬件抽象，6个函数指针接口

### 核心机制
- **动态命令注册**：通过`shell_register()`添加新命令
- **中断安全设计**：UART接收在中断中处理，命令执行在主循环
- **内存高效**：环形缓冲区管理，RAM占用<2KB
- **硬件抽象**：更换MCU只需重写HAL层的6个函数

## 模拟演示

即使没有硬件，也可以通过以下方式演示项目：

### Python模拟器
运行Python脚本体验完整交互：
```bash
python python_simulator.py
```

**演示流程：**
1. 运行脚本，显示欢迎横幅
2. 输入`help`查看所有命令
3. 尝试`led on`控制LED
4. 输入`read_temp`读取温度
5. 使用Tab补全功能
6. 按上下键查看历史记录

### 关键演示点
- **技术亮点**：强调三层架构和硬件抽象
- **用户体验**：展示Tab补全和历史记录
- **扩展性**：说明如何添加新命令
- **性能**：提到RAM占用和中断响应时间

## 面试常见问题

### HR常见问题
- **"这个项目最大的挑战是什么？"**
- **"从这个项目中学到了什么？"**
- **"如果重做这个项目，你会改进什么？"**

### 技术常见问题
- **"为什么不用现成的库？"**
- **"Shell在中断中处理是否安全？"**
- **"如何扩展一个新命令？"**
- **"DMA模式和中断模式的区别？"**

所有问题的详细答案见 **[面试准备_UART_Shell项目.md](./面试准备_UART_Shell项目.md)**。

## 延伸学习

### 项目演进方向
1. **集成RTOS**：将Shell作为独立FreeRTOS任务
2. **网络化**：添加Telnet/SSH支持
3. **文件系统**：集成FATFS，支持文件操作命令
4. **脚本引擎**：支持简单的Shell脚本

### 推荐学习资源
- 《嵌入式实时操作系统应用开发》- RTOS集成
- 《C语言接口与实现》- 更优秀的模块化设计
- FreeRTOS源码分析 - 工业级架构设计
- STM32社区论坛 - 实际工程问题解决

## 许可证

本项目仅用于学习和面试准备。代码可自由使用、修改和分发。

---

## 更新日志

### 2026-04-30
- 创建完整的STM32 UART Shell项目
- 添加PC模拟版本
- 创建Python演示脚本
- 编写简历项目描述
- 制作面试准备材料
- 准备项目亮点卡

---

**祝面试顺利！**

> *提示：面试前至少花30分钟阅读面试准备材料，重点记忆量化数据和架构关键词。*