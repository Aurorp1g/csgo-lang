/**
 * @file bytecode_generator.h
 * @brief CSGO 编程语言字节码生成器头文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件定义了 CSGO 语言的字节码生成器。
 * 基于 CPython 3.8 Python/compile.c 设计，
 * 将 SSA IR 转换为可执行的字节码（Chunk）。
 *
 * @section features 功能特性
 * - SSA IR 到字节码的转换
 * - 常量池管理
 * - 名称表管理
 * - 基本块到字节码的映射
 * - 栈深度计算
 * - 跳转目标解析
 *
 * @section reference 参考
 * - CPython Python/compile.c: 字节码编译核心
 * - CPython Include/code.h: 代码对象定义
 * - CPython Include/opcode.h: 操作码定义
 *
 * @see SSAIR SSA中间表示
 * @see Optimizer 优化器
 */

#ifndef CSGO_BYTECODE_GENERATOR_H
#define CSGO_BYTECODE_GENERATOR_H

#include "../ir/ssa_ir.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace csgo {
namespace bytecode {

/**
 * @enum BytecodeOpcode
 * @brief 字节码操作码枚举
 *
 * 定义所有字节码操作码，基于 CPython opcode.h 设计。
 */
enum class BytecodeOpcode {
    NOP = 0,

    // 加载指令
    LOAD_CONST = 1,
    LOAD_FAST = 2,
    LOAD_GLOBAL = 3,
    LOAD_NAME = 4,
    LOAD_ATTR = 5,
    LOAD_SUPER_ATTR = 6,
    LOAD_DEREF = 7,
    LOAD_CLOSURE = 8,
    LOAD_SUBSCR = 9,

    // 存储指令
    STORE_FAST = 10,
    STORE_GLOBAL = 11,
    STORE_NAME = 12,
    STORE_ATTR = 13,
    STORE_DEREF = 14,
    STORE_SUBSCR = 15,

    // 删除指令
    DELETE_FAST = 16,
    DELETE_GLOBAL = 17,
    DELETE_NAME = 18,
    DELETE_ATTR = 19,
    DELETE_DEREF = 20,
    DELETE_SUBSCR = 21,

    // 二元运算
    BINARY_ADD = 22,
    BINARY_SUBTRACT = 23,
    BINARY_MULTIPLY = 24,
    BINARY_TRUE_DIVIDE = 25,
    BINARY_FLOOR_DIVIDE = 26,
    BINARY_MODULO = 27,
    BINARY_POWER = 28,
    BINARY_LSHIFT = 29,
    BINARY_RSHIFT = 30,
    BINARY_AND = 31,
    BINARY_OR = 32,
    BINARY_XOR = 33,
    BINARY_MATRIX_MULTIPLY = 34,

    // 一元运算
    UNARY_POSITIVE = 35,
    UNARY_NEGATIVE = 36,
    UNARY_NOT = 37,
    UNARY_INVERT = 38,

    // 比较运算
    COMPARE_OP = 39,

    // 分支跳转
    POP_JUMP_IF_TRUE = 40,
    POP_JUMP_IF_FALSE = 41,
    JUMP_IF_TRUE_OR_POP = 42,
    JUMP_IF_FALSE_OR_POP = 43,
    JUMP_ABSOLUTE = 44,
    JUMP_FORWARD = 45,

    // 循环控制
    BREAK_LOOP = 46,
    CONTINUE_LOOP = 47,

    // 返回
    RETURN_VALUE = 48,
    YIELD_VALUE = 49,
    YIELD_FROM = 50,

    // 栈操作
    POP_TOP = 51,
    ROT_TWO = 52,
    ROT_THREE = 53,
    ROT_FOUR = 54,
    DUP_TOP = 55,
    DUP_TOP_TWO = 56,

    // 函数调用
    CALL_FUNCTION = 57,
    CALL_FUNCTION_KW = 58,
    CALL_FUNCTION_EX = 59,
    CALL_METHOD = 60,
    MAKE_FUNCTION = 61,
    LOAD_METHOD = 62,

