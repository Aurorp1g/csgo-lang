/**
 * @file cgd.cpp
 * @brief CSGO 字节码反汇编器 v2.2 (美化终端版)
 *
 * 用于查看 .cgb 字节码文件的内容
 * 特性：ANSI 颜色支持，美化输出格式
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <map>
#include <cstring>

#include "bytecode/bytecode_generator.h"

using namespace csgo;

namespace {
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string DIM = "\033[2m";

const std::string C_RED = "\033[31m";
const std::string C_GREEN = "\033[32m";
const std::string C_YELLOW = "\033[33m";
const std::string C_BLUE = "\033[34m";
const std::string C_MAGENTA = "\033[35m";
const std::string C_CYAN = "\033[36m";
const std::string C_WHITE = "\033[37m";

const std::string C_BRIGHT_BLACK = "\033[90m";
const std::string C_BRIGHT_RED = "\033[91m";
const std::string C_BRIGHT_GREEN = "\033[92m";
const std::string C_BRIGHT_YELLOW = "\033[93m";
const std::string C_BRIGHT_BLUE = "\033[94m";
const std::string C_BRIGHT_MAGENTA = "\033[95m";
const std::string C_BRIGHT_CYAN = "\033[96m";

bool use_color = true;
}

void print_usage(const char* program_name) {
    std::cout << C_CYAN << "CSGO Bytecode Disassembler" << RESET << " v2.2" << std::endl;
    std::cout << "Usage: " << program_name << " " << C_YELLOW << "<input.cgb>" << RESET << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  " << C_YELLOW << "--no-color" << RESET << "    Disable colored output" << std::endl;
    std::cout << "  " << C_YELLOW << "-h, --help" << RESET << "   Show this help message" << std::endl;
}

std::vector<uint8_t> read_file(const std::string& filename) {
    std::string adjusted_filename = filename;
    for (char& c : adjusted_filename) {
        if (c == '\\') c = '/';
    }
    
    std::ifstream file(adjusted_filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filename << std::endl;
        return {};
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
        default: return "UNKNOWN(" + std::to_string(opcode) + ")";
    }
}

// 判断操作码是否有参数（与生成器严格一致）
bool has_argument(uint8_t opcode) {
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

// 判断是否为跳转指令
bool is_jump_instruction(uint8_t opcode) {
    using namespace csgo::bytecode;
    switch (static_cast<BytecodeOpcode>(opcode)) {
        case BytecodeOpcode::POP_JUMP_IF_TRUE:
        case BytecodeOpcode::POP_JUMP_IF_FALSE:
        case BytecodeOpcode::JUMP_IF_TRUE_OR_POP:
        case BytecodeOpcode::JUMP_IF_FALSE_OR_POP:
        case BytecodeOpcode::JUMP_ABSOLUTE:
        case BytecodeOpcode::JUMP_FORWARD:
        case BytecodeOpcode::FOR_ITER:
        case BytecodeOpcode::CONTINUE_LOOP:
        case BytecodeOpcode::BREAK_LOOP:
            return true;
        default:
            return false;
    }
}

// 获取跳转指令类型描述
std::string get_jump_type(uint8_t opcode) {
    using namespace csgo::bytecode;
    switch (static_cast<BytecodeOpcode>(opcode)) {
        case BytecodeOpcode::JUMP_FORWARD:
            return "forward";
        case BytecodeOpcode::FOR_ITER:
            return "for-exit";
        default:
            return "absolute";
    }
}

// 解析常量值并返回可读字符串
std::string format_constant(const std::vector<uint8_t>& data, size_t& idx) {
    if (idx >= data.size()) return "<out of bounds>";
    
    uint8_t type = data[idx++];
    std::ostringstream oss;
    
    switch (type) {
        case 0: // None
            oss << "None";
            break;
        case 1: // Bool
            if (idx < data.size()) {
                oss << (data[idx++] ? "True" : "False");
            }
            break;
        case 2: // Int64
            if (idx + 8 > data.size()) {
                oss << "<truncated int64>";
            } else {
                int64_t val = *reinterpret_cast<const int64_t*>(&data[idx]);
                idx += 8;
                oss << val << " (int)";
            }
            break;
        case 3: // Double
            if (idx + 8 > data.size()) {
                oss << "<truncated float>";
            } else {
                double val = *reinterpret_cast<const double*>(&data[idx]);
                idx += 8;
                oss << val << " (float)";
            }
            break;
        case 4: // String
            if (idx + 4 > data.size()) {
                oss << "<truncated string len>";
            } else {
                uint32_t str_len = *reinterpret_cast<const uint32_t*>(&data[idx]);
                idx += 4;
                oss << "\"";
                for (uint32_t j = 0; j < str_len && idx < data.size(); j++) {
                    char c = static_cast<char>(data[idx++]);
                    switch (c) {
                        case '\n': oss << "\\n"; break;
                        case '\r': oss << "\\r"; break;
                        case '\t': oss << "\\t"; break;
                        case '\"': oss << "\\\""; break;
                        case '\\': oss << "\\\\"; break;
                        default:
                            if (c >= 32 && c < 127) oss << c;
                            else oss << "\\x" << std::hex << (uint8_t)c << std::dec;
                    }
                }
                oss << "\"";
            }
            break;
        default:
            oss << "<unknown type " << (int)type << ">";
    }
    return oss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_file;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--no-color") {
            use_color = false;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg[0] != '-') {
            input_file = arg;
        }
    }
    
    if (input_file.empty()) {
        std::cerr << C_RED << "Error:" << RESET << " No input file specified" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    std::vector<uint8_t> data = read_file(input_file);
    
    if (data.empty()) {
        return 1;
    }
    
    auto color = [&](const std::string& c) -> const std::string& {
        return use_color ? c : RESET;
    };
    
    std::cout << color(C_CYAN) << "============================================================" << RESET << std::endl;
    std::cout << color(C_CYAN) << "          " << BOLD << "CSGO Bytecode Disassembler" << RESET << color(C_CYAN) << " v2.2" << RESET << std::endl;
    std::cout << color(C_CYAN) << "============================================================" << RESET << std::endl;
    std::cout << std::endl;
    std::cout << "  " << C_WHITE << "File:" << RESET << " " << C_YELLOW << input_file << RESET << std::endl;
    std::cout << "  " << C_WHITE << "Size:" << RESET << " " << C_GREEN << data.size() << RESET << " bytes" << std::endl;
    std::cout << "Format: Fixed 2-byte argument (3 bytes per instr with arg)" << std::endl;
    std::cout << std::endl;
    
    // 解析魔数
    if (data.size() < 4) {
        std::cerr << "Error: File too small for magic number" << std::endl;
        return 1;
    }
    
    if (data[0] != 'C' || data[1] != 'G' || data[2] != 'O') {
        std::cerr << "Warning: Invalid magic number" << std::endl;
    }
    std::cout << "Magic: " << (char)data[0] << (char)data[1] << (char)data[2] 
              << " (version " << (int)data[3] << ")" << std::endl;
    
    // 解析头部（与生成器写入顺序严格一致）
    size_t offset = 4;
    
    if (offset + 4 > data.size()) { std::cerr << "Truncated file at name_len" << std::endl; return 1; }
    uint32_t name_len = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += 4;
    
    if (offset + name_len > data.size()) { std::cerr << "Truncated file at name" << std::endl; return 1; }
    std::string func_name(reinterpret_cast<const char*>(&data[offset]), name_len);
    offset += name_len;
    
    if (offset + 4 > data.size()) { std::cerr << "Truncated file at flags" << std::endl; return 1; }
    uint32_t flags = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += 4;
    
    if (offset + 2 > data.size()) { std::cerr << "Truncated file at argcount" << std::endl; return 1; }
    uint16_t argcount = *reinterpret_cast<const uint16_t*>(&data[offset]);
    offset += 2;
    
    if (offset + 2 > data.size()) { std::cerr << "Truncated file at posonlyargcount" << std::endl; return 1; }
    uint16_t posonlyargcount = *reinterpret_cast<const uint16_t*>(&data[offset]);
    offset += 2;
    
    if (offset + 2 > data.size()) { std::cerr << "Truncated file at kwonlyargcount" << std::endl; return 1; }
    uint16_t kwonlyargcount = *reinterpret_cast<const uint16_t*>(&data[offset]);
    offset += 2;
    
    if (offset + 2 > data.size()) { std::cerr << "Truncated file at nlocals" << std::endl; return 1; }
    uint16_t nlocals = *reinterpret_cast<const uint16_t*>(&data[offset]);
    offset += 2;
    
    if (offset + 2 > data.size()) { std::cerr << "Truncated file at stacksize" << std::endl; return 1; }
    uint16_t stacksize = *reinterpret_cast<const uint16_t*>(&data[offset]);
    offset += 2;
    
    if (offset + 4 > data.size()) { std::cerr << "Truncated file at instruction_count" << std::endl; return 1; }
    uint32_t instruction_count = *reinterpret_cast<const uint32_t*>(&data[offset]);
    offset += 4;
    
    if (offset + 2 > data.size()) { std::cerr << "Truncated file at firstlineno" << std::endl; return 1; }
    uint16_t firstlineno = *reinterpret_cast<const uint16_t*>(&data[offset]);
    offset += 2;
    
    size_t code_start = offset;
    
    std::cout << "Function: " << func_name << std::endl;
    std::cout << "Flags: " << flags << std::endl;
    std::cout << "Arguments: " << argcount << " (pos:" << posonlyargcount << ", kw:" << kwonlyargcount << ")" << std::endl;
    std::cout << "Locals: " << nlocals << std::endl;
    std::cout << "Stack size: " << stacksize << std::endl;
    std::cout << "Instructions: " << instruction_count << std::endl;
    std::cout << "First line: " << firstlineno << std::endl;
    std::cout << "Code offset: 0x" << std::hex << code_start << std::dec << std::endl;
    std::cout << std::endl;
    
    // ============================================================================
    // 第一遍扫描：建立字节偏移到指令索引的映射（关键修改：按3字节解析有参指令）
    // ============================================================================
    std::map<size_t, size_t> offset_to_instr_idx;
    std::map<size_t, size_t> instr_idx_to_offset;
    size_t idx = code_start;
    size_t instr_idx = 0;
    uint32_t ext_arg = 0;
    
    while (idx < data.size() && instr_idx < instruction_count) {
        offset_to_instr_idx[idx] = instr_idx;
        instr_idx_to_offset[instr_idx] = idx;
        
        uint8_t opcode = data[idx];
        
        if (has_argument(opcode)) {
            // 关键修改：现在需要至少3字节（1 opcode + 2 arg）
            if (idx + 3 > data.size()) break;
            
            // 读取16位参数（小端序）
            uint16_t arg = data[idx + 1] | (data[idx + 2] << 8);
            
            // 处理 EXTENDED_ARG 链（EXTENDED_ARG 现在也是3字节：opcode + 2byte arg）
            if (static_cast<csgo::bytecode::BytecodeOpcode>(opcode) == 
                csgo::bytecode::BytecodeOpcode::EXTENDED_ARG) {
                ext_arg = (ext_arg << 16) | arg;  // 累积16位而不是8位
                idx += 3;  // 关键修改：从 +2 改为 +3
                instr_idx++;
                continue;
            }
            
            ext_arg = 0;
            idx += 3;  // 关键修改：从 +2 改为 +3
        } else {
            ext_arg = 0;
            idx += 1;
        }
        instr_idx++;
    }
    
    // ============================================================================
    // 计算代码区大小（关键修改：最后一条指令如果是带参，占3字节而不是2字节）
    // ============================================================================
    size_t code_size = 0;
    if (!instr_idx_to_offset.empty()) {
        size_t last_idx = instr_idx_to_offset.rbegin()->first;
        size_t last_offset = instr_idx_to_offset[last_idx];
        uint8_t last_opcode = data[last_offset];
        if (has_argument(last_opcode)) {
            code_size = (last_offset - code_start) + 3;  // 关键修改：+3 而不是 +2
        } else {
            code_size = (last_offset - code_start) + 1;
        }
    }
    
    size_t consts_offset = code_start + code_size;
    
    // 存储解析出的常量值
    std::vector<std::string> constants;
    
    // 解析常量池
    std::cout << std::endl << color(C_MAGENTA) << "+-- " << BOLD << "Constants" << RESET << color(C_MAGENTA) << " ----------------------------------------+" << RESET << std::endl;
    if (consts_offset + 4 > data.size()) {
        std::cerr << C_YELLOW << "Warning: No constants table found" << RESET << std::endl;
    } else {
        uint32_t consts_count = *reinterpret_cast<const uint32_t*>(&data[consts_offset]);
        std::cout << color(C_MAGENTA) << "|" << RESET << "  Count: " << C_GREEN << consts_count << RESET << std::endl;
        
        size_t const_idx = consts_offset + 4;
        for (uint32_t i = 0; i < consts_count && const_idx < data.size(); i++) {
            if (const_idx >= data.size()) break;
            
            size_t save_idx = const_idx;
            std::string const_str = format_constant(data, const_idx);
            constants.push_back(const_str);
            
            std::cout << color(C_MAGENTA) << "|" << RESET << "  [" << C_YELLOW << i << RESET << "] " << const_str << std::endl;
        }
    }
    
    // 计算 names 偏移（紧跟在常量池后面）
    size_t names_offset = consts_offset + 4;
    {
        size_t const_idx = consts_offset + 4;
        for (uint32_t i = 0; i < constants.size() && const_idx < data.size(); i++) {
            const std::string& c = constants[i];
            if (c == "None") {
                const_idx += 1;
            } else if (c == "True" || c == "False") {
                const_idx += 2;
            } else if (c.find("(int)") != std::string::npos) {
                const_idx += 9;
            } else if (c.find("(float)") != std::string::npos) {
                const_idx += 9;
            } else if (!c.empty() && c[0] == '"') {
                if (const_idx + 1 <= data.size()) {
                    uint32_t str_len = *reinterpret_cast<const uint32_t*>(&data[const_idx + 1]);
                    const_idx += 1 + 4 + str_len;
                }
            } else {
                const_idx += 1;
            }
        }
        names_offset = const_idx;
    }
    
    // 解析 names 表（全局变量/名称）
    std::vector<std::string> names;
    size_t varnames_offset = names_offset;
    
    std::cout << std::endl << color(C_MAGENTA) << "+-- " << BOLD << "Names (globals)" << RESET << color(C_MAGENTA) << " ------------------------------------+" << RESET << std::endl;
    if (names_offset + 4 <= data.size()) {
        uint32_t names_count = *reinterpret_cast<const uint32_t*>(&data[names_offset]);
        std::cout << color(C_MAGENTA) << "|" << RESET << "  Count: " << C_GREEN << names_count << RESET << std::endl;
        
        size_t name_idx = names_offset + 4;
        for (uint32_t i = 0; i < names_count && name_idx < data.size(); i++) {
            if (name_idx + 4 > data.size()) break;
            uint32_t name_len = *reinterpret_cast<const uint32_t*>(&data[name_idx]);
            name_idx += 4;
            
            if (name_idx + name_len <= data.size()) {
                std::string g_name(reinterpret_cast<const char*>(&data[name_idx]), name_len);
                names.push_back(g_name);
                std::cout << "  [" << i << "] " << g_name << std::endl;
                name_idx += name_len;
            }
        }
        varnames_offset = name_idx;
    }
    
    // 存储解析出的变量名
    std::vector<std::string> varnames;
    
    // 解析 varnames 表（局部变量）
    std::cout << std::endl << color(C_MAGENTA) << "+-- " << BOLD << "Local Variables (varnames)" << RESET << color(C_MAGENTA) << " -----------------------+" << RESET << std::endl;
    if (varnames_offset + 4 <= data.size()) {
        uint32_t varnames_count = *reinterpret_cast<const uint32_t*>(&data[varnames_offset]);
        std::cout << color(C_MAGENTA) << "|" << RESET << "  Count: " << C_GREEN << varnames_count << RESET << std::endl;
        
        size_t name_idx = varnames_offset + 4;
        for (uint32_t i = 0; i < varnames_count && name_idx < data.size(); i++) {
            if (name_idx + 4 > data.size()) break;
            uint32_t name_len = *reinterpret_cast<const uint32_t*>(&data[name_idx]);
            name_idx += 4;
            
            if (name_idx + name_len <= data.size()) {
                std::string var_name(reinterpret_cast<const char*>(&data[name_idx]), name_len);
                varnames.push_back(var_name);
                std::cout << "  [" << i << "] " << var_name << std::endl;
                name_idx += name_len;
            }
        }
    }
    
    // Hex Dump
    std::cout << std::endl << color(C_MAGENTA) << "+-- " << BOLD << "Hex Dump" << RESET << color(C_MAGENTA) << " ---------------------------------------------+" << RESET << std::endl;
    for (size_t i = 0; i < data.size(); i += 16) {
        std::cout << color(C_MAGENTA) << "|" << RESET << " " << std::hex << std::setfill('0') << std::setw(4) << i << ": ";
        
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < data.size()) {
                std::cout << std::setw(2) << (int)data[i+j] << " ";
            } else {
                std::cout << "   ";
            }
            if (j == 7) std::cout << " ";
        }
        
        std::cout << "| ";
        for (size_t j = 0; j < 16 && i + j < data.size(); ++j) {
            char c = data[i+j];
            std::cout << (c >= 32 && c < 127 ? c : '.');
        }
        std::cout << std::endl;
    }
    
    // ============================================================================
    // 反汇编指令（关键修改：按3字节解析带参指令）
    // ============================================================================
    std::cout << color(C_MAGENTA) << "+-- " << BOLD << "Disassembly" << RESET << color(C_MAGENTA) << " --------------------------------------------+" << RESET << std::endl;
    std::cout << color(C_MAGENTA) << "|" << RESET << " Offset  Idx   Opcode                 Arg   Comment" << std::endl;
    std::cout << color(C_MAGENTA) << "|" << RESET << " " << std::string(55, '-') << std::endl;
    
    idx = code_start;
    instr_idx = 0;
    ext_arg = 0;
    
    while (idx < data.size() && instr_idx < instruction_count) {
        uint8_t opcode = data[idx];
        bool has_arg_flag = has_argument(opcode);
        
        std::string op_name = get_opcode_name(opcode);
        
        // 根据操作码类型选择颜色
        std::string op_color = C_WHITE;
        if (op_name == "LOAD_GLOBAL" || op_name == "LOAD_FAST" || op_name == "LOAD_CONST") {
            op_color = C_GREEN;
        } else if (op_name == "STORE_FAST" || op_name == "STORE_GLOBAL") {
            op_color = C_YELLOW;
        } else if (op_name == "CALL_FUNCTION") {
            op_color = C_CYAN;
        } else if (op_name == "JUMP_ABSOLUTE" || op_name == "POP_JUMP_IF_FALSE" || 
                   op_name == "POP_JUMP_IF_TRUE" || op_name == "FOR_ITER" || op_name == "JUMP_FORWARD") {
            op_color = C_MAGENTA;
        } else if (op_name == "RETURN_VALUE") {
            op_color = C_RED;
        } else if (op_name == "BINARY_ADD" || op_name == "BINARY_SUB" || 
                   op_name == "BINARY_MUL" || op_name == "BINARY_DIV") {
            op_color = C_BLUE;
        } else if (op_name == "COMPARE_OP") {
            op_color = C_BRIGHT_CYAN;
        }
        
        // 显示指令偏移（字节）和索引
        std::cout << color(C_MAGENTA) << "|" << RESET << " " 
                  << color(C_BRIGHT_BLACK) << std::dec << std::setfill(' ') << std::setw(4) << (idx - code_start) << RESET << " " 
                  << C_BRIGHT_BLACK << std::setw(3) << instr_idx << RESET << "  "
                  << color(op_color) << std::left << std::setw(20) << op_name << RESET << " ";
        
        if (has_arg_flag) {
            // 关键修改：检查至少3字节可用
            if (idx + 3 > data.size()) {
                std::cout << "<truncated>";
                break;
            }
            
            // 关键修改：读取16位参数（小端序：低字节在前，高字节在后）
            uint16_t arg = data[idx + 1] | (data[idx + 2] << 8);
            
            // 处理 EXTENDED_ARG 累积（现在累积16位）
            if (static_cast<csgo::bytecode::BytecodeOpcode>(opcode) == 
                csgo::bytecode::BytecodeOpcode::EXTENDED_ARG) {
                ext_arg = (ext_arg << 16) | arg;
                std::cout << std::setw(4) << arg << " (ext_arg accum=" << ext_arg << ")";
                idx += 3;  // 关键修改：+3 而不是 +2
                instr_idx++;
                std::cout << std::right << std::endl;
                continue;
            }
            
            // 计算实际参数值（EXTENDED_ARG 累积高位，当前 arg 是低位）
            uint32_t full_arg = (ext_arg << 16) | arg;
            ext_arg = 0; // 重置
            
            if (is_jump_instruction(opcode)) {
                std::string jump_type = get_jump_type(opcode);
                
                if (jump_type == "forward") {
                    // 相对跳转：arg 是相对于下一条指令的偏移
                    // 关键修改：下一条指令在 idx + 3 处（而不是 idx + 2）
                    size_t next_instr_offset = idx + 3;
                    size_t target_offset = next_instr_offset + full_arg;
                    if (offset_to_instr_idx.count(target_offset)) {
                        size_t target_idx = offset_to_instr_idx[target_offset];
                        std::cout << std::setw(4) << target_idx << " (to +" << full_arg 
                                  << " bytes, offset " << (target_offset - code_start) << ")";
                    } else {
                        std::cout << std::setw(4) << full_arg << " (rel +" << full_arg 
                                  << " bytes, raw)";
                    }
                } else {
                    // 绝对跳转：arg 是字节偏移（相对于 code_start）
                    size_t abs_target_offset = code_start + full_arg;
                    if (offset_to_instr_idx.count(abs_target_offset)) {
                        size_t target_idx = offset_to_instr_idx[abs_target_offset];
                        std::cout << std::setw(4) << target_idx << " (to offset " 
                                  << full_arg << ")";
                    } else if (full_arg < instruction_count && instr_idx_to_offset.count(full_arg)) {
                        // 兼容旧格式（指令索引）
                        size_t target_offset = instr_idx_to_offset[full_arg];
                        std::cout << std::setw(4) << full_arg << " (to instr " << full_arg 
                                  << ", offset " << (target_offset - code_start) << ")";
                    } else {
                        std::cout << std::setw(4) << full_arg << " (raw offset)";
                    }
                }
            } else {
                // 普通参数显示
                std::cout << std::setw(4) << full_arg;
                
                // 添加参数语义提示
                switch (static_cast<csgo::bytecode::BytecodeOpcode>(opcode)) {
                    case csgo::bytecode::BytecodeOpcode::LOAD_CONST:
                        if (full_arg < constants.size()) {
                            std::cout << " (" << constants[full_arg] << ")";
                        }
                        break;
                    case csgo::bytecode::BytecodeOpcode::LOAD_FAST:
                    case csgo::bytecode::BytecodeOpcode::STORE_FAST:
                        if (full_arg < varnames.size()) {
                            std::cout << " (" << varnames[full_arg] << ")";
                        } else {
                            std::cout << " (slot " << full_arg << ")";
                        }
                        break;
                    case csgo::bytecode::BytecodeOpcode::LOAD_GLOBAL:
                    case csgo::bytecode::BytecodeOpcode::STORE_GLOBAL:
                    case csgo::bytecode::BytecodeOpcode::LOAD_NAME:
                    case csgo::bytecode::BytecodeOpcode::STORE_NAME:
                        if (full_arg < names.size()) {
                            std::cout << " (" << names[full_arg] << ")";
                        }
                        break;
                    case csgo::bytecode::BytecodeOpcode::CALL_FUNCTION:
                        std::cout << " (argc " << full_arg << ")";
                        break;
                    case csgo::bytecode::BytecodeOpcode::COMPARE_OP: {
                        const char* cmp_ops[] = {"<", "<=", "==", "!=", ">", ">="};
                        if (full_arg < 6) {
                            std::cout << " (" << cmp_ops[full_arg] << ")";
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
            idx += 3;  // 关键修改：前进3字节而不是2字节
        } else {
            ext_arg = 0;
            idx += 1;
        }
        
        std::cout << std::right << std::endl;
        instr_idx++;
    }
    
    std::cout << color(C_MAGENTA) << "|" << RESET << " " << std::string(55, '-') << std::endl;
    std::cout << color(C_MAGENTA) << "`" << RESET << "  " << C_WHITE << "Total:" << RESET << " " << C_GREEN << instr_idx << RESET << " instructions, " 
              << C_GREEN << (idx - code_start) << RESET << " bytes" << std::endl;
    std::cout << color(C_MAGENTA) << "`" << RESET << std::string(60, '-') << RESET << std::endl;
    
    return 0;
}