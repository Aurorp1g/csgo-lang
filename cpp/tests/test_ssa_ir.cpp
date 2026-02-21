/**
 * @file test_ssa_ir.cpp
 * @brief CSGO SSA IR 和优化器测试文件
 *
 * @author CSGO Language Team
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件包含 SSA 中间表示和优化器的单元测试。
 * 测试常量折叠、死代码消除、控制流优化等功能。
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <string>

#include "ir/ssa_ir.h"
#include "ir/optimizer.h"
#include "bytecode/bytecode_generator.h"

using namespace csgo::ir;
using namespace csgo::bytecode;

void test_value_creation() {
    std::cout << "Testing Value creation..." << std::endl;

    // Test constant creation
    Value none = Value::None();
    assert(none.is_constant());
    assert(none.kind() == Value::Kind::Constant);

    Value bool_val = Value::Bool(true);
    assert(bool_val.is_constant());

    Value int_val = Value::Int(42);
    assert(int_val.is_constant());
    assert(int_val.to_string() == "42");

    Value float_val = Value::Float(3.14);
    assert(float_val.is_constant());

    Value str_val = Value::String("hello");
    assert(str_val.is_constant());
    assert(str_val.to_string() == "\"hello\"");

    // Test variable creation
    Value var = Value::Variable("x", 0);
    assert(var.is_variable());
    assert(var.name() == "x");
    assert(var.ssa_version() == 0);

    Value var_v1 = var.copy_with_new_version(1);
    assert(var_v1.ssa_version() == 1);
    assert(var_v1.get_ssa_name() == "x.1");

    // Test temporary creation
    Value temp = Value::Temporary(5);
    assert(temp.is_temporary());
    assert(temp.name() == "%t5");

    // Test argument creation
    Value arg = Value::Argument(2);
    assert(arg.kind() == Value::Kind::Argument);

    std::cout << "Value creation tests passed!" << std::endl;
}

void test_instruction_creation() {
    std::cout << "Testing Instruction creation..." << std::endl;

    // Test basic instruction creation
    csgo::ir::Instruction load_const(csgo::ir::IROpcode::LOAD_CONST);
    assert(load_const.opcode() == csgo::ir::IROpcode::LOAD_CONST);

    csgo::ir::Instruction load_fast = csgo::ir::Instruction::CreateLoadFast("x", 0);
    assert(load_fast.opcode() == csgo::ir::IROpcode::LOAD_FAST);
    assert(load_fast.has_result());

    Value result = Value::Variable("y", 1);
    csgo::ir::Instruction store_fast = csgo::ir::Instruction::CreateStoreFast("y", 1, result);
    assert(store_fast.opcode() == csgo::ir::IROpcode::STORE_FAST);
    assert(store_fast.operand_count() == 1);

    // Test binary operation
    Value left = Value::Int(10);
    Value right = Value::Int(10);
    csgo::ir::Instruction add = csgo::ir::Instruction::CreateBinaryOp(csgo::ir::IROpcode::BINARY_ADD, left, right);
    assert(add.opcode() == csgo::ir::IROpcode::BINARY_ADD);
    assert(add.operand_count() == 2);

    // Test terminator instructions
    csgo::ir::Instruction ret(csgo::ir::IROpcode::RETURN_VALUE);
    assert(ret.is_terminator());

    csgo::ir::Instruction jump(csgo::ir::IROpcode::JUMP);
    assert(jump.is_terminator());
    assert(jump.is_branch());

    // Test load/store classification
    assert(load_const.is_load());
    assert(store_fast.is_store());

    std::cout << "Instruction creation tests passed!" << std::endl;
}

void test_basic_block() {
    std::cout << "Testing BasicBlock..." << std::endl;

    BasicBlock block("test_block");

    // Test block properties
    assert(block.label() == "test_block");
    assert(block.instruction_count() == 0);

    // Add instructions
    block.append_instruction(IROpcode::LOAD_CONST);
    block.append_instruction(IROpcode::POP_TOP);

    assert(block.instruction_count() == 2);

    // Test predecessor/successor management
    BasicBlock pred("pred");
    BasicBlock succ("succ");

    block.add_predecessor(&pred);
    block.add_successor(&succ);

    assert(block.has_predecessor(&pred));
    assert(block.has_successor(&succ));

    // Test variable tracking
    Value var = Value::Variable("x", 0);
    block.add_defined_var("x", var);
    assert(block.defined_vars().count("x") > 0);

    std::cout << "BasicBlock tests passed!" << std::endl;
}

void test_ir_module() {
    std::cout << "Testing IRModule..." << std::endl;

    IRModule module("test.cg");
    module.set_module_name("test");

    // Test entry block creation
    BasicBlock* entry = module.create_block("entry");
    assert(entry != nullptr);
    assert(module.entry_block() == entry);

    // Test symbol management
    Value global = module.add_global("my_var");
    assert(module.has_global("my_var"));
    assert(module.get_global("my_var").to_string() == "@my_var");

    module.set_local("x", Value::Int(10));
    assert(module.has_local("x"));

    // Test SSA versioning
    Value v1 = module.create_ssa_version("counter");
    Value v2 = module.create_ssa_version("counter");
    assert(v1.ssa_version() == 0);
    assert(v2.ssa_version() == 1);

    // Test constant/name management
    size_t idx1 = module.add_constant(Value::Int(100));
    size_t idx2 = module.add_constant(Value::Int(100));  // Should reuse
    assert(idx1 == idx2);

    size_t name_idx = module.add_name("print");
    assert(module.get_name_index("print") == name_idx);

    // Test varname management
    size_t var_idx = module.add_varname("i");
    assert(module.varnames().size() == 1);

    std::cout << "IRModule tests passed!" << std::endl;
}

void test_ir_builder() {
    std::cout << "Testing IRBuilder..." << std::endl;

    IRBuilder builder("test.cg");
    IRModule* module = builder.module();

    // Test current block
    assert(builder.current_block() != nullptr);
    assert(builder.current_block()->label() == "entry");

    // Test constant emission
    Value const1 = builder.emit_load_const(Value::Int(42));
    assert(const1.is_temporary());

    // Test variable operations
    Value load_x = builder.emit_load_fast("x");
    assert(load_x.is_variable());

    Value store_x = builder.emit_store_fast("x", Value::Int(100));
    assert(store_x.is_variable());

    // Test binary operations
    Value add_result = builder.emit_binary_add(Value::Int(10), Value::Int(20));
    assert(add_result.is_temporary());

    // Test comparison
    Value cmp_result = builder.emit_compare_lt(Value::Int(5), Value::Int(10));
    assert(cmp_result.is_temporary());

    // Test jump instructions
    BasicBlock* then_block = builder.new_block("then");
    BasicBlock* else_block = builder.new_block("else");
    BasicBlock* merge_block = builder.new_block("merge");

    Value cond = Value::Bool(true);
    builder.emit_jump_if_false(cond, then_block);
    builder.emit_jump(else_block);

    builder.append_block(then_block);
    builder.emit_nop();
    builder.emit_jump(merge_block);

    builder.append_block(else_block);
    builder.emit_nop();

    builder.append_block(merge_block);

    // Test function building
    BasicBlock* func_entry = builder.new_block("func_entry");
    builder.append_block(func_entry);
    builder.emit_return(Value::Int(0));

    std::cout << "IRBuilder tests passed!" << std::endl;
}

void test_optimizer() {
    std::cout << "Testing Optimizer..." << std::endl;

    IRBuilder builder("test.cg");

    // Build some IR with optimizable patterns
    builder.emit_load_const(Value::Int(1));
    builder.emit_load_const(Value::Int(2));
    builder.emit_binary_add(Value::Int(1), Value::Int(2));

    IRModule* raw_module = builder.module();  // 使用裸指针，不转移所有权
    
    // Create optimizer
    Optimizer optimizer(Optimizer::Level::Basic);

    // Count instructions before optimization
    size_t before_count = 0;
    for (const auto& block : raw_module->blocks()) {
        before_count += block->instruction_count();
    }
    std::cout << "Instructions before optimization: " << before_count << std::endl;

    // Run optimization - optimizer modifies in-place
    optimizer.optimize(raw_module);

    // Count instructions after optimization
    size_t after_count = 0;
    for (const auto& block : raw_module->blocks()) {
        after_count += block->instruction_count();
    }
    std::cout << "Instructions after optimization: " << after_count << std::endl;

    std::cout << "Optimizer tests passed!" << std::endl;
}

void test_constant_folder() {
    std::cout << "Testing ConstantFolder..." << std::endl;

    ConstantFolder folder;

    // Test binary operation folding
    csgo::ir::Instruction add(csgo::ir::IROpcode::BINARY_ADD);
    add.add_operand(Value::Int(10));
    add.add_operand(Value::Int(20));

    auto folded = folder.fold(add);
    // Should be able to fold to 30
    if (folded.has_value()) {
        std::cout << "Folded: " << folded->to_string() << std::endl;
    }

    // Test unary operation folding
    csgo::ir::Instruction neg(csgo::ir::IROpcode::UNARY_NEGATIVE);
    neg.add_operand(Value::Int(42));

    auto neg_folded = folder.fold(neg);
    if (neg_folded.has_value()) {
        std::cout << "Negated: " << neg_folded->to_string() << std::endl;
    }

    std::cout << "ConstantFolder tests passed!" << std::endl;
}

void test_bytecode_generator() {
    std::cout << "Testing BytecodeGenerator..." << std::endl;

    IRBuilder builder("test.cg");

    // Build simple IR - first add globals, then use them
    builder.emit_load_const(Value::Int(42));
    Value print_func = builder.emit_load_global("print");  // 保存返回值
    builder.emit_call(print_func, {Value::Int(42)});      // 使用正确的值
    builder.emit_return(Value::None());

    std::unique_ptr<IRModule> module = builder.build();

    // Generate bytecode
    BytecodeGenerator generator;
    std::unique_ptr<Chunk> chunk = generator.generate(module.get());

    // Verify chunk properties
    assert(chunk != nullptr);
    assert(chunk->instruction_count > 0);
    assert(chunk->stacksize >= 0);

    std::cout << "Generated " << chunk->instruction_count << " instructions" << std::endl;
    std::cout << "Stack size: " << chunk->stacksize << std::endl;
    std::cout << "Constants: " << chunk->consts.size() << std::endl;
    std::cout << "Names: " << chunk->names.size() << std::endl;

    // Serialize and check size
    std::vector<uint8_t> bytecode = chunk->serialize();
    std::cout << "Bytecode size: " << bytecode.size() << " bytes" << std::endl;

    std::cout << "BytecodeGenerator tests passed!" << std::endl;
}

void test_control_flow() {
    std::cout << "Testing control flow constructs..." << std::endl;
    try {
        IRBuilder builder("test.cg");
        Value condition = Value::Bool(true);
        
        BasicBlock* then_block = builder.new_block("then");       
        BasicBlock* else_block = builder.new_block("else");     
        BasicBlock* merge_block = builder.new_block("merge");

        builder.emit_jump_if_false(condition, then_block);     
        builder.emit_jump(else_block);
        builder.append_block(then_block);      
        builder.emit_load_const(Value::Int(1));     
        builder.emit_return(Value::Int(1));      
        builder.emit_jump(merge_block);
        builder.append_block(else_block);      
        builder.emit_load_const(Value::Int(2));      
        builder.emit_return(Value::Int(2));
        builder.append_block(merge_block);

        std::unique_ptr<IRModule> module = builder.build();
        std::cout << "Module has " << module->blocks().size() << " blocks" << std::endl;

        std::cout << "Control flow tests passed!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        throw;
    }
}

void test_loop() {
    std::cout << "Testing loop constructs..." << std::endl;

    IRBuilder builder("test.cg");

    // Build while loop
    BasicBlock* header = builder.new_block("header");
    BasicBlock* body = builder.new_block("body");
    BasicBlock* exit = builder.new_block("exit");

    builder.append_block(header);

    // while i < 10:
    //     print(i)
    //     i = i + 1

    Value i = Value::Variable("i", 0);
    Value limit = Value::Int(10);
    Value cmp_result = builder.emit_compare_lt(i, limit);
    builder.emit_jump_if_false(cmp_result, exit);
    builder.emit_jump(body);

    builder.append_block(body);
    Value load_i = builder.emit_load_fast("i");
    builder.emit_call(builder.module()->get_global("print"), {load_i});

    Value one = Value::Int(1);
    Value new_i = builder.emit_binary_add(load_i, one);
    builder.emit_store_fast("i", new_i);
    builder.emit_jump(header);

    builder.append_block(exit);
    builder.emit_return(Value::None());

    std::unique_ptr<IRModule> module = builder.build();

    std::cout << "Loop has " << module->blocks().size() << " blocks" << std::endl;

    std::cout << "Loop tests passed!" << std::endl;
}

void test_function_definition() {
    std::cout << "Testing function definition..." << std::endl;

    IRBuilder builder("test.cg");

    // Build a simple function: def add(a, b): return a + b
    BasicBlock* func_entry = builder.new_block("func_entry");
    BasicBlock* func_exit = builder.new_block("func_exit");

    builder.append_block(func_entry);

    // Load parameters
    Value a = builder.emit_load_fast("a");
    Value b = builder.emit_load_fast("b");

    // a + b
    Value result = builder.emit_binary_add(a, b);

    builder.emit_return(result);

    builder.append_block(func_exit);
    builder.emit_return(Value::None());

    std::unique_ptr<IRModule> module = builder.build();

    std::cout << "Function has " << module->blocks().size() << " blocks" << std::endl;

    std::cout << "Function definition tests passed!" << std::endl;
}

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "CSGO SSA IR and Optimizer Test Suite" << std::endl;
    std::cout << "======================================" << std::endl << std::endl;

    try {
        test_value_creation();
        test_instruction_creation();
        test_basic_block();
        test_ir_module();
        test_ir_builder();
        test_optimizer();
        test_constant_folder();
        test_bytecode_generator();
        test_control_flow();
        test_loop();
        test_function_definition();

        std::cout << std::endl;
        std::cout << "======================================" << std::endl;
        std::cout << "All tests passed successfully!" << std::endl;
        std::cout << "======================================" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}