    // 构建复合对象
    BUILD_TUPLE = 63,
    BUILD_LIST = 64,
    BUILD_SET = 65,
    BUILD_MAP = 66,
    BUILD_STRING = 67,
    BUILD_TUPLE_UNPACK = 68,
    BUILD_LIST_UNPACK = 69,
    BUILD_SET_UNPACK = 70,
    BUILD_MAP_UNPACK = 71,
    BUILD_MAP_UNPACK_WITH_CALL = 72,
    BUILD_CONST_KEY_MAP = 73,
    BUILD_SLICE = 74,

    // 序列操作
    UNPACK_SEQUENCE = 75,
    UNPACK_EX = 76,

    // 迭代器操作
    GET_ITER = 77,
    FOR_ITER = 78,
    GET_YIELD_FROM_ITER = 79,

    // 异步操作
    GET_AWAITABLE = 80,
    GET_AITER = 81,
    GET_ANEXT = 82,
    BEFORE_ASYNC_WITH = 83,
    SETUP_ASYNC_WITH = 84,
    END_ASYNC_FOR = 85,
    SETUP_CLEANUP = 86,
    END_CLEANUP = 87,

    // 异常处理
    RAISE_VARARGS = 88,
    SETUP_EXCEPT = 89,
    SETUP_FINALLY = 90,
    SETUP_WITH = 91,
    END_FINALLY = 92,
    POP_FINALLY = 93,
    BEGIN_FINALLY = 94,
    CALL_FINALLY = 95,

    // With语句
    WITH_CLEANUP_START = 96,
    WITH_CLEANUP_FINISH = 97,

    // 格式化
    FORMAT_VALUE = 98,

    // 导入
    IMPORT_NAME = 99,
    IMPORT_FROM = 100,
    IMPORT_STAR = 101,

    // 闭包
    LOAD_CLASSDEREF = 102,

    // 扩展参数
    EXTENDED_ARG = 103,

    // 特殊操作
    SET_ADD = 104,
    LIST_ADD = 105,

    // 新版本保留
    RESERVED = 106,

    // 向量化操作
    BINARY_OP = 107,

    // 生成器
    SEND = 108,
    THROW = 109,
    CLOSE = 110,

    // 异步生成器
    ASYNC_GENERATOR_WRAP = 111,

    // 指令计数（用于验证）
    INSTRUCTION_COUNT = 112
};

/**
 * @enum ExceptionHandling
 * @brief 异常处理类型
 */
enum class ExceptionHandling {
    None = 0,
    Except = 1,
    Finally = 2,
    Cleanup = 3
};

/**
 * @struct ExceptionHandler
 * @brief 异常处理器结构
 */
struct ExceptionHandler {
    uint16_t start;           // 处理器起始偏移
    uint16_t end;             // 处理器结束偏移
    uint16_t target;          // 跳转目标偏移
    ExceptionHandling type;   // 处理器类型
    uint16_t stack_depth;     // 进入时的栈深度
};

/**
 * @struct LineNumberEntry
 * @brief 行号映射条目
 */
struct LineNumberEntry {
    uint16_t offset;          // 字节码偏移
    uint16_t line;            // 源代码行号
};

/**
 * @struct StackEffectEntry
 * @brief 栈效果条目
 */
struct StackEffectEntry {
    int8_t effect;            // 栈效果（正数为压入，负数为弹出）
    bool reliable;            // 是否可靠
};

/**
 * @class Instruction
 * @brief 字节码指令结构
 */
class Instruction {
public:
    uint8_t opcode;           // 操作码
    uint32_t oparg;          // 操作数（32位，目前支持16位，未支持扩展参数）

    Instruction() : opcode(0), oparg(0) {}
    Instruction(uint8_t op, uint32_t arg = 0) : opcode(op), oparg(arg) {}

    // 检查是否有参数
    bool has_arg() const {
        return has_arg(opcode);  // 调用静态函数
    }

