# CSGO 编程语言

![GitHub](https://img.shields.io/github/license/csgo-language/csgo-language)
![Go](https://img.shields.io/badge/Go-1.19+-00ADD8?style=flat&logo=go)
![C++](https://img.shields.io/badge/C++-17+-00599C?style=flat&logo=c%2B%2B)

一款混合型解释编程语言，采用 C++ 编译器前端 + Go 运行时后端架构。

## 语言特性

- **文件扩展名**：`.cg`
- **混合架构**：C++ 负责编译，Go 负责执行
- **字节码虚拟机**：基于栈的解释器
- **集成 GC**：标记-清除垃圾回收
- **Goroutine 支持**：通过 Go 运行时实现轻量级并发
- **完整工具链**：编译器 (cgc)、反汇编器 (cgd)、虚拟机 (cgr)

## 架构设计

```
源代码 (.cg) → [C++ 编译器 cgc] → 字节码 (.cgb) → [Go 虚拟机 cgr] → 执行
```

- **C++ 编译器** (`cgc`): 词法分析 → 语法分析 → AST → 语义分析 → SSA IR → 优化 → 字节码生成
- **Go 运行时** (`cgr`): 虚拟机执行 → GC → 标准库 → Goroutine 调度器

## 当前状态

**开发中** - 语言核心功能已基本实现。

### 已实现

**C++ 编译器前端：**
- [x] 词法分析器 (Tokenizer/DFA)
- [x] 抽象语法树 (AST) + Visitor 模式
- [x] 递归下降语法分析器
- [x] 符号表与类型检查
- [x] SSA 中间表示
- [x] IR 优化 (常量折叠、死代码消除)
- [x] 字节码生成器

**Go 运行时后端：**
- [x] 字节码虚拟机 (VM)
- [x] 操作码定义与执行
- [x] 值对象系统
- [x] 标记-清除 GC
- [x] Goroutine 调度器

### 计划中
- [ ] 完整标准库 (print, len, 集合类型)
- [ ] Channel 并发支持
- [ ] 更多内置函数
- [ ] REPL 交互式解释器

## 项目结构

```
csgo-language/
├── cpp/                      # C++ 编译器
│   ├── include/              # 头文件
│   │   ├── ast/              # AST 节点
│   │   ├── bytecode/         # 字节码生成
│   │   ├── ir/               # SSA IR 与优化
│   │   ├── lexer/            # 词法分析
│   │   ├── parser/           # 语法分析
│   │   └── semantic/         # 语义分析
│   ├── src/                  # 源文件
│   ├── build/                # 构建产物
│   ├── examples/             # 示例与测试
│   └── tests/                # 单元测试
├── go/                       # Go 运行时
│   ├── cmd/                  # 命令行工具
│   ├── runtime/
│   │   ├── gc/               # 垃圾回收
│   │   └── vm/               # 虚拟机
│   └── scheduler/            # 协程调度
├── dist/                     # 发布版本
├── docs/                     # 文档
│   └── bytecode_spec.md      # 字节码规范
├── examples/                 # 示例代码
├── script/                   # 构建脚本
└── csgo.go                   # Go 启动器
```

## 工具链

| 工具 | 路径 | 说明 |
|------|------|------|
| `cgc` | `dist/cg_compiler/cgc.exe` | CSGO 编译器 |
| `cgd` | `dist/cg_compiler/cgd.exe` | 字节码反汇编器 |
| `cgr` | `dist/cg_vm/cgr.exe` | CSGO 虚拟机 |

## 开发路线图

详细开发任务和进度请查看 [任务清单.md](任务清单.md)。

## 许可证

MIT License