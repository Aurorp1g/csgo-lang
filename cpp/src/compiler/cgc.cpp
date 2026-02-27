/**
 * @file cgc.cpp
 * @brief CSGO 语言编译器主程序
 *
 * 将 .cg 源文件编译为 .cgb 字节码文件
 *
 * 编译流程:
 * 1. 词法分析 (Tokenizer) - 源代码 -> Token 序列
 * 2. 语法分析 (Parser) - Token 序列 -> AST
 * 3. IR 构建 (IRBuilder) - AST -> SSA IR
 * 4. 优化 (Optimizer) - SSA IR 优化
 * 5. 字节码生成 (BytecodeGenerator) - SSA IR -> 字节码
 * 6. 序列化输出 - 字节码 -> .cgb 文件
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <cstdlib>
#include <iomanip>
#include <vector>
#include <chrono>

#include "lexer/tokenizer.h"
#include "parser/parser.h"
#include "ast/ast_node.h"
#include "ir/ssa_ir.h"
#include "ir/optimizer.h"
#include "bytecode/bytecode_generator.h"

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

std::string color(const std::string& c) {
    return use_color ? c : "";
}

void print_banner() {
    std::cout << color(C_BRIGHT_CYAN) << R"(
     ######  #######  ######   ###### 
    ##      ##       ##       ##    ##
    ##      ##  #### ##  #### ##    ##
    ##      ##    ## ##    ## ##    ##
     ######  #######  ######   ######
)" << RESET << std::endl;
    std::cout << color(C_BRIGHT_BLUE) << BOLD << "        CSGO Language Compiler v1.0.0" << RESET << std::endl;
    std::cout << color(C_BRIGHT_BLACK) << "              Built by Aurorp1g" << RESET << std::endl;
    std::cout << std::endl;
}

void print_progress_bar(int current, int total, const std::string& task) {
    const int bar_width = 40;
    float progress = (float)current / total;
    int pos = (int)(bar_width * progress);
    
    std::cout << color(C_BRIGHT_BLACK) << "  [";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << color(C_BRIGHT_GREEN) << "=";
        else if (i == pos) std::cout << color(C_BRIGHT_YELLOW) << ">";
        else std::cout << color(C_BRIGHT_BLACK) << "-";
    }
    std::cout << color(C_BRIGHT_BLACK) << "] ";
    std::cout << color(C_WHITE) << std::setw(3) << (int)(progress * 100) << "% ";
    std::cout << color(C_BRIGHT_CYAN) << task << RESET << std::endl;
}

void print_success(const std::string& msg) {
    std::cout << color(C_BRIGHT_GREEN) << "  [OK] " << RESET << msg << std::endl;
}

void print_error(const std::string& msg) {
    std::cout << color(C_BRIGHT_RED) << " [ERROR] " << RESET << msg << std::endl;
}

void print_info(const std::string& msg) {
     std::cout << color(C_BRIGHT_BLUE) << "  [INFO] " << RESET << msg << std::endl;
 }

}

using namespace csgo;

void print_usage(const char* program_name) {
    std::cout << color(C_BRIGHT_CYAN) << "CSGO " << RESET << color(C_BRIGHT_BLUE) << "Language Compiler " << color(C_YELLOW) << "v1.0.0" << RESET << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << color(C_WHITE) << program_name << RESET << " " << color(C_YELLOW) << "<input.cg>" << RESET << " [" << color(C_YELLOW) << "output.cgb" << RESET << "]" << std::endl;
    std::cout << std::endl;
    std::cout << color(C_BRIGHT_BLUE) << "Options:" << RESET << std::endl;
    std::cout << "  " << color(C_CYAN) << "-h, --help    " << RESET << "  Show this help message" << std::endl;
    std::cout << "  " << color(C_CYAN) << "-O0           " << RESET << "  No optimization" << std::endl;
    std::cout << "  " << color(C_CYAN) << "-O1           " << RESET << "  Basic optimization (default)" << std::endl;
    std::cout << "  " << color(C_CYAN) << "-O2           " << RESET << "  Medium optimization" << std::endl;
    std::cout << "  " << color(C_CYAN) << "-O3           " << RESET << "  Aggressive optimization" << std::endl;
    std::cout << "  " << color(C_CYAN) << "-v, --verbose " << RESET << "  Verbose output" << std::endl;
    std::cout << "  " << color(C_CYAN) << "-IR           " << RESET << "  Print IR to debug file" << std::endl;
    std::cout << "  " << color(C_CYAN) << "-debug        " << RESET << "  Generate debug files" << std::endl;
    std::cout << std::endl;
    std::cout << color(C_BRIGHT_BLACK) << "Copyright (c) 2026 Aurorp1g" << RESET << std::endl;
    std::cout << color(C_BRIGHT_BLACK) << "Licensed under MIT License" << RESET << std::endl;
}

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << color(C_BRIGHT_RED) << "Error: " << RESET << "Cannot open file: " << filename << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool write_file(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << color(C_BRIGHT_RED) << "Error: " << RESET << "Cannot write file: " << filename << std::endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_file;
    std::string output_file;
    int optimization_level = 1;
    bool verbose = false;
    bool print_ir = false;
    bool debug_output = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-O0") {
            optimization_level = 0;
        } else if (arg == "-O1") {
            optimization_level = 1;
        } else if (arg == "-O2") {
            optimization_level = 2;
        } else if (arg == "-O3") {
            optimization_level = 3;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-IR") {
            print_ir = true;
        } else if (arg == "-debug") {
            debug_output = true;
        } else if (arg[0] != '-') {
            if (input_file.empty()) {
                input_file = arg;
            } else if (output_file.empty()) {
                output_file = arg;
            }
        }
    }
    
    if (input_file.empty()) {
        std::cerr << color(C_BRIGHT_RED) << "Error: " << RESET << "No input file specified" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    // 生成调试输出文件名
    std::string base_name = input_file;
    size_t pos = base_name.rfind('.');
    if (pos != std::string::npos) {
        base_name = base_name.substr(0, pos);
    }
    
    // 获取源代码所在目录
    std::string source_dir = ".";
    size_t dir_pos = input_file.rfind('/');
    if (dir_pos == std::string::npos) {
        dir_pos = input_file.rfind('\\');
    }
    if (dir_pos != std::string::npos) {
        source_dir = input_file.substr(0, dir_pos);
    }
    
    // 创建debug目录路径
    std::string debug_dir = source_dir + "/debug";
    
    // 确保debug目录存在（创建目录）
    // 使用 Windows 的 md 命令或 Unix 的 mkdir -p
    std::string mkdir_cmd;
    #ifdef _WIN32
    mkdir_cmd = "if not exist \"" + debug_dir + "\" mkdir \"" + debug_dir + "\"";
    #else
    mkdir_cmd = "mkdir -p " + debug_dir;
    #endif
    system(mkdir_cmd.c_str());
    
    // 获取base_name的文件名部分
    std::string base_filename = base_name;
    size_t filename_pos = base_name.rfind('/');
    if (filename_pos == std::string::npos) {
        filename_pos = base_name.rfind('\\');
    }
    if (filename_pos != std::string::npos) {
        base_filename = base_name.substr(filename_pos + 1);
    }
    
    std::string tokens_file = debug_dir + "/" + base_filename + ".tokens.json";
    std::string ast_file = debug_dir + "/" + base_filename + ".ast.txt";
    std::string ir_file = debug_dir + "/" + base_filename + ".ir.txt";
    std::string ir_opt_file = debug_dir + "/" + base_filename + ".ir_opt.txt";
    std::string bytecode_file = debug_dir + "/" + base_filename + ".bytecode.txt";
    // 辅助函数：写入调试文件
    auto write_debug_file = [](const std::string& filename, const std::string& content) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << content;
            file.close();
            std::cout << color(C_BRIGHT_BLACK) << "  [DEBUG] " << color(C_BRIGHT_MAGENTA) << filename << RESET << std::endl;
        }
    };
    
    // 辅助函数：转义JSON字符串
    auto escape_json = [](const std::string& s) -> std::string {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '\\': result += "\\\\"; break;
                case '"': result += "\\\""; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    };
    
    if (verbose) {
        std::cout << std::endl;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!verbose) {
        print_banner();
    }
    
    if (verbose) std::cout << color(C_BRIGHT_CYAN) << "[1/5] " << color(C_WHITE) << "Reading source file..." << RESET << std::endl;
    std::string source = read_file(input_file);
    if (source.empty()) {
        return 1;
    }
    
    if (verbose) {
        std::cout << color(C_BRIGHT_CYAN) << "[2/5] " << color(C_WHITE) << "Tokenizing..." << RESET << std::endl;
    }
    csgo::Tokenizer tokenizer(source);
    std::vector<csgo::Token> tokens = tokenizer.tokenize();
    if (tokenizer.hasError()) {
        std::cerr << color(C_BRIGHT_RED) << "Lexical error: " << RESET << tokenizer.errorMessage() << std::endl;
        return 1;
    }
    if (verbose) std::cout << "  " << color(C_BRIGHT_GREEN) << "->" << RESET << " Generated " << color(C_YELLOW) << tokens.size() << RESET << " tokens" << std::endl;
    
    // 输出Tokens到调试文件
    if (debug_output) {
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"file\": \"" << input_file << "\",\n";
        ss << "  \"generated\": \"" << __DATE__ << " " << __TIME__ << "\",\n";
        ss << "  \"total\": " << tokens.size() << ",\n";
        ss << "  \"tokens\": [\n";
        for (size_t i = 0; i < tokens.size(); i++) {
            const auto& tok = tokens[i];
            ss << "    {";
            ss << "\"index\":" << i << ", ";
            ss << "\"type\":\"" << csgo::tokenTypeName(tok.type) << "\", ";
            ss << "\"text\":\"" << escape_json(tok.text) << "\", ";
            ss << "\"line\":" << tok.line << ", ";
            ss << "\"col\":" << tok.column << "}";
            if (i < tokens.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << "  ]\n";
        ss << "}\n";
        write_debug_file(tokens_file, ss.str());
    }
    
    if (verbose) {
        std::cout << color(C_BRIGHT_CYAN) << "[3/5] " << color(C_WHITE) << "Parsing..." << RESET << std::endl;
    }
    csgo::Parser parser(tokens);
    std::unique_ptr<csgo::Module> ast = parser.parseModule();
    if (parser.hasError()) {
        std::cerr << color(C_BRIGHT_RED) << "Parse error: " << RESET << parser.errorMessage() << std::endl;
        return 1;
    }
    if (verbose) std::cout << "  " << color(C_BRIGHT_GREEN) << "->" << RESET << " AST parsed successfully" << std::endl;
    // 输出AST到调试文件
    if (debug_output && ast) {
        std::stringstream ss;
        ss << "// AST for: " << input_file << "\n";
        ss << "// Generated: " << __DATE__ << " " << __TIME__ << "\n\n";
        ss << ast->toString(0);  // 使用带缩进的重载，从0级缩进开始
        write_debug_file(ast_file, ss.str());
    }
    
    if (verbose) {
        std::cout << color(C_BRIGHT_CYAN) << "[4/5] " << color(C_WHITE) << "Building IR and optimizing..." << RESET << std::endl;
    }
    
    try {
        csgo::ir::IRBuilder ir_builder(input_file);
        ir_builder.build_from_module(*ast);
        std::unique_ptr<csgo::ir::IRModule> ir_module = ir_builder.build();
        

        
        if (verbose) {
            size_t instr_count = 0;
            for (const auto& block : ir_module->blocks()) {
                instr_count += block->instruction_count();
            }
            std::cout << "  " << color(C_BRIGHT_GREEN) << "->" << RESET << " IR built: " << color(C_YELLOW) << instr_count << RESET << " instructions in " << color(C_YELLOW) << ir_module->blocks().size() << RESET << " blocks" << std::endl;
        }
        
        csgo::ir::Optimizer::Level opt_level;
        switch (optimization_level) {
            case 0: opt_level = csgo::ir::Optimizer::Level::None; break;
            case 1: opt_level = csgo::ir::Optimizer::Level::Basic; break;
            case 2: opt_level = csgo::ir::Optimizer::Level::Medium; break;
            default: opt_level = csgo::ir::Optimizer::Level::Aggressive; break;
        }
        
        // 输出优化前的IR到调试文件
        if (debug_output && ir_module) {
            std::stringstream ss;
            ss << "// IR (before optimization) for: " << input_file << "\n";
            ss << "// Generated: " << __DATE__ << " " << __TIME__ << "\n\n";
            for (const auto& block : ir_module->blocks()) {
                ss << "Block: " << block->label() << "\n";
                for (const auto& instr_ptr : block->instructions()) {
                    ss << "  " << instr_ptr->to_string() << "\n";
                }
                ss << "\n";
            }
            write_debug_file(ir_file, ss.str());
        }
        
        csgo::ir::Optimizer optimizer(opt_level);
        optimizer.optimize(ir_module.get());
        
        if (verbose) {
            std::cout << "  " << color(C_BRIGHT_GREEN) << "->" << RESET << " Optimization complete (level -O" << optimization_level << ")" << std::endl;
        }
        
        // 输出优化后的IR到调试文件
        if (debug_output && ir_module) {
            std::stringstream ss;
            ss << "// IR (after optimization) for: " << input_file << "\n";
            ss << "// Generated: " << __DATE__ << " " << __TIME__ << "\n\n";
            for (const auto& block : ir_module->blocks()) {
                ss << "Block: " << block->label() << "\n";
                for (const auto& instr_ptr : block->instructions()) {
                    ss << "  " << instr_ptr->to_string() << "\n";
                }
                ss << "\n";
            }
            write_debug_file(ir_opt_file, ss.str());
        }
        
        if (verbose) {
            std::cout << color(C_BRIGHT_CYAN) << "[5/5] " << color(C_WHITE) << "Generating bytecode..." << RESET << std::endl;
        }
        csgo::bytecode::BytecodeGenerator bytecode_generator;
        std::unique_ptr<csgo::bytecode::Chunk> chunk = bytecode_generator.generate(ir_module.get());
        
        if (!chunk) {
            std::cerr << color(C_BRIGHT_RED) << "Error: " << RESET << "Bytecode generation failed" << std::endl;
            return 1;
        }

        // 输出字节码调试文件（IR 到字节映射，hex view）
        if (debug_output) {
            std::stringstream ss;
            ss << "// Bytecode Debug: IR to Byte Mapping for " << input_file << "\n";
            ss << "// Generated: " << __DATE__ << " " << __TIME__ << "\n\n";
            
            const auto& mappings = bytecode_generator.get_instr_mappings();
            size_t total_bytes = 0;
            
            for (size_t i = 0; i < mappings.size(); ++i) {
                const auto& m = mappings[i];
                ss << "// -----" << "\n";
                ss << "// IR Instr " << i << ": " << m.ir_instr << "\n";
                ss << "// Offset  : " << m.byte_offset << " (" << std::hex << m.byte_offset << std::dec << "h)" << "\n";
                ss << "// Bytes   : ";
                
                // Hex view 格式
                std::ostringstream hex_ss;
                std::ostringstream ascii_ss;
                for (size_t j = 0; j < m.bytes.size(); ++j) {
                    uint8_t b = m.bytes[j];
                    hex_ss << std::hex << std::setfill('0') << std::setw(2) << (int)b << " ";
                    if (b >= 32 && b <= 126) {
                        ascii_ss << (char)b;
                    } else {
                        ascii_ss << '.';
                    }
                }
                ss << hex_ss.str();
                // 对齐
                for (size_t pad = m.bytes.size(); pad < 8; ++pad) {
                    ss << "   ";
                }
                ss << " | " << ascii_ss.str() << " |" << "\n";
                ss << "// Bytecode: " << m.bytecode_instr << "\n";
                
                total_bytes += m.bytes.size();
            }
            
            ss << "// -----" << "\n";
            ss << "// Total bytes: " << total_bytes << "\n";
            ss << "// Total instructions: " << mappings.size() << "\n\n";
            
            // 附加完整 hex dump
            ss << "// ====================================" << "\n";
            ss << "// Full Bytecode Hex Dump" << "\n";
            ss << "// ====================================" << "\n";
            
            std::vector<uint8_t> bytecode = chunk->serialize();
            for (size_t i = 0; i < bytecode.size(); ++i) {
                if (i % 16 == 0) {
                    ss << std::hex << std::setfill('0') << std::setw(4) << i << ": ";
                }
                ss << std::setw(2) << (int)bytecode[i] << " ";
                if ((i + 1) % 16 == 0 || i == bytecode.size() - 1) {
                    // 补充空格对齐
                    for (size_t p = (i + 1) % 16; p < 16 && i == bytecode.size() - 1; ++p) {
                        ss << "   ";
                    }
                    ss << "| ";
                    // ASCII 表示
                    size_t start = (i / 16) * 16;
                    for (size_t j = start; j <= i; ++j) {
                        uint8_t b = bytecode[j];
                        if (b >= 32 && b <= 126) {
                            ss << (char)b;
                        } else {
                            ss << '.';
                        }
                    }
                    ss << "\n";
                }
            }
            
            ss << std::dec;  // 恢复十进制
            
            write_debug_file(bytecode_file, ss.str());
        }

        
        std::vector<uint8_t> bytecode = chunk->serialize();
        
        if (!write_file(output_file, bytecode)) {
            std::cerr << color(C_BRIGHT_RED) << "Error: " << RESET << "Cannot write output file: " << output_file << std::endl;
            return 1;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (verbose) {
            std::cout << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "+-- " << BOLD << "Compilation Summary" << RESET << color(C_BRIGHT_GREEN) << " ----------------------------------------+" << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << "  " << color(C_BRIGHT_GREEN) << "[OK]" << RESET << " Compilation successful!" << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << "  " << color(C_BRIGHT_CYAN) << "Output:" << RESET << "       " << color(C_WHITE) << output_file << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << "  " << color(C_BRIGHT_CYAN) << "Size:" << RESET << "       " << color(C_YELLOW) << bytecode.size() << RESET << " bytes" << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << "  " << color(C_BRIGHT_CYAN) << "Instructions:" << RESET << " " << color(C_YELLOW) << chunk->instruction_count << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << "  " << color(C_BRIGHT_CYAN) << "Stack size:" << RESET << " " << color(C_YELLOW) << chunk->stacksize << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << "  " << color(C_BRIGHT_CYAN) << "Constants:" << RESET << "  " << color(C_YELLOW) << chunk->consts.size() << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << "  " << color(C_BRIGHT_CYAN) << "Names:" << RESET << "       " << color(C_YELLOW) << chunk->names.size() << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "|" << RESET << "  " << color(C_BRIGHT_MAGENTA) << "Time:" << RESET << "       " << color(C_YELLOW) << duration.count() << " ms" << RESET << std::endl;
            std::cout << color(C_BRIGHT_GREEN) << "+" << std::string(56, '-') << "+" << RESET << std::endl;
        } else {
            std::cout << color(C_BRIGHT_GREEN) << "[OK] " << RESET << color(C_WHITE) << output_file << RESET << color(C_BRIGHT_BLACK) << " (" << color(C_YELLOW) << bytecode.size() << color(C_BRIGHT_BLACK) << " bytes, " << color(C_YELLOW) << duration.count() << "ms)" << RESET << std::endl;
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << color(C_BRIGHT_RED) << "Error: " << RESET << e.what() << std::endl;
        return 1;
    }
}