    // 静态版本：供全局使用
    static bool has_arg(uint8_t op) {
        switch (op) {
            // 明确无参的操作码（0-21, 34-38, 46-56, 77, 79-80, 85, 92-94, 101）
            case 0:   // NOP
            case 22:  // BINARY_ADD
            case 23:  // BINARY_SUBTRACT
            case 24:  // BINARY_MULTIPLY
            case 25:  // BINARY_TRUE_DIVIDE
            case 26:  // BINARY_FLOOR_DIVIDE
            case 27:  // BINARY_MODULO
            case 28:  // BINARY_POWER
            case 29:  // BINARY_LSHIFT
            case 30:  // BINARY_RSHIFT
            case 31:  // BINARY_AND
            case 32:  // BINARY_OR
            case 33:  // BINARY_XOR
            case 34:  // BINARY_MATRIX_MULTIPLY
            case 35:  // UNARY_POSITIVE
            case 36:  // UNARY_NEGATIVE
            case 37:  // UNARY_NOT
            case 38:  // UNARY_INVERT
            case 46:  // BREAK_LOOP
            case 47:  // CONTINUE_LOOP
            case 48:  // RETURN_VALUE
            case 49:  // YIELD_VALUE
            case 50:  // YIELD_FROM
            case 51:  // POP_TOP
            case 52:  // ROT_TWO
            case 53:  // ROT_THREE
            case 54:  // ROT_FOUR
            case 55:  // DUP_TOP
            case 56:  // DUP_TOP_TWO
            case 77:  // GET_ITER
            case 79:  // GET_YIELD_FROM_ITER
            case 80:  // GET_AWAITABLE
            case 85:  // END_ASYNC_FOR
            case 92:  // END_FINALLY
            case 93:  // POP_FINALLY
            case 94:  // BEGIN_FINALLY
            case 101: // IMPORT_STAR
                return false;
            default:
                return true;
        }
    }

    // 获取指令字节大小
    size_t size() const {
        size_t sz = 1;  // opcode
        if (has_arg()) {
            if (oparg > 0xFFFFFF) sz += 4;
            else if (oparg > 0xFFFF) sz += 4;
            else if (oparg > 0xFF) sz += 3;
            else sz += 1;
        }
        return sz;
    }

    // 序列化到字节向量
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> bytes;
        bytes.push_back(opcode);

        if (has_arg()) {
            if (oparg <= 0xFF) {
                bytes.push_back(static_cast<uint8_t>(oparg));
            } else if (oparg <= 0xFFFF) {
                bytes.push_back(static_cast<uint8_t>(oparg & 0xFF));
                bytes.push_back(static_cast<uint8_t>((oparg >> 8) & 0xFF));
            } else {
                bytes.push_back(static_cast<uint8_t>(oparg & 0xFF));
                bytes.push_back(static_cast<uint8_t>((oparg >> 8) & 0xFF));
                bytes.push_back(static_cast<uint8_t>((oparg >> 16) & 0xFF));
                bytes.push_back(static_cast<uint8_t>((oparg >> 24) & 0xFF));
            }
        }

        return bytes;
    }
};

/**
 * @class Chunk
 * @brief 字节码块（可执行单元）
 *
 * 表示一个可执行的字节码单元，包含指令、常量、名称等。
 * 基于 CPython 的 PyCodeObject 设计。
 */
class Chunk {
public:
    // 元数据
    std::string filename;              // 文件名
    std::string name;                  // 函数/模块名
    std::string qualname;             // 限定名

    // 常量池
    std::vector<csgo::ir::Value> consts;         // 常量列表

    // 名称表
    std::vector<std::string> names;    // 名称列表

    // 变量名
    std::vector<std::string> varnames; // 局部变量名
    std::vector<std::string> freevars; // 自由变量
    std::vector<std::string> cellvars; // 单元变量

    // 指令
    std::vector<Instruction> code;      // 字节码指令

    // 异常处理
    std::vector<ExceptionHandler> exception_handlers;  // 异常处理器

    // 行号信息
    std::vector<LineNumberEntry> lnotab;  // 行号表

    // 属性
    uint16_t argcount = 0;              // 参数个数
    uint16_t posonlyargcount = 0;       // 位置参数个数
    uint16_t kwonlyargcount = 0;        // 关键字参数个数
    uint16_t nlocals = 0;               // 局部变量数
    uint16_t stacksize = 0;             // 最大栈深度
    uint16_t firstlineno = 1;           // 首行号

    // 标志位
    uint32_t flags = 0;                // 代码标志

    // 指令计数
    uint32_t instruction_count = 0;

    Chunk() = default;

    // 序列化到字节向量
    std::vector<uint8_t> serialize() const;
    void serialize_constant(std::vector<uint8_t>& data, const ir::Value& constant) const;

    // 字节大小
    size_t size() const {
        size_t sz = 0;
        for (const auto& instr : code) {
            sz += instr.size();
        }
        return sz;
    }

