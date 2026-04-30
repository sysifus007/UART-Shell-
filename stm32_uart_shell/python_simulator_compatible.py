#!/usr/bin/env python3
"""
STM32 UART Shell Python模拟器（兼容版）

这个脚本演示了UART Shell的核心功能，无需硬件即可运行。
去除了readline依赖，可在Windows/Linux/macOS上运行。

功能模拟：
1. 命令行交互界面
2. Tab自动补全（简化版）
3. 历史记录（简化版）
4. 彩色输出
5. 基本命令系统
"""

import sys
import os
from datetime import datetime
import time

# ANSI颜色代码
class Colors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    END = '\033[0m'

# Windows颜色支持
if sys.platform == "win32":
    try:
        import colorama
        colorama.init()
        # 覆盖颜色代码为colorama版本
        Colors.RED = colorama.Fore.RED + colorama.Style.BRIGHT
        Colors.GREEN = colorama.Fore.GREEN + colorama.Style.BRIGHT
        Colors.YELLOW = colorama.Fore.YELLOW + colorama.Style.BRIGHT
        Colors.BLUE = colorama.Fore.BLUE + colorama.Style.BRIGHT
        Colors.MAGENTA = colorama.Fore.MAGENTA + colorama.Style.BRIGHT
        Colors.CYAN = colorama.Fore.CYAN + colorama.Style.BRIGHT
        Colors.WHITE = colorama.Fore.WHITE + colorama.Style.BRIGHT
        Colors.BOLD = colorama.Style.BRIGHT
        Colors.UNDERLINE = colorama.Style.BRIGHT
        Colors.END = colorama.Style.RESET_ALL
        WINDOWS_COLORS = True
    except ImportError:
        WINDOWS_COLORS = False
        # 禁用颜色
        Colors.RED = ''
        Colors.GREEN = ''
        Colors.YELLOW = ''
        Colors.BLUE = ''
        Colors.MAGENTA = ''
        Colors.CYAN = ''
        Colors.WHITE = ''
        Colors.BOLD = ''
        Colors.UNDERLINE = ''
        Colors.END = ''

