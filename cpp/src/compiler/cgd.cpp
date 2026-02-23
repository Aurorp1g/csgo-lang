/**
 * @file cgd.cpp
 * @brief CSGO 字节码反汇编器
 *
 * 用于查看 .cgb 字节码文件的内容
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <map>

#include "bytecode/bytecode_generator.h"

using namespace csgo;

void print_usage(const char* program_name) {
    std::cout << "CSGO Bytecode Disassembler v1.0" << std::endl;
    std::cout << "Usage: " << program_name << " <input.cgb>" << std::endl;
}

std::vector<uint8_t> read_file(const std::string& filename) {
    std::string adjusted_filename = filename;
    // 替换正斜杠为反斜杠
    for (char& c : adjusted_filename) {
        if (c == '/') c = '\\';
    }
    
    std::ifstream file(adjusted_filename, std::ios::binary);
    if (!file.is_open()) {
        // 尝试原始文件名
        file.open(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file: " << filename << std::endl;
            return {};
        }
    }
    
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    return data;
}

std::string get_opcode_name(uint8_t opcode) {
    using namespace csgo::bytecode;
    
    switch (static_cast<BytecodeOpcode>(opcode)) {
        case BytecodeOpcode::NOP: return "NOP";
        case BytecodeOpcode::LOAD_CONST: return "LOAD_CONST";
        case BytecodeOpcode::LOAD_FAST: return "LOAD_FAST";
        case BytecodeOpcode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case BytecodeOpcode::LOAD_NAME: return "LOAD_NAME";
        case BytecodeOpcode::LOAD_ATTR: return "LOAD_ATTR";
        case BytecodeOpcode::LOAD_SUPER_ATTR: return "LOAD_SUPER_ATTR";
        case BytecodeOpcode::LOAD_DEREF: return "LOAD_DEREF";
        case BytecodeOpcode::LOAD_CLOSURE: return "LOAD_CLOSURE";
        case BytecodeOpcode::LOAD_SUBSCR: return "LOAD_SUBSCR";
        case BytecodeOpcode::STORE_FAST: return "STORE_FAST";
        case BytecodeOpcode::STORE_GLOBAL: return "STORE_GLOBAL";
        case BytecodeOpcode::STORE_NAME: return "STORE_NAME";
        case BytecodeOpcode::STORE_ATTR: return "STORE_ATTR";
        case BytecodeOpcode::STORE_DEREF: return "STORE_DEREF";
        case BytecodeOpcode::STORE_SUBSCR: return "STORE_SUBSCR";
        case BytecodeOpcode::DELETE_FAST: return "DELETE_FAST";
        case BytecodeOpcode::DELETE_GLOBAL: return "DELETE_GLOBAL";
        case BytecodeOpcode::DELETE_NAME: return "DELETE_NAME";
        case BytecodeOpcode::DELETE_ATTR: return "DELETE_ATTR";
        case BytecodeOpcode::DELETE_DEREF: return "DELETE_DEREF";
        case BytecodeOpcode::DELETE_SUBSCR: return "DELETE_SUBSCR";
        case BytecodeOpcode::BINARY_ADD: return "BINARY_ADD";
        case BytecodeOpcode::BINARY_SUBTRACT: return "BINARY_SUBTRACT";
        case BytecodeOpcode::BINARY_MULTIPLY: return "BINARY_MULTIPLY";
        case BytecodeOpcode::BINARY_TRUE_DIVIDE: return "BINARY_TRUE_DIVIDE";
        case BytecodeOpcode::BINARY_FLOOR_DIVIDE: return "BINARY_FLOOR_DIVIDE";
        case BytecodeOpcode::BINARY_MODULO: return "BINARY_MODULO";
        case BytecodeOpcode::BINARY_POWER: return "BINARY_POWER";
        case BytecodeOpcode::BINARY_LSHIFT: return "BINARY_LSHIFT";
        case BytecodeOpcode::BINARY_RSHIFT: return "BINARY_RSHIFT";
        case BytecodeOpcode::BINARY_AND: return "BINARY_AND";
        case BytecodeOpcode::BINARY_OR: return "BINARY_OR";
        case BytecodeOpcode::BINARY_XOR: return "BINARY_XOR";
        case BytecodeOpcode::BINARY_MATRIX_MULTIPLY: return "BINARY_MATRIX_MULTIPLY";
        case BytecodeOpcode::BINARY_OP: return "BINARY_OP";
        case BytecodeOpcode::UNARY_POSITIVE: return "UNARY_POSITIVE";
        case BytecodeOpcode::UNARY_NEGATIVE: return "UNARY_NEGATIVE";
        case BytecodeOpcode::UNARY_NOT: return "UNARY_NOT";
        case BytecodeOpcode::UNARY_INVERT: return "UNARY_INVERT";
        case BytecodeOpcode::COMPARE_OP: return "COMPARE_OP";
        case BytecodeOpcode::POP_JUMP_IF_TRUE: return "POP_JUMP_IF_TRUE";
        case BytecodeOpcode::POP_JUMP_IF_FALSE: return "POP_JUMP_IF_FALSE";
        case BytecodeOpcode::JUMP_IF_TRUE_OR_POP: return "JUMP_IF_TRUE_OR_POP";
        case BytecodeOpcode::JUMP_IF_FALSE_OR_POP: return "JUMP_IF_FALSE_OR_POP";
        case BytecodeOpcode::JUMP_ABSOLUTE: return "JUMP_ABSOLUTE";
        case BytecodeOpcode::JUMP_FORWARD: return "JUMP_FORWARD";
        case BytecodeOpcode::BREAK_LOOP: return "BREAK_LOOP";
        case BytecodeOpcode::CONTINUE_LOOP: return "CONTINUE_LOOP";
        case BytecodeOpcode::RETURN_VALUE: return "RETURN_VALUE";
        case BytecodeOpcode::YIELD_VALUE: return "YIELD_VALUE";
        case BytecodeOpcode::YIELD_FROM: return "YIELD_FROM";
        case BytecodeOpcode::POP_TOP: return "POP_TOP";
        case BytecodeOpcode::ROT_TWO: return "ROT_TWO";
        case BytecodeOpcode::ROT_THREE: return "ROT_THREE";
        case BytecodeOpcode::ROT_FOUR: return "ROT_FOUR";
        case BytecodeOpcode::DUP_TOP: return "DUP_TOP";
        case BytecodeOpcode::DUP_TOP_TWO: return "DUP_TOP_TWO";
        case BytecodeOpcode::CALL_FUNCTION: return "CALL_FUNCTION";
        case BytecodeOpcode::CALL_FUNCTION_KW: return "CALL_FUNCTION_KW";
        case BytecodeOpcode::CALL_FUNCTION_EX: return "CALL_FUNCTION_EX";
        case BytecodeOpcode::CALL_METHOD: return "CALL_METHOD";
        case BytecodeOpcode::MAKE_FUNCTION: return "MAKE_FUNCTION";
        case BytecodeOpcode::LOAD_METHOD: return "LOAD_METHOD";
        case BytecodeOpcode::BUILD_TUPLE: return "BUILD_TUPLE";
        case BytecodeOpcode::BUILD_LIST: return "BUILD_LIST";
        case BytecodeOpcode::BUILD_SET: return "BUILD_SET";
        case BytecodeOpcode::BUILD_MAP: return "BUILD_MAP";
        case BytecodeOpcode::BUILD_STRING: return "BUILD_STRING";
        case BytecodeOpcode::BUILD_TUPLE_UNPACK: return "BUILD_TUPLE_UNPACK";
        case BytecodeOpcode::BUILD_LIST_UNPACK: return "BUILD_LIST_UNPACK";
        case BytecodeOpcode::BUILD_SET_UNPACK: return "BUILD_SET_UNPACK";
        case BytecodeOpcode::BUILD_MAP_UNPACK: return "BUILD_MAP_UNPACK";
        case BytecodeOpcode::BUILD_MAP_UNPACK_WITH_CALL: return "BUILD_MAP_UNPACK_WITH_CALL";
        case BytecodeOpcode::BUILD_CONST_KEY_MAP: return "BUILD_CONST_KEY_MAP";
        case BytecodeOpcode::BUILD_SLICE: return "BUILD_SLICE";
        case BytecodeOpcode::UNPACK_SEQUENCE: return "UNPACK_SEQUENCE";
        case BytecodeOpcode::UNPACK_EX: return "UNPACK_EX";
        case BytecodeOpcode::GET_ITER: return "GET_ITER";
        case BytecodeOpcode::FOR_ITER: return "FOR_ITER";
        case BytecodeOpcode::GET_YIELD_FROM_ITER: return "GET_YIELD_FROM_ITER";
        case BytecodeOpcode::GET_AWAITABLE: return "GET_AWAITABLE";
        case BytecodeOpcode::GET_AITER: return "GET_AITER";
        case BytecodeOpcode::GET_ANEXT: return "GET_ANEXT";
        case BytecodeOpcode::BEFORE_ASYNC_WITH: return "BEFORE_ASYNC_WITH";
        case BytecodeOpcode::SETUP_ASYNC_WITH: return "SETUP_ASYNC_WITH";
        case BytecodeOpcode::END_ASYNC_FOR: return "END_ASYNC_FOR";
        case BytecodeOpcode::SETUP_CLEANUP: return "SETUP_CLEANUP";
        case BytecodeOpcode::END_CLEANUP: return "END_CLEANUP";
        case BytecodeOpcode::RAISE_VARARGS: return "RAISE_VARARGS";
        case BytecodeOpcode::SETUP_EXCEPT: return "SETUP_EXCEPT";
        case BytecodeOpcode::SETUP_FINALLY: return "SETUP_FINALLY";
        case BytecodeOpcode::SETUP_WITH: return "SETUP_WITH";
        case BytecodeOpcode::END_FINALLY: return "END_FINALLY";
        case BytecodeOpcode::POP_FINALLY: return "POP_FINALLY";
        case BytecodeOpcode::BEGIN_FINALLY: return "BEGIN_FINALLY";
        case BytecodeOpcode::CALL_FINALLY: return "CALL_FINALLY";
        case BytecodeOpcode::WITH_CLEANUP_START: return "WITH_CLEANUP_START";
        case BytecodeOpcode::WITH_CLEANUP_FINISH: return "WITH_CLEANUP_FINISH";
        case BytecodeOpcode::FORMAT_VALUE: return "FORMAT_VALUE";
        case BytecodeOpcode::IMPORT_NAME: return "IMPORT_NAME";
        case BytecodeOpcode::IMPORT_FROM: return "IMPORT_FROM";
        case BytecodeOpcode::IMPORT_STAR: return "IMPORT_STAR";
        case BytecodeOpcode::EXTENDED_ARG: return "EXTENDED_ARG";
        case BytecodeOpcode::LOAD_CLASSDEREF: return "LOAD_CLASSDEREF";
        default: return "UNKNOWN_" + std::to_string(opcode);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_file = argv[1];
    
    // 确保路径使用反斜杠
    for (char& c : input_file) {
        if (c == '/') c = '\\';
    }
    
    std::vector<uint8_t> data = read_file(input_file);
    
    if (data.empty()) {
        return 1;
    }
    
    std::cout << "=== CSGO Bytecode Disassembler ===" << std::endl;
    std::cout << "File: " << input_file << std::endl;
    std::cout << "Size: " << data.size() << " bytes" << std::endl;
    std::cout << std::endl;
    
    // 解析魔数
    if (data.size() >= 4) {
        std::cout << "Magic: " << (char)data[0] << (char)data[1] << (char)data[2] 
                  << (int)data[3] << std::endl;
    }
    
    // 解析常量数量 - 使用更简单的方法
    size_t offset = 4; // 魔数后
    uint32_t name_len = data[offset] | (data[offset+1] << 8) | 
                        (data[offset+2] << 16) | (data[offset+3] << 24);
    std::cout << "[DEBUG] name_len = " << name_len << std::endl;
    
    // 指令从 name + flags + argcount + ... + firstlineno 之后开始
    size_t code_start = offset + 4 + name_len + 4 + 2 + 2 + 2 + 2 + 2 + 4 + 2; // firstlineno之后
    std::cout << "[DEBUG] code_start = " << code_start << std::endl;
    
    // instruction_count 在 code_start - 6 的位置
    uint32_t instruction_count = data[code_start - 6] | (data[code_start - 5] << 8) | 
                                  (data[code_start - 4] << 16) | (data[code_start - 3] << 24);
    std::cout << "[DEBUG] instruction_count at " << (code_start - 6) << " = " << instruction_count << std::endl;
    
    // 计算指令总大小
    size_t code_size = 0;
    size_t idx = code_start;
    int instr_count = 0;
    while (idx < data.size() && instr_count < (int)instruction_count) {
        uint8_t opcode = data[idx];
        code_size += 1;
        bool has_arg = (opcode >= 1 && opcode <= 21) ||
                       (opcode >= 40 && opcode <= 44) ||  // 条件/无条件跳转
                       (opcode >= 110 && opcode <= 114) ||  // 更多跳转指令
                       opcode == 57 ||  // CALL_FUNCTION
                       opcode == 107 ||  // BINARY_OP
                       (opcode >= 100 && opcode <= 143) ||
                       (opcode >= 90 && opcode <= 93);
        if (has_arg && idx + 1 < data.size()) {
            code_size += 1;
            idx += 2;
        } else {
            idx += 1;
        }
        instr_count++;
    }
    
    size_t consts_offset = code_start + code_size;
    if (consts_offset + 4 > data.size()) {
        std::cout << "Error: Invalid bytecode file" << std::endl;
        return 1;
    }
    uint32_t consts_count = data[consts_offset] | (data[consts_offset+1] << 8) | 
                            (data[consts_offset+2] << 16) | (data[consts_offset+3] << 24);
    std::cout << "Constants count: " << consts_count << std::endl;
    
    // 解析并显示常量表
    std::cout << std::endl << "=== Constants ===" << std::endl;
    size_t const_idx = consts_offset + 4;
    for (uint32_t i = 0; i < consts_count && const_idx < data.size(); i++) {
        if (const_idx >= data.size()) break;
        
        uint8_t type = data[const_idx++];
        std::cout << "  [" << i << "] ";
        
        switch (type) {
            case 0:  // None
                std::cout << "None";
                break;
            case 1:  // Bool
                if (const_idx < data.size()) {
                    std::cout << (data[const_idx++] ? "True" : "False");
                }
                break;
            case 2:  // Int64
                if (const_idx + 8 <= data.size()) {
                    int64_t val = 0;
                    for (int j = 0; j < 8; j++) {
                        val |= (static_cast<int64_t>(data[const_idx++]) << (j * 8));
                    }
                    std::cout << val << " (int)";
                }
                break;
            case 3:  // Double
                if (const_idx + 8 <= data.size()) {
                    double val;
                    memcpy(&val, &data[const_idx], 8);
                    const_idx += 8;
                    std::cout << val << " (float)";
                }
                break;
            case 4:  // String
                if (const_idx + 4 <= data.size()) {
                    uint32_t str_len = data[const_idx] | (data[const_idx+1] << 8) |
                                       (data[const_idx+2] << 16) | (data[const_idx+3] << 24);
                    const_idx += 4;
                    std::cout << "\"";
                    for (uint32_t j = 0; j < str_len && const_idx < data.size(); j++) {
                        std::cout << static_cast<char>(data[const_idx++]);
                    }
                    std::cout << "\"";
                }
                break;
            default:
                std::cout << "Unknown type: " << (int)type;
        }
        std::cout << std::endl;
    }
    
    // 调试输出
    std::cout << "[DEBUG] code_start: " << std::dec << code_start << std::endl;
    std::cout << "[DEBUG] code_size: " << code_size << std::endl;
    std::cout << "[DEBUG] consts_offset: " << consts_offset << std::endl;
    std::cout << "[DEBUG] bytes at consts_offset: ";
    for (size_t j = 0; j < 8 && consts_offset + j < data.size(); j++) {
        std::cout << std::hex << (int)data[consts_offset + j] << " ";
    }
    std::cout << std::endl;
    
    std::cout << std::endl;
    std::cout << "=== Hex Dump ===" << std::endl;
    
    for (size_t i = 0; i < data.size(); i += 16) {
        std::cout << std::hex << std::setfill('0') << std::setw(4) << i << ": ";
        
        // Hex
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < data.size()) {
                std::cout << std::setw(2) << (int)data[i+j] << " ";
            } else {
                std::cout << "   ";
            }
            if (j == 7) std::cout << " ";
        }
        
        std::cout << "| ";
        
        // ASCII
        for (size_t j = 0; j < 16 && i + j < data.size(); ++j) {
            char c = data[i+j];
            std::cout << (c >= 32 && c < 127 ? c : '.');
        }
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== Instructions ===" << std::endl;
    
    // 计算指令开始位置（使用之前已定义的 offset）
    
    // name_len (4 bytes)
    offset += 4 + name_len;
    
    // flags (4 bytes)
    offset += 4;
    
    // argcount (2 bytes)
    offset += 2;
    // posonlyargcount (2 bytes)
    offset += 2;
    // kwonlyargcount (2 bytes)
    offset += 2;
    // nlocals (2 bytes)
    offset += 2;
    // stacksize (2 bytes)
    uint16_t stacksize = data[offset] | (data[offset+1] << 8);
    offset += 2;
    
    // instruction_count (4 bytes)
    offset += 4;
    
    // firstlineno (2 bytes)
    offset += 2;
    
    std::cout << "Name: " << std::string((char*)&data[8], name_len) << std::endl;
    std::cout << "Stack size: " << stacksize << std::endl;
    std::cout << "Instruction count: " << instruction_count << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Instructions ===" << std::endl;
    
    // 解析字节码指令
    idx = code_start;
    instr_count = 0;
    
    // 构建指令索引到字节偏移的映射
    std::map<size_t, size_t> instr_idx_to_offset;
    size_t temp_idx = code_start;
    int temp_count = 0;
    while (temp_idx < data.size() && temp_count < (int)instruction_count) {
        uint8_t temp_opcode = data[temp_idx];
        instr_idx_to_offset[temp_count] = temp_idx;
        
        bool temp_has_arg = (temp_opcode >= 1 && temp_opcode <= 21) ||
                           (temp_opcode >= 40 && temp_opcode <= 44) ||
                           (temp_opcode >= 110 && temp_opcode <= 114) ||
                           temp_opcode == 57 ||
                           temp_opcode == 107 ||
                           (temp_opcode >= 100 && temp_opcode <= 143) ||
                           (temp_opcode >= 90 && temp_opcode <= 93);
        
        if (temp_has_arg && temp_idx + 1 < data.size()) {
            temp_idx += 2;
        } else {
            temp_idx += 1;
        }
        temp_count++;
    }
    
    // 构建字节偏移到指令索引的映射
    std::map<size_t, size_t> offset_to_instr_idx;
    for (auto& pair : instr_idx_to_offset) {
        offset_to_instr_idx[pair.second] = pair.first;
    }
    
    while (idx < data.size() && instr_count < (int)instruction_count) {
        uint8_t opcode = data[idx];
        
        std::cout << std::dec << std::setfill(' ') << std::setw(4) << instr_count 
                  << ": " << get_opcode_name(opcode);
        
        // 检查是否有参数
        bool has_arg = (opcode >= 1 && opcode <= 21) ||
                       (opcode >= 40 && opcode <= 44) ||  // 条件/无条件跳转
                       (opcode >= 110 && opcode <= 114) ||  // 更多跳转指令
                       opcode == 57 ||  // CALL_FUNCTION
                       opcode == 107 ||  // BINARY_OP
                       (opcode >= 100 && opcode <= 143) ||
                       (opcode >= 90 && opcode <= 93);
        
        if (has_arg && idx + 1 < data.size()) {
            uint8_t arg = data[idx+1];
            size_t next_instr_offset = idx + 2;
            
            // 检查是否是跳转指令
            bool is_jump = (opcode >= 40 && opcode <= 44) ||
                          (opcode >= 110 && opcode <= 114);
            
            if (is_jump) {
                // 跳转目标是相对于chunk开始的绝对偏移，需要加上code_start
                // 生成器存储的是block在chunk中的绝对偏移
                size_t abs_target_offset = code_start + static_cast<size_t>(arg);
                
                          
                if (offset_to_instr_idx.count(abs_target_offset)) {
                    // 显示指令索引
                    std::cout << " " << offset_to_instr_idx[abs_target_offset];
                } else {
                    // 普通参数直接显示
                    std::cout << " " << (int)arg;
                }
            } else {
                // 普通参数直接显示
                std::cout << " " << (int)arg;
            }
            idx += 2;
        } else {
            idx += 1;
        }
        std::cout << std::endl;
        instr_count++;
    }
    
    std::cout << std::endl;
    std::cout << "Total instructions: " << instr_count << std::endl;
    
    return 0;
}