    // 添加指令
    void add_instruction(const Instruction& instr) {
        code.push_back(instr);
        instruction_count++;
    }

    void add_instruction(uint8_t opcode, uint32_t oparg = 0) {
        code.emplace_back(opcode, oparg);
        instruction_count++;
    }

    // 添加常量
    size_t add_constant(const csgo::ir::Value& value) {
        for (size_t i = 0; i < consts.size(); ++i) {
            if (consts[i].to_string() == value.to_string()) {
                return i;
            }
        }
        consts.push_back(value);
        return consts.size() - 1;
    }

    // 添加名称
    size_t add_name(const std::string& name) {
        for (size_t i = 0; i < names.size(); ++i) {
            if (names[i] == name) {
                return i;
            }
        }
        names.push_back(name);
        return names.size() - 1;
    }

    // 添加变量名
    size_t add_varname(const std::string& name) {
        for (size_t i = 0; i < varnames.size(); ++i) {
            if (varnames[i] == name) {
                return i;
            }
        }
        varnames.push_back(name);
        nlocals = varnames.size();
        return varnames.size() - 1;
    }
};

/**
 * @class BytecodeGenerator
 * @brief 字节码生成器
 *
 * 将 SSA IR 转换为字节码 Chunk。
 * 基于 CPython 的 compile.c 和 assemble.c 设计。
 */
class BytecodeGenerator {
public:
    explicit BytecodeGenerator();
    ~BytecodeGenerator() = default;

    // 从 SSA IR 生成字节码
    std::unique_ptr<Chunk> generate(ir::IRModule* module);

    // 从基本块生成字节码
    void generate_from_basic_block(ir::BasicBlock* block, Chunk* chunk);

    // 从指令生成字节码
    void generate_from_instruction(const ir::Instruction& instr, Chunk* chunk);

    // 配置
    void set_optimize_level(int level);
    void set_debug_mode(bool debug);

    // 调试：获取 IR 到字节的映射
    struct InstrMapping {
        std::string ir_instr;           // IR 指令的字符串表示
        uint16_t byte_offset;           // 字节码偏移
        std::vector<uint8_t> bytes;      // 生成的字节
        std::string bytecode_instr;      // 字节码指令字符串
    };
    const std::vector<InstrMapping>& get_instr_mappings() const { return instr_mappings_; }

private:
    int optimize_level_ = 0;
    bool debug_mode_ = false;

    // 指令映射
    std::unordered_map<ir::IROpcode, BytecodeOpcode> opcode_map_;

    // 基本块处理
    std::vector<ir::BasicBlock*> block_order_;
    std::unordered_map<ir::BasicBlock*, uint16_t> block_offset_;

    // 跳转目标解析
    uint16_t resolve_jump_target(ir::BasicBlock* target);
    uint16_t resolve_relative_jump(uint16_t current_offset, uint16_t target_offset);

    // 调试：IR 到字节映射
    std::vector<InstrMapping> instr_mappings_;

    // 栈深度计算
    int compute_stack_depth(ir::IRModule* module);
    int compute_instruction_stack_effect(const ir::Instruction& instr);

