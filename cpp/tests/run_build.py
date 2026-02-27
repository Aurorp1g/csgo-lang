# -*- coding: utf-8 -*-

"""
CSGO Language Tests工具构建脚本
"""

import json
import os
import re
import subprocess
import sys
import time
import shutil
import unicodedata
from pathlib import Path
from datetime import datetime
from typing import Tuple, List, Dict

# ═══════════════════════════════════════════════════════════════
# 工具函数 - 终端宽度计算
# ═══════════════════════════════════════════════════════════════

def display_width(text: str) -> int:
    """计算字符串在终端中的显示宽度"""
    width = 0
    for char in str(text):
        if unicodedata.east_asian_width(char) in ('F', 'W', 'A'):
            width += 2
        else:
            width += 1
    return width

def pad(text: str, width: int, align: str = 'left') -> str:
    """按显示宽度填充字符串"""
    text_width = display_width(text)
    padding = width - text_width
    if padding <= 0:
        return text
    if align == 'left':
        return text + ' ' * padding
    elif align == 'right':
        return ' ' * padding + text
    else:  # center
        left = padding // 2
        right = padding - left
        return ' ' * left + text + ' ' * right

def truncate(text: str, max_width: int, suffix: str = '...') -> str:
    """按显示宽度截断字符串"""
    if display_width(text) <= max_width:
        return text
    result = ''
    current_width = 0
    for char in text:
        char_width = 2 if unicodedata.east_asian_width(char) in ('F', 'W', 'A') else 1
        if current_width + char_width + display_width(suffix) > max_width:
            return result + suffix
        result += char
        current_width += char_width
    return result

# ═══════════════════════════════════════════════════════════════
# 进程管理
# ═══════════════════════════════════════════════════════════════

def kill_process_tree(pid: int):
    """完全终止进程及其所有子进程（Windows）"""
    try:
        subprocess.run(
            ['taskkill', '/F', '/T', '/PID', str(pid)],
            capture_output=True,
            timeout=5
        )
        return True
    except Exception:
        return False

def terminate_process(proc):
    """优雅终止进程，如果失败则强制杀死"""
    try:
        proc.terminate()
        try:
            proc.wait(timeout=3)
            return True
        except subprocess.TimeoutExpired:
            proc.kill()
            try:
                proc.wait(timeout=2)
                return True
            except Exception:
                pass
            
            if proc.pid:
                return kill_process_tree(proc.pid)
            return False
    except Exception:
        if proc.pid:
            return kill_process_tree(proc.pid)
        return False

def cleanup_old_processes():
    """清理可能残留的旧进程"""
    try:
        for proc_name in ['test_tokenizer.exe', 'test_parser.exe', 'test_ast.exe', 
                          'test_semantic.exe', 'test_interpreter.exe', 'test_optimizer.exe']:
            try:
                subprocess.run(
                    ['taskkill', '/F', '/IM', proc_name],
                    capture_output=True,
                    timeout=3
                )
            except Exception:
                pass
    except Exception:
        pass

# ═══════════════════════════════════════════════════════════════
# 颜色系统
# ═══════════════════════════════════════════════════════════════

class Colors:
    """真彩色支持"""
    CYAN = '\033[38;2;0;255;255m'
    BULE = '\033[38;2;0;191;255m'
    ORANGE = '\033[38;2;255;165;0m'
    PINK = '\033[38;2;255;105;180m'
    SUCCESS = '\033[38;2;0;255;127m'
    ERROR = '\033[38;2;255;69;0m'
    WARNING = '\033[38;2;255;193;7m'
    INFO = '\033[38;2;100;149;237m'
    WHITE = '\033[38;2;255;255;255m'
    GRAY_LIGHT = '\033[38;2;200;200;200m'
    GRAY = '\033[38;2;150;150;150m'
    GRAY_DARK = '\033[38;2;100;100;100m'
    BOLD = '\033[1m'
    DIM = '\033[2m'
    END = '\033[0m'

    @classmethod
    def gradient(cls, text: str, start: Tuple[int, int, int], end: Tuple[int, int, int]) -> str:
        """渐变文字"""
        result = []
        length = len(text)
        for i, char in enumerate(text):
            ratio = i / max(length - 1, 1)
            r = int(start[0] + (end[0] - start[0]) * ratio)
            g = int(start[1] + (end[1] - start[1]) * ratio)
            b = int(start[2] + (end[2] - start[2]) * ratio)
            result.append(f'\033[38;2;{r};{g};{b}m{char}')
        return ''.join(result) + cls.END

    @classmethod
    def disable(cls):
        for attr in dir(cls):
            if not attr.startswith('_') and isinstance(getattr(cls, attr), str):
                setattr(cls, attr, '')

