/**
 * @file bytecode_generator.cpp
 * @brief CSGO 编程语言字节码生成器实现文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件实现了 CSGO 语言的字节码生成器。
 * 基于 CPython 3.8 Python/compile.c 和 Python/assemble.c 设计，
 * 将 SSA IR 转换为可执行的字节码（Chunk）。
 *
 * @section implementation 实现细节
 * - Chunk 类：字节码块的结构化管理
 * - BytecodeGenerator 类：SSA IR 到字节码的转换引擎
 * - 指令映射：SSA opcode 到 bytecode opcode 的转换
 * - 栈深度计算：确保运行时栈不会溢出
 * - 跳转目标解析：相对/绝对跳转的处理
 * - 异常处理：try/except/finally 的字节码支持
 *
 * @section reference 参考
 * - CPython Python/compile.c: 字节码编译核心
 * - CPython Python/assemble.c: 字节码组装器
 * - CPython Include/code.h: 代码对象定义
 * - CPython Include/opcode.h: 操作码定义
 *
 * @see bytecode_generator.h 字节码生成器头文件
 * @see ssa_ir.h SSA中间表示头文件
 */

#include "bytecode/bytecode_generator.h"
#include <algorithm>
#include <cassert>
#include <map>
#include <sstream>
#include <iostream>
#include "ir/ssa_ir.h"