    size_t calc_instruction_size(const ir::Instruction& instr);
    // 指令生成 - 加载/存储
    void generate_load_const(const ir::Instruction& instr, Chunk* chunk);
    void generate_load_fast(const ir::Instruction& instr, Chunk* chunk);
    void generate_load_global(const ir::Instruction& instr, Chunk* chunk);
    void generate_load_name(const ir::Instruction& instr, Chunk* chunk);
    void generate_load_attr(const ir::Instruction& instr, Chunk* chunk);
    void generate_load_subscript(const ir::Instruction& instr, Chunk* chunk);
    void generate_load_closure(const ir::Instruction& instr, Chunk* chunk);
    void generate_load_deref(const ir::Instruction& instr, Chunk* chunk);
    void generate_store_fast(const ir::Instruction& instr, Chunk* chunk);
    void generate_store_global(const ir::Instruction& instr, Chunk* chunk);
    void generate_store_name(const ir::Instruction& instr, Chunk* chunk);
    void generate_store_attr(const ir::Instruction& instr, Chunk* chunk);
    void generate_store_subscript(const ir::Instruction& instr, Chunk* chunk);
    void generate_store_deref(const ir::Instruction& instr, Chunk* chunk);
    void generate_delete_fast(const ir::Instruction& instr, Chunk* chunk);
    void generate_delete_global(const ir::Instruction& instr, Chunk* chunk);
    void generate_delete_name(const ir::Instruction& instr, Chunk* chunk);
    void generate_delete_attr(const ir::Instruction& instr, Chunk* chunk);
    void generate_delete_subscript(const ir::Instruction& instr, Chunk* chunk);
    void generate_delete_deref(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 运算
    void generate_binary_op(const ir::Instruction& instr, Chunk* chunk);
    void generate_unary_op(const ir::Instruction& instr, Chunk* chunk);
    void generate_compare(const ir::Instruction& instr, Chunk* chunk);
    void generate_logical_op(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 控制流
    void generate_jump(const ir::Instruction& instr, Chunk* chunk);
    void generate_cond_jump(const ir::Instruction& instr, Chunk* chunk);
    void generate_return(const ir::Instruction& instr, Chunk* chunk);
    void generate_break_loop(const ir::Instruction& instr, Chunk* chunk);
    void generate_continue_loop(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 函数调用
    void generate_call(const ir::Instruction& instr, Chunk* chunk);
    void generate_make_function(const ir::Instruction& instr, Chunk* chunk);
    void generate_load_method(const ir::Instruction& instr, Chunk* chunk);
    void generate_call_method(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 构建复合对象
    void generate_build(const ir::Instruction& instr, Chunk* chunk);
    void generate_build_slice(const ir::Instruction& instr, Chunk* chunk);
    void generate_unpack(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 迭代器
    void generate_get_iter(const ir::Instruction& instr, Chunk* chunk);
    void generate_for_iter(const ir::Instruction& instr, Chunk* chunk);
    void generate_get_aiter(const ir::Instruction& instr, Chunk* chunk);
    void generate_get_anext(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - SSA特有
    void generate_phi(const ir::Instruction& instr, Chunk* chunk);
    void generate_copy(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 栈操作
    void generate_pop_top(const ir::Instruction& instr, Chunk* chunk);
    void generate_dup_top(const ir::Instruction& instr, Chunk* chunk);
    void generate_rot_two(const ir::Instruction& instr, Chunk* chunk);
    void generate_rot_three(const ir::Instruction& instr, Chunk* chunk);
    void generate_rot_four(const ir::Instruction& instr, Chunk* chunk);
    void generate_dup_top_two(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 导入
    void generate_import_name(const ir::Instruction& instr, Chunk* chunk);
    void generate_import_from(const ir::Instruction& instr, Chunk* chunk);
    void generate_import_star(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 异步操作
    void generate_get_awaitable(const ir::Instruction& instr, Chunk* chunk);
    void generate_setup_async_with(const ir::Instruction& instr, Chunk* chunk);
    void generate_before_async_with(const ir::Instruction& instr, Chunk* chunk);
    void generate_end_async_for(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 异常处理
    void generate_raise(const ir::Instruction& instr, Chunk* chunk);
    void generate_setup_except(const ir::Instruction& instr, Chunk* chunk);
    void generate_setup_finally(const ir::Instruction& instr, Chunk* chunk);
    void generate_setup_with(const ir::Instruction& instr, Chunk* chunk);
    void generate_end_finally(const ir::Instruction& instr, Chunk* chunk);
    void generate_pop_finally(const ir::Instruction& instr, Chunk* chunk);
    void generate_begin_finally(const ir::Instruction& instr, Chunk* chunk);
    void generate_call_finally(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 生成器
    void generate_yield_value(const ir::Instruction& instr, Chunk* chunk);
    void generate_yield_from(const ir::Instruction& instr, Chunk* chunk);
    void generate_send(const ir::Instruction& instr, Chunk* chunk);

    // 指令生成 - 格式化
    void generate_format_value(const ir::Instruction& instr, Chunk* chunk);

    // 辅助方法
    void init_opcode_map();
    BytecodeOpcode ir_opcode_to_bytecode(ir::IROpcode opcode) const;
    uint32_t get_variable_index(const ir::Value& value, Chunk* chunk);
    void update_lnotab(Chunk* chunk, uint16_t offset, uint16_t line);
    void compute_exception_handlers(ir::IRModule* module, Chunk* chunk);
};

} // namespace bytecode
} // namespace csgo

#endif // CSGO_BYTECODE_GENERATOR_H