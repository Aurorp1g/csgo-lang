# CSGO 字节码规范文档 (v1.0)

## 文件格式 (.cgb)

CSGO 编译器生成的字节码文件，采用小端序 (little-endian) 二进制格式存储。

### 文件头结构 (Header)

| 偏移 | 字段 | 大小 | 说明 |
|------|------|------|------|
| 0x00 | Magic | 4 bytes | 魔数: `C` `G` `O` `1` (0x43 0x47 0x4F 0x01) |
| 0x04 | name_len | 4 bytes | 函数名称长度 (uint32_t) |
| 0x08 | name | name_len | 函数名称字符串 (UTF-8) |
| 0x08+name_len | flags | 4 bytes | 代码标志位 (uint32_t) |
| ... | argcount | 2 bytes | 位置参数个数 (uint16_t) |
| ... | posonlyargcount | 2 bytes | 仅位置参数个数 (uint16_t) |
| ... | kwonlyargcount | 2 bytes | 仅关键字参数个数 (uint16_t) |
| ... | nlocals | 2 bytes | 局部变量个数 (uint16_t) |
| ... | stacksize | 2 bytes | 所需栈空间大小 (uint16_t) |
| ... | instruction_count | 4 bytes | 指令条数 (uint32_t) |
| ... | firstlineno | 2 bytes | 首行号 (uint16_t) |

**注意**: `qualname` (限定名) 当前在 Chunk 类中定义，但**未序列化**到字节码文件中。

### 代码区 (Code)

指令采用**变长编码**：

- **无参数指令**: 1 byte (`[opcode]`)
- **有参数指令**: 3 bytes (`[opcode] [arg_lo] [arg_hi]`)

参数使用**固定 2 字节** (uint16_t) 存储，小端序。

**有参数指令判定规则** (白名单制度)：
以下操作码**无参数** (返回 false)，其余均有参数：
- 0 (NOP), 22-38 (二元/一元运算), 46-56 (控制流/栈操作)
- 77 (GET_ITER), 79 (GET_YIELD_FROM_ITER), 80 (GET_AWAITABLE)
- 85 (END_ASYNC_FOR), 92-94 (异常处理), 101 (IMPORT_STAR)

### 常量区 (Constants)

| 字段 | 大小 | 说明 |
|------|------|------|
| count | 4 bytes | 常量个数 (uint32_t) |
| constants | 变长 | 常量列表 |

#### 常量类型编码 (与实现一致)

| type byte | 类型 | 数据格式 |
|-----------|------|----------|
| 0x00 | None | 无后续数据 |
| 0x01 | Bool | 1 byte (0x00=False, 0x01=True) |
| 0x02 | Integer | 8 bytes (int64_t, little-endian) |
| 0x03 | Float | 8 bytes (double, IEEE 754) |
| 0x04 | String | 4 bytes (len) + len bytes (UTF-8) |

**注意**: 与旧版规范不同，Bool 和 Integer 是分开的类型，且 Integer 固定使用 8 字节。

### 名称区 (Names)

| 字段 | 大小 | 说明 |
|------|------|------|
| count | 4 bytes | 名称个数 (uint32_t) |
| names | 变长 | 每个名称: 4 bytes (len) + len bytes |

### 局部变量区 (Varnames)

| 字段 | 大小 | 说明 |
|------|------|------|
| count | 4 bytes | 变量个数 (uint32_t) |
| varnames | 变长 | 每个变量名: 4 bytes (len) + len bytes |

### 自由变量区 (Freevars)

| 字段 | 大小 | 说明 |
|------|------|------|
| count | 4 bytes | 自由变量个数 (uint32_t) |
| freevars | 变长 | 每个名称: 4 bytes (len) + len bytes |

### 单元变量区 (Cellvars)

| 字段 | 大小 | 说明 |
|------|------|------|
| count | 4 bytes | 单元变量个数 (uint32_t) |
| cellvars | 变长 | 每个名称: 4 bytes (len) + len bytes |