namespace csgo {
namespace bytecode {

// ============================================================================
// 工具函数
// ============================================================================

/**
 * @brief 检查操作码是否需要参数
 */
static bool has_arg(uint8_t opcode) {
    switch (opcode) {
        // 明确无参的操作码（0-21, 34-38, 46-56, 77, 88, 92-97等）
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
        case 88:  // RAISE_VARARGS (注意：实际可能有参，需要确认)
        case 92:  // END_FINALLY
        case 93:  // POP_FINALLY
        case 94:  // BEGIN_FINALLY
        case 101: // IMPORT_STAR
            return false;
        default:
            // 其他所有操作码（1-21, 39-45, 57-76, 78, 81-84, 86-87, 89-91等）都有参
            return true;
    }
}

/**
 * @brief 计算参数需要的字节数
 */
static size_t calc_arg_size(uint32_t oparg) {
    // if (oparg > 0xFFFF) return 3;  // 需要 EXTENDED_ARG
    // if (oparg > 0xFF) return 2;
    if (oparg > 0xFFFF) {
        // 编译期断言：确保不会静默截断
        std::cerr << "FATAL: oparg " << oparg << " exceeds 16-bit limit (65535)" << std::endl;
        std::cerr << "Consider reducing constant pool size or implementing EXTENDED_ARG" << std::endl;
        std::abort();  // 或者抛出异常
    }
    return 2;
}

// ============================================================================
// Chunk 类实现
// ============================================================================

std::vector<uint8_t> Chunk::serialize() const {
    std::vector<uint8_t> data;

    // 魔数
    data.push_back('C');
    data.push_back('G');
    data.push_back('O');
    data.push_back(1);

    // 名称
    uint32_t name_len = name.size();
    data.push_back(static_cast<uint8_t>(name_len & 0xFF));
    data.push_back(static_cast<uint8_t>((name_len >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((name_len >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((name_len >> 24) & 0xFF));
    data.insert(data.end(), name.begin(), name.end());

    // 标志位
    data.push_back(static_cast<uint8_t>(flags & 0xFF));
    data.push_back(static_cast<uint8_t>((flags >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((flags >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((flags >> 24) & 0xFF));

    // 参数计数
    data.push_back(static_cast<uint8_t>(argcount & 0xFF));
    data.push_back(static_cast<uint8_t>((argcount >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(posonlyargcount & 0xFF));
    data.push_back(static_cast<uint8_t>((posonlyargcount >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(kwonlyargcount & 0xFF));
    data.push_back(static_cast<uint8_t>((kwonlyargcount >> 8) & 0xFF));

    // 局部变量数
    data.push_back(static_cast<uint8_t>(nlocals & 0xFF));
    data.push_back(static_cast<uint8_t>((nlocals >> 8) & 0xFF));

    // 栈大小
    data.push_back(static_cast<uint8_t>(stacksize & 0xFF));
    data.push_back(static_cast<uint8_t>((stacksize >> 8) & 0xFF));

    // 指令计数
    data.push_back(static_cast<uint8_t>(instruction_count & 0xFF));
    data.push_back(static_cast<uint8_t>((instruction_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((instruction_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((instruction_count >> 24) & 0xFF));

    // 首行号
    data.push_back(static_cast<uint8_t>(firstlineno & 0xFF));
    data.push_back(static_cast<uint8_t>((firstlineno >> 8) & 0xFF));

    // 序列化指令
    for (const auto& instr : code) {
        data.push_back(instr.opcode);
        if (has_arg(instr.opcode)) {
            size_t arg_size = calc_arg_size(instr.oparg);
            for (size_t i = 0; i < arg_size; i++) {
                data.push_back(static_cast<uint8_t>((instr.oparg >> (i * 8)) & 0xFF));
            }
        }
    }

    // 序列化常量
    uint32_t consts_count = consts.size();
    data.push_back(static_cast<uint8_t>(consts_count & 0xFF));
    data.push_back(static_cast<uint8_t>((consts_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((consts_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((consts_count >> 24) & 0xFF));
    for (const auto& constant : consts) {
        serialize_constant(data, constant);
    }

    // 序列化名称
    uint32_t names_count = names.size();
    data.push_back(static_cast<uint8_t>(names_count & 0xFF));
    data.push_back(static_cast<uint8_t>((names_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((names_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((names_count >> 24) & 0xFF));
    for (const auto& name_str : names) {
        uint32_t name_len = name_str.size();
        data.push_back(static_cast<uint8_t>(name_len & 0xFF));
        data.push_back(static_cast<uint8_t>((name_len >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((name_len >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((name_len >> 24) & 0xFF));
        data.insert(data.end(), name_str.begin(), name_str.end());
    }

    // 序列化变量名
    uint32_t varnames_count = varnames.size();
    data.push_back(static_cast<uint8_t>(varnames_count & 0xFF));
    data.push_back(static_cast<uint8_t>((varnames_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((varnames_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((varnames_count >> 24) & 0xFF));
    for (const auto& varname : varnames) {
        uint32_t varname_len = varname.size();
        data.push_back(static_cast<uint8_t>(varname_len & 0xFF));
        data.push_back(static_cast<uint8_t>((varname_len >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((varname_len >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((varname_len >> 24) & 0xFF));
        data.insert(data.end(), varname.begin(), varname.end());
    }

    // 序列化自由变量
    uint32_t freevars_count = freevars.size();
    data.push_back(static_cast<uint8_t>(freevars_count & 0xFF));
    data.push_back(static_cast<uint8_t>((freevars_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((freevars_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((freevars_count >> 24) & 0xFF));
    for (const auto& freevar : freevars) {
        uint32_t freevar_len = freevar.size();
        data.push_back(static_cast<uint8_t>(freevar_len & 0xFF));
        data.push_back(static_cast<uint8_t>((freevar_len >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((freevar_len >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((freevar_len >> 24) & 0xFF));
        data.insert(data.end(), freevar.begin(), freevar.end());
    }

    // 序列化单元变量
    uint32_t cellvars_count = cellvars.size();
    data.push_back(static_cast<uint8_t>(cellvars_count & 0xFF));
    data.push_back(static_cast<uint8_t>((cellvars_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((cellvars_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((cellvars_count >> 24) & 0xFF));
    for (const auto& cellvar : cellvars) {
        uint32_t cellvar_len = cellvar.size();
        data.push_back(static_cast<uint8_t>(cellvar_len & 0xFF));
        data.push_back(static_cast<uint8_t>((cellvar_len >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((cellvar_len >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((cellvar_len >> 24) & 0xFF));
        data.insert(data.end(), cellvar.begin(), cellvar.end());
    }

    // 序列化异常处理器
    uint32_t handlers_count = exception_handlers.size();
    data.push_back(static_cast<uint8_t>(handlers_count & 0xFF));
    data.push_back(static_cast<uint8_t>((handlers_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((handlers_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((handlers_count >> 24) & 0xFF));
    for (const auto& handler : exception_handlers) {
        data.push_back(static_cast<uint8_t>(handler.start & 0xFF));
        data.push_back(static_cast<uint8_t>((handler.start >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(handler.end & 0xFF));
        data.push_back(static_cast<uint8_t>((handler.end >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(handler.target & 0xFF));
        data.push_back(static_cast<uint8_t>((handler.target >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(static_cast<int>(handler.type) & 0xFF));
        data.push_back(static_cast<uint8_t>(handler.stack_depth & 0xFF));
        data.push_back(static_cast<uint8_t>((handler.stack_depth >> 8) & 0xFF));
    }

    // 序列化行号表
    uint32_t lnotab_count = lnotab.size();
    data.push_back(static_cast<uint8_t>(lnotab_count & 0xFF));
    data.push_back(static_cast<uint8_t>((lnotab_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((lnotab_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((lnotab_count >> 24) & 0xFF));
    for (const auto& entry : lnotab) {
        data.push_back(static_cast<uint8_t>(entry.offset & 0xFF));
        data.push_back(static_cast<uint8_t>((entry.offset >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(entry.line & 0xFF));
        data.push_back(static_cast<uint8_t>((entry.line >> 8) & 0xFF));
    }

    return data;
}

void Chunk::serialize_constant(std::vector<uint8_t>& data, const ir::Value& constant) const {
    if (!constant.is_constant()) {
        data.push_back(0);
        return;
    }
    
    try {
        auto& data_variant = const_cast<ir::Value&>(constant).get_data();
        
        if (std::holds_alternative<std::nullptr_t>(data_variant)) {
            data.push_back(0);
        } else if (std::holds_alternative<bool>(data_variant)) {
            data.push_back(1);
            data.push_back(std::get<bool>(data_variant) ? 1 : 0);
        } else if (std::holds_alternative<int64_t>(data_variant)) {
            data.push_back(2);
            int64_t val = std::get<int64_t>(data_variant);
            for (int i = 0; i < 8; i++) {
                data.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
            }
        } else if (std::holds_alternative<double>(data_variant)) {
            data.push_back(3);
            double val = std::get<double>(data_variant);
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&val);
            data.insert(data.end(), bytes, bytes + 8);
        } else if (std::holds_alternative<std::string>(data_variant)) {
            data.push_back(4);
            const std::string& val = std::get<std::string>(data_variant);
            uint32_t str_size = val.size();
            data.push_back(static_cast<uint8_t>(str_size & 0xFF));
            data.push_back(static_cast<uint8_t>((str_size >> 8) & 0xFF));
            data.push_back(static_cast<uint8_t>((str_size >> 16) & 0xFF));
            data.push_back(static_cast<uint8_t>((str_size >> 24) & 0xFF));
            data.insert(data.end(), val.begin(), val.end());
        } else {
            data.push_back(0);
        }
    } catch (...) {
        data.push_back(0);
    }
}

// ============================================================================
// BytecodeGenerator 类实现
// ============================================================================

BytecodeGenerator::BytecodeGenerator() {
    init_opcode_map();
}

void BytecodeGenerator::init_opcode_map() {
    // =========================================================================
    // 加载/存储指令
    // =========================================================================
    opcode_map_[ir::IROpcode::LOAD_CONST] = BytecodeOpcode::LOAD_CONST;
    opcode_map_[ir::IROpcode::LOAD_FAST] = BytecodeOpcode::LOAD_FAST;
    opcode_map_[ir::IROpcode::STORE_FAST] = BytecodeOpcode::STORE_FAST;
    opcode_map_[ir::IROpcode::LOAD_GLOBAL] = BytecodeOpcode::LOAD_GLOBAL;
    opcode_map_[ir::IROpcode::STORE_GLOBAL] = BytecodeOpcode::STORE_GLOBAL;
    opcode_map_[ir::IROpcode::LOAD_NAME] = BytecodeOpcode::LOAD_NAME;
    opcode_map_[ir::IROpcode::STORE_NAME] = BytecodeOpcode::STORE_NAME;
    
    // 删除操作
    opcode_map_[ir::IROpcode::DELETE_FAST] = BytecodeOpcode::DELETE_FAST;
    opcode_map_[ir::IROpcode::DELETE_GLOBAL] = BytecodeOpcode::DELETE_GLOBAL;
    opcode_map_[ir::IROpcode::DELETE_NAME] = BytecodeOpcode::DELETE_NAME;

    // =========================================================================
    // 二元运算指令
    // =========================================================================
    opcode_map_[ir::IROpcode::BINARY_ADD] = BytecodeOpcode::BINARY_ADD;
    opcode_map_[ir::IROpcode::BINARY_SUBTRACT] = BytecodeOpcode::BINARY_SUBTRACT;
    opcode_map_[ir::IROpcode::BINARY_MULTIPLY] = BytecodeOpcode::BINARY_MULTIPLY;
    opcode_map_[ir::IROpcode::BINARY_TRUE_DIVIDE] = BytecodeOpcode::BINARY_TRUE_DIVIDE;
    opcode_map_[ir::IROpcode::BINARY_FLOOR_DIVIDE] = BytecodeOpcode::BINARY_FLOOR_DIVIDE;
    opcode_map_[ir::IROpcode::BINARY_MODULO] = BytecodeOpcode::BINARY_MODULO;
    opcode_map_[ir::IROpcode::BINARY_POWER] = BytecodeOpcode::BINARY_POWER;
    opcode_map_[ir::IROpcode::BINARY_LSHIFT] = BytecodeOpcode::BINARY_LSHIFT;
    opcode_map_[ir::IROpcode::BINARY_RSHIFT] = BytecodeOpcode::BINARY_RSHIFT;
    opcode_map_[ir::IROpcode::BINARY_AND] = BytecodeOpcode::BINARY_AND;
    opcode_map_[ir::IROpcode::BINARY_OR] = BytecodeOpcode::BINARY_OR;
    opcode_map_[ir::IROpcode::BINARY_XOR] = BytecodeOpcode::BINARY_XOR;

    // =========================================================================
    // 一元运算指令
    // =========================================================================
    opcode_map_[ir::IROpcode::UNARY_POSITIVE] = BytecodeOpcode::UNARY_POSITIVE;
    opcode_map_[ir::IROpcode::UNARY_NEGATIVE] = BytecodeOpcode::UNARY_NEGATIVE;
    opcode_map_[ir::IROpcode::UNARY_NOT] = BytecodeOpcode::UNARY_NOT;
    opcode_map_[ir::IROpcode::UNARY_INVERT] = BytecodeOpcode::UNARY_INVERT;

    // =========================================================================
    // 比较运算指令（统一映射到 COMPARE_OP，通过操作数区分具体比较类型）
    // =========================================================================
    opcode_map_[ir::IROpcode::COMPARE_EQ] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_NE] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_LT] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_LE] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_GT] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_GE] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_IS] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_IS_NOT] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_IN] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_NOT_IN] = BytecodeOpcode::COMPARE_OP; // 或 JUMP_IF_TRUE_OR_POP

    // =========================================================================
    // 控制流指令
    // =========================================================================
    opcode_map_[ir::IROpcode::JUMP] = BytecodeOpcode::JUMP_ABSOLUTE;
    opcode_map_[ir::IROpcode::JUMP_IF_TRUE] = BytecodeOpcode::POP_JUMP_IF_TRUE;
    opcode_map_[ir::IROpcode::JUMP_IF_FALSE] = BytecodeOpcode::POP_JUMP_IF_FALSE;
    opcode_map_[ir::IROpcode::JUMP_IF_FALSE_OR_POP] = BytecodeOpcode::JUMP_IF_FALSE_OR_POP;
    opcode_map_[ir::IROpcode::JUMP_IF_TRUE_OR_POP] = BytecodeOpcode::JUMP_IF_TRUE_OR_POP;
    opcode_map_[ir::IROpcode::RETURN_VALUE] = BytecodeOpcode::RETURN_VALUE;
    
    // 循环控制
    opcode_map_[ir::IROpcode::BREAK_LOOP] = BytecodeOpcode::BREAK_LOOP;
    opcode_map_[ir::IROpcode::CONTINUE_LOOP] = BytecodeOpcode::CONTINUE_LOOP;
    
    // 异常与生成器
    opcode_map_[ir::IROpcode::RAISE_VARARGS] = BytecodeOpcode::RAISE_VARARGS;
    opcode_map_[ir::IROpcode::YIELD_VALUE] = BytecodeOpcode::YIELD_VALUE;
    opcode_map_[ir::IROpcode::YIELD_FROM] = BytecodeOpcode::YIELD_FROM;
    
    // 迭代器
    opcode_map_[ir::IROpcode::GET_ITER] = BytecodeOpcode::GET_ITER;
    opcode_map_[ir::IROpcode::FOR_ITER] = BytecodeOpcode::FOR_ITER;

    // =========================================================================
    // 函数调用指令
    // =========================================================================
    opcode_map_[ir::IROpcode::CALL_FUNCTION] = BytecodeOpcode::CALL_FUNCTION;
    opcode_map_[ir::IROpcode::CALL_FUNCTION_KW] = BytecodeOpcode::CALL_FUNCTION_KW;
    opcode_map_[ir::IROpcode::CALL_FUNCTION_EX] = BytecodeOpcode::CALL_FUNCTION_EX;
    opcode_map_[ir::IROpcode::CALL_METHOD] = BytecodeOpcode::CALL_METHOD;
    opcode_map_[ir::IROpcode::MAKE_FUNCTION] = BytecodeOpcode::MAKE_FUNCTION;
    opcode_map_[ir::IROpcode::LOAD_METHOD] = BytecodeOpcode::LOAD_METHOD;

    // =========================================================================
    // 构建复合对象指令
    // =========================================================================
    opcode_map_[ir::IROpcode::BUILD_TUPLE] = BytecodeOpcode::BUILD_TUPLE;
    opcode_map_[ir::IROpcode::BUILD_LIST] = BytecodeOpcode::BUILD_LIST;
    opcode_map_[ir::IROpcode::BUILD_SET] = BytecodeOpcode::BUILD_SET;
    opcode_map_[ir::IROpcode::BUILD_MAP] = BytecodeOpcode::BUILD_MAP;
    opcode_map_[ir::IROpcode::BUILD_STRING] = BytecodeOpcode::BUILD_STRING;
    opcode_map_[ir::IROpcode::BUILD_CONST_KEY_MAP] = BytecodeOpcode::BUILD_CONST_KEY_MAP;
    
    // 解包构建
    opcode_map_[ir::IROpcode::BUILD_TUPLE_UNPACK] = BytecodeOpcode::BUILD_TUPLE_UNPACK;
    opcode_map_[ir::IROpcode::BUILD_LIST_UNPACK] = BytecodeOpcode::BUILD_LIST_UNPACK;
    opcode_map_[ir::IROpcode::BUILD_SET_UNPACK] = BytecodeOpcode::BUILD_SET_UNPACK;
    opcode_map_[ir::IROpcode::BUILD_MAP_UNPACK] = BytecodeOpcode::BUILD_MAP_UNPACK;

    // =========================================================================
    // 序列操作指令
    // =========================================================================
    opcode_map_[ir::IROpcode::UNPACK_SEQUENCE] = BytecodeOpcode::UNPACK_SEQUENCE;
    opcode_map_[ir::IROpcode::GET_AITER] = BytecodeOpcode::GET_AITER;
    opcode_map_[ir::IROpcode::GET_ANEXT] = BytecodeOpcode::GET_ANEXT;
    opcode_map_[ir::IROpcode::GET_YIELD_FROM_ITER] = BytecodeOpcode::GET_YIELD_FROM_ITER;

    // =========================================================================
    // 属性/下标操作指令（关键：解决操作码0问题）
    // =========================================================================
    opcode_map_[ir::IROpcode::LOAD_ATTR] = BytecodeOpcode::LOAD_ATTR;
    opcode_map_[ir::IROpcode::STORE_ATTR] = BytecodeOpcode::STORE_ATTR;
    opcode_map_[ir::IROpcode::DELETE_ATTR] = BytecodeOpcode::DELETE_ATTR;
    opcode_map_[ir::IROpcode::LOAD_SUBSCR] = BytecodeOpcode::LOAD_SUBSCR;      // 修正：之前缺失导致操作码0
    opcode_map_[ir::IROpcode::STORE_SUBSCR] = BytecodeOpcode::STORE_SUBSCR;
    opcode_map_[ir::IROpcode::DELETE_SUBSCR] = BytecodeOpcode::DELETE_SUBSCR;
    opcode_map_[ir::IROpcode::BUILD_SLICE] = BytecodeOpcode::BUILD_SLICE;

    // =========================================================================
    // 闭包和作用域指令
    // =========================================================================
    opcode_map_[ir::IROpcode::LOAD_CLOSURE] = BytecodeOpcode::LOAD_CLOSURE;
    opcode_map_[ir::IROpcode::LOAD_DEREF] = BytecodeOpcode::LOAD_DEREF;
    opcode_map_[ir::IROpcode::STORE_DEREF] = BytecodeOpcode::STORE_DEREF;
    opcode_map_[ir::IROpcode::DELETE_DEREF] = BytecodeOpcode::DELETE_DEREF;
    opcode_map_[ir::IROpcode::LOAD_CLASSDEREF] = BytecodeOpcode::LOAD_CLASSDEREF;

    // =========================================================================
    // 栈操作指令
    // =========================================================================
    opcode_map_[ir::IROpcode::POP_TOP] = BytecodeOpcode::POP_TOP;
    opcode_map_[ir::IROpcode::ROT_TWO] = BytecodeOpcode::ROT_TWO;
    opcode_map_[ir::IROpcode::ROT_THREE] = BytecodeOpcode::ROT_THREE;
    opcode_map_[ir::IROpcode::ROT_FOUR] = BytecodeOpcode::ROT_FOUR;
    opcode_map_[ir::IROpcode::DUP_TOP] = BytecodeOpcode::DUP_TOP;
    opcode_map_[ir::IROpcode::DUP_TOP_TWO] = BytecodeOpcode::DUP_TOP_TWO;

    // =========================================================================
    // 异步操作指令
    // =========================================================================
    opcode_map_[ir::IROpcode::GET_AWAITABLE] = BytecodeOpcode::GET_AWAITABLE;
    opcode_map_[ir::IROpcode::SETUP_ASYNC_WITH] = BytecodeOpcode::SETUP_ASYNC_WITH;
    opcode_map_[ir::IROpcode::BEGIN_FINALLY] = BytecodeOpcode::BEGIN_FINALLY;
    opcode_map_[ir::IROpcode::END_ASYNC_FOR] = BytecodeOpcode::END_ASYNC_FOR;
    opcode_map_[ir::IROpcode::BEFORE_ASYNC_WITH] = BytecodeOpcode::BEFORE_ASYNC_WITH;
    opcode_map_[ir::IROpcode::CALL_FINALLY] = BytecodeOpcode::CALL_FINALLY;

    // =========================================================================
    // 异常处理指令
    // =========================================================================
    opcode_map_[ir::IROpcode::SETUP_EXCEPT] = BytecodeOpcode::SETUP_EXCEPT;
    opcode_map_[ir::IROpcode::SETUP_FINALLY] = BytecodeOpcode::SETUP_FINALLY;
    opcode_map_[ir::IROpcode::SETUP_WITH] = BytecodeOpcode::SETUP_WITH;
    opcode_map_[ir::IROpcode::END_FINALLY] = BytecodeOpcode::END_FINALLY;
    opcode_map_[ir::IROpcode::POP_FINALLY] = BytecodeOpcode::POP_FINALLY;

    // =========================================================================
    // 格式化指令
    // =========================================================================
    opcode_map_[ir::IROpcode::FORMAT_VALUE] = BytecodeOpcode::FORMAT_VALUE;

    // =========================================================================
    // 导入指令
    // =========================================================================
    opcode_map_[ir::IROpcode::IMPORT_NAME] = BytecodeOpcode::IMPORT_NAME;
    opcode_map_[ir::IROpcode::IMPORT_FROM] = BytecodeOpcode::IMPORT_FROM;
    opcode_map_[ir::IROpcode::IMPORT_STAR] = BytecodeOpcode::IMPORT_STAR;

    // =========================================================================
    // SSA 特有指令（在字节码层面消除，映射为 NOP）
    // =========================================================================
    opcode_map_[ir::IROpcode::PHI] = BytecodeOpcode::NOP;
    opcode_map_[ir::IROpcode::COPY] = BytecodeOpcode::NOP;
}

BytecodeOpcode BytecodeGenerator::ir_opcode_to_bytecode(ir::IROpcode opcode) const {
    auto it = opcode_map_.find(opcode);
    if (it != opcode_map_.end()) {
        return it->second;
    }
    return BytecodeOpcode::NOP;
}

static bool should_skip_instruction(ir::IROpcode opcode) {
    return opcode == ir::IROpcode::PHI || opcode == ir::IROpcode::COPY;
}

/**
 * @brief 计算指令的字节大小（用于第一遍偏移计算）
 */
size_t BytecodeGenerator::calc_instruction_size(const ir::Instruction& instr) {
    if (should_skip_instruction(instr.opcode())) {
        return 0;
    }
    
    BytecodeOpcode bc_opcode = ir_opcode_to_bytecode(instr.opcode());
    uint8_t opcode_val = static_cast<uint8_t>(bc_opcode);
    
    size_t size = 1;  // opcode
    
    if (has_arg(opcode_val)) {
        size += 2;  // 固定 2 字节参数
    }
    
    return size;
}

std::unique_ptr<Chunk> BytecodeGenerator::generate(ir::IRModule* module) {
    if (!module) {
        return nullptr;
    }

    auto chunk = std::make_unique<Chunk>();
    chunk->filename = module->filename();
    chunk->name = module->module_name();
    chunk->qualname = module->module_name();
    chunk->firstlineno = 1;

    // =========================================================================
    // 初始化变量表 - 从模块中预加载所有变量名，确保索引一致
    // =========================================================================
    chunk->varnames = module->varnames();
    chunk->nlocals = chunk->varnames.size();

    // =========================================================================
    // 第一遍：收集指令并计算精确偏移
    // =========================================================================
    
    struct InstrInfo {
        const ir::Instruction* instr;
        ir::BasicBlock* block;
        uint16_t offset;  // 实际字节码偏移
    };
    
    std::vector<InstrInfo> instr_infos;
    std::unordered_map<const ir::Instruction*, uint16_t> instr_to_offset;
    std::unordered_map<ir::BasicBlock*, uint16_t> block_to_offset;
    
    uint16_t current_offset = 0;
    
    // 收集所有块
    for (const auto& block : module->blocks()) {
        block_to_offset[block.get()] = current_offset;
        
        for (const auto& instr_ptr : block->instructions()) {
            if (should_skip_instruction(instr_ptr->opcode())) {
                continue;
            }
            
            InstrInfo info;
            info.instr = instr_ptr.get();
            info.block = block.get();
            info.offset = current_offset;
            
            instr_infos.push_back(info);
            instr_to_offset[instr_ptr.get()] = current_offset;
            
            current_offset += calc_instruction_size(*instr_ptr);
        }
    }
    
    // =========================================================================
    // 第二遍：生成字节码（使用精确偏移回填跳转目标）
    // =========================================================================
    
    instr_mappings_.clear();
    
    for (const auto& info : instr_infos) {
        const ir::Instruction& instr = *info.instr;
        
        // 记录映射（用于调试输出）
        InstrMapping mapping;
        mapping.ir_instr = instr.to_string();
        mapping.byte_offset = info.offset;
        
        // 生成指令
        BytecodeOpcode bc_opcode = ir_opcode_to_bytecode(instr.opcode());
        uint32_t oparg = 0;
        
        // 计算操作数（特别是跳转目标）
        switch (instr.opcode()) {
            case ir::IROpcode::LOAD_CONST:
                if (instr.operand_count() > 0) {
                    oparg = chunk->add_constant(instr.operand(0));
                }
                break;

            case ir::IROpcode::LOAD_FAST:
                if (instr.has_result()) {
                    const std::string& var_name = instr.result().name();
                    if (!var_name.empty()) {
                        oparg = chunk->add_varname(var_name);
                    }
                }
                break;

            case ir::IROpcode::STORE_FAST:
                if (instr.has_result()) {
                    std::string var_name = instr.result().name();
                    oparg = chunk->add_varname(var_name);
                }
                break;

            case ir::IROpcode::LOAD_GLOBAL:
                if (instr.has_result()) {
                    std::string var_name = instr.result().name();
                    oparg = chunk->add_name(var_name);
                }
                break;

            case ir::IROpcode::STORE_GLOBAL:
                if (instr.has_result()) {
                    std::string var_name = instr.result().name();
                    oparg = chunk->add_name(var_name);
                }
                break;

            case ir::IROpcode::LOAD_ATTR:
                oparg = chunk->add_name(instr.metadata("attr"));
                break;

            case ir::IROpcode::STORE_ATTR:
                oparg = chunk->add_name(instr.metadata("attr"));
                break;
            
            case ir::IROpcode::DELETE_ATTR:
                oparg = chunk->add_name(instr.metadata("attr"));
                break;

            // 一元运算（修复：之前缺失导致 00）
            case ir::IROpcode::UNARY_POSITIVE:
            case ir::IROpcode::UNARY_NEGATIVE:
            case ir::IROpcode::UNARY_NOT:
            case ir::IROpcode::UNARY_INVERT:
                oparg = 0;
                break;

            // 二元运算
            case ir::IROpcode::BINARY_ADD:
            case ir::IROpcode::BINARY_SUBTRACT:
            case ir::IROpcode::BINARY_MULTIPLY:
            case ir::IROpcode::BINARY_TRUE_DIVIDE:
            case ir::IROpcode::BINARY_FLOOR_DIVIDE:
            case ir::IROpcode::BINARY_MODULO:
            case ir::IROpcode::BINARY_POWER:
            case ir::IROpcode::BINARY_LSHIFT:
            case ir::IROpcode::BINARY_RSHIFT:
            case ir::IROpcode::BINARY_AND:
            case ir::IROpcode::BINARY_OR:
            case ir::IROpcode::BINARY_XOR:
                oparg = 0;
                break;

            // 比较运算
            case ir::IROpcode::COMPARE_EQ:
                oparg = 2; break;
            case ir::IROpcode::COMPARE_NE:
                oparg = 3; break;
            case ir::IROpcode::COMPARE_LT:
                oparg = 0; break;
            case ir::IROpcode::COMPARE_LE:
                oparg = 1; break;
            case ir::IROpcode::COMPARE_GT:
                oparg = 4; break;
            case ir::IROpcode::COMPARE_GE:
                oparg = 5; break;
            case ir::IROpcode::COMPARE_IS:
                oparg = 8; break;
            case ir::IROpcode::COMPARE_IS_NOT:
                oparg = 9; break;
            case ir::IROpcode::COMPARE_IN:
                oparg = 10; break;
            case ir::IROpcode::COMPARE_NOT_IN:
                oparg = 11; break;

            // 跳转指令
            case ir::IROpcode::JUMP:
            case ir::IROpcode::JUMP_IF_TRUE:
            case ir::IROpcode::JUMP_IF_FALSE:
            case ir::IROpcode::FOR_ITER: 
            case ir::IROpcode::JUMP_IF_TRUE_OR_POP:
            case ir::IROpcode::JUMP_IF_FALSE_OR_POP: {
                ir::BasicBlock* target = instr.target();
                if (target) {
                    auto it = block_to_offset.find(target);
                    if (it != block_to_offset.end()) {
                        oparg = it->second;
                    } else {
                        std::cerr << "Warning: Jump target block not found, using 0" << std::endl;
                        oparg = 0;
                    }
                }
                break;
            }
            
            // 循环控制
            case ir::IROpcode::BREAK_LOOP:
                oparg = 0;
                break;
            case ir::IROpcode::CONTINUE_LOOP: {
                ir::BasicBlock* target = instr.target();
                if (target) {
                    auto it = block_to_offset.find(target);
                    if (it != block_to_offset.end()) oparg = it->second;
                }
                break;
            }

            // 函数调用
            case ir::IROpcode::CALL_FUNCTION:
            case ir::IROpcode::CALL_FUNCTION_KW:
                oparg = instr.operand_count() > 0 ? instr.operand_count() - 1 : 0;
                break;
            
            case ir::IROpcode::CALL_METHOD:
                oparg = instr.operand_count() > 0 ? instr.operand_count() - 1 : 0;
                break;

            // 容器构建（修复：之前缺失导致 00）
            case ir::IROpcode::BUILD_TUPLE:
            case ir::IROpcode::BUILD_LIST:
            case ir::IROpcode::BUILD_SET:
                oparg = instr.operand_count();
                break;
            
            case ir::IROpcode::BUILD_MAP:
                oparg = instr.operand_count() / 2;
                break;
            
            case ir::IROpcode::BUILD_SLICE:
                oparg = instr.operand_count(); // 2 或 3
                break;

            // 序列操作
            case ir::IROpcode::UNPACK_SEQUENCE:
                oparg = std::stoul(instr.metadata("count"));
                break;
            
            case ir::IROpcode::GET_ITER:
                oparg = 0;
                break;

            // 下标操作（修复：之前缺失导致 00）
            case ir::IROpcode::LOAD_SUBSCR:
            case ir::IROpcode::STORE_SUBSCR:
            case ir::IROpcode::DELETE_SUBSCR:
                oparg = 0;
                break;

            // 删除操作（修复：之前缺失）
            case ir::IROpcode::DELETE_FAST:
                if (instr.operand_count() > 0) {
                    oparg = chunk->add_varname(instr.operand(0).name());
                }
                break;
            case ir::IROpcode::DELETE_GLOBAL:
            case ir::IROpcode::DELETE_NAME:
                if (instr.operand_count() > 0) {
                    oparg = chunk->add_name(instr.operand(0).name());
                }
                break;
            case ir::IROpcode::DELETE_DEREF:
                if (instr.operand_count() > 0) {
                    oparg = chunk->add_varname(instr.operand(0).name());
                }
                break;

            // 栈操作（修复：之前缺失导致 00）
            case ir::IROpcode::POP_TOP:
            case ir::IROpcode::DUP_TOP:
            case ir::IROpcode::DUP_TOP_TWO:
            case ir::IROpcode::ROT_TWO:
            case ir::IROpcode::ROT_THREE:
            case ir::IROpcode::ROT_FOUR:
                oparg = 0;
                break;

            // 逻辑运算（修复：之前缺失）
            case ir::IROpcode::LOGICAL_AND:
            case ir::IROpcode::LOGICAL_OR: {
                ir::BasicBlock* target = instr.target();
                if (target) {
                    auto it = block_to_offset.find(target);
                    if (it != block_to_offset.end()) oparg = it->second;
                }
                break;
            }

            // 导入（修复：之前缺失）
            case ir::IROpcode::IMPORT_NAME:
            case ir::IROpcode::IMPORT_FROM:
                if (instr.operand_count() > 0) {
                    oparg = chunk->add_name(instr.operand(0).name());
                }
                break;
            case ir::IROpcode::IMPORT_STAR:
                oparg = 0;
                break;

            // 异步（修复：之前缺失）
            case ir::IROpcode::GET_AWAITABLE:
            case ir::IROpcode::BEFORE_ASYNC_WITH:
            case ir::IROpcode::END_ASYNC_FOR:
                oparg = 0;
                break;
            
            case ir::IROpcode::SETUP_ASYNC_WITH:
            case ir::IROpcode::SETUP_EXCEPT:
            case ir::IROpcode::SETUP_FINALLY:
            case ir::IROpcode::SETUP_WITH: {
                ir::BasicBlock* target = instr.target();
                if (target) {
                    auto it = block_to_offset.find(target);
                    if (it != block_to_offset.end()) oparg = it->second;
                }
                break;
            }
            
            case ir::IROpcode::END_FINALLY:
            case ir::IROpcode::POP_FINALLY:
            case ir::IROpcode::BEGIN_FINALLY:
                oparg = 0;
                break;
            
            case ir::IROpcode::CALL_FINALLY: {
                ir::BasicBlock* target = instr.target();
                if (target) {
                    auto it = block_to_offset.find(target);
                    if (it != block_to_offset.end()) oparg = it->second;
                }
                break;
            }

            // 异常与生成器（修复：之前缺失）
            case ir::IROpcode::RAISE_VARARGS:
                oparg = instr.operand_count();
                break;
            case ir::IROpcode::YIELD_VALUE:
            case ir::IROpcode::YIELD_FROM:
                oparg = 0;
                break;

            // 格式化（修复：之前缺失）
            case ir::IROpcode::FORMAT_VALUE:
                oparg = instr.operand_count() > 1 ? std::stoul(instr.metadata("format_spec")) : 0;
                break;
            
            // 闭包（修复：之前缺失）
            case ir::IROpcode::LOAD_CLOSURE:
            case ir::IROpcode::LOAD_DEREF:
            case ir::IROpcode::STORE_DEREF:
            case ir::IROpcode::LOAD_CLASSDEREF:
                if (instr.operand_count() > 0) {
                    oparg = chunk->add_varname(instr.operand(0).name());
                }
                break;

            case ir::IROpcode::RETURN_VALUE:
                break;

            default:
                // 如果真的到了这里，说明有未处理的 IROpcode
                std::cerr << "Warning: Unhandled IROpcode in generate(), using NOP: " 
                          << static_cast<int>(instr.opcode()) << std::endl;
                bc_opcode = BytecodeOpcode::NOP;
                oparg = 0;
                break;
        }
        
        // 添加指令到 chunk
        chunk->add_instruction(static_cast<uint8_t>(bc_opcode), oparg);
        
        // 记录生成的字节（用于调试）
        std::vector<uint8_t> bytes;
        bytes.push_back(static_cast<uint8_t>(bc_opcode));
        if (has_arg(static_cast<uint8_t>(bc_opcode))) {
            size_t arg_size = calc_arg_size(oparg);
            for (size_t i = 0; i < arg_size; i++) {
                bytes.push_back(static_cast<uint8_t>((oparg >> (i * 8)) & 0xFF));
            }
        }
        mapping.bytes = bytes;
        
        std::ostringstream oss;
        oss << static_cast<int>(bc_opcode);
        if (has_arg(static_cast<uint8_t>(bc_opcode))) {
            oss << " " << oparg;
        }
        mapping.bytecode_instr = oss.str();
        
        instr_mappings_.push_back(mapping);
    }

    // 计算栈深度
    chunk->stacksize = compute_stack_depth(module);

    // 复制常量（不再覆盖 names 和 varnames，因为我们已经正确初始化了）
    chunk->consts.assign(module->constants().begin(), module->constants().end());
    // 注意：不执行 chunk->names = module->names()
    // 因为 names 已经在 generate_load_global 中被正确填充
    // 删除这行：chunk->varnames = module->varnames(); 
    // 因为我们已经在开头初始化，并且在生成过程中保持了一致性
    chunk->nlocals = chunk->varnames.size();

    // 计算实际指令数
    chunk->instruction_count = instr_infos.size();

    return chunk;
}


void BytecodeGenerator::generate_from_basic_block(ir::BasicBlock* block, Chunk* chunk) {
    if (!block || !chunk) return;

    for (const auto& instr_ptr : block->instructions()) {
        // 跳过 PHI/COPY
        if (should_skip_instruction(instr_ptr->opcode())) {
            continue;
        }
        generate_from_instruction(*instr_ptr, chunk);
    }
}

void BytecodeGenerator::generate_from_instruction(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;

    // 提前返回：跳过 PHI 和 COPY
    if (should_skip_instruction(instr.opcode())) {
        return;
    }

    BytecodeOpcode bc_opcode = ir_opcode_to_bytecode(instr.opcode());
    uint32_t oparg = 0;

    switch (instr.opcode()) {
        case ir::IROpcode::LOAD_CONST:
            if (instr.operand_count() > 0) {
                oparg = chunk->add_constant(instr.operand(0));
            }
            break;

        case ir::IROpcode::LOAD_FAST:
            // 修复：LOAD_FAST 的变量名存储在 result 中，不是 operand 中
            if (instr.has_result()) {
                const std::string& var_name = instr.result().name();
                if (!var_name.empty()) {
                    oparg = chunk->add_varname(var_name);
                }
            }
            break;

        case ir::IROpcode::STORE_FAST:
            if (instr.has_result()) {
                std::string var_name = instr.result().name();
                oparg = chunk->add_varname(var_name);
            }
            break;

        case ir::IROpcode::LOAD_GLOBAL:
            if (instr.has_result()) {
                std::string var_name = instr.result().name();
                oparg = chunk->add_name(var_name);
            }
            break;

        case ir::IROpcode::STORE_GLOBAL:
            if (instr.has_result()) {
                std::string var_name = instr.result().name();
                oparg = chunk->add_name(var_name);
            }
            break;

        case ir::IROpcode::LOAD_ATTR:
            oparg = chunk->add_name(instr.metadata("attr"));
            break;

        case ir::IROpcode::STORE_ATTR:
            oparg = chunk->add_name(instr.metadata("attr"));
            break;

        case ir::IROpcode::BINARY_ADD:
        case ir::IROpcode::BINARY_SUBTRACT:
        case ir::IROpcode::BINARY_MULTIPLY:
        case ir::IROpcode::BINARY_TRUE_DIVIDE:
        case ir::IROpcode::BINARY_FLOOR_DIVIDE:
        case ir::IROpcode::BINARY_MODULO:
        case ir::IROpcode::BINARY_POWER:
        case ir::IROpcode::BINARY_LSHIFT:
        case ir::IROpcode::BINARY_RSHIFT:
        case ir::IROpcode::BINARY_AND:
        case ir::IROpcode::BINARY_OR:
        case ir::IROpcode::BINARY_XOR:
            oparg = 0;
            break;

        case ir::IROpcode::COMPARE_EQ:
        case ir::IROpcode::COMPARE_NE:
        case ir::IROpcode::COMPARE_LT:
        case ir::IROpcode::COMPARE_LE:
        case ir::IROpcode::COMPARE_GT:
        case ir::IROpcode::COMPARE_GE:
        case ir::IROpcode::COMPARE_IS:
        case ir::IROpcode::COMPARE_IS_NOT:
        case ir::IROpcode::COMPARE_IN:
        case ir::IROpcode::COMPARE_NOT_IN: {
            static const std::map<csgo::ir::IROpcode, uint32_t> compare_map = {
                {csgo::ir::IROpcode::COMPARE_EQ, 2},
                {csgo::ir::IROpcode::COMPARE_NE, 3},
                {csgo::ir::IROpcode::COMPARE_LT, 0},
                {csgo::ir::IROpcode::COMPARE_LE, 1},
                {csgo::ir::IROpcode::COMPARE_GT, 4},
                {csgo::ir::IROpcode::COMPARE_GE, 5},
                {csgo::ir::IROpcode::COMPARE_IS, 8},
                {csgo::ir::IROpcode::COMPARE_IS_NOT, 9},
                {csgo::ir::IROpcode::COMPARE_IN, 10},
                {csgo::ir::IROpcode::COMPARE_NOT_IN, 11},
            };
            auto it = compare_map.find(instr.opcode());
            if (it != compare_map.end()) {
                oparg = it->second;
            }
            break;
        }

        case ir::IROpcode::JUMP:
        case ir::IROpcode::JUMP_IF_TRUE:
        case ir::IROpcode::JUMP_IF_FALSE:
        case ir::IROpcode::JUMP_IF_TRUE_OR_POP:
        case ir::IROpcode::JUMP_IF_FALSE_OR_POP: {
            ir::BasicBlock* target = instr.target();
            if (target) {
                oparg = resolve_jump_target(target);
            }
            break;
        }

        case ir::IROpcode::BUILD_TUPLE:
        case ir::IROpcode::BUILD_LIST:
        case ir::IROpcode::BUILD_SET:
            oparg = instr.operand_count();
            break;

        case ir::IROpcode::BUILD_MAP:
            oparg = instr.operand_count() / 2;
            break;

        case ir::IROpcode::BUILD_SLICE:
            oparg = instr.operand_count();
            break;

        case ir::IROpcode::CALL_FUNCTION:
            oparg = instr.operand_count() > 0 ? instr.operand_count() - 1 : 0;
            break;

        case ir::IROpcode::CALL_FUNCTION_KW:
            oparg = instr.operand_count() > 0 ? instr.operand_count() - 1 : 0;
            break;

        case ir::IROpcode::UNPACK_SEQUENCE:
            oparg = std::stoul(instr.metadata("count"));
            break;

        case ir::IROpcode::GET_ITER:
            break;

        case ir::IROpcode::FOR_ITER: {
            ir::BasicBlock* target = instr.target();
            if (target) {
                oparg = resolve_jump_target(target);
            }
            break;
        }

        case ir::IROpcode::RETURN_VALUE:
            break;

        case ir::IROpcode::POP_TOP:
        case ir::IROpcode::DUP_TOP:
        case ir::IROpcode::DUP_TOP_TWO:
        case ir::IROpcode::ROT_TWO:
        case ir::IROpcode::ROT_THREE:
        case ir::IROpcode::ROT_FOUR:
            oparg = 0;
            break;

        case ir::IROpcode::DELETE_FAST:
            if (instr.operand_count() > 0) {
                std::string var_name = instr.operand(0).name();
                oparg = chunk->add_varname(var_name);
            }
            break;

        case ir::IROpcode::DELETE_GLOBAL:
        case ir::IROpcode::DELETE_NAME:
            if (instr.operand_count() > 0) {
                std::string var_name = instr.operand(0).name();
                oparg = chunk->add_name(var_name);
            }
            break;

        case ir::IROpcode::DELETE_ATTR:
            oparg = chunk->add_name(instr.metadata("attr"));
            break;

        case ir::IROpcode::DELETE_SUBSCR:
            oparg = 0;
            break;

        case ir::IROpcode::DELETE_DEREF:
            if (instr.operand_count() > 0) {
                std::string var_name = instr.operand(0).name();
                oparg = chunk->add_varname(var_name);
            }
            break;

        case ir::IROpcode::LOGICAL_AND:
        case ir::IROpcode::LOGICAL_OR: {
            ir::BasicBlock* target = instr.target();
            if (target) {
                oparg = resolve_jump_target(target);
            }
            break;
        }

        case ir::IROpcode::IMPORT_NAME:
        case ir::IROpcode::IMPORT_FROM:
            if (instr.operand_count() > 0) {
                std::string var_name = instr.operand(0).name();
                oparg = chunk->add_name(var_name);
            }
            break;

        case ir::IROpcode::IMPORT_STAR:
            oparg = 0;
            break;

        case ir::IROpcode::GET_AWAITABLE:
        case ir::IROpcode::BEFORE_ASYNC_WITH:
        case ir::IROpcode::END_ASYNC_FOR:
            oparg = 0;
            break;

        case ir::IROpcode::SETUP_ASYNC_WITH:
        case ir::IROpcode::SETUP_EXCEPT:
        case ir::IROpcode::SETUP_FINALLY:
        case ir::IROpcode::SETUP_WITH:
            if (instr.target()) {
                oparg = resolve_jump_target(instr.target());
            }
            break;

        case ir::IROpcode::END_FINALLY:
        case ir::IROpcode::POP_FINALLY:
        case ir::IROpcode::BEGIN_FINALLY:
            oparg = 0;
            break;

        case ir::IROpcode::CALL_FINALLY:
            if (instr.target()) {
                oparg = resolve_jump_target(instr.target());
            }
            break;

        case ir::IROpcode::RAISE_VARARGS:
            oparg = instr.operand_count();
            break;

        case ir::IROpcode::YIELD_VALUE:
        case ir::IROpcode::YIELD_FROM:
            oparg = 0;
            break;

        case ir::IROpcode::FORMAT_VALUE:
            oparg = instr.operand_count() > 1 ? std::stoul(instr.metadata("format_spec")) : 0;
            break;

        case ir::IROpcode::BREAK_LOOP:
            oparg = 0;
            break;

        case ir::IROpcode::CONTINUE_LOOP:
            if (instr.target()) {
                oparg = resolve_jump_target(instr.target());
            }
            break;

        default:
            // 未知指令映射为 NOP
            bc_opcode = BytecodeOpcode::NOP;
            oparg = 0;
            break;
    }

    chunk->add_instruction(static_cast<uint8_t>(bc_opcode), oparg);
}

uint16_t BytecodeGenerator::resolve_jump_target(ir::BasicBlock* target) {
    auto it = block_offset_.find(target);
    if (it != block_offset_.end()) {
        return it->second;
    }
    // 如果找不到目标，返回当前位置（避免崩溃）
    std::cerr << "Warning: Jump target not found, using offset 0" << std::endl;
    return 0;
}

uint16_t BytecodeGenerator::resolve_relative_jump(uint16_t current_offset, uint16_t target_offset) {
    return target_offset - current_offset;
}

int BytecodeGenerator::compute_stack_depth(ir::IRModule* module) {
    int max_depth = 0;
    int current_depth = 0;

    for (const auto& block : module->blocks()) {
        for (const auto& instr_ptr : block->instructions()) {
            const ir::Instruction& instr = *instr_ptr;
            current_depth += compute_instruction_stack_effect(instr);
            if (current_depth > max_depth) {
                max_depth = current_depth;
            }
        }
        // 重置每个块的深度（简化处理，实际应该做数据流分析）
        current_depth = 0;
    }

    return max_depth > 0 ? max_depth : 2;  // 最小栈深度为 2
}

int BytecodeGenerator::compute_instruction_stack_effect(const ir::Instruction& instr) {
    switch (instr.opcode()) {
        // 加载指令：+1
        case ir::IROpcode::LOAD_CONST:
        case ir::IROpcode::LOAD_FAST:
        case ir::IROpcode::LOAD_GLOBAL:
        case ir::IROpcode::LOAD_NAME:
        case ir::IROpcode::LOAD_ATTR:
        case ir::IROpcode::LOAD_SUBSCR:
        case ir::IROpcode::LOAD_CLOSURE:
        case ir::IROpcode::LOAD_DEREF:
            return 1;

        // 存储指令：-1
        case ir::IROpcode::STORE_FAST:
        case ir::IROpcode::STORE_GLOBAL:
        case ir::IROpcode::STORE_NAME:
        case ir::IROpcode::STORE_ATTR:
        case ir::IROpcode::STORE_SUBSCR:
        case ir::IROpcode::STORE_DEREF:
            return -1;

        // 二元运算：-2 + 1 = -1
        case ir::IROpcode::BINARY_ADD:
        case ir::IROpcode::BINARY_SUBTRACT:
        case ir::IROpcode::BINARY_MULTIPLY:
        case ir::IROpcode::BINARY_TRUE_DIVIDE:
        case ir::IROpcode::BINARY_FLOOR_DIVIDE:
        case ir::IROpcode::BINARY_MODULO:
        case ir::IROpcode::BINARY_POWER:
        case ir::IROpcode::BINARY_LSHIFT:
        case ir::IROpcode::BINARY_RSHIFT:
        case ir::IROpcode::BINARY_AND:
        case ir::IROpcode::BINARY_OR:
        case ir::IROpcode::BINARY_XOR:
            return -1;

        // 一元运算：-1 + 1 = 0
        case ir::IROpcode::UNARY_POSITIVE:
        case ir::IROpcode::UNARY_NEGATIVE:
        case ir::IROpcode::UNARY_NOT:
        case ir::IROpcode::UNARY_INVERT:
            return 0;

        // 比较运算：-2 + 1 = -1
        case ir::IROpcode::COMPARE_EQ:
        case ir::IROpcode::COMPARE_NE:
        case ir::IROpcode::COMPARE_LT:
        case ir::IROpcode::COMPARE_LE:
        case ir::IROpcode::COMPARE_GT:
        case ir::IROpcode::COMPARE_GE:
        case ir::IROpcode::COMPARE_IS:
        case ir::IROpcode::COMPARE_IS_NOT:
        case ir::IROpcode::COMPARE_IN:
        case ir::IROpcode::COMPARE_NOT_IN:
            return -1;

        // 返回：-1
        case ir::IROpcode::RETURN_VALUE:
            return -1;

        // 函数调用：-(1+n) + 1 = -n
        case ir::IROpcode::CALL_FUNCTION:
            return -static_cast<int>(instr.operand_count() > 0 ? instr.operand_count() - 1 : 0);

        // 构建元组/列表/集合：-n + 1 = 1-n
        case ir::IROpcode::BUILD_TUPLE:
        case ir::IROpcode::BUILD_LIST:
        case ir::IROpcode::BUILD_SET:
            return 1 - static_cast<int>(instr.operand_count());

        // 构建字典：-2n + 1 = 1-2n
        case ir::IROpcode::BUILD_MAP:
            return 1 - 2 * static_cast<int>(instr.operand_count() / 2);

        // 解包：-1 + n = n-1
        case ir::IROpcode::UNPACK_SEQUENCE:
            return static_cast<int>(std::stoul(instr.metadata("count"))) - 1;

        // 栈操作
        case ir::IROpcode::POP_TOP:
            return -1;
        case ir::IROpcode::DUP_TOP:
            return 1;
        case ir::IROpcode::DUP_TOP_TWO:
            return 2;
        case ir::IROpcode::ROT_TWO:
        case ir::IROpcode::ROT_THREE:
        case ir::IROpcode::ROT_FOUR:
            return 0;

        // 迭代器
        case ir::IROpcode::GET_ITER:
            return 0;  // 替换栈顶
        case ir::IROpcode::FOR_ITER:
            return 1;  // 成功时压入下一个值

        // 跳转：无影响
        case ir::IROpcode::JUMP:
        case ir::IROpcode::JUMP_IF_TRUE:
        case ir::IROpcode::JUMP_IF_FALSE:
        case ir::IROpcode::JUMP_IF_TRUE_OR_POP:
        case ir::IROpcode::JUMP_IF_FALSE_OR_POP:
            return 0;

        // 删除：无影响
        case ir::IROpcode::DELETE_FAST:
        case ir::IROpcode::DELETE_GLOBAL:
        case ir::IROpcode::DELETE_NAME:
        case ir::IROpcode::DELETE_ATTR:
        case ir::IROpcode::DELETE_SUBSCR:
        case ir::IROpcode::DELETE_DEREF:
            return 0;

        // 逻辑运算
        case ir::IROpcode::LOGICAL_AND:
        case ir::IROpcode::LOGICAL_OR:
            return -1;

        // 导入
        case ir::IROpcode::IMPORT_NAME:
            return 0;
        case ir::IROpcode::IMPORT_FROM:
            return 1;
        case ir::IROpcode::IMPORT_STAR:
            return -1;

        // 异步
        case ir::IROpcode::GET_AWAITABLE:
            return 0;
        case ir::IROpcode::SETUP_ASYNC_WITH:
            return 0;
        case ir::IROpcode::BEFORE_ASYNC_WITH:
            return 1;
        case ir::IROpcode::END_ASYNC_FOR:
            return -1;

        // 异常处理
        case ir::IROpcode::SETUP_EXCEPT:
        case ir::IROpcode::SETUP_FINALLY:
        case ir::IROpcode::SETUP_WITH:
            return 0;
        case ir::IROpcode::END_FINALLY:
            return -1;
        case ir::IROpcode::POP_FINALLY:
            return 0;
        case ir::IROpcode::BEGIN_FINALLY:
            return 0;
        case ir::IROpcode::CALL_FINALLY:
            return 1;
        case ir::IROpcode::RAISE_VARARGS:
            return -static_cast<int>(instr.operand_count());

        // 生成器
        case ir::IROpcode::YIELD_VALUE:
            return 0;
        case ir::IROpcode::YIELD_FROM:
            return -1;

        // 格式化
        case ir::IROpcode::FORMAT_VALUE:
            return 0;

        // SSA 特有（已消除）
        case ir::IROpcode::PHI:
        case ir::IROpcode::COPY:
            return 0;

        // 循环控制
        case ir::IROpcode::BREAK_LOOP:
        case ir::IROpcode::CONTINUE_LOOP:
            return 0;

        default:
            return 0;
    }
}

void BytecodeGenerator::set_optimize_level(int level) {
    optimize_level_ = level;
}

void BytecodeGenerator::set_debug_mode(bool debug) {
    debug_mode_ = debug;
}

// ============================================================================
// 具体指令生成方法（简化版，直接使用 generate_from_instruction）
// ============================================================================

void BytecodeGenerator::generate_load_const(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_load_fast(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_load_global(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_load_attr(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_store_fast(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_store_attr(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_binary_op(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_unary_op(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_compare(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_jump(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_call(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_return(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_phi(const ir::Instruction& instr, Chunk* chunk) {
    // PHI 指令不生成字节码
    (void)instr;
    (void)chunk;
}

void BytecodeGenerator::generate_build(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_load_subscript(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_get_iter(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_for_iter(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_unpack(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

uint32_t BytecodeGenerator::get_variable_index(const ir::Value& value, Chunk* chunk) {
    if (!chunk) return 0;
    std::string var_name = value.name();
    return chunk->add_varname(var_name);
}

void BytecodeGenerator::update_lnotab(Chunk* chunk, uint16_t offset, uint16_t line) {
    if (!chunk) return;
    chunk->lnotab.push_back({offset, line});
}

void BytecodeGenerator::compute_exception_handlers(ir::IRModule* module, Chunk* chunk) {
    if (!module || !chunk) return;
    
    for (const auto& block : module->blocks()) {
        for (const auto& instr_ptr : block->instructions()) {
            const ir::Instruction& instr = *instr_ptr;
            if (instr.opcode() == ir::IROpcode::SETUP_EXCEPT ||
                instr.opcode() == ir::IROpcode::SETUP_FINALLY ||
                instr.opcode() == ir::IROpcode::SETUP_WITH) {
                ir::BasicBlock* target = instr.target();
                if (target) {
                    ExceptionHandler handler;
                    handler.start = block_offset_[block.get()];
                    handler.target = resolve_jump_target(target);
                    handler.end = 0;  // 需要后续计算
                    handler.stack_depth = 0;
                    
                    if (instr.opcode() == ir::IROpcode::SETUP_EXCEPT) {
                        handler.type = ExceptionHandling::Except;
                    } else if (instr.opcode() == ir::IROpcode::SETUP_FINALLY) {
                        handler.type = ExceptionHandling::Finally;
                    } else {
                        handler.type = ExceptionHandling::Cleanup;
                    }
                    chunk->exception_handlers.push_back(handler);
                }
            }
        }
    }
}

// ============================================================================
// 其他指令生成方法（简化实现）
// ============================================================================

void BytecodeGenerator::generate_load_name(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_load_closure(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_load_deref(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_store_global(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_store_name(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_store_subscript(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_store_deref(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_delete_fast(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_delete_global(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_delete_name(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_delete_attr(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_delete_subscript(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_delete_deref(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_logical_op(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_cond_jump(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_break_loop(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_continue_loop(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_make_function(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_load_method(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_call_method(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_build_slice(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_get_aiter(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_get_anext(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_copy(const ir::Instruction& instr, Chunk* chunk) {
    // COPY 不生成字节码
    (void)instr;
    (void)chunk;
}

void BytecodeGenerator::generate_pop_top(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_dup_top(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_rot_two(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_rot_three(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_rot_four(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_dup_top_two(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_import_name(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_import_from(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_import_star(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_get_awaitable(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_setup_async_with(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_before_async_with(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_end_async_for(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_raise(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_setup_except(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_setup_finally(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_setup_with(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_end_finally(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_pop_finally(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_begin_finally(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_call_finally(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_yield_value(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_yield_from(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_send(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

void BytecodeGenerator::generate_format_value(const ir::Instruction& instr, Chunk* chunk) {
    generate_from_instruction(instr, chunk);
}

} // namespace bytecode
} // namespace csgo