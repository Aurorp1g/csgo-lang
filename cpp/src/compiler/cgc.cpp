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

#include "lexer/tokenizer.h"
#include "parser/parser.h"
#include "ast/ast_node.h"
#include "ir/ssa_ir.h"
#include "ir/optimizer.h"
#include "bytecode/bytecode_generator.h"

using namespace csgo;

void print_usage(const char* program_name) {
    std::cout << "CSGO Language Compiler v1.0" << std::endl;
    std::cout << "Usage: " << program_name << " <input.cg> [output.cgb]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help     Show this help message" << std::endl;
    std::cout << "  -O0            No optimization" << std::endl;
    std::cout << "  -O1            Basic optimization (default)" << std::endl;
    std::cout << "  -O2            Medium optimization" << std::endl;
    std::cout << "  -O3            Aggressive optimization" << std::endl;
    std::cout << "  -v, --verbose  Verbose output" << std::endl;
}

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filename << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool write_file(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write file: " << filename << std::endl;
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
        } else if (arg[0] != '-') {
            if (input_file.empty()) {
                input_file = arg;
            } else if (output_file.empty()) {
                output_file = arg;
            }
        }
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: No input file specified" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    if (output_file.empty()) {
        size_t pos = input_file.rfind('.');
        if (pos != std::string::npos) {
            output_file = input_file.substr(0, pos) + ".cgb";
        } else {
            output_file = input_file + ".cgb";
        }
    }
    
    if (verbose) {
        std::cout << "CSGO Language Compiler" << std::endl;
        std::cout << "=======================" << std::endl;
        std::cout << "Input:  " << input_file << std::endl;
        std::cout << "Output: " << output_file << std::endl;
        std::cout << "Optimization: -O" << optimization_level << std::endl;
        std::cout << std::endl;
    }
    
    if (verbose) std::cout << "[1/5] Reading source file..." << std::endl;
    std::string source = read_file(input_file);
    if (source.empty()) {
        return 1;
    }
    
    if (verbose) {
        std::cout << "[2/5] Tokenizing..." << std::endl;
    }
    csgo::Tokenizer tokenizer(source);
    std::vector<csgo::Token> tokens = tokenizer.tokenize();
    if (tokenizer.hasError()) {
        std::cerr << "Lexical error: " << tokenizer.errorMessage() << std::endl;
        return 1;
    }
    if (verbose) std::cout << "  - Generated " << tokens.size() << " tokens" << std::endl;
    
    if (verbose) {
        std::cout << "[3/5] Parsing..." << std::endl;
    }
    csgo::Parser parser(tokens);
    std::unique_ptr<csgo::Module> ast = parser.parseModule();
    if (parser.hasError()) {
        std::cerr << "Parse error: " << parser.errorMessage() << std::endl;
        return 1;
    }
    if (verbose) std::cout << "  - AST parsed successfully" << std::endl;
    
    if (verbose) {
        std::cout << "[4/5] Building IR and optimizing..." << std::endl;
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
            std::cout << "  - IR built: " << instr_count << " instructions in " 
                      << ir_module->blocks().size() << " blocks" << std::endl;
        }
        
        csgo::ir::Optimizer::Level opt_level;
        switch (optimization_level) {
            case 0: opt_level = csgo::ir::Optimizer::Level::None; break;
            case 1: opt_level = csgo::ir::Optimizer::Level::Basic; break;
            case 2: opt_level = csgo::ir::Optimizer::Level::Medium; break;
            default: opt_level = csgo::ir::Optimizer::Level::Aggressive; break;
        }
        
        csgo::ir::Optimizer optimizer(opt_level);
        optimizer.optimize(ir_module.get());
        
        if (verbose) {
            std::cout << "  - Optimization complete" << std::endl;
        }
        
        if (verbose) {
            std::cout << "[5/5] Generating bytecode..." << std::endl;
        }
        csgo::bytecode::BytecodeGenerator bytecode_generator;
        std::unique_ptr<csgo::bytecode::Chunk> chunk = bytecode_generator.generate(ir_module.get());
        
        if (!chunk) {
            std::cerr << "Error: Bytecode generation failed" << std::endl;
            return 1;
        }
        
        std::vector<uint8_t> bytecode = chunk->serialize();
        
        if (!write_file(output_file, bytecode)) {
            return 1;
        }
        
        if (verbose) {
            std::cout << std::endl;
            std::cout << "Compilation successful!" << std::endl;
            std::cout << "  - Output file: " << output_file << std::endl;
            std::cout << "  - Bytecode size: " << bytecode.size() << " bytes" << std::endl;
            std::cout << "  - Instructions: " << chunk->instruction_count << std::endl;
            std::cout << "  - Stack size: " << chunk->stacksize << std::endl;
            std::cout << "  - Constants: " << chunk->consts.size() << std::endl;
            std::cout << "  - Names: " << chunk->names.size() << std::endl;
        } else {
            std::cout << "Compiled successfully: " << output_file << std::endl;
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}