### 异常处理表区 (Exception Handlers)

| 字段 | 大小 | 说明 |
|------|------|------|
| count | 4 bytes | 处理器个数 (uint32_t) |
| handlers | 变长 | 每个处理器 9 bytes |

**ExceptionHandler 结构** (9 bytes):
- start: 2 bytes (uint16_t) - 处理器起始偏移
- end: 2 bytes (uint16_t) - 处理器结束偏移  
- target: 2 bytes (uint16_t) - 跳转目标偏移
- type: 1 byte (enum) - 0=None, 1=Except, 2=Finally, 3=Cleanup
- stack_depth: 2 bytes (uint16_t) - 进入时的栈深度

### 行号表区 (Lnotab)

| 字段 | 大小 | 说明 |
|------|------|------|
| count | 4 bytes | 条目个数 (uint32_t) |
| entries | 变长 | 每个条目 4 bytes |

**LineNumberEntry 结构** (4 bytes):
- offset: 2 bytes (uint16_t) - 字节码偏移
- line: 2 bytes (uint16_t) - 源代码行号

---

## 字节码指令集

### 操作码枚举表 (与 bytecode_generator.h 一致)

| 十进制 | 十六进制 | 名称 | 参数 | 说明 |
|--------|----------|------|------|------|
| **加载指令** |||||
| 1 | 0x01 | LOAD_CONST | arg(2) | 加载常量到栈顶，arg 为常量表索引 |
| 2 | 0x02 | LOAD_FAST | arg(2) | 加载局部变量，arg 为 varnames 索引 |
| 3 | 0x03 | LOAD_GLOBAL | arg(2) | 加载全局变量，arg 为 names 索引 |
| 4 | 0x04 | LOAD_NAME | arg(2) | 加载名称 |
| 5 | 0x05 | LOAD_ATTR | arg(2) | 加载属性，arg 为 names 索引 |
| 6 | 0x06 | LOAD_SUPER_ATTR | arg(2) | 加载父类属性 |
| 7 | 0x07 | LOAD_DEREF | arg(2) | 加载闭包变量 |
| 8 | 0x08 | LOAD_CLOSURE | arg(2) | 加载闭包 |
| 9 | 0x09 | LOAD_SUBSCR | - | 加载下标 (无参) |
| **存储指令** |||||
| 10 | 0x0A | STORE_FAST | arg(2) | 存储到局部变量 |
| 11 | 0x0B | STORE_GLOBAL | arg(2) | 存储到全局变量 |
| 12 | 0x0C | STORE_NAME | arg(2) | 存储到名称 |
| 13 | 0x0D | STORE_ATTR | arg(2) | 存储属性 |
| 14 | 0x0E | STORE_DEREF | arg(2) | 存储闭包变量 |
| 15 | 0x0F | STORE_SUBSCR | - | 存储下标 (无参) |
| **删除指令** |||||
| 16 | 0x10 | DELETE_FAST | arg(2) | 删除局部变量 |
| 17 | 0x11 | DELETE_GLOBAL | arg(2) | 删除全局变量 |
| 18 | 0x12 | DELETE_NAME | arg(2) | 删除名称 |
| 19 | 0x13 | DELETE_ATTR | arg(2) | 删除属性 |
| 20 | 0x14 | DELETE_DEREF | arg(2) | 删除闭包变量 |
| 21 | 0x15 | DELETE_SUBSCR | - | 删除下标 (无参) |
| **二元运算 (无参)** |||||
| 22 | 0x16 | BINARY_ADD | - | 加法: (a + b) |
| 23 | 0x17 | BINARY_SUBTRACT | - | 减法: (a - b) |
| 24 | 0x18 | BINARY_MULTIPLY | - | 乘法: (a * b) |
| 25 | 0x19 | BINARY_TRUE_DIVIDE | - | 除法: (a / b) |
| 26 | 0x1A | BINARY_FLOOR_DIVIDE | - | 整除: (a // b) |
| 27 | 0x1B | BINARY_MODULO | - | 取模: (a % b) |
| 28 | 0x1C | BINARY_POWER | - | 幂运算: (a ** b) |
| 29 | 0x1D | BINARY_LSHIFT | - | 左移: (a &lt;&lt; b) |
| 30 | 0x1E | BINARY_RSHIFT | - | 右移: (a &gt;&gt; b) |
| 31 | 0x1F | BINARY_AND | - | 按位与: (a & b) |
| 32 | 0x20 | BINARY_OR | - | 按位或: (a \| b) |
| 33 | 0x21 | BINARY_XOR | - | 按位异或: (a ^ b) |
| 34 | 0x22 | BINARY_MATRIX_MULTIPLY | - | 矩阵乘法 |
| **一元运算 (无参)** |||||
| 35 | 0x23 | UNARY_POSITIVE | - | 正号: (+a) |
| 36 | 0x24 | UNARY_NEGATIVE | - | 负号: (-a) |
| 37 | 0x25 | UNARY_NOT | - | 逻辑非: (not a) |
| 38 | 0x26 | UNARY_INVERT | - | 按位取反: (~a) |
| **比较运算** |||||
| 39 | 0x27 | COMPARE_OP | arg(2) | 比较运算，arg 为比较类型 (0-11) |
| **分支跳转** |||||
| 40 | 0x28 | POP_JUMP_IF_TRUE | arg(2) | 条件为真跳转，弹出栈顶 |
| 41 | 0x29 | POP_JUMP_IF_FALSE | arg(2) | 条件为假跳转，弹出栈顶 |
| 42 | 0x2A | JUMP_IF_TRUE_OR_POP | arg(2) | 为真跳转，否则弹出 |
| 43 | 0x2B | JUMP_IF_FALSE_OR_POP | arg(2) | 为假跳转，否则弹出 |
| 44 | 0x2C | JUMP_ABSOLUTE | arg(2) | 绝对跳转 |
| 45 | 0x2D | JUMP_FORWARD | arg(2) | 相对跳转 |
| **循环控制 (无参)** |||||
| 46 | 0x2E | BREAK_LOOP | - | 跳出循环 |
| 47 | 0x2F | CONTINUE_LOOP | - | 继续循环 |
| **返回 (无参)** |||||
| 48 | 0x30 | RETURN_VALUE | - | 返回栈顶值 |
| 49 | 0x31 | YIELD_VALUE | - | 生成值 |
| 50 | 0x32 | YIELD_FROM | - | 从生成器 yield |
| **栈操作 (无参)** |||||
| 51 | 0x33 | POP_TOP | - | 弹出栈顶 |
| 52 | 0x34 | ROT_TWO | - | 交换栈顶两个值 |
| 53 | 0x35 | ROT_THREE | - | 旋转栈顶三个值 |
| 54 | 0x36 | ROT_FOUR | - | 旋转栈顶四个值 |
| 55 | 0x37 | DUP_TOP | - | 复制栈顶值 |
| 56 | 0x38 | DUP_TOP_TWO | - | 复制栈顶两个值 |
| **函数调用** |||||
| 57 | 0x39 | CALL_FUNCTION | arg(2) | 调用函数，arg=参数个数 |
| 58 | 0x3A | CALL_FUNCTION_KW | arg(2) | 调用函数(有关键字参数) |
| 59 | 0x3B | CALL_FUNCTION_EX | arg(2) | 调用函数(解包参数) |
| 60 | 0x3C | CALL_METHOD | arg(2) | 调用方法 |
| 61 | 0x3D | MAKE_FUNCTION | arg(2) | 创建函数对象 |
| 62 | 0x3E | LOAD_METHOD | arg(2) | 加载方法 |
| **构建复合对象** |||||
| 63 | 0x3F | BUILD_TUPLE | arg(2) | 构建元组，arg=元素个数 |
| 64 | 0x40 | BUILD_LIST | arg(2) | 构建列表，arg=元素个数 |
| 65 | 0x41 | BUILD_SET | arg(2) | 构建集合，arg=元素个数 |
| 66 | 0x42 | BUILD_MAP | arg(2) | 构建字典，arg=键值对个数 |
| 67 | 0x43 | BUILD_STRING | arg(2) | 构建字符串 |
| 68 | 0x44 | BUILD_TUPLE_UNPACK | arg(2) | 解包构建元组 |
| 69 | 0x45 | BUILD_LIST_UNPACK | arg(2) | 解包构建列表 |
| 70 | 0x46 | BUILD_SET_UNPACK | arg(2) | 解包构建集合 |
| 71 | 0x47 | BUILD_MAP_UNPACK | arg(2) | 解包构建字典 |
| 72 | 0x48 | BUILD_MAP_UNPACK_WITH_CALL | arg(2) | 解包构建字典并调用 |
| 73 | 0x49 | BUILD_CONST_KEY_MAP | arg(2) | 用常量键构建字典 |
| 74 | 0x4A | BUILD_SLICE | arg(2) | 构建切片 |
| **序列操作** |||||
| 75 | 0x4B | UNPACK_SEQUENCE | arg(2) | 解包序列，arg=元素个数 |
| 76 | 0x4C | UNPACK_EX | arg(2) | 扩展解包 |
| **迭代器操作** |||||
| 77 | 0x4D | GET_ITER | - | 获取迭代器 (无参) |
| 78 | 0x4E | FOR_ITER | arg(2) | 迭代器循环，arg=跳转偏移 |
| 79 | 0x4F | GET_YIELD_FROM_ITER | - | 获取生成器迭代器 (无参) |
| **异步操作** |||||
| 80 | 0x50 | GET_AWAITABLE | - | 获取可等待对象 (无参) |
| 81 | 0x51 | GET_AITER | arg(2) | 获取异步迭代器 |
| 82 | 0x52 | GET_ANEXT | arg(2) | 获取异步下一个 |
| 83 | 0x53 | BEFORE_ASYNC_WITH | - | 异步 with 之前 (无参) |
| 84 | 0x54 | SETUP_ASYNC_WITH | arg(2) | 异步 with 设置 |
| 85 | 0x55 | END_ASYNC_FOR | - | 结束异步 for (无参) |
| 86 | 0x56 | SETUP_CLEANUP | arg(2) | 清理设置 |
| 87 | 0x57 | END_CLEANUP | - | 结束清理 (无参) |
| **异常处理** |||||
| 88 | 0x58 | RAISE_VARARGS | arg(2) | 抛出异常，arg=参数个数 |
| 89 | 0x59 | SETUP_EXCEPT | arg(2) | 设置异常处理，arg=跳转目标 |
| 90 | 0x5A | SETUP_FINALLY | arg(2) | 设置 finally 块 |
| 91 | 0x5B | SETUP_WITH | arg(2) | 设置 with 块 |
| 92 | 0x5C | END_FINALLY | - | 结束 finally (无参) |
| 93 | 0x5D | POP_FINALLY | - | 弹出 finally (无参) |
| 94 | 0x5E | BEGIN_FINALLY | - | 开始 finally (无参) |
| 95 | 0x5F | CALL_FINALLY | arg(2) | 调用 finally |
| **With 语句 (无参)** |||||
| 96 | 0x60 | WITH_CLEANUP_START | - | With 清理开始 |
| 97 | 0x61 | WITH_CLEANUP_FINISH | - | With 清理结束 |
| **格式化** |||||
| 98 | 0x62 | FORMAT_VALUE | arg(2) | 格式化值 |
| **导入** |||||
| 99 | 0x63 | IMPORT_NAME | arg(2) | 导入模块 |
| 100 | 0x64 | IMPORT_FROM | arg(2) | 从模块导入 |
| 101 | 0x65 | IMPORT_STAR | - | 导入所有 (无参) |
| **闭包** |||||
| 102 | 0x66 | LOAD_CLASSDEREF | arg(2) | 加载类作用域自由变量 |
| **扩展参数** |||||
| 103 | 0x67 | EXTENDED_ARG | arg(2) | 扩展参数 (未实现) |
| **特殊操作** |||||
| 104 | 0x68 | SET_ADD | arg(2) | 集合添加 |
| 105 | 0x69 | LIST_ADD | arg(2) | 列表添加 |
| **保留** |||||
| 106 | 0x6A | RESERVED | - | 保留 |
| **向量化操作** |||||
| 107 | 0x6B | BINARY_OP | arg(2) | 二元运算(扩展) |
| **生成器** |||||
| 108 | 0x6C | SEND | arg(2) | 发送值到生成器 |
| 109 | 0x6D | THROW | arg(2) | 抛出异常到生成器 |
| 110 | 0x6E | CLOSE | - | 关闭生成器 (无参) |
| **异步生成器** |||||
| 111 | 0x6F | ASYNC_GENERATOR_WRAP | arg(2) | 异步生成器包装 |
| **无操作** |||||
| 0 | 0x00 | NOP | - | 无操作 (无参) |
| **指令计数 (内部使用)** |||||
| 112 | 0x70 | INSTRUCTION_COUNT | - | 指令总数标记 |

---

## 比较操作类型 (COMPARE_OP)

COMPARE_OP 指令的参数 (arg) 映射如下：

| 值 | 操作 | 说明 |
|----|------|------|
| 0 | LT | 小于 (&lt;) |
| 1 | LE | 小于等于 (&lt;=) |
| 2 | EQ | 等于 (==) |
| 3 | NE | 不等于 (!=) |
| 4 | GT | 大于 (&gt;) |
| 5 | GE | 大于等于 (&gt;=) |
| 6 | (保留) | 未使用 |
| 7 | (保留) | 未使用 |
| 8 | IS | 是 (is) |
| 9 | IS_NOT | 不是 (is not) |
| 10 | IN | 在...中 (in) |
| 11 | NOT_IN | 不在...中 (not in) |

**注意**: 与旧版规范不同，IS/IS_NOT 为 8/9，IN/NOT_IN 为 10/11，而非连续的 6/7/8/9。

---

## 文件解析流程

1. **读取魔数** (4 bytes): 验证为 `C` `G` `O` `1`
2. **读取函数名** (4 + n bytes): 名称长度 (uint32_t) + 名称字符串
3. **读取元数据** (20 bytes): flags (4), argcount (2), posonlyargcount (2), kwonlyargcount (2), nlocals (2), stacksize (2), instruction_count (4), firstlineno (2)
4. **读取指令** (变长): 根据 instruction_count 循环读取，每条指令根据 has_arg() 判定读 1 字节或 3 字节
5. **读取常量表** (4 + n bytes): 个数 (uint32_t) + 常量数据 (根据类型字节解析)
6. **读取名称表** (4 + n bytes): 个数 (uint32_t) + 名称数据 (每个: 4 bytes len + len bytes)
7. **读取局部变量表** (4 + n bytes): 格式同名称表
8. **读取自由变量表** (4 + n bytes): 格式同名称表
9. **读取单元变量表** (4 + n bytes): 格式同名称表
10. **读取异常处理表** (4 + n*9 bytes): 个数 (uint32_t) + 处理器数组 (每个 9 bytes)
11. **读取行号表** (4 + n*4 bytes): 个数 (uint32_t) + 行号条目数组 (每个 4 bytes)

---

## 实现注意事项

1. **参数大小限制**: 当前实现限制参数最大为 65535 (0xFFFF)，超过会触发 `std::abort()`
2. **EXTENDED_ARG**: 操作码 103 已定义但未实现，当前编译器在参数超过 16 位时会直接终止
3. **SSA 消除**: PHI 和 COPY 指令在字节码生成阶段被消除，映射为 NOP (0x00)
4. **跳转目标**: 使用绝对偏移量（字节位置），在生成时通过两遍扫描（先计算偏移，再回填）确保正确
5. **大小端序**: 所有多字节数据均采用小端序 (little-endian)