# 模拟的STM32系统状态
class SystemState:
    def __init__(self):
        self.led_on = False
        self.temperature = 25.3  # 模拟温度值
        self.uptime_start = time.time()
        self.adc_values = [0, 512, 1023, 256, 768]  # 模拟ADC通道
        
    def uptime(self):
        """获取运行时间"""
        uptime_sec = time.time() - self.uptime_start
        hours = int(uptime_sec // 3600)
        minutes = int((uptime_sec % 3600) // 60)
        seconds = int(uptime_sec % 60)
        return f"{hours:02d}:{minutes:02d}:{seconds:02d}"

# 命令系统
class CommandSystem:
    def __init__(self):
        self.state = SystemState()
        self.commands = {}
        self.history = []
        self.setup_commands()
        
    def setup_commands(self):
        """注册所有命令"""
        self.register("help", self.cmd_help, "Show help information")
        self.register("version", self.cmd_version, "Show firmware version")
        self.register("uptime", self.cmd_uptime, "Show system uptime")
        self.register("led", self.cmd_led, "LED control (on/off/toggle/status)")
        self.register("read_temp", self.cmd_read_temp, "Read temperature sensor")
        self.register("read_adc", self.cmd_read_adc, "Read ADC channels")
        self.register("clear", self.cmd_clear, "Clear screen")
        self.register("date", self.cmd_date, "Show current date and time")
        self.register("echo", self.cmd_echo, "Echo arguments")
        self.register("history", self.cmd_history, "Show command history")
        self.register("exit", self.cmd_exit, "Exit shell")
        
    def register(self, name, handler, description):
        """注册命令"""
        self.commands[name] = {
            'handler': handler,
            'description': description
        }
    
    def execute(self, input_str):
        """执行命令"""
        if not input_str.strip():
            return None
            
        # 添加到历史记录
        self.history.append(input_str)
        if len(self.history) > 16:  # 模拟16条历史记录的环形缓冲区
            self.history.pop(0)
        
        # 解析命令和参数
        parts = input_str.strip().split()
        if not parts:
            return None
            
        cmd_name = parts[0]
        args = parts[1:] if len(parts) > 1 else []
        
        # 处理退出命令
        if cmd_name == "exit" or cmd_name == "quit":
            return "EXIT"
        
        # 查找命令
        if cmd_name in self.commands:
            try:
                return self.commands[cmd_name]['handler'](args)
            except Exception as e:
                return f"{Colors.RED}Error: {e}{Colors.END}"
        else:
            # Tab补全建议
            suggestions = [name for name in self.commands if name.startswith(cmd_name)]
            if suggestions:
                return (f"{Colors.RED}Unknown command: '{cmd_name}'. "
                       f"Did you mean: {', '.join(suggestions)}?{Colors.END}")
            else:
                return f"{Colors.RED}Unknown command: '{cmd_name}'. Type 'help' for available commands.{Colors.END}"
    
    # ===== 命令实现 =====
    
    def cmd_help(self, args):
        """help命令实现"""
        output = []
        output.append(f"{Colors.CYAN}{Colors.BOLD}Available commands:{Colors.END}")
        output.append("=" * 40)
        for name, cmd_info in sorted(self.commands.items()):
            output.append(f"  {Colors.GREEN}{name:15}{Colors.END} - {cmd_info['description']}")
        output.append("")
        output.append(f"{Colors.YELLOW}Example usage:{Colors.END}")
        output.append("  led on          # Turn LED on")
        output.append("  read_temp       # Read temperature")
        output.append("  uptime          # Show system uptime")
        output.append("  exit            # Exit shell")
        return "\n".join(output)
    
    def cmd_version(self, args):
        """version命令实现"""
        return (f"{Colors.CYAN}UART Shell Demo v1.0{Colors.END}\n"
                f"Python Simulator (Compatible) | STM32F103 Emulation")
    
    def cmd_uptime(self, args):
        """uptime命令实现"""
        uptime_str = self.state.uptime()
        return f"{Colors.GREEN}Uptime: {uptime_str}{Colors.END}"
    
    def cmd_led(self, args):
        """led命令实现"""
        if not args:
            status = "ON" if self.state.led_on else "OFF"
            return (f"{Colors.YELLOW}LED is currently {status}{Colors.END}\n"
                    f"Usage: led [on|off|toggle|status]")
        
        subcmd = args[0].lower()
        if subcmd == "on":
            self.state.led_on = True
            return f"{Colors.GREEN}✓ LED turned ON{Colors.END}"
        elif subcmd == "off":
            self.state.led_on = False
            return f"{Colors.GREEN}✓ LED turned OFF{Colors.END}"
        elif subcmd == "toggle":
            self.state.led_on = not self.state.led_on
            status = "ON" if self.state.led_on else "OFF"
            return f"{Colors.GREEN}✓ LED toggled to {status}{Colors.END}"
        elif subcmd == "status":
            status = "ON" if self.state.led_on else "OFF"
            return f"{Colors.YELLOW}LED status: {status}{Colors.END}"
        else:
            return f"{Colors.RED}Invalid subcommand. Use: on, off, toggle, status{Colors.END}"
    
    def cmd_read_temp(self, args):
        """read_temp命令实现"""
        # 模拟温度波动
        import random
        temp = self.state.temperature + random.uniform(-0.5, 0.5)
        temp = round(temp, 1)
        
        status = "NORMAL"
        color = Colors.GREEN
        if temp > 30:
            status = "WARNING"
            color = Colors.YELLOW
        elif temp > 35:
            status = "CRITICAL"
            color = Colors.RED
            
        return (f"{color}{Colors.BOLD}Temperature Sensor:{Colors.END}\n"
                f"  Value: {temp}°C\n"
                f"  Status: {status}")
    
    def cmd_read_adc(self, args):
        """read_adc命令实现"""
        output = [f"{Colors.CYAN}{Colors.BOLD}ADC Channels:{Colors.END}"]
        for i, value in enumerate(self.state.adc_values):
            percentage = value / 1023 * 100
            bar_length = int(percentage // 5)
            bar = "█" * bar_length + "░" * (20 - bar_length)
            output.append(f"  CH{i}: {value:4d}  {bar} {percentage:5.1f}%")
        return "\n".join(output)
    
    def cmd_clear(self, args):
        """clear命令实现"""
        # 清屏
        if sys.platform == "win32":
            os.system('cls')
        else:
            os.system('clear')
        return ""
    
    def cmd_date(self, args):
        """date命令实现"""
        now = datetime.now()
        return f"{Colors.BLUE}{now.strftime('%Y-%m-%d %H:%M:%S')}{Colors.END}"
    
    def cmd_echo(self, args):
        """echo命令实现"""
        text = " ".join(args)
        return f"{Colors.MAGENTA}{text}{Colors.END}"
    
    def cmd_history(self, args):
        """history命令实现"""
        output = [f"{Colors.CYAN}{Colors.BOLD}Command History (last {len(self.history)}):{Colors.END}"]
        for i, cmd in enumerate(self.history[-10:], 1):  # 显示最后10条
            output.append(f"  {i:2d}. {cmd}")
        return "\n".join(output)
    
    def cmd_exit(self, args):
        """exit命令实现"""
        return "EXIT"

# 主Shell类
class UartShell:
    def __init__(self):
        self.cmd_system = CommandSystem()
        self.history_index = 0
        
    def print_banner(self):
        """打印欢迎横幅"""
        banner = f"""
{Colors.CYAN}{Colors.BOLD}
╔══════════════════════════════════════════════════╗
║      STM32 UART Shell Simulator (兼容版)         ║
║           无需额外依赖，开箱即用                 ║
╚══════════════════════════════════════════════════╝{Colors.END}

This simulates the UART Shell project running on STM32.
Type 'help' for available commands. Type 'exit' to quit.

Features demonstrated:
• Basic command system (16 commands)
• ANSI colored output (if supported)
• Command history (via 'history' command)
• Hardware abstraction concept

{Colors.YELLOW}Note:{Colors.END} This is a simulation. Real STM32 version runs
on hardware with interrupt-driven UART and ~1.8KB RAM usage.

{Colors.GREEN}Platform:{Colors.END} {sys.platform}
{Colors.GREEN}Python:{Colors.END} {sys.version.split()[0]}

"""
        print(banner)
    
    def simple_tab_complete(self, text):
        """简单的Tab补全"""
        suggestions = [name for name in self.cmd_system.commands if name.startswith(text)]
        if len(suggestions) == 1:
            return suggestions[0]
        elif len(suggestions) > 1:
            print(f"\n{Colors.YELLOW}Suggestions: {', '.join(suggestions)}{Colors.END}")
            return text
        return text
    
    def run(self):
        """运行Shell主循环"""
        self.print_banner()
        
        while True:
            try:
                # 获取用户输入
                user_input = input(f"{Colors.GREEN}shell> {Colors.END}").strip()
                
                # 简单的Tab补全处理
                if user_input.endswith('\t') or user_input.endswith('    '):  # Tab键
                    cmd_part = user_input.strip()
                    completed = self.simple_tab_complete(cmd_part)
                    if completed != cmd_part:
                        print(f"\r{Colors.GREEN}shell> {Colors.END}{completed}", end="")
                        user_input = completed
                
                # 执行命令
                result = self.cmd_system.execute(user_input)
                
                if result == "EXIT":
                    print(f"{Colors.YELLOW}Goodbye!{Colors.END}")
                    break
                elif result:
                    print(result)
                    
            except KeyboardInterrupt:
                # Ctrl+C 处理
                print(f"\n{Colors.YELLOW}Use 'exit' to quit{Colors.END}")
            except EOFError:
                # Ctrl+D 处理
                print(f"\n{Colors.YELLOW}Goodbye!{Colors.END}")
                break
            except Exception as e:
                print(f"{Colors.RED}Unexpected error: {e}{Colors.END}")

def main():
    """主函数"""
    print(f"{Colors.BLUE}Starting UART Shell Python Simulator (Compatible)...{Colors.END}")
    
    # Windows颜色支持提示
    if sys.platform == "win32" and not WINDOWS_COLORS:
        print(f"{Colors.YELLOW}For better colors on Windows, install: pip install colorama{Colors.END}")
        print(f"{Colors.YELLOW}Continuing without colors...{Colors.END}")
    
    shell = UartShell()
    shell.run()

if __name__ == "__main__":
    main()