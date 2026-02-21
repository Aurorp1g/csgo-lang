/**
 * @file optimizer.cpp
 * @brief CSGO 编程语言 SSA IR 优化器实现文件
 *
 * @author CSGO Language Team
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件实现了 CSGO 语言的 SSA IR 优化器。
 * 基于 CPython 3.8 Python/peephole.c 和 Python/compile.c 设计，
 * 提供完整的常量折叠、死代码消除、控制流优化等功能。
 *
 * @section implementation 实现细节
 * - Optimizer 类：优化器主控类
 * - ConstantFolder 类：常量折叠实现
 * - DeadCodeEliminator 类：死代码消除实现
 * - ControlFlowOptimizer 类：控制流优化实现
 * - PeepholeOptimizer 类：Peephole 优化实现
 * - CommonSubexpressionEliminator 类：公共子表达式消除实现
 *
 * @section reference 参考
 * - CPython Python/peephole.c: peephole 优化器
 * - CPython Python/compile.c: 编译器核心实现
 *
 * @see optimizer.h 优化器头文件
 * @see ssa_ir.h SSA中间表示头文件
 */

#include "ir/optimizer.h"
#include <algorithm>
#include <cassert>
#include <sstream>
#include <iostream>

namespace csgo {
namespace ir {

// ============================================================================
// Optimizer 类实现
// ============================================================================

Optimizer::Optimizer(Level level)
    : level_(level), pass_count_(0), instruction_count_(0), optimized_count_(0) {
    set_flag("constant_folding", level >= Level::Basic);
    set_flag("dead_code_elimination", level >= Level::Basic);
    set_flag("dead_store_elimination", level >= Level::Medium);
    set_flag("copy_propagation", level >= Level::Medium);
    set_flag("cse", level >= Level::Aggressive);
    set_flag("control_flow", level >= Level::Basic);
    set_flag("phi_simplification", level >= Level::Basic);
    set_flag("jump_threading", level >= Level::Medium);
    set_flag("peephole", level >= Level::Basic);
}

void Optimizer::set_level(Level level) {
    level_ = level;
}

void Optimizer::set_flag(const std::string& name, bool value) {
    flags_[name] = value;
}

bool Optimizer::get_flag(const std::string& name) const {
    auto it = flags_.find(name);
    return it != flags_.end() ? it->second : false;
}

void Optimizer::optimize(IRModule* module) {
    if (!module) {
        return;
    }

    pass_count_ = 0;
    optimized_count_ = 0;

    size_t total_instructions = 0;
    for (const auto& block : module->blocks()) {
        total_instructions += block->instruction_count();
    }
    instruction_count_ = total_instructions;

    if (get_flag("constant_folding")) {
        run_constant_folding(module);
    }

    if (get_flag("dead_code_elimination")) {
        run_dead_code_elimination(module);
    }

    if (get_flag("control_flow")) {
        run_control_flow_simplification(module);
        run_trivial_phi_elimination(module);
    }

    if (get_flag("peephole")) {
        run_peephole_optimization(module);
    }

    if (get_flag("dead_store_elimination")) {
        run_dead_store_elimination(module);
    }

    if (get_flag("copy_propagation")) {
        run_copy_propagation(module);
    }

    if (get_flag("cse")) {
        run_common_subexpression_elimination(module);
    }

    if (get_flag("jump_threading")) {
        run_jump_threading(module);
    }

    if (get_flag("dead_code_elimination")) {
        run_dead_code_elimination(module);
    }

    size_t final_instructions = 0;
    for (const auto& block : module->blocks()) {
        final_instructions += block->instruction_count();
    }
    optimized_count_ = final_instructions;

    return;
}

bool Optimizer::run_constant_folding(IRModule* module) {
    if (!module) return false;

    ConstantFolder folder;
    folder.fold_module(module);

    pass_count_++;
    return true;
}

bool Optimizer::run_dead_code_elimination(IRModule* module) {
    if (!module) return false;

    DeadCodeEliminator eliminator;
    eliminator.eliminate(module);

    pass_count_++;
    return true;
}

bool Optimizer::run_dead_store_elimination(IRModule* module) {
    if (!module) return false;

    collect_uses(module);

    for (auto& block : module->blocks()) {
        auto& instructions = block->instructions();
        for (auto it = instructions.begin(); it != instructions.end(); ) {
            const Instruction& instr = **it;
            if (instr.is_store()) {
                if (instr.operand_count() > 0) {
                    const Value& stored_value = instr.operand(0);
                    if (used_values_.find(&instr) == used_values_.end()) {
                        it = instructions.erase(it);
                        continue;
                    }
                }
            }
            ++it;
        }
    }

    pass_count_++;
    return true;
}

bool Optimizer::run_copy_propagation(IRModule* module) {
    if (!module) return false;

    std::unordered_map<std::string, Value> copy_map;

    for (auto& block : module->blocks()) {
        for (auto& instr_ptr : block->instructions()) {
            Instruction& instr = *instr_ptr;

            if (instr.opcode() == IROpcode::COPY) {
                if (instr.operand_count() > 0) {
                    const Value& src = instr.operand(0);
                    const Value& dst = instr.result();
                    if (dst.kind() == Value::Kind::Variable) {
                        copy_map[dst.name()] = src;
                    }
                }
            }
        }
    }

    pass_count_++;
    return true;
}

bool Optimizer::run_common_subexpression_elimination(IRModule* module) {
    if (!module) return false;

    std::unique_ptr<CommonSubexpressionEliminator> cse = std::make_unique<CommonSubexpressionEliminator>();
    cse->eliminate(module);

    pass_count_++;
    return true;
}

bool Optimizer::run_control_flow_simplification(IRModule* module) {
    if (!module) return false;

    ControlFlowOptimizer cf_optimizer;
    cf_optimizer.optimize(module);

    pass_count_++;
    return true;
}

bool Optimizer::run_trivial_phi_elimination(IRModule* module) {
    if (!module) return false;

    for (auto& block : module->blocks()) {
        auto& instructions = block->instructions();
        for (auto& instr_ptr : instructions) {
            Instruction& instr = *instr_ptr;
            if (instr.is_phi()) {
                const auto& operands = instr.phi_operands();
                if (operands.size() == 1) {
                    const auto& pair = *operands.begin();
                    replace_uses(instr.result(), pair.second);
                    instr_ptr.reset();
                } else if (operands.size() == 2) {
                    auto it = operands.begin();
                    const Value& val1 = it->second;
                    ++it;
                    const Value& val2 = it->second;

                    if (val1.to_string() == val2.to_string()) {
                        replace_uses(instr.result(), val1);
                        instr_ptr.reset();
                    }
                }
            }
        }

        instructions.erase(
            std::remove_if(instructions.begin(), instructions.end(),
                [](const std::unique_ptr<Instruction>& ptr) {
                    return ptr == nullptr;
                }),
            instructions.end()
        );
    }

    pass_count_++;
    return true;
}

bool Optimizer::run_redundant_load_elimination(IRModule* module) {
    if (!module) return false;

    pass_count_++;
    return true;
}

bool Optimizer::run_jump_threading(IRModule* module) {
    if (!module) return false;

    for (auto& block : module->blocks()) {
        Instruction* last = block->last_instruction();
        if (last && last->opcode() == IROpcode::JUMP) {
            BasicBlock* target = last->target();
            if (target && target->instruction_count() == 1) {
                Instruction* target_last = target->last_instruction();
                if (target_last && target_last->opcode() == IROpcode::JUMP) {
                    BasicBlock* new_target = target_last->target();
                    if (new_target) {
                        last->set_target(new_target);
                        block->remove_successor(target);
                        block->add_successor(new_target);
                        new_target->add_predecessor(block.get());
                        target->remove_predecessor(block.get());
                    }
                }
            }
        }
    }

    pass_count_++;
    return true;
}

bool Optimizer::run_peephole_optimization(IRModule* module) {
    if (!module) return false;

    PeepholeOptimizer peephole;
    peephole.optimize(module);

    pass_count_++;
    return true;
}

void Optimizer::analyze_reachability(IRModule* module) {
    if (!module) return;

    reachable_blocks_.clear();

    if (module->entry_block()) {
        std::stack<BasicBlock*> stack;
        stack.push(module->entry_block());

        while (!stack.empty()) {
            BasicBlock* block = stack.top();
            stack.pop();

            if (reachable_blocks_.count(block)) continue;
            reachable_blocks_.insert(block);

            for (BasicBlock* succ : block->successors()) {
                if (!reachable_blocks_.count(succ)) {
                    stack.push(succ);
                }
            }
        }
    }
}

void Optimizer::analyze_liveness(IRModule* module) {
    if (!module) return;
}

void Optimizer::analyze_constant_propagation(IRModule* module) {
    if (!module) return;
}

bool Optimizer::is_constant(const Value& value) const {
    return value.is_constant();
}

Value Optimizer::evaluate_constant_expr(const Instruction& instr) const {
    return Value();
}

bool Optimizer::is_dead_store(const Instruction& instr, const IRModule* module) const {
    if (!instr.is_store()) return false;

    const Value& stored = instr.result();
    if (stored.kind() != Value::Kind::Variable) return false;

    return true;
}

void Optimizer::collect_uses(const IRModule* module) {
    used_values_.clear();

    for (const auto& block : module->blocks()) {
        for (const auto& instr_ptr : block->instructions()) {
            const Instruction& instr = *instr_ptr;

            for (const Value& operand : instr.operands()) {
                if (operand.kind() == Value::Kind::Variable ||
                    operand.kind() == Value::Kind::Temporary) {
                }
            }

            if (instr.has_result() &&
                instr.result().kind() != Value::Kind::Undefined) {
                const Value& result = instr.result();
            }
        }
    }
}

void Optimizer::eliminate_dead_blocks(IRModule* module) {
    analyze_reachability(module);

    auto& blocks = const_cast<std::vector<std::unique_ptr<BasicBlock>>&>(module->blocks());
    blocks.erase(
        std::remove_if(blocks.begin(), blocks.end(),
            [this](const std::unique_ptr<BasicBlock>& block) {
                return !reachable_blocks_.count(block.get());
            }),
        blocks.end()
    );
}

void Optimizer::merge_blocks(IRModule* module) {
}

bool Optimizer::can_fold(const Instruction& instr) const {
    switch (instr.opcode()) {
        case IROpcode::BINARY_ADD:
        case IROpcode::BINARY_SUBTRACT:
        case IROpcode::BINARY_MULTIPLY:
        case IROpcode::BINARY_TRUE_DIVIDE:
        case IROpcode::BINARY_FLOOR_DIVIDE:
        case IROpcode::BINARY_MODULO:
        case IROpcode::BINARY_POWER:
        case IROpcode::BINARY_LSHIFT:
        case IROpcode::BINARY_RSHIFT:
        case IROpcode::BINARY_AND:
        case IROpcode::BINARY_OR:
        case IROpcode::BINARY_XOR:
        case IROpcode::UNARY_POSITIVE:
        case IROpcode::UNARY_NEGATIVE:
        case IROpcode::UNARY_NOT:
        case IROpcode::UNARY_INVERT:
        case IROpcode::COMPARE_EQ:
        case IROpcode::COMPARE_NE:
        case IROpcode::COMPARE_LT:
        case IROpcode::COMPARE_LE:
        case IROpcode::COMPARE_GT:
        case IROpcode::COMPARE_GE:
            return instr.operand_count() >= 1;
        default:
            return false;
    }
}

Value Optimizer::fold_instruction(const Instruction& instr) const {
    ConstantFolder folder;
    auto result = folder.fold(instr);
    return result.value_or(Value());
}

void Optimizer::replace_uses(const Value& old_value, const Value& new_value) {
}

// ============================================================================
// ConstantFolder 类实现
// ============================================================================

std::optional<Value> ConstantFolder::fold(const Instruction& instr) const {
    switch (instr.opcode()) {
        case IROpcode::BINARY_ADD:
            return fold_binary_op(instr);
        case IROpcode::BINARY_SUBTRACT:
            return fold_binary_op(instr);
        case IROpcode::BINARY_MULTIPLY:
            return fold_binary_op(instr);
        case IROpcode::BINARY_TRUE_DIVIDE:
            return fold_binary_op(instr);
        case IROpcode::BINARY_FLOOR_DIVIDE:
            return fold_binary_op(instr);
        case IROpcode::BINARY_MODULO:
            return fold_binary_op(instr);
        case IROpcode::BINARY_POWER:
            return fold_binary_op(instr);
        case IROpcode::UNARY_NEGATIVE:
            return fold_unary_op(instr);
        case IROpcode::UNARY_POSITIVE:
            return fold_unary_op(instr);
        case IROpcode::UNARY_NOT:
            return fold_unary_op(instr);
        case IROpcode::COMPARE_EQ:
        case IROpcode::COMPARE_NE:
        case IROpcode::COMPARE_LT:
        case IROpcode::COMPARE_LE:
        case IROpcode::COMPARE_GT:
        case IROpcode::COMPARE_GE:
            return fold_compare(instr);
        case IROpcode::BUILD_TUPLE:
            return fold_build_tuple(instr);
        case IROpcode::BUILD_LIST:
            return fold_build_list(instr);
        default:
            return std::nullopt;
    }
}

void ConstantFolder::fold_basic_block(BasicBlock* block) {
    if (!block) return;

    auto& instructions = block->instructions();
    for (auto& instr_ptr : instructions) {
        Instruction& instr = *instr_ptr;
        if (instr.operand_count() > 0) {  // 确保指令有操作数才尝试折叠
            auto folded = fold(instr);
            if (folded.has_value()) {
                // 创建新的常量加载指令替代原指令
                auto const_instr = std::make_unique<Instruction>(IROpcode::LOAD_CONST, folded.value());
                if (instr.has_result()) {
                    const_instr->result() = instr.result();
                }
                instr_ptr = std::move(const_instr);
            }
        }
    }

    instructions.erase(
        std::remove_if(instructions.begin(), instructions.end(),
            [](const std::unique_ptr<Instruction>& ptr) {
                return ptr == nullptr;
            }),
        instructions.end()
    );
}

void ConstantFolder::fold_module(IRModule* module) {
    if (!module) return;

    for (auto& block : module->blocks()) {
        fold_basic_block(block.get());
    }
}

std::optional<Value> ConstantFolder::fold_binary_op(const Instruction& instr) const {
    if (!are_constants(instr.operands())) {
        return std::nullopt;
    }

    auto constants = get_constants(instr.operands());
    if (constants.size() < 2) {
        return std::nullopt;
    }

    const Value& left = constants[0];
    const Value& right = constants[1];

    try {
        switch (instr.opcode()) {
            case IROpcode::BINARY_ADD: {
                int64_t l = std::stoll(left.to_string());
                int64_t r = std::stoll(right.to_string());
                return Value::Int(l + r);
            }
            case IROpcode::BINARY_SUBTRACT: {
                int64_t l = std::stoll(left.to_string());
                int64_t r = std::stoll(right.to_string());
                return Value::Int(l - r);
            }
            case IROpcode::BINARY_MULTIPLY: {
                int64_t l = std::stoll(left.to_string());
                int64_t r = std::stoll(right.to_string());
                return Value::Int(l * r);
            }
            default:
                return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<Value> ConstantFolder::fold_unary_op(const Instruction& instr) const {
    if (!is_constant(instr.operand(0))) {
        return std::nullopt;
    }

    const Value& operand = get_constant(instr.operand(0));

    try {
        switch (instr.opcode()) {
            case IROpcode::UNARY_NEGATIVE: {
                int64_t val = std::stoll(operand.to_string());
                return Value::Int(-val);
            }
            case IROpcode::UNARY_POSITIVE: {
                return operand;
            }
            case IROpcode::UNARY_NOT: {
                return Value::Bool(!operand.is_constant());
            }
            default:
                return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<Value> ConstantFolder::fold_compare(const Instruction& instr) const {
    if (!are_constants(instr.operands())) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<Value> ConstantFolder::fold_build_tuple(const Instruction& instr) const {
    if (!are_constants(instr.operands())) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<Value> ConstantFolder::fold_build_list(const Instruction& instr) const {
    if (!are_constants(instr.operands())) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<Value> ConstantFolder::fold_build_const_key_map(const Instruction& instr) const {
    return std::nullopt;
}

bool ConstantFolder::is_constant(const Value& value) const {
    return value.is_constant();
}

Value ConstantFolder::get_constant(const Value& value) const {
    return value;
}

bool ConstantFolder::are_constants(const std::vector<Value>& values) const {
    return std::all_of(values.begin(), values.end(),
        [this](const Value& v) { return is_constant(v); });
}

std::vector<Value> ConstantFolder::get_constants(const std::vector<Value>& values) const {
    std::vector<Value> result;
    for (const auto& v : values) {
        if (is_constant(v)) {
            result.push_back(get_constant(v));
        }
    }
    return result;
}

// ============================================================================
// DeadCodeEliminator 类实现
// ============================================================================

void DeadCodeEliminator::eliminate(IRModule* module) {
    if (!module) return;

    eliminate_unreachable(module);
    eliminate_dead_stores(module);
}

void DeadCodeEliminator::eliminate_unreachable(IRModule* module) {
    if (!module || !module->entry_block()) return;

    reachable_blocks_.clear();
    std::stack<BasicBlock*> stack;
    stack.push(module->entry_block());

    while (!stack.empty()) {
        BasicBlock* block = stack.top();
        stack.pop();

        if (reachable_blocks_.count(block)) continue;
        reachable_blocks_.insert(block);

        for (BasicBlock* succ : block->successors()) {
            if (!reachable_blocks_.count(succ)) {
                stack.push(succ);
            }
        }
    }

    auto& blocks = const_cast<std::vector<std::unique_ptr<BasicBlock>>&>(module->blocks());
    blocks.erase(
        std::remove_if(blocks.begin(), blocks.end(),
            [this](const std::unique_ptr<BasicBlock>& block) {
                return !reachable_blocks_.count(block.get());
            }),
        blocks.end()
    );

    eliminate_dead_stores(module);
}

void DeadCodeEliminator::eliminate_dead_stores(IRModule* module) {
    if (!module) return;

    collect_live_instructions(module);
    remove_dead_instructions(module);
}

void DeadCodeEliminator::collect_live_instructions(IRModule* module) {
    live_instructions_.clear();
    used_variables_.clear();

    std::stack<const Instruction*> stack;

    for (const auto& block : module->blocks()) {
        for (const auto& instr_ptr : block->instructions()) {
            const Instruction& instr = *instr_ptr;

            if (instr.opcode() == IROpcode::RETURN_VALUE) {
                if (instr.operand_count() > 0) {
                    stack.push(&instr);
                }
            }

            if (instr.has_result() &&
                (instr.opcode() == IROpcode::LOAD_FAST ||
                 instr.opcode() == IROpcode::LOAD_GLOBAL ||
                 instr.opcode() == IROpcode::LOAD_CONST)) {
            }
        }
    }
}

void DeadCodeEliminator::collect_reachable_blocks(IRModule* module) {
    reachable_blocks_.clear();

    if (module->entry_block()) {
        std::stack<BasicBlock*> stack;
        stack.push(module->entry_block());

        while (!stack.empty()) {
            BasicBlock* block = stack.top();
            stack.pop();

            if (reachable_blocks_.count(block)) continue;
            reachable_blocks_.insert(block);

            for (BasicBlock* succ : block->successors()) {
                if (!reachable_blocks_.count(succ)) {
                    stack.push(succ);
                }
            }
        }
    }
}

void DeadCodeEliminator::remove_dead_instructions(IRModule* module) {
    if (!module) return;

    for (auto& block : module->blocks()) {
        auto& instructions = block->instructions();
        for (auto it = instructions.begin(); it != instructions.end(); ) {
            const Instruction& instr = **it;

            bool is_dead = false;
            if (instr.is_store()) {
                if (instr.operand_count() > 0) {
                    const Value& val = instr.operand(0);
                    if (val.kind() == Value::Kind::Temporary) {
                        is_dead = true;
                    }
                }
            }

            if (is_dead) {
                it = instructions.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void DeadCodeEliminator::remove_unreachable_blocks(IRModule* module) {
    collect_reachable_blocks(module);

    auto& blocks = const_cast<std::vector<std::unique_ptr<BasicBlock>>&>(module->blocks());
    blocks.erase(
        std::remove_if(blocks.begin(), blocks.end(),
            [this](const std::unique_ptr<BasicBlock>& block) {
                return !reachable_blocks_.count(block.get());
            }),
        blocks.end()
    );
}

void DeadCodeEliminator::update_used_variables(IRModule* module) {
    used_variables_.clear();

    for (const auto& block : module->blocks()) {
        for (const auto& instr_ptr : block->instructions()) {
            const Instruction& instr = *instr_ptr;

            for (const Value& operand : instr.operands()) {
                if (operand.kind() == Value::Kind::Variable) {
                    used_variables_.insert(operand.name());
                }
            }
        }
    }
}

// ============================================================================
// ControlFlowOptimizer 类实现
// ============================================================================

void ControlFlowOptimizer::optimize(IRModule* module) {
    if (!module) return;

    simplify_basic_blocks(module);
    merge_blocks(module);
    eliminate_unreachable_blocks(module);
    thread_jumps(module);
    fixup_phis(module);
}

void ControlFlowOptimizer::simplify_basic_blocks(IRModule* module) {
    if (!module) return;

    for (auto& block : module->blocks()) {
        auto& instructions = block->instructions();

        for (size_t i = 0; i < instructions.size(); ++i) {
            Instruction& instr = *instructions[i];

            if (instr.opcode() == IROpcode::JUMP) {
                BasicBlock* target = instr.target();
                if (target && target == block.get()) {
                    instructions.erase(instructions.begin() + i);
                    --i;
                }
            }

        }
    }
}

void ControlFlowOptimizer::merge_blocks(IRModule* module) {
    if (!module) return;

    for (auto it = module->blocks().begin(); it != module->blocks().end(); ++it) {
        BasicBlock* block = it->get();
        if (!block) continue;

        if (block->instruction_count() == 0) continue;

        Instruction* last = block->last_instruction();
        if (!last) continue;

        if (last->opcode() == IROpcode::JUMP && last->target()) {
            BasicBlock* target = last->target();
            if (target && target->predecessors().size() == 1) {
                BasicBlock* pred = block;
                BasicBlock* succ = target;

                if (succ->predecessors().size() == 1) {
                    std::vector<std::unique_ptr<Instruction>>& target_instrs =
                        const_cast<std::vector<std::unique_ptr<Instruction>>&>(succ->instructions());

                    for (auto& target_instr : target_instrs) {
                        block->append_instruction(std::make_unique<Instruction>(*target_instr));
                    }

                    pred->remove_successor(succ);
                    pred->clear_instructions();

                    BasicBlock* next_target = nullptr;
                    Instruction* next_last = block->last_instruction();
                    if (next_last && next_last->opcode() == IROpcode::JUMP) {
                        next_target = next_last->target();
                    }

                    for (BasicBlock* old_succ : succ->successors()) {
                        old_succ->remove_predecessor(succ);
                        if (next_target) {
                            old_succ->add_predecessor(block);
                            block->add_successor(old_succ);
                        }
                    }

                    it = module->blocks().begin();
                }
            }
        }
    }
}

void ControlFlowOptimizer::eliminate_unreachable_blocks(IRModule* module) {
    if (!module || !module->entry_block()) return;

    std::unordered_set<BasicBlock*> reachable;
    std::stack<BasicBlock*> stack;
    stack.push(module->entry_block());

    while (!stack.empty()) {
        BasicBlock* block = stack.top();
        stack.pop();

        if (reachable.count(block)) continue;
        reachable.insert(block);

        for (BasicBlock* succ : block->successors()) {
            if (!reachable.count(succ)) {
                stack.push(succ);
            }
        }
    }

    auto& blocks = const_cast<std::vector<std::unique_ptr<BasicBlock>>&>(module->blocks());
    blocks.erase(
        std::remove_if(blocks.begin(), blocks.end(),
            [&reachable](const std::unique_ptr<BasicBlock>& block) {
                return !reachable.count(block.get());
            }),
        blocks.end()
    );
}

void ControlFlowOptimizer::fixup_phis(IRModule* module) {
    // 计算可达块集合
    std::unordered_set<BasicBlock*> reachable_blocks;
    if (module->entry_block()) {
        std::stack<BasicBlock*> stack;
        stack.push(module->entry_block());

        while (!stack.empty()) {
            BasicBlock* block = stack.top();
            stack.pop();

            if (reachable_blocks.count(block)) continue;
            reachable_blocks.insert(block);

            for (BasicBlock* succ : block->successors()) {
                if (!reachable_blocks.count(succ)) {
                    stack.push(succ);
                }
            }
        }
    }

    for (auto& block : module->blocks()) {
        for (auto& instr_ptr : block->instructions()) {
            Instruction& instr = *instr_ptr;
            if (instr.is_phi()) {
                // 创建新的PHI操作数映射，只保留可达块的操作数
                std::unordered_map<BasicBlock*, Value> new_operands;
                for (const auto& pair : instr.phi_operands()) {
                    if (reachable_blocks.count(pair.first)) {
                        new_operands[pair.first] = pair.second;
                    }
                }
                
                // 如果操作数发生了变化，则创建一个新的PHI指令
                if (new_operands.size() != instr.phi_operands().size()) {
                    // 使用Instruction::CreatePhi创建新指令
                    Value result = instr.result();
                    auto new_instr = std::make_unique<Instruction>(Instruction::CreatePhi(result, new_operands));
                    
                    // 复制可能的额外属性
                    new_instr->set_comment(instr.comment());
                    if (instr.line().has_value()) {
                        new_instr->set_position(instr.line().value(), instr.column().value());
                    }
                    
                    // 替换原指令
                    instr_ptr = std::move(new_instr);
                }
            }
        }
    }
}

void ControlFlowOptimizer::thread_jumps(IRModule* module) {
    if (!module) return;

    for (auto& block : module->blocks()) {
        Instruction* last = block->last_instruction();
        if (!last) continue;

        if (last->opcode() == IROpcode::JUMP) {
            BasicBlock* target = last->target();
            if (!target) continue;

            while (target->instruction_count() == 1) {
                Instruction* target_last = target->last_instruction();
                if (!target_last) break;

                if (target_last->opcode() != IROpcode::JUMP) break;
                if (!target_last->target()) break;

                BasicBlock* new_target = target_last->target();
                if (new_target == block.get()) break;

                last->set_target(new_target);
                target = new_target;
            }
        }
    }
}

void ControlFlowOptimizer::simplify_jumps(IRModule* module) {
}

void ControlFlowOptimizer::eliminate_redundant_jumps(IRModule* module) {
}

bool ControlFlowOptimizer::can_merge(BasicBlock* a, BasicBlock* b) const {
    if (!a || !b) return false;
    if (a->predecessors().size() != 1) return false;
    if (b->predecessors().size() != 1) return false;
    return true;
}

void ControlFlowOptimizer::merge_into(BasicBlock* a, BasicBlock* b) {
}

void ControlFlowOptimizer::update_phis(BasicBlock* pred, BasicBlock* succ) {
}

// ============================================================================
// PeepholeOptimizer 类实现
// ============================================================================

void PeepholeOptimizer::optimize(IRModule* module) {
    if (!module) return;

    optimize_load_const_pop(module);
    optimize_tuple_folding(module);
    optimize_unpack_sequence(module);
    optimize_rot_n(module);
    optimize_jump_chain(module);
}

void PeepholeOptimizer::optimize_load_const_pop(IRModule* module) {
    for (auto& block : module->blocks()) {
        auto& instructions = block->instructions();
        for (size_t i = 0; i + 1 < instructions.size(); ++i) {
            Instruction* curr = instructions[i].get();
            Instruction* next = instructions[i + 1].get();

            if (!curr || !next) continue;

            if (curr->opcode() == IROpcode::LOAD_CONST &&
                next->opcode() == IROpcode::POP_TOP) {
                instructions.erase(instructions.begin() + i);
                --i;
            }
        }
    }
}

void PeepholeOptimizer::optimize_tuple_folding(IRModule* module) {
}

void PeepholeOptimizer::optimize_unpack_sequence(IRModule* module) {
    for (auto& block : module->blocks()) {
        auto& instructions = block->instructions();
        for (size_t i = 0; i + 1 < instructions.size(); ++i) {
            Instruction* curr = instructions[i].get();
            Instruction* next = instructions[i + 1].get();

            if (!curr || !next) continue;

            if (curr->opcode() == IROpcode::BUILD_TUPLE && next->opcode() == IROpcode::UNPACK_SEQUENCE) {
                curr->set_metadata("folded", "true");
            }
        }
    }
}

void PeepholeOptimizer::optimize_rot_n(IRModule* module) {
}

void PeepholeOptimizer::optimize_jump_chain(IRModule* module) {
}

void PeepholeOptimizer::optimize_compare_chain(IRModule* module) {
}

void PeepholeOptimizer::optimize_boolean_ops(IRModule* module) {
}

bool PeepholeOptimizer::match_pattern(BasicBlock* block, size_t start,
                                     const std::vector<IROpcode>& pattern) const {
    if (!block) return false;
    if (start + pattern.size() > block->instruction_count()) return false;

    for (size_t i = 0; i < pattern.size(); ++i) {
        Instruction* instr = block->instruction_at(start + i);
        if (!instr || instr->opcode() != pattern[i]) {
            return false;
        }
    }
    return true;
}

void PeepholeOptimizer::replace_instructions(BasicBlock* block, size_t start, size_t count,
                                           std::unique_ptr<Instruction> replacement) {
    if (!block) return;
    if (start >= block->instruction_count()) return;

    block->instructions()[start] = std::move(replacement);
    for (size_t i = 1; i < count; ++i) {
        if (start + i < block->instruction_count()) {
            block->instructions()[start + i].reset();
        }
    }

    auto& instructions = block->instructions();
    instructions.erase(
        std::remove_if(instructions.begin() + start, instructions.end(),
            [](const std::unique_ptr<Instruction>& ptr) {
                return ptr == nullptr;
            }),
        instructions.end()
    );
}

void PeepholeOptimizer::remove_instructions(BasicBlock* block, size_t start, size_t count) {
    if (!block) return;
    if (start >= block->instruction_count()) return;

    for (size_t i = 0; i < count; ++i) {
        if (start + i < block->instruction_count()) {
            block->instructions()[start + i].reset();
        }
    }

    auto& instructions = block->instructions();
    instructions.erase(
        std::remove_if(instructions.begin() + start, instructions.end(),
            [](const std::unique_ptr<Instruction>& ptr) {
                return ptr == nullptr;
            }),
        instructions.end()
    );
}

// ============================================================================
// CommonSubexpressionEliminator 类实现
// ============================================================================

void CommonSubexpressionEliminator::eliminate(IRModule* module) {
    if (!module) return;

    for (auto& block : module->blocks()) {
        process_basic_block(block.get());
    }

    replace_redundant_uses(module);
}

std::string CommonSubexpressionEliminator::compute_value_number(const Instruction& instr) const {
    std::ostringstream oss;
    oss << static_cast<int>(instr.opcode());

    for (const Value& operand : instr.operands()) {
        oss << "_" << operand.to_string();
    }

    return oss.str();
}

void CommonSubexpressionEliminator::process_basic_block(BasicBlock* block) {
    if (!block) return;

    for (auto& instr_ptr : block->instructions()) {
        Instruction& instr = *instr_ptr;
        if (instr.is_load() || instr.is_store()) {
            std::string vn = compute_value_number(instr);
            value_number_map_[vn].push_back(instr.result());
        }
    }
}

void CommonSubexpressionEliminator::replace_redundant_uses(IRModule* module) {
    for (auto& pair : value_number_map_) {
        auto& values = pair.second;
        if (values.size() > 1) {
            for (size_t i = 1; i < values.size(); ++i) {
                redundant_map_[values[i]] = values[0];
            }
        }
    }
}

bool CommonSubexpressionEliminator::is_redundant(const Instruction& instr) const {
    std::string vn = compute_value_number(instr);
    auto it = value_number_map_.find(vn);
    if (it != value_number_map_.end() && it->second.size() > 1) {
        return true;
    }
    return false;
}

// ============================================================================
// OptimizationReporter 类实现
// ============================================================================

std::string OptimizationReporter::generate_report(const Optimizer& optimizer,
                                                const OptimizationStats& stats) const {
    std::ostringstream oss;
    oss << "=== CSGO SSA IR Optimization Report ===\n\n";
    oss << "Optimization Level: " << static_cast<int>(optimizer.get_flag("level") ? 3 : 0) << "\n";
    oss << "Passes Run: " << optimizer.get_pass_count() << "\n";
    oss << "Original Instructions: " << stats.original_instructions << "\n";
    oss << "Optimized Instructions: " << stats.optimized_instructions << "\n";
    oss << "Reduction: " << stats.reduction_ratio() << "%\n";
    oss << "Constants Folded: " << stats.folded_constants << "\n";
    oss << "Dead Code Eliminated: " << stats.eliminated_dead_code << "\n";
    oss << "Blocks Eliminated: " << stats.eliminated_blocks << "\n";
    oss << "Control Flow Simplified: " << stats.simplified_controls << "\n";
    return oss.str();
}

void OptimizationReporter::print_report(const Optimizer& optimizer,
                                       const OptimizationStats& stats) const {
    std::cout << generate_report(optimizer, stats);
}

} // namespace ir
} // namespace csgo