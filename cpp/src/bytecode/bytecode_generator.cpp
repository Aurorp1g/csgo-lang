/**
 * @file bytecode_generator.cpp
 * @brief CSGO 编程语言字节码生成器实现文件
 *
 * @author CSGO Language Team
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

static bool is_extended_arg(uint8_t opcode) {
    return opcode == static_cast<uint8_t>(BytecodeOpcode::EXTENDED_ARG);
}

static bool has_arg(uint8_t opcode) {
    return opcode >= 144 ||
           (opcode >= 90 && opcode <= 93) ||
           (opcode >= 100 && opcode <= 143) ||
           (opcode >= 1 && opcode <= 23);
}

static size_t instr_size(uint32_t oparg) {
    if (oparg > 0xFFFFFF) return 4;
    if (oparg > 0xFFFF) return 4;
    if (oparg > 0xFF) return 3;
    return 1;
}

static void write_arg(std::vector<uint8_t>& code, uint32_t oparg, size_t size) {
    switch (size) {
        case 1:
            code.push_back(static_cast<uint8_t>(oparg));
            break;
        case 2:
            code.push_back(static_cast<uint8_t>(oparg & 0xFF));
            code.push_back(static_cast<uint8_t>((oparg >> 8) & 0xFF));
            break;
        case 3:
            code.push_back(static_cast<uint8_t>(oparg & 0xFF));
            code.push_back(static_cast<uint8_t>((oparg >> 8) & 0xFF));
            code.push_back(static_cast<uint8_t>((oparg >> 16) & 0xFF));
            break;
        case 4:
            if (oparg > 0xFFFFFF) {
                code.push_back(static_cast<uint8_t>(BytecodeOpcode::EXTENDED_ARG));
                code.push_back(static_cast<uint8_t>((oparg >> 16) & 0xFF));
                code.push_back(static_cast<uint8_t>((oparg >> 24) & 0xFF));
                code.push_back(static_cast<uint8_t>(oparg & 0xFF));
            } else if (oparg > 0xFFFF) {
                code.push_back(static_cast<uint8_t>(BytecodeOpcode::EXTENDED_ARG));
                code.push_back(static_cast<uint8_t>((oparg >> 8) & 0xFF));
                code.push_back(static_cast<uint8_t>((oparg >> 16) & 0xFF));
                code.push_back(static_cast<uint8_t>(oparg & 0xFF));
            } else {
                code.push_back(static_cast<uint8_t>(BytecodeOpcode::EXTENDED_ARG));
                code.push_back(static_cast<uint8_t>(oparg & 0xFF));
                code.push_back(static_cast<uint8_t>((oparg >> 8) & 0xFF));
                code.push_back(static_cast<uint8_t>(0));
            }
            break;
    }
}

// ============================================================================
// Chunk 类实现
// ============================================================================

std::vector<uint8_t> Chunk::serialize() const {
    std::vector<uint8_t> data;

    // 序列化魔数（CSGO bytecode v1）
    data.push_back('C');
    data.push_back('G');
    data.push_back('O');
    data.push_back(1);

    // 元数据
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
    uint32_t code_size = 0;
    for (const auto& instr : code) {
        data.push_back(instr.opcode);
        if (has_arg(instr.opcode)) {
            write_arg(data, instr.oparg, instr_size(instr.oparg));
        }
        code_size += 1 + (has_arg(instr.opcode) ? instr_size(instr.oparg) : 0);
    }

    // 序列化常量
    uint32_t consts_count = consts.size();
    data.push_back(static_cast<uint8_t>(consts_count & 0xFF));
    data.push_back(static_cast<uint8_t>((consts_count >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((consts_count >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((consts_count >> 24) & 0xFF));

    for (const auto& constant : consts) {
        std::string const_str = constant.to_string();  // 只调用一次，保存到局部变量
        uint32_t const_size = const_str.size();
        data.push_back(static_cast<uint8_t>(const_size & 0xFF));
        data.push_back(static_cast<uint8_t>((const_size >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((const_size >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((const_size >> 24) & 0xFF));
        data.insert(data.end(), const_str.begin(), const_str.end());  // 使用同一个对象
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

// ============================================================================
// BytecodeGenerator 类实现
// ============================================================================

BytecodeGenerator::BytecodeGenerator() {
    init_opcode_map();
}

void BytecodeGenerator::init_opcode_map() {
    opcode_map_[ir::IROpcode::LOAD_CONST] = BytecodeOpcode::LOAD_CONST;
    opcode_map_[ir::IROpcode::LOAD_FAST] = BytecodeOpcode::LOAD_FAST;
    opcode_map_[ir::IROpcode::STORE_FAST] = BytecodeOpcode::STORE_FAST;
    opcode_map_[ir::IROpcode::LOAD_GLOBAL] = BytecodeOpcode::LOAD_GLOBAL;
    opcode_map_[ir::IROpcode::STORE_GLOBAL] = BytecodeOpcode::STORE_GLOBAL;
    opcode_map_[ir::IROpcode::LOAD_NAME] = BytecodeOpcode::LOAD_NAME;
    opcode_map_[ir::IROpcode::STORE_NAME] = BytecodeOpcode::STORE_NAME;
    opcode_map_[ir::IROpcode::DELETE_FAST] = BytecodeOpcode::DELETE_FAST;
    opcode_map_[ir::IROpcode::DELETE_GLOBAL] = BytecodeOpcode::DELETE_GLOBAL;
    opcode_map_[ir::IROpcode::DELETE_NAME] = BytecodeOpcode::DELETE_NAME;

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

    opcode_map_[ir::IROpcode::UNARY_POSITIVE] = BytecodeOpcode::UNARY_POSITIVE;
    opcode_map_[ir::IROpcode::UNARY_NEGATIVE] = BytecodeOpcode::UNARY_NEGATIVE;
    opcode_map_[ir::IROpcode::UNARY_NOT] = BytecodeOpcode::UNARY_NOT;
    opcode_map_[ir::IROpcode::UNARY_INVERT] = BytecodeOpcode::UNARY_INVERT;

    opcode_map_[ir::IROpcode::COMPARE_EQ] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_NE] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_LT] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_LE] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_GT] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_GE] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_IS] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_IS_NOT] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_IN] = BytecodeOpcode::COMPARE_OP;
    opcode_map_[ir::IROpcode::COMPARE_NOT_IN] = BytecodeOpcode::COMPARE_OP;

    opcode_map_[ir::IROpcode::JUMP] = BytecodeOpcode::JUMP_ABSOLUTE;
    opcode_map_[ir::IROpcode::JUMP_IF_TRUE] = BytecodeOpcode::POP_JUMP_IF_TRUE;
    opcode_map_[ir::IROpcode::JUMP_IF_FALSE] = BytecodeOpcode::POP_JUMP_IF_FALSE;
    opcode_map_[ir::IROpcode::JUMP_IF_TRUE_OR_POP] = BytecodeOpcode::JUMP_IF_TRUE_OR_POP;
    opcode_map_[ir::IROpcode::JUMP_IF_FALSE_OR_POP] = BytecodeOpcode::JUMP_IF_FALSE_OR_POP;

    opcode_map_[ir::IROpcode::RETURN_VALUE] = BytecodeOpcode::RETURN_VALUE;
    opcode_map_[ir::IROpcode::RAISE_VARARGS] = BytecodeOpcode::RAISE_VARARGS;

    opcode_map_[ir::IROpcode::CALL_FUNCTION] = BytecodeOpcode::CALL_FUNCTION;
    opcode_map_[ir::IROpcode::CALL_FUNCTION_KW] = BytecodeOpcode::CALL_FUNCTION_KW;
    opcode_map_[ir::IROpcode::CALL_FUNCTION_EX] = BytecodeOpcode::CALL_FUNCTION_EX;
    opcode_map_[ir::IROpcode::CALL_METHOD] = BytecodeOpcode::CALL_METHOD;
    opcode_map_[ir::IROpcode::MAKE_FUNCTION] = BytecodeOpcode::MAKE_FUNCTION;
    opcode_map_[ir::IROpcode::LOAD_METHOD] = BytecodeOpcode::LOAD_METHOD;

    opcode_map_[ir::IROpcode::BUILD_TUPLE] = BytecodeOpcode::BUILD_TUPLE;
    opcode_map_[ir::IROpcode::BUILD_LIST] = BytecodeOpcode::BUILD_LIST;
    opcode_map_[ir::IROpcode::BUILD_SET] = BytecodeOpcode::BUILD_SET;
    opcode_map_[ir::IROpcode::BUILD_MAP] = BytecodeOpcode::BUILD_MAP;
    opcode_map_[ir::IROpcode::BUILD_STRING] = BytecodeOpcode::BUILD_STRING;
    opcode_map_[ir::IROpcode::BUILD_SLICE] = BytecodeOpcode::BUILD_SLICE;

    opcode_map_[ir::IROpcode::UNPACK_SEQUENCE] = BytecodeOpcode::UNPACK_SEQUENCE;

    opcode_map_[ir::IROpcode::GET_ITER] = BytecodeOpcode::GET_ITER;
    opcode_map_[ir::IROpcode::FOR_ITER] = BytecodeOpcode::FOR_ITER;

    opcode_map_[ir::IROpcode::LOAD_ATTR] = BytecodeOpcode::LOAD_ATTR;
    opcode_map_[ir::IROpcode::STORE_ATTR] = BytecodeOpcode::STORE_ATTR;
    opcode_map_[ir::IROpcode::DELETE_ATTR] = BytecodeOpcode::DELETE_ATTR;

    opcode_map_[ir::IROpcode::LOAD_SUBSCR] = BytecodeOpcode::LOAD_SUBSCR;
    opcode_map_[ir::IROpcode::STORE_SUBSCR] = BytecodeOpcode::STORE_SUBSCR;
    opcode_map_[ir::IROpcode::DELETE_SUBSCR] = BytecodeOpcode::DELETE_SUBSCR;

    opcode_map_[ir::IROpcode::LOAD_CLOSURE] = BytecodeOpcode::LOAD_CLOSURE;
    opcode_map_[ir::IROpcode::LOAD_DEREF] = BytecodeOpcode::LOAD_DEREF;
    opcode_map_[ir::IROpcode::STORE_DEREF] = BytecodeOpcode::STORE_DEREF;
    opcode_map_[ir::IROpcode::DELETE_DEREF] = BytecodeOpcode::DELETE_DEREF;
    opcode_map_[ir::IROpcode::LOAD_CLASSDEREF] = BytecodeOpcode::LOAD_CLASSDEREF;

    opcode_map_[ir::IROpcode::POP_TOP] = BytecodeOpcode::POP_TOP;
    opcode_map_[ir::IROpcode::ROT_TWO] = BytecodeOpcode::ROT_TWO;
    opcode_map_[ir::IROpcode::ROT_THREE] = BytecodeOpcode::ROT_THREE;
    opcode_map_[ir::IROpcode::ROT_FOUR] = BytecodeOpcode::ROT_FOUR;
    opcode_map_[ir::IROpcode::DUP_TOP] = BytecodeOpcode::DUP_TOP;
    opcode_map_[ir::IROpcode::DUP_TOP_TWO] = BytecodeOpcode::DUP_TOP_TWO;

    opcode_map_[ir::IROpcode::BREAK_LOOP] = BytecodeOpcode::BREAK_LOOP;
    opcode_map_[ir::IROpcode::CONTINUE_LOOP] = BytecodeOpcode::CONTINUE_LOOP;

    opcode_map_[ir::IROpcode::YIELD_VALUE] = BytecodeOpcode::YIELD_VALUE;
    opcode_map_[ir::IROpcode::YIELD_FROM] = BytecodeOpcode::YIELD_FROM;

    opcode_map_[ir::IROpcode::IMPORT_NAME] = BytecodeOpcode::IMPORT_NAME;
    opcode_map_[ir::IROpcode::IMPORT_FROM] = BytecodeOpcode::IMPORT_FROM;
    opcode_map_[ir::IROpcode::IMPORT_STAR] = BytecodeOpcode::IMPORT_STAR;

    opcode_map_[ir::IROpcode::PHI] = BytecodeOpcode::NOP;
}

BytecodeOpcode BytecodeGenerator::ir_opcode_to_bytecode(ir::IROpcode opcode) const {
    auto it = opcode_map_.find(opcode);
    if (it != opcode_map_.end()) {
        return it->second;
    }
    return BytecodeOpcode::NOP;
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

    // 收集基本块并计算拓扑顺序
    block_order_.clear();
    block_offset_.clear();

    for (const auto& block : module->blocks()) {
        block_order_.push_back(block.get());
    }

    // 计算每个基本块的偏移量
    uint16_t current_offset = 0;
    for (auto* block : block_order_) {
        block_offset_[block] = current_offset;  // 先记录块起始位置
        
        for (const auto& instr_ptr : block->instructions()) {
            const ir::Instruction& instr = *instr_ptr;
            uint8_t bc_opcode = static_cast<uint8_t>(ir_opcode_to_bytecode(instr.opcode()));

            current_offset += 1;  // opcode
            if (has_arg(bc_opcode)) {
                current_offset += instr_size(0);
            }
        }
    }

    // 生成字节码
    for (auto* block : block_order_) {
        for (const auto& instr_ptr : block->instructions()) {
            const ir::Instruction& instr = *instr_ptr;
            generate_from_instruction(*instr_ptr, chunk.get());
        }
    }

    // 计算栈深度
    chunk->stacksize = compute_stack_depth(module);

    // 复制常量、名称、变量名
    chunk->consts.assign(module->constants().begin(), module->constants().end());
    chunk->names = module->names();
    chunk->varnames = module->varnames();
    chunk->nlocals = chunk->varnames.size();

    // 计算指令计数
    chunk->instruction_count = chunk->code.size();

    return chunk;
}

void BytecodeGenerator::generate_from_basic_block(ir::BasicBlock* block, Chunk* chunk) {
    if (!block || !chunk) return;

    for (const auto& instr_ptr : block->instructions()) {
        generate_from_instruction(*instr_ptr, chunk);
    }
}

void BytecodeGenerator::generate_from_instruction(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;

    BytecodeOpcode bc_opcode = ir_opcode_to_bytecode(instr.opcode());
    uint32_t oparg = 0;

    switch (instr.opcode()) {
        case ir::IROpcode::LOAD_CONST:
            if (instr.operand_count() > 0) {
                oparg = chunk->add_constant(instr.operand(0));
            }
            break;

        case ir::IROpcode::LOAD_FAST:
        case ir::IROpcode::STORE_FAST:
            if (instr.operand_count() > 0) {
                const csgo::ir::Value& var = instr.operand(0);
                oparg = chunk->add_varname(var.name());
            }
            break;

        case ir::IROpcode::LOAD_GLOBAL:
        case ir::IROpcode::STORE_GLOBAL:
            if (instr.operand_count() > 0) {
                const csgo::ir::Value& var = instr.operand(0);
                oparg = chunk->add_name(var.name());
            }
            break;

        case ir::IROpcode::LOAD_ATTR:
            oparg = chunk->add_name(instr.metadata("attr"));
            break;

        case ir::IROpcode::STORE_ATTR:
            if (instr.operand_count() > 1) {
                oparg = chunk->add_name(instr.metadata("attr"));
            }
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
            oparg = static_cast<uint32_t>(instr.opcode()) - static_cast<uint32_t>(ir::IROpcode::BINARY_ADD) + 22;
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
            oparg = instr.operand_count() - 1;
            break;

        case ir::IROpcode::CALL_FUNCTION_KW:
            oparg = instr.operand_count() - 1;
            break;

        case ir::IROpcode::UNPACK_SEQUENCE:
            oparg = std::stoul(instr.metadata("count"));
            break;

        case ir::IROpcode::GET_ITER:
        case ir::IROpcode::FOR_ITER:
        case ir::IROpcode::GET_AITER:
        case ir::IROpcode::GET_ANEXT:
            break;

        case ir::IROpcode::RETURN_VALUE:
            if (instr.operand_count() > 0) {
                const csgo::ir::Value& ret_val = instr.operand(0);
                if (ret_val.is_constant()) {
                    if (ret_val.name() == "None") {
                        oparg = 0;
                    }
                }
            }
            break;

        default:
            break;
    }

    chunk->add_instruction(static_cast<uint8_t>(bc_opcode), oparg);
}

uint16_t BytecodeGenerator::resolve_jump_target(ir::BasicBlock* target) {
    auto it = block_offset_.find(target);
    if (it != block_offset_.end()) {
        return it->second;
    }
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
            max_depth = std::max(max_depth, current_depth);
        }
        current_depth = 0;
    }

    return max_depth;
}

int BytecodeGenerator::compute_instruction_stack_effect(const ir::Instruction& instr) {
    switch (instr.opcode()) {
        case ir::IROpcode::LOAD_CONST:
        case ir::IROpcode::LOAD_FAST:
        case ir::IROpcode::LOAD_GLOBAL:
        case ir::IROpcode::LOAD_NAME:
        case ir::IROpcode::LOAD_ATTR:
        case ir::IROpcode::LOAD_SUBSCR:
        case ir::IROpcode::LOAD_CLOSURE:
        case ir::IROpcode::LOAD_DEREF:
            return 1;

        case ir::IROpcode::STORE_FAST:
        case ir::IROpcode::STORE_GLOBAL:
        case ir::IROpcode::STORE_NAME:
        case ir::IROpcode::STORE_ATTR:
        case ir::IROpcode::STORE_SUBSCR:
        case ir::IROpcode::STORE_DEREF:
            return -1;

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

        case ir::IROpcode::UNARY_POSITIVE:
        case ir::IROpcode::UNARY_NEGATIVE:
        case ir::IROpcode::UNARY_NOT:
        case ir::IROpcode::UNARY_INVERT:
            return 0;

        case ir::IROpcode::RETURN_VALUE:
            return -1;

        case ir::IROpcode::CALL_FUNCTION:
            return -static_cast<int>(instr.operand_count()) + 1;

        case ir::IROpcode::BUILD_TUPLE:
        case ir::IROpcode::BUILD_LIST:
        case ir::IROpcode::BUILD_SET:
            return 1 - static_cast<int>(instr.operand_count());

        case ir::IROpcode::BUILD_MAP:
            return 1 - 2 * static_cast<int>(instr.operand_count() / 2);

        case ir::IROpcode::UNPACK_SEQUENCE:
            return static_cast<int>(instr.operand_count()) - 1;

        case ir::IROpcode::POP_TOP:
            return -1;

        case ir::IROpcode::DUP_TOP:
            return 1;

        case ir::IROpcode::ROT_TWO:
        case ir::IROpcode::ROT_THREE:
        case ir::IROpcode::ROT_FOUR:
            return 0;

        case ir::IROpcode::GET_ITER:
            return 0;

        case ir::IROpcode::FOR_ITER:
            return 1;

        case ir::IROpcode::JUMP:
        case ir::IROpcode::JUMP_IF_TRUE:
        case ir::IROpcode::JUMP_IF_FALSE:
        case ir::IROpcode::JUMP_IF_TRUE_OR_POP:
        case ir::IROpcode::JUMP_IF_FALSE_OR_POP:
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

void BytecodeGenerator::generate_load_const(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    uint32_t oparg = chunk->add_constant(instr.result());
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::LOAD_CONST), oparg);
}

void BytecodeGenerator::generate_load_fast(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk || instr.operand_count() == 0) return;
    const csgo::ir::Value& var = instr.operand(0);
    uint32_t oparg = chunk->add_varname(var.name());
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::LOAD_FAST), oparg);
}

void BytecodeGenerator::generate_load_global(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk || instr.operand_count() == 0) return;
    const csgo::ir::Value& var = instr.operand(0);
    uint32_t oparg = chunk->add_name(var.name());
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::LOAD_GLOBAL), oparg);
}

void BytecodeGenerator::generate_load_attr(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    uint32_t oparg = chunk->add_name(instr.metadata("attr"));
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::LOAD_ATTR), oparg);
}

void BytecodeGenerator::generate_store_fast(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk || instr.operand_count() == 0) return;
    const csgo::ir::Value& var = instr.operand(0);
    uint32_t oparg = chunk->add_varname(var.name());
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::STORE_FAST), oparg);
}

void BytecodeGenerator::generate_store_attr(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk || instr.operand_count() < 2) return;
    uint32_t oparg = chunk->add_name(instr.metadata("attr"));
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::STORE_ATTR), oparg);
}

void BytecodeGenerator::generate_binary_op(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    uint32_t oparg = static_cast<uint32_t>(instr.opcode()) - static_cast<uint32_t>(ir::IROpcode::BINARY_ADD) + 22;
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::BINARY_OP), oparg);
}

void BytecodeGenerator::generate_unary_op(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    uint32_t oparg = static_cast<uint32_t>(instr.opcode()) - static_cast<uint32_t>(ir::IROpcode::UNARY_POSITIVE) + 35;
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::UNARY_POSITIVE), oparg);
}

void BytecodeGenerator::generate_compare(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
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
    uint32_t oparg = 0;
    auto it = compare_map.find(instr.opcode());
    if (it != compare_map.end()) {
        oparg = it->second;
    }
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::COMPARE_OP), oparg);
}

void BytecodeGenerator::generate_jump(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    ir::BasicBlock* target = instr.target();
    uint32_t oparg = 0;
    if (target) {
        oparg = resolve_jump_target(target);
    }
    chunk->add_instruction(static_cast<uint8_t>(ir_opcode_to_bytecode(instr.opcode())), oparg);
}

void BytecodeGenerator::generate_call(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    uint32_t oparg = 0;
    switch (instr.opcode()) {
        case ir::IROpcode::CALL_FUNCTION:
            oparg = instr.operand_count() > 0 ? instr.operand_count() - 1 : 0;
            break;
        case ir::IROpcode::CALL_FUNCTION_KW:
            oparg = instr.operand_count() > 0 ? instr.operand_count() - 1 : 0;
            break;
        case ir::IROpcode::CALL_FUNCTION_EX:
            oparg = 0;
            break;
        default:
            break;
    }
    BytecodeOpcode bc_opcode = ir_opcode_to_bytecode(instr.opcode());
    chunk->add_instruction(static_cast<uint8_t>(bc_opcode), oparg);
}

void BytecodeGenerator::generate_return(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::RETURN_VALUE), 0);
}

void BytecodeGenerator::generate_phi(const ir::Instruction& instr, Chunk* chunk) {
    (void)instr;
    (void)chunk;
}

void BytecodeGenerator::generate_build(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    uint32_t oparg = 0;
    switch (instr.opcode()) {
        case ir::IROpcode::BUILD_TUPLE:
        case ir::IROpcode::BUILD_LIST:
        case ir::IROpcode::BUILD_SET:
            oparg = instr.operand_count();
            break;
        case ir::IROpcode::BUILD_MAP:
            oparg = instr.operand_count() / 2;
            break;
        case ir::IROpcode::BUILD_STRING:
        case ir::IROpcode::BUILD_SLICE:
            oparg = instr.operand_count();
            break;
        default:
            break;
    }
    BytecodeOpcode bc_opcode = ir_opcode_to_bytecode(instr.opcode());
    chunk->add_instruction(static_cast<uint8_t>(bc_opcode), oparg);
}

void BytecodeGenerator::generate_subscript(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    BytecodeOpcode bc_opcode = ir_opcode_to_bytecode(instr.opcode());
    chunk->add_instruction(static_cast<uint8_t>(bc_opcode), 0);
}

void BytecodeGenerator::generate_get_iter(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    BytecodeOpcode bc_opcode = ir_opcode_to_bytecode(instr.opcode());
    chunk->add_instruction(static_cast<uint8_t>(bc_opcode), 0);
}

void BytecodeGenerator::generate_for_iter(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    ir::BasicBlock* target = instr.target();
    uint32_t oparg = 0;
    if (target) {
        oparg = resolve_jump_target(target);
    }
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::FOR_ITER), oparg);
}

void BytecodeGenerator::generate_unpack(const ir::Instruction& instr, Chunk* chunk) {
    if (!chunk) return;
    uint32_t oparg = 0;
    try {
        instr.metadata("count");
        // 如果metadata("count")不抛出异常，则说明存在该元数据
        oparg = std::stoul(instr.metadata("count"));
    } catch (const std::out_of_range&) {
        // 如果metadata("count")不存在会抛出异常，这时使用operand数量
        oparg = instr.operand_count();
    }
    chunk->add_instruction(static_cast<uint8_t>(BytecodeOpcode::UNPACK_SEQUENCE), oparg);
}

uint32_t BytecodeGenerator::get_variable_index(const ir::Value& value, Chunk* chunk) {
    if (!chunk) return 0;
    return chunk->add_varname(value.name());
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
                    handler.end = 0;
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

} // namespace bytecode
} // namespace csgo