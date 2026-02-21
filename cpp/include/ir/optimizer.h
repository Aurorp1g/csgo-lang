/**
 * @file optimizer.h
 * @brief CSGO 编程语言 SSA IR 优化器头文件
 *
 * @author CSGO Language Team
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件定义了 CSGO 语言的 SSA IR 优化器。
 * 基于 CPython 3.8 Python/peephole.c 和 Python/compile.c 设计，
 * 支持常量折叠、死代码消除、公共子表达式消除、冗余指令消除等优化。
 *
 * @section features 功能特性
 * - 常量折叠：编译期计算常量表达式
 * - 死代码消除：移除不可达代码和无用赋值
 * - 冗余指令消除：移除重复加载和无效操作
 * - 控制流优化：简化跳转指令
 * - PHI节点简化：处理trivial PHI节点
 *
 * @section reference 参考
 * - CPython Python/peephole.c: peephole 优化器
 * - CPython Python/compile.c: 编译器核心实现
 * - LLVM: SSA 优化技术参考
 *
 * @see SSAIR SSA中间表示
 * @see BytecodeGenerator 字节码生成器
 */

#ifndef CSGO_OPTIMIZER_H
#define CSGO_OPTIMIZER_H

#include "ssa_ir.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <functional>

namespace csgo {
namespace ir {

/**
 * @class Optimizer
 * @brief SSA IR 优化器类
 *
 * 提供多种优化pass的组合，支持流水线式优化。
 * 基于 CPython 的 peephole 优化器设计，但增加了 SSA 特有的优化。
 */
class Optimizer {
public:
    /**
     * @enum OptimizationLevel
     * @brief 优化级别枚举
     */
    enum class Level {
        None = 0,           ///< 无优化
        Basic = 1,          ///< 基础优化（常量折叠、死代码）
        Medium = 2,         ///< 中等优化（添加冗余消除）
        Aggressive = 3      ///< 激进优化（全面优化）
    };

private:
    Level level_;
    std::unordered_map<std::string, bool> flags_;  // 优化开关
    size_t pass_count_;                            // 优化pass计数
    size_t instruction_count_;                     // 优化前指令数
    size_t optimized_count_;                       // 优化后指令数

    // 分析结果
    std::unordered_set<BasicBlock*> reachable_blocks_;   // 可达基本块
    std::unordered_set<const Instruction*> used_values_; // 使用的值
    std::unordered_map<std::string, Value> constant_values_; // 常量值

public:
    explicit Optimizer(Level level = Level::Basic);
    ~Optimizer() = default;

    // 配置
    void set_level(Level level);
    void set_flag(const std::string& name, bool value);
    bool get_flag(const std::string& name) const;

    // 优化入口
    void optimize(IRModule* module);

    // 优化pass
    bool run_constant_folding(IRModule* module);
    bool run_dead_code_elimination(IRModule* module);
    bool run_dead_store_elimination(IRModule* module);
    bool run_copy_propagation(IRModule* module);
    bool run_common_subexpression_elimination(IRModule* module);
    bool run_control_flow_simplification(IRModule* module);
    bool run_trivial_phi_elimination(IRModule* module);
    bool run_redundant_load_elimination(IRModule* module);
    bool run_jump_threading(IRModule* module);
    bool run_peephole_optimization(IRModule* module);

    // 分析pass
    void analyze_reachability(IRModule* module);
    void analyze_liveness(IRModule* module);
    void analyze_constant_propagation(IRModule* module);

    // 统计信息
    size_t get_pass_count() const { return pass_count_; }
    size_t get_instruction_count() const { return instruction_count_; }
    size_t get_optimized_count() const { return optimized_count_; }

private:
    // 辅助方法
    bool is_constant(const Value& value) const;
    Value evaluate_constant_expr(const Instruction& instr) const;
    bool is_dead_store(const Instruction& instr, const IRModule* module) const;
    void collect_uses(const IRModule* module);
    void eliminate_dead_blocks(IRModule* module);
    void merge_blocks(IRModule* module);
    bool can_fold(const Instruction& instr) const;
    Value fold_instruction(const Instruction& instr) const;
    void replace_uses(const Value& old_value, const Value& new_value);
};

// ============================================================================
// 常量折叠器
// ============================================================================

/**
 * @class ConstantFolder
 * @brief 常量折叠器
 *
 * 负责检测和折叠常量表达式，基于 CPython 的常量合并实现。
 */
class ConstantFolder {
public:
    explicit ConstantFolder() = default;
    ~ConstantFolder() = default;

    // 尝试折叠指令
    std::optional<Value> fold(const Instruction& instr) const;

    // 折叠基本块
    void fold_basic_block(BasicBlock* block);

    // 折叠整个模块
    void fold_module(IRModule* module);

private:
    // 常量运算实现
    std::optional<Value> fold_binary_op(const Instruction& instr) const;
    std::optional<Value> fold_unary_op(const Instruction& instr) const;
    std::optional<Value> fold_compare(const Instruction& instr) const;
    std::optional<Value> fold_build_tuple(const Instruction& instr) const;
    std::optional<Value> fold_build_list(const Instruction& instr) const;
    std::optional<Value> fold_build_const_key_map(const Instruction& instr) const;

    // 辅助方法
    bool is_constant(const Value& value) const;
    Value get_constant(const Value& value) const;
    bool are_constants(const std::vector<Value>& values) const;
    std::vector<Value> get_constants(const std::vector<Value>& values) const;
};

// ============================================================================
// 死代码消除器
// ============================================================================

/**
 * @class DeadCodeEliminator
 * @brief 死代码消除器
 *
 * 移除不可达代码和无用赋值，基于 CPython 的 peephole 优化器设计。
 */
class DeadCodeEliminator {
private:
    std::unordered_set<const Instruction*> live_instructions_;
    std::unordered_set<BasicBlock*> reachable_blocks_;
    std::unordered_set<std::string> used_variables_;

public:
    explicit DeadCodeEliminator() = default;
    ~DeadCodeEliminator() = default;

    // 消除死代码
    void eliminate(IRModule* module);

    // 消除不可达代码
    void eliminate_unreachable(IRModule* module);

    // 消除无用赋值
    void eliminate_dead_stores(IRModule* module);

private:
    void collect_live_instructions(IRModule* module);
    void collect_reachable_blocks(IRModule* module);
    void remove_dead_instructions(IRModule* module);
    void remove_unreachable_blocks(IRModule* module);
    void update_used_variables(IRModule* module);
};

// ============================================================================
// 控制流优化器
// ============================================================================

/**
 * @class ControlFlowOptimizer
 * @brief 控制流优化器
 *
 * 简化控制流图，优化跳转指令，基于 CPython 的块合并实现。
 */
class ControlFlowOptimizer {
public:
    explicit ControlFlowOptimizer() = default;
    ~ControlFlowOptimizer() = default;

    // 优化控制流
    void optimize(IRModule* module);

private:
    // 块简化
    void simplify_basic_blocks(IRModule* module);
    void merge_blocks(IRModule* module);
    void eliminate_unreachable_blocks(IRModule* module);
    void fixup_phis(IRModule* module);

    // 跳转优化
    void thread_jumps(IRModule* module);
    void simplify_jumps(IRModule* module);
    void eliminate_redundant_jumps(IRModule* module);

    // 辅助方法
    bool can_merge(BasicBlock* a, BasicBlock* b) const;
    void merge_into(BasicBlock* a, BasicBlock* b);
    void update_phis(BasicBlock* pred, BasicBlock* succ);
};

// ============================================================================
// Peephole 优化器
// ============================================================================

/**
 * @class PeepholeOptimizer
 * @brief Peephole 优化器
 *
 * 基于滑动窗口的局部优化，基于 CPython 的 peephole.c 设计。
 */
class PeepholeOptimizer {
public:
    explicit PeepholeOptimizer() = default;
    ~PeepholeOptimizer() = default;

    // 运行 peephole 优化
    void optimize(IRModule* module);

private:
    // 优化规则
    void optimize_load_const_pop(IRModule* module);
    void optimize_tuple_folding(IRModule* module);
    void optimize_unpack_sequence(IRModule* module);
    void optimize_rot_n(IRModule* module);
    void optimize_jump_chain(IRModule* module);
    void optimize_compare_chain(IRModule* module);
    void optimize_boolean_ops(IRModule* module);

    // 辅助方法
    bool match_pattern(BasicBlock* block, size_t start,
                       const std::vector<IROpcode>& pattern) const;
    void replace_instructions(BasicBlock* block, size_t start, size_t count,
                            std::unique_ptr<Instruction> replacement);
    void remove_instructions(BasicBlock* block, size_t start, size_t count);
};

// ============================================================================
// 公共子表达式消除器
// ============================================================================

/**
 * @class CommonSubexpressionEliminator
 * @brief 公共子表达式消除器
 *
 * 识别并消除重复的计算，基于 SSA 形式的值编号。
 */
class CommonSubexpressionEliminator {
private:
    // 值编号映射
    std::unordered_map<std::string, std::vector<Value>> value_number_map_;
    std::unordered_map<Value, Value> redundant_map_;

public:
    explicit CommonSubexpressionEliminator() = default;
    ~CommonSubexpressionEliminator() = default;

    // 消除公共子表达式
    void eliminate(IRModule* module);

private:
    std::string compute_value_number(const Instruction& instr) const;
    void process_basic_block(BasicBlock* block);
    void replace_redundant_uses(IRModule* module);
    bool is_redundant(const Instruction& instr) const;
};

// ============================================================================
// 优化统计
// ============================================================================

/**
 * @struct OptimizationStats
 * @brief 优化统计信息
 */
struct OptimizationStats {
    size_t original_instructions = 0;
    size_t optimized_instructions = 0;
    size_t eliminated_blocks = 0;
    size_t folded_constants = 0;
    size_t eliminated_dead_code = 0;
    size_t simplified_controls = 0;

    size_t total_eliminated() const {
        return eliminated_blocks + eliminated_dead_code + folded_constants;
    }

    double reduction_ratio() const {
        if (original_instructions == 0) return 0.0;
        return static_cast<double>(original_instructions - optimized_instructions)
               / original_instructions * 100.0;
    }
};

/**
 * @class OptimizationReporter
 * @brief 优化报告生成器
 */
class OptimizationReporter {
public:
    explicit OptimizationReporter() = default;
    ~OptimizationReporter() = default;

    // 生成报告
    std::string generate_report(const Optimizer& optimizer,
                               const OptimizationStats& stats) const;

    // 打印报告
    void print_report(const Optimizer& optimizer,
                     const OptimizationStats& stats) const;
};

} // namespace ir
} // namespace csgo

#endif // CSGO_OPTIMIZER_H