def grad_cyan_orange(text: str) -> str:
    return Colors.gradient(text, (0, 255, 255), (255, 165, 0))

def grad_pink_gold(text: str) -> str:
    return Colors.gradient(text, (255, 105, 180), (255, 215, 0))

# ═══════════════════════════════════════════════════════════════
# 智能分析器
# ═══════════════════════════════════════════════════════════════

class Analyzer:
    """智能输出分析"""

    PATTERNS = {
        'real_error': [
            r'^\s*error[\s:]', r'\[ERROR\]', r'build failed', 
            r'undefined reference', r'cannot find', r'permission denied',
            r'segmentation fault', r'core dumped', r'fatal error'
        ],
        'test_scene': [
            r'test.*error.*handling', r'error.*test', r'testing.*exceptions?',
            r'expect.*error', r'should.*fail', r'negative.*test'
        ],
        'success': [
            r'\bpassed\b', r'\bsuccess\b', r'\bdone\b', 
            r'\[\s*OK\s*\]', r'100%.*passed', r'build.*complete',
            r'\[100%\]'
        ],
        'warning': [
            r'^\s*warning[\s:]', r'\[WARNING\]', r'deprecated', r'obsolete'
        ],
        'progress': [
            r'\[.*?\]', r'building\s+\w+', r'linking\s+\w+', 
            r'generating', r'scanning', r'-->'
        ]
    }

    @classmethod
    def analyze(cls, line: str) -> Tuple[str, str]:
        line_lower = line.lower()

        # 测试场景优先
        for pattern in cls.PATTERNS['test_scene']:
            if re.search(pattern, line_lower):
                if any(re.search(p, line_lower) for p in cls.PATTERNS['success']):
                    return ('test_pass', Colors.SUCCESS)
                return ('test', Colors.INFO)

        for pattern in cls.PATTERNS['real_error']:
            if re.search(pattern, line_lower):
                return ('error', Colors.ERROR)

        for pattern in cls.PATTERNS['warning']:
            if re.search(pattern, line_lower):
                return ('warning', Colors.WARNING)

        for pattern in cls.PATTERNS['success']:
            if re.search(pattern, line_lower):
                return ('success', Colors.SUCCESS)

        for pattern in cls.PATTERNS['progress']:
            if re.search(pattern, line_lower):
                return ('progress', Colors.CYAN)

        return ('normal', Colors.GRAY_LIGHT)

# ═══════════════════════════════════════════════════════════════
# UI 组件 - 修复对齐
# ═══════════════════════════════════════════════════════════════

class UI:
    ICONS = {
        'check': '✓', 'cross': '✗', 'warning': '⚡', 'info': '◉',
        'gear': '⚙', 'star': '★', 'arrow': '➜', 'play': '▶',
        'success': '✔', 'error': '✖', 'test': '🧪', 'build': '🔨',
        'time': '⏱', 'cog': '⚙', 'fire': '🔥', 'zap': '⚡',
        'box': '■', 'circle': '●', 'dot': '·'
    }

    # 列宽配置
    COL_ID = 4
    COL_DESC = 28
    COL_CMD = 40

    @staticmethod
    def clear():
        print('\033[2J\033[H', end='')

    @staticmethod
    def header():
        """ASCII标题"""
        lines = [
            "",
            "      ██████╗███████╗ ██████╗  ██████╗     ",
            "     ██╔════╝██╔════╝██╔════╝ ██╔═══██╗    ",
            "     ██║     ███████╗██║  ███╗██║   ██║    ",
            "     ██║     ╚════██║██║   ██║██║   ██║    ",
            "     ╚██████╗███████║╚██████╔╝╚██████╔╝    ",
            "      ╚═════╝╚══════╝ ╚═════╝  ╚═════╝     "
        ]
        print()
        for line in lines:
            print(f"{Colors.BOLD}{grad_cyan_orange(line)}{Colors.END}")
        subtitle = "L A N G U A G E   B U I L D   T O O L"
        print(f"\n{Colors.GRAY}{pad(subtitle, 41, 'right')}{Colors.END}")
        print(f"{Colors.PINK}{'━' * 47}{Colors.END}\n")

    @staticmethod
    def separator(char: str = '─', color: str = None):
        if color is None:
            color = Colors.GRAY_DARK
        try:
            width = shutil.get_terminal_size().columns
        except:
            width = 80
        print(f"{color}{char * width}{Colors.END}")

    @staticmethod
    def menu_table(commands: List[Dict]):
        """完美对齐的菜单表格"""
        print(f"{Colors.BOLD}{grad_pink_gold('构建菜单')}{Colors.END}")
        UI.separator('━', Colors.PINK)

        # 表头
        id_h = pad("ID", UI.COL_ID, 'center')
        desc_h = pad("描述", UI.COL_DESC, 'center')
        cmd_h = pad("命令", UI.COL_CMD, 'center')

        print(f"\n {Colors.ORANGE}{id_h}{Colors.END} {Colors.GRAY}│{Colors.END} "
              f"{Colors.WHITE}{desc_h}{Colors.END} {Colors.GRAY}│{Colors.END} "
              f"{Colors.GRAY}{cmd_h}{Colors.END}")

        UI.separator('─', Colors.GRAY_DARK)

        # 菜单项
        for i, cmd in enumerate(commands):
            is_even = (i % 2 == 0)
            bg = Colors.DIM if not is_even else ''

            cmd_id = str(cmd['id'])
            desc = cmd.get('description', 'Unknown')
            command = cmd.get('command', '')

            # 截断并填充
            id_col = pad(cmd_id, UI.COL_ID, 'center')
            desc_col = pad(truncate(desc, UI.COL_DESC), UI.COL_DESC)
            cmd_col = pad(truncate(command, UI.COL_CMD - 3), UI.COL_CMD)

            # 颜色
            id_color = Colors.ORANGE if cmd_id != '0' else Colors.ERROR

            line = (f" {bg}{id_color}{id_col}{Colors.END} {Colors.GRAY}│{Colors.END} "
                    f"{bg}{desc_col}{Colors.END} {Colors.GRAY}│{Colors.END} "
                    f"{bg}{Colors.GRAY_DARK}{cmd_col}{Colors.END}")
            print(line)

        # 退出选项
        exit_id = pad('0', UI.COL_ID, 'center')
        exit_desc = pad(truncate('退出程序', UI.COL_DESC), UI.COL_DESC)
        exit_cmd = pad('Exit', UI.COL_CMD)
        is_even = (len(commands) % 2 == 0)
        bg = Colors.DIM if not is_even else ''

        print(f" {bg}{Colors.ERROR}{exit_id}{Colors.END} {Colors.GRAY}│{Colors.END} "
              f"{bg}{exit_desc}{Colors.END} {Colors.GRAY}│{Colors.END} "
              f"{bg}{Colors.GRAY_DARK}{exit_cmd}{Colors.END}")

        UI.separator('━', Colors.PINK)

    @staticmethod
    def info_panel(title: str, items: List[Tuple[str, str]], icon: str = 'gear'):
        """信息面板"""
        icon_char = UI.ICONS.get(icon, '•')
        max_label = max(display_width(item[0]) for item in items) if items else 10

        print(f"\n{Colors.CYAN}╔═══ {icon_char} {title} {'═' * (40 - display_width(title))}╗{Colors.END}")
        for label, value in items:
            label_padded = pad(label, max_label + 2)
            print(f"{Colors.CYAN}║{Colors.END} {Colors.GRAY}{label_padded}{Colors.END} {value}")
        print(f"{Colors.CYAN}╚{'═' * 48}╝{Colors.END}")

    @staticmethod
    def result_stats(success: bool, elapsed: float, stats: Dict, timed_out: bool = False):
        """结果统计"""
        if timed_out:
            color = Colors.WARNING
            icon = UI.ICONS['warning']
            text = "TIMED OUT"
        elif success:
            color = Colors.SUCCESS
            icon = UI.ICONS['success']
            text = "BUILD SUCCESS"
        else:
            color = Colors.ERROR
            icon = UI.ICONS['error']
            text = "BUILD FAILED"

        print()
        UI.separator('═', color)

        big_text = f"{icon}  {text}  {icon}"
        padding = (50 - display_width(big_text)) // 2
        print(' ' * padding + f"{Colors.BOLD}{color}{big_text}{Colors.END}")

        # 统计行
        stat_line = (f"{Colors.SUCCESS}{UI.ICONS['success']} {stats.get('success', 0)}{Colors.END}   "
                     f"{Colors.WARNING}{UI.ICONS['warning']} {stats.get('warning', 0)}{Colors.END}   "
                     f"{Colors.ERROR}{UI.ICONS['error']} {stats.get('error', 0)}{Colors.END}   "
                     f"{Colors.INFO}{UI.ICONS['test']} {stats.get('test', 0)}{Colors.END}")
        stat_padding = (50 - display_width("✔ 0   ⚡ 0   ✖ 0   🧪 0")) // 2
        print(' ' * stat_padding + stat_line)

        time_text = f"耗时: {elapsed:.2f}s"
        time_padding = (50 - display_width(time_text)) // 2
        print(' ' * time_padding + f"{Colors.GRAY}{time_text}{Colors.END}")

        UI.separator('═', color)

# ═══════════════════════════════════════════════════════════════
# 核心功能
# ═══════════════════════════════════════════════════════════════

def load_commands(json_path: str) -> List[Dict]:
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            commands = data.get('commands', [])
            print(f"{Colors.SUCCESS}✔ 配置加载成功{Colors.END}")
            return commands
    except Exception as e:
        print(f"{Colors.ERROR}✖ 错误: {e}{Colors.END}")
        return []

def run_command(command: str, cwd: str, description: str, timeout: int = 30) -> Tuple[int, List[str]]:
    start = time.time()

    # 信息面板
    UI.info_panel("构建任务", [
        ("目标", f"{Colors.WHITE}{description}{Colors.END}"),
        ("命令", f"{Colors.GRAY_DARK}{truncate(command, 35)}{Colors.END}"),
        ("目录", f"{Colors.GRAY_DARK}{truncate(cwd, 35)}{Colors.END}"),
        ("时间", f"{Colors.CYAN}{datetime.now().strftime('%H:%M:%S')}{Colors.END}"),
        ("时限", f"{Colors.WARNING}{timeout}s{Colors.END}")
    ], icon='build')
    print()

    output_lines = []
    stats = {'error': 0, 'warning': 0, 'success': 0, 'test': 0}
    timed_out = False

    try:
        ps_cmd = f'powershell -Command "cd \'{cwd}\'; {command}"'
        proc = subprocess.Popen(
            ps_cmd, shell=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='ignore'
        )

        # 输出框
        print(f"{Colors.BULE}{'OUTPUT'}{Colors.END}")
        UI.separator('━', Colors.BULE)

        # 使用多线程读取输出，支持超时检测
        import threading
        output_queue = []
        lock = threading.Lock()
        
        def read_output():
            try:
                for line in iter(proc.stdout.readline, ''):
                    if line:
                        with lock:
                            output_queue.append(line.rstrip())
            except:
                pass
        
        reader_thread = threading.Thread(target=read_output, daemon=True)
        reader_thread.start()
        
        # 主循环：处理输出并检查超时
        last_output_time = start
        
        while True:
            # 检查是否超时
            if (time.time() - start) > timeout:
                print(f"\n{Colors.ERROR}✖ Timeout!Command execution exceeded {timeout} seconds{Colors.END}")
                print(f"{Colors.WARNING}⚡ 正在终止进程...{Colors.END}")
                terminate_process(proc)
                timed_out = True
                break
            
            # 检查是否有新输出
            with lock:
                if len(output_queue) > len(output_lines):
                    last_output_time = time.time()
                    new_lines = output_queue[len(output_lines):]
                    
                    for line in new_lines:
                        output_lines.append(line)
                        
                        ltype, lcolor = Analyzer.analyze(line)
                        if ltype == 'error': stats['error'] += 1
                        elif ltype == 'warning': stats['warning'] += 1
                        elif ltype == 'success': stats['success'] += 1
                        elif ltype == 'test_pass': stats['test'] += 1
                        
                        prefixes = {
                            'error': ('✖', Colors.ERROR),
                            'warning': ('⚡', Colors.WARNING),
                            'success': ('✔', Colors.SUCCESS),
                            'test_pass': ('✔', Colors.SUCCESS),
                            'test': ('🧪', Colors.INFO),
                            'progress': ('→', Colors.CYAN),
                        }
                        prefix, pcolor = prefixes.get(ltype, ('', Colors.GRAY_DARK))
                        
                        print(f"{Colors.GRAY}{Colors.END} {pcolor}{prefix}{Colors.END} {line}")
            
            # 检查进程是否结束
            if proc.poll() is not None and len(output_queue) == len(output_lines):
                break
            
            # 短暂休眠避免CPU占用过高
            time.sleep(0.05)

        UI.separator('━', Colors.BULE)

        elapsed = time.time() - start
        
        if timed_out:
            success = False
            return_code = -1
            output_lines.append(f"\n[Timeout: Command execution exceeded {timeout} seconds]")
        else:
            return_code = proc.returncode
            success = (return_code == 0)

        UI.result_stats(success, elapsed, stats, timed_out=timed_out)

        return return_code, output_lines

    except Exception as e:
        print(f"{Colors.ERROR}执行异常: {e}{Colors.END}")
        return 1, output_lines

def copy_to_clipboard(text: str) -> bool:
    """将文本复制到剪贴板（Windows），去除ANSI转义序列"""
    # 去除 ANSI 转义序列 (CSI 格式: ESC [...参数...字母)
    # 颜色代码: m, 光标移动: H/f, 清屏: J, 上移: A-K 等
    ansi_escape = re.compile(r'\x1b\[[0-9;]*[A-Za-z]')
    clean_text = ansi_escape.sub('', text)
    
    try:
        import subprocess
        process = subprocess.Popen(
            'clip',
            stdin=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        process.communicate(input=clean_text.encode('utf-8'))
        return True
    except Exception:
        try:
            p = subprocess.Popen(
                ['powershell', '-Command', 'Set-Clipboard -Value $args[0]'],
                stdin=subprocess.PIPE
            )
            p.communicate(input=clean_text.encode('utf-8'))
            return True
        except Exception:
            return False

def main():
    if not sys.stdout.isatty():
        Colors.disable()

    UI.clear()
    UI.header()

    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent.parent
    json_path = os.path.join(script_dir, 'cmd.json')

    commands = load_commands(json_path)
    if not commands:
        sys.exit(1)

    print(f"{Colors.GRAY}项目根目录:{Colors.END} {Colors.CYAN}{project_root}{Colors.END}\n")

    while True:
        UI.menu_table(commands)

        try:
            choice = input(f"\n{Colors.ORANGE}▶{Colors.END} 选择命令: ").strip()

            if choice == '0':
                print(f"\n{Colors.SUCCESS}✔ ヾ(ToT)Bye~Bye~！{Colors.END}\n")
                break

            selected = None
            for cmd in commands:
                if str(cmd['id']) == choice:
                    selected = cmd
                    break

            if selected:
                desc = selected.get('description', 'Unknown')
                cmd_str = selected['command']

                _, output_lines = run_command(cmd_str, str(project_root), desc)

                if output_lines:
                    print(f"\n{Colors.INFO}[{Colors.END}{Colors.CYAN}c{Colors.END}{Colors.INFO}]{Colors.END} {Colors.GRAY}复制输出结果{Colors.END}")
                    print(f"{Colors.INFO}[{Colors.END}{Colors.CYAN}p{Colors.END}{Colors.INFO}]{Colors.END} {Colors.GRAY}复制完整命令{Colors.END}")
                    print(f"{Colors.INFO}[{Colors.END}{Colors.CYAN}Enter{Colors.END}{Colors.INFO}]{Colors.END} {Colors.GRAY}直接继续{Colors.END}")
                    
                    action = input(f"\n{Colors.ORANGE}▶{Colors.END} 选择操作: ").strip().lower()
                    
                    if action == 'c' and output_lines:
                        output_text = '\n'.join(output_lines)
                        if copy_to_clipboard(output_text):
                            print(f"{Colors.SUCCESS}✓ 输出已复制到剪贴板 ({len(output_lines)} 行){Colors.END}")
                        else:
                            print(f"{Colors.ERROR}✗ 复制失败{Colors.END}")
                    elif action == 'p':
                        if copy_to_clipboard(cmd_str):
                            print(f"{Colors.SUCCESS}✓ 命令已复制到剪贴板{Colors.END}")
                        else:
                            print(f"{Colors.ERROR}✗ 复制失败{Colors.END}")
                    
                    time.sleep(0.5)
                    
                cleanup_old_processes()
                UI.clear()
                UI.header()
            else:
                print(f"{Colors.WARNING}⚡ 无效选择 '{choice}'{Colors.END}")
                time.sleep(0.5)

        except KeyboardInterrupt:
            print(f"\n{Colors.WARNING}\n⚡ 已取消{Colors.END}")
            break
        except EOFError:
            break

if __name__ == '__main__':
    main()