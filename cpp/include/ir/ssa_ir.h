/**
 * @file ssa_ir.h
 * @brief CSGO 编程语言 SSA 中间表示（IR）定义头文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件定义了 CSGO 语言的 SSA（Static Single Assignment）中间表示。
 * 基于 CPython 3.8 Python/compile.c 和 Python/peephole.c 设计，
 * 支持完整的 SSA 形式、 PHI 节点、基本块、控制流图等。
 *
 * @section design 设计原则
 * - 严格遵循 SSA 形式：每个变量只被赋值一次
 * - 支持 PHI 节点处理控制流合并
 * - 基于 CPython 的 basicblock 和 instr 结构
 * - 完整的指令类型覆盖（算术、逻辑、控制流、函数调用等）
 * - 支持常量折叠、死代码消除等基础优化
 *
 * @section reference 参考
 * - CPython Python/compile.c: 编译器核心实现
 * - CPython Python/peephole.c: peephole 优化器
 * - CPython Include/opcode.h: 字节码操作码定义
 *
 * @see Optimizer 优化器
 * @see BytecodeGenerator 字节码生成器
 */

#ifndef CSGO_SSA_IR_H
#define CSGO_SSA_IR_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <variant>
#include <functional>
#include <algorithm>
#include "../ast/ast_node.h"
#include "../semantic/type_checker.h"

namespace csgo {
namespace ir {

// ============================================================================
// 前向声明
// ============================================================================

class IRModule;
class BasicBlock;
class Instruction;
class Value;
class IRBuilder;

// ============================================================================
// 类型定义
// ============================================================================

/**
 * @enumIROpcode
 * @brief IR 指令操作码枚举
 *
 * 定义所有 SSA IR 指令的操作码，基于 CPython 的 opcode.h 设计。
 * 包含算术运算、逻辑运算、比较运算、控制流、内存操作等。
 */
enum class IROpcode {
    // =========================================================================
    // 加载/存储指令（基于 CPython LOAD/STORE 系列指令）
    // =========================================================================
    LOAD_CONST,     ///< 加载常量值
    LOAD_FAST,      ///< 加载局部变量（SSA 版本化）
    STORE_FAST,     ///< 存储局部变量（SSA 版本化）
    LOAD_GLOBAL,    ///< 加载全局变量
    STORE_GLOBAL,   ///< 存储全局变量
    LOAD_NAME,      ///< 加载名称（模块级）
    STORE_NAME,     ///< 存储名称（模块级）
    DELETE_FAST,    ///< 删除局部变量
    DELETE_GLOBAL,  ///< 删除全局变量
    DELETE_NAME,    ///< 删除名称

    // =========================================================================
    // 二元运算指令（基于 CPython BINARY_* 系列指令）
    // =========================================================================
    BINARY_ADD,     ///< 加法：+
    BINARY_SUBTRACT,///< 减法：-
    BINARY_MULTIPLY,///< 乘法：*
    BINARY_TRUE_DIVIDE, ///< 除法：/
    BINARY_FLOOR_DIVIDE, ///< 整除：//
    BINARY_MODULO,  ///< 取模：%
    BINARY_POWER,   ///< 幂运算：**
    BINARY_LSHIFT,  ///< 左移：<<
    BINARY_RSHIFT,  ///< 右移：>>
    BINARY_AND,     ///< 按位与：&
    BINARY_OR,      ///< 按位或：|
    BINARY_XOR,     ///< 按位异或：^

    // =========================================================================
    // 一元运算指令（基于 CPython UNARY_* 系列指令）
    // =========================================================================
    UNARY_POSITIVE, ///< 正号：+
    UNARY_NEGATIVE,///< 负号：-
    UNARY_NOT,     ///< 逻辑非：not
    UNARY_INVERT,  ///< 按位取反：~

    // =========================================================================
    // 比较运算指令（基于 CPython COMPARE_OP 系列指令）
    // =========================================================================
    COMPARE_EQ,     ///< 等于：==
    COMPARE_NE,    ///< 不等于：!=
    COMPARE_LT,    ///< 小于：<
    COMPARE_LE,    ///< 小于等于：<=
    COMPARE_GT,    ///< 大于：>
    COMPARE_GE,    ///< 大于等于：>=
    COMPARE_IS,    ///< 是：is
    COMPARE_IS_NOT,///< 不是：is not
    COMPARE_IN,    ///< 在...中：in
    COMPARE_NOT_IN,///< 不在...中：not in

    // =========================================================================
    // 逻辑运算指令（基于 CPython BoolOp）
    // =========================================================================
    LOGICAL_AND,    ///< 逻辑与：and
    LOGICAL_OR,     ///< 逻辑或：or

    // =========================================================================
    // 控制流指令（基于 CPython 控制流指令）
    // =========================================================================
    JUMP,          ///< 无条件跳转
    JUMP_IF_TRUE,  ///< 条件为真时跳转
    JUMP_IF_FALSE, ///< 条件为假时跳转
    JUMP_IF_FALSE_OR_POP, ///< 条件为假时跳转并弹出栈
    JUMP_IF_TRUE_OR_POP, ///< 条件为真时跳转并弹出栈
    RETURN_VALUE,  ///< 返回值
    RAISE_VARARGS, ///< 抛出异常
    YIELD_VALUE,   ///< 生成值（用于生成器）
    YIELD_FROM,    ///< 从另一个生成器生成
    GET_ITER,      ///< 获取迭代器
    FOR_ITER,      ///< 迭代器迭代
    BREAK_LOOP,    ///< 跳出循环
    CONTINUE_LOOP, ///< 继续循环

    // =========================================================================
    // 函数调用指令（基于 CPython CALL_* 系列指令）
    // =========================================================================
    CALL_FUNCTION,      ///< 调用函数（位置参数）
    CALL_FUNCTION_KW,  ///< 调用函数（关键字参数）
    CALL_FUNCTION_EX,  ///< 调用函数（解包参数）
    CALL_METHOD,       ///< 调用方法
    MAKE_FUNCTION,      ///< 创建函数对象
    LOAD_METHOD,       ///< 加载方法

    // =========================================================================
    // 构建复合对象指令（基于 CPython BUILD_* 系列指令）
    // =========================================================================
    BUILD_TUPLE,       ///< 构建元组
    BUILD_LIST,        ///< 构建列表
    BUILD_SET,         ///< 构建集合
    BUILD_MAP,         ///< 构建字典
    BUILD_STRING,      ///< 构建字符串
    BUILD_TUPLE_UNPACK,    ///< 解包构建元组
    BUILD_LIST_UNPACK,      ///< 解包构建列表
    BUILD_SET_UNPACK,       ///< 解包构建集合
    BUILD_MAP_UNPACK,       ///< 解包构建字典
    BUILD_CONST_KEY_MAP,    ///< 用常量键构建字典

    // =========================================================================
    // 序列操作指令（基于 CPython 序列操作指令）
    // =========================================================================
    UNPACK_SEQUENCE,   ///< 解包序列
    GET_AITER,        ///< 获取异步迭代器
    GET_ANEXT,        ///< 获取异步迭代器的下一个值
    GET_YIELD_FROM_ITER, ///< 从可迭代对象获取生成器

    // =========================================================================
    // 属性/下标操作指令（基于 CPython 属性/下标操作指令）
    // =========================================================================
    LOAD_ATTR,        ///< 加载属性
    STORE_ATTR,       ///< 存储属性
    DELETE_ATTR,      ///< 删除属性
    LOAD_SUBSCR,      ///< 加载下标值
    STORE_SUBSCR,     ///< 存储下标值
    DELETE_SUBSCR,    ///< 删除下标值
    BUILD_SLICE,      ///< 构建切片对象

    // =========================================================================
    // 闭包和作用域指令（基于 CPython 闭包相关指令）
    // =========================================================================
    LOAD_CLOSURE,     ///< 加载闭包变量
    LOAD_DEREF,       ///< 加载闭包变量（自由变量）
    STORE_DEREF,      ///< 存储闭包变量
    DELETE_DEREF,     ///< 删除闭包变量
    LOAD_CLASSDEREF,  ///< 加载类作用域自由变量

    // =========================================================================
    // 栈操作指令（基于 CPython 栈操作指令）
    // =========================================================================
    POP_TOP,          ///< 弹出栈顶
    ROT_TWO,          ///< 交换栈顶两个值
    ROT_THREE,        ///< 旋转栈顶三个值
    ROT_FOUR,         ///< 旋转栈顶四个值
    DUP_TOP,          ///< 复制栈顶值
    DUP_TOP_TWO,      ///< 复制栈顶两个值

    // =========================================================================
    // 异步操作指令（基于 CPython 异步相关指令）
    // =========================================================================
    GET_AWAITABLE,    ///< 获取可等待对象
    SETUP_ASYNC_WITH, ///< 异步 with 设置
    BEGIN_FINALLY,    ///< 开始 finally 块
    END_ASYNC_FOR,    ///< 结束异步 for 循环
    BEFORE_ASYNC_WITH,///< 异步 with 之前操作
    CALL_FINALLY,     ///< 调用 finally 块

    // =========================================================================
    // 异常处理指令（基于 CPython 异常处理指令）
    // =========================================================================
    SETUP_EXCEPT,     ///< 设置异常处理
    SETUP_FINALLY,    ///< 设置 finally 块
    SETUP_WITH,       ///< 设置 with 块
    END_FINALLY,      ///< 结束 finally 块
    POP_FINALLY,      ///< 弹出 finally 块

    // =========================================================================
    // 格式化指令（基于 CPython 格式化相关指令）
    // =========================================================================
    FORMAT_VALUE,      ///< 格式化值

    // =========================================================================
    // 导入指令（基于 CPython 导入相关指令）
    // =========================================================================
    IMPORT_NAME,       ///< 导入模块
    IMPORT_FROM,       ///< 从模块导入
    IMPORT_STAR,       ///< 导入所有（from ... import *）

    // =========================================================================
    // SSA 特有指令
    // =========================================================================
    PHI,              ///< PHI 节点（SSA 控制流合并）
    COPY,             ///< 复制指令（SSA 变量重命名）
};

// ============================================================================
// 比较操作类型枚举
// ============================================================================

/**
 * @enum CompareOp
 * @brief 比较操作类型枚举
 *
 * 定义所有比较操作的类型，基于 CPython 的比较操作设计。
 */
enum class CompareOp {
    LT = 0,    ///< 小于：<
    LE = 1,    ///< 小于等于：<=
    EQ = 2,    ///< 等于：==
    NE = 3,    ///< 不等于：!=
    GT = 4,    ///< 大于：>
    GE = 5,    ///< 大于等于：>=
    IS = 6,    ///< 是：is
    IS_NOT = 7,///< 不是：is not
    IN = 8,    ///< 在...中：in
    NOT_IN = 9 ///< 不在...中：not in
};

// ============================================================================
// SSA Value 类
// ============================================================================

/**
 * @class Value
 * @brief SSA IR 值类型
 *
 * 表示 SSA IR 中的所有可能值，包括常量、变量、临时值等。
 * 采用变体类型设计，支持多种值类型的统一表示。
 */
class Value {
public:
    /**
     * @enum ValueKind
     * @brief 值的类型种类
     */
    enum class Kind {
        Undefined,      ///< 未定义值
        Constant,       ///< 常量值
        Variable,       ///< 变量值（SSA 版本化）
        Temporary,      ///< 临时值（指令结果）
        Argument,       ///< 函数参数
        Global,         ///< 全局变量
        Closure,        ///< 闭包变量
        FreeVar         ///< 自由变量
    };

private:
    /**
     * @typedef StringValue
     * @brief 表示字符串常量值
     */
    using StringValue = std::string;

    /**
     * @typedef VariableName
     * @brief 表示变量名
     */
    struct VariableName {
        std::string name;
        size_t version;
        VariableName() : name(), version(0) {}
        VariableName(const std::string& n, size_t v) : name(n), version(v) {}
    };
 
private:
    Kind kind_;
    std::variant<
        std::nullptr_t,            // Constant: null/None
        bool,                      // Constant: boolean
        int64_t,                   // Constant: integer
        double,                    // Constant: float
        std::string,               // Constant: string value
        std::vector<uint8_t>,      // Constant: bytes
        VariableName,              // Variable: variable name with version
        int                        // Temporary/Argument: SSA version number
    > data_;
    std::string name_;              // 变量名（用于调试）
    size_t ssa_version_;           // SSA 版本号
    Type type_;                     // 值类型

public:
    Value();
    explicit Value(Kind kind);

    // 常量创建工厂方法
    static Value None();
    static Value Bool(bool value);
    static Value Int(int64_t value);
    static Value Float(double value);
    static Value String(const std::string& value);
    static Value Bytes(const std::vector<uint8_t>& value);
    static Value Undefined();

    // 变量创建工厂方法
    static Value Variable(const std::string& name, size_t version = 0);
    static Value Temporary(size_t id);
    static Value Argument(size_t index);
    static Value Global(const std::string& name);
    static Value Closure(const std::string& name);
    static Value FreeVar(const std::string& name);

    // 属性访问
    Kind kind() const { return kind_; }
    const std::string& name() const { return name_; }
    size_t ssa_version() const { return ssa_version_; }
    const Type& type() const { return type_; }
    Type& type() { return type_; }

    // 值检查
    bool is_constant() const { return kind_ == Kind::Constant; }
    bool is_variable() const { return kind_ == Kind::Variable; }
    bool is_temporary() const { return kind_ == Kind::Temporary; }
    bool is_undefined() const { return kind_ == Kind::Undefined; }
    // 获取常量数据变体（用于序列化）
    std::variant<
        std::nullptr_t,
        bool,
        int64_t,
        double,
        std::string,
        std::vector<uint8_t>,
        VariableName,
        int
    >& get_data() { return data_; }
    
    // 获取变量标识符（用于SSA版本管理）
    std::string get_ssa_name() const;

    // 字符串表示
    std::string to_string() const;

    // 复制并更新版本
    Value copy_with_new_version(size_t new_version) const;
    
    // 等价比较
    bool operator==(const Value& other) const;
};

// ============================================================================
// Instruction 类
// ============================================================================

/**
 * @class Instruction
 * @brief SSA IR 指令类
 *
 * 表示 SSA IR 中的单条指令，包含操作码、操作数、结果等。
 * 基于 CPython 的 instr 结构设计，添加 SSA 特有的 PHI 节点支持。
 */
class Instruction {
private:
    IROpcode opcode_;
    std::vector<Value> operands_;     // 操作数列表
    Value result_;                     // 结果值（SSA形式）
    std::optional<size_t> line_;       // 源文件行号
    std::optional<size_t> column_;      // 源文件列号
    std::string comment_;              // 注释（用于调试）

    // 控制流相关
    BasicBlock* target_;               // 跳转目标（条件跳转）
    std::vector<std::pair<BasicBlock*, Value>> phi_operands_; // PHI节点的操作数（按插入顺序）

    // 元数据
    std::unordered_map<std::string, std::string> metadata_; // 指令元数据

public:
    explicit Instruction(IROpcode opcode);
    Instruction(IROpcode opcode, const Value& result);
    Instruction(IROpcode opcode, const std::vector<Value>& operands);

    // 工厂方法 - 基础指令
    static Instruction CreateLoadConst(const Value& constant);
    static Instruction CreateLoadFast(const std::string& name, size_t version);
    static Instruction CreateStoreFast(const std::string& name, size_t version, const Value& value);
    static Instruction CreateBinaryOp(IROpcode op, const Value& left, const Value& right);
    static Instruction CreateUnaryOp(IROpcode op, const Value& operand);
    static Instruction CreateCompare(CompareOp op, const Value& left, const Value& right);
    static Instruction CreateJump(BasicBlock* target);
    static Instruction CreateCondJump(BasicBlock* target, const Value& condition);
    static Instruction CreateReturn(const Value& value);
    static Instruction CreateCall(const Value& function, const std::vector<Value>& args);
    static Instruction CreatePhi(const Value& result, const std::unordered_map<BasicBlock*, Value>& incoming);

    // 工厂方法 - CPython 风格指令
    static Instruction CreateLoadAttr(const Value& object, const std::string& attr);
    static Instruction CreateStoreAttr(const Value& object, const std::string& attr, const Value& value);
    static Instruction CreateLoadSubscript(const Value& container, const Value& index);
    static Instruction CreateStoreSubscript(const Value& container, const Value& index, const Value& value);
    static Instruction CreateBuildTuple(const std::vector<Value>& elements);
    static Instruction CreateBuildList(const std::vector<Value>& elements);
    static Instruction CreateBuildMap(const std::vector<Value>& keys, const std::vector<Value>& values);
    static Instruction CreateUnpackSequence(const Value& sequence, size_t count);
    static Instruction CreateGetIter(const Value& iterable);
    static Instruction CreateForIter(const Value& iterator);

    // 属性访问
    IROpcode opcode() const { return opcode_; }
    const std::vector<Value>& operands() const { return operands_; }
    const Value& result() const { return result_; }
    Value& result() { return result_; }
    bool has_result() const { return result_.kind() != Value::Kind::Undefined; }

    // 操作数管理
    void add_operand(const Value& value) { operands_.push_back(value); }
    void set_operands(const std::vector<Value>& operands) { operands_ = operands; }
    size_t operand_count() const { return operands_.size(); }
    const Value& operand(size_t index) const { return operands_.at(index); }

    // 位置信息
    void set_position(size_t line, size_t column) {
        line_ = line;
        column_ = column;
    }
    std::optional<size_t> line() const { return line_; }
    std::optional<size_t> column() const { return column_; }

    // 控制流
    BasicBlock* target() const { return target_; }
    void set_target(BasicBlock* target) { target_ = target; }
        const std::vector<std::pair<BasicBlock*, Value>>& phi_operands() const { return phi_operands_; }
    void add_phi_operand(BasicBlock* pred, const Value& value) { 
        phi_operands_.push_back({pred, value}); 
    }

    // 注释和元数据
    void set_comment(const std::string& comment) { comment_ = comment; }
    const std::string& comment() const { return comment_; }
    void set_metadata(const std::string& key, const std::string& value) { metadata_[key] = value; }
    const std::string& metadata(const std::string& key) const { return metadata_.at(key); }

    // 指令类型检查
    bool is_terminator() const;
    bool is_branch() const;
    bool is_phi() const { return opcode_ == IROpcode::PHI; }
    bool is_call() const;
    bool is_load() const;
    bool is_store() const;

    // 栈效果计算（基于 CPython stack_effect 实现）
    int stack_effect() const;

    // 字符串表示
    std::string to_string() const;
    std::string to_string_with_result() const;
};

// ============================================================================
// BasicBlock 类
// ============================================================================

/**
 * @class BasicBlock
 * @brief SSA IR 基本块类
 *
 * 表示 SSA IR 中的基本块，包含一组顺序执行的指令。
 * 基于 CPython 的 basicblock 结构设计，支持前驱/后继基本块管理。
 */
class BasicBlock {
public:
    using Instructions = std::vector<std::unique_ptr<Instruction>>;

private:
    std::string label_;                  // 基本块标签（用于跳转目标）
    Instructions instructions_;          // 指令列表
    std::vector<BasicBlock*> predecessors_;  // 前驱基本块列表
    std::vector<BasicBlock*> successors_;    // 后继基本块列表

    // SSA 相关
    std::unordered_map<std::string, std::vector<Value>> defined_vars_;    // 定义的变量
    std::unordered_map<std::string, Value> used_vars_;                    // 使用的变量

    // 元数据
    size_t index_;                       // 基本块索引（用于调试）
    bool is_start_;                      // 是否为入口块
    bool is_exit_;                       // 是否为退出块
    int stack_depth_;                    // 进入块时的栈深度
    int stack_offset_;                   // 块内偏移

public:
    explicit BasicBlock(const std::string& label = "");
    ~BasicBlock() = default;

    // 指令管理
    Instruction* append_instruction(std::unique_ptr<Instruction> instr);
    Instruction* append_instruction(IROpcode opcode);
    template<typename... Args>
    Instruction* append_instruction(Args&&... args) {
        return append_instruction(std::make_unique<Instruction>(std::forward<Args>(args)...));
    }

    void insert_instruction(size_t position, std::unique_ptr<Instruction> instr);
    void remove_instruction(size_t position);
    void clear_instructions();

    // 指令访问
    const Instructions& instructions() const { return instructions_; }
    Instructions& instructions() { return instructions_; }
    Instruction* first_instruction() const { return instructions_.empty() ? nullptr : instructions_.front().get(); }
    Instruction* last_instruction() const { return instructions_.empty() ? nullptr : instructions_.back().get(); }
    Instruction* instruction_at(size_t index) const { return instructions_.at(index).get(); }
    size_t instruction_count() const { return instructions_.size(); }

    // 遍历
    template<typename Func>
    void for_each_instruction(Func&& func) const {
        for (const auto& instr : instructions_) {
            func(*instr);
        }
    }

    template<typename Func>
    void for_each_instruction_mut(Func&& func) {
        for (auto& instr : instructions_) {
            func(*instr);
        }
    }

    // 控制流管理
    void add_predecessor(BasicBlock* pred) {
        if (pred && std::find(predecessors_.begin(), predecessors_.end(), pred) == predecessors_.end()) {
            predecessors_.push_back(pred);
        }
    }
    void add_successor(BasicBlock* succ) {
        if (succ && std::find(successors_.begin(), successors_.end(), succ) == successors_.end()) {
            successors_.push_back(succ);
        }
    }
    void remove_predecessor(BasicBlock* pred);
    void remove_successor(BasicBlock* succ);

    const std::vector<BasicBlock*>& predecessors() const { return predecessors_; }
    const std::vector<BasicBlock*>& successors() const { return successors_; }

    // 判断是否为前驱/后继
    bool has_predecessor(BasicBlock* pred) const;
    bool has_successor(BasicBlock* succ) const;

    // SSA 变量管理
    void add_defined_var(const std::string& name, const Value& value);
    void add_used_var(const std::string& name, const Value& value);
    const std::unordered_map<std::string, std::vector<Value>>& defined_vars() const { return defined_vars_; }
    const std::unordered_map<std::string, Value>& used_vars() const { return used_vars_; }

    // 基本块属性
    const std::string& label() const { return label_; }
    void set_label(const std::string& label) { label_ = label; }

    size_t index() const { return index_; }
    void set_index(size_t idx) { index_ = idx; }

    bool is_entry() const { return is_start_; }
    void set_entry(bool entry) { is_start_ = entry; }

    bool is_exit() const { return is_exit_; }
    void set_exit(bool exit) { is_exit_ = exit; }

    int stack_depth() const { return stack_depth_; }
    void set_stack_depth(int depth) { stack_depth_ = depth; }

    int stack_offset() const { return stack_offset_; }
    void set_stack_offset(int offset) { stack_offset_ = offset; }

    // 基本块操作
    void merge(BasicBlock* other);                  // 合并基本块
    void hoist_instructions(BasicBlock* target);    // 提升指令

    // 字符串表示
    std::string to_string() const;
    std::string to_string_detailed() const;
};

// ============================================================================
// IRModule 类
// ============================================================================

/**
 * @class IRModule
 * @brief SSA IR 模块类
 *
 * 表示完整的 SSA IR 模块，包含全局代码、函数定义等。
 * 基于 CPython 的 compiler 结构设计，支持嵌套作用域。
 */
class IRModule {
public:
    using FunctionMap = std::unordered_map<std::string, std::unique_ptr<IRModule>>;
    using GlobalMap = std::unordered_map<std::string, Value>;

private:
    std::string filename_;                      // 文件名
    std::string module_name_;                    // 模块名
    std::vector<std::unique_ptr<BasicBlock>> blocks_;      // 基本块列表
    BasicBlock* entry_block_;                   // 入口基本块
    BasicBlock* exit_block_;                    // 退出基本块

    // 符号表
    std::unordered_map<std::string, Value> globals_;       // 全局变量
    std::unordered_map<std::string, Value> locals_;       // 局部变量
    std::unordered_map<std::string, Value> constants_;     // 常量池

    // 嵌套函数
    FunctionMap nested_functions_;              // 嵌套函数

    // 元数据
    std::vector<std::string> names_;            // 名称列表（对应 CPython co_names）
    std::vector<Value> consts_;                // 常量列表（对应 CPython co_consts）
    std::vector<std::string> varnames_;        // 局部变量名列表（对应 CPython co_varnames）

    // SSA 版本管理
    std::unordered_map<std::string, size_t> next_ssa_version_;  // 下一个 SSA 版本号

public:
    explicit IRModule(const std::string& filename = "");
    ~IRModule() = default;

    // 基本块管理
    BasicBlock* create_block(const std::string& label = "");
    void add_block(std::unique_ptr<BasicBlock> block);
    void set_entry_block(BasicBlock* block);
    void set_exit_block(BasicBlock* block);

    const std::vector<std::unique_ptr<BasicBlock>>& blocks() const { return blocks_; }
    BasicBlock* entry_block() const { return entry_block_; }
    BasicBlock* exit_block() const { return exit_block_; }

    // 符号管理
    Value add_global(const std::string& name);
    Value get_global(const std::string& name) const;
    bool has_global(const std::string& name) const;
    void set_global(const std::string& name, const Value& value);

    Value add_local(const std::string& name);
    Value get_local(const std::string& name) const;
    bool has_local(const std::string& name) const;
    void set_local(const std::string& name, const Value& value);

    // 常量管理
    size_t add_constant(const Value& constant);
    size_t get_constant_index(const Value& constant) const;
    const std::vector<Value>& constants() const { return consts_; }

    // 名称管理
    size_t add_name(const std::string& name);
    size_t get_name_index(const std::string& name) const;
    const std::vector<std::string>& names() const { return names_; }

    // 局部变量名管理
    size_t add_varname(const std::string& name);
    const std::vector<std::string>& varnames() const { return varnames_; }

    // SSA 版本管理
    size_t get_next_ssa_version(const std::string& name);
    Value create_ssa_version(const std::string& name);

    // 嵌套函数
    IRModule* add_nested_function(const std::string& name);
    const FunctionMap& nested_functions() const { return nested_functions_; }

    // 模块信息
    const std::string& filename() const { return filename_; }
    void set_filename(const std::string& filename) { filename_ = filename; }

    const std::string& module_name() const { return module_name_; }
    void set_module_name(const std::string& name) { module_name_ = name; }

    // 字符串表示
    std::string to_string() const;
    std::string to_string_detailed() const;
};

// ============================================================================
// IRBuilder 类
// ============================================================================

/**
 * @class IRBuilder
 * @brief SSA IR 构建器类
 *
 * 提供高层 API 用于构建 SSA IR，基于 CPython 的 compiler 结构设计。
 * 支持从 AST 节点构建 SSA IR，进行基本块构建和指令生成。
 */
class IRBuilder {
private:
    std::unique_ptr<IRModule> module_;
    BasicBlock* current_block_;
    BasicBlock* exit_block_;                // 退出基本块
    std::vector<BasicBlock*> block_stack_;
    size_t temp_counter_ = 0;
    size_t block_counter_ = 0;

    // 作用域管理
    struct Scope {
        std::unordered_map<std::string, Value> locals_;
        std::unordered_map<std::string, Value> globals_;
        size_t arg_count_;
    };
    std::vector<Scope> scope_stack_;

    // 临时值栈
    std::vector<Value> value_stack_;
    std::vector<BasicBlock*> jump_targets_;

    // 循环上下文
    struct LoopContext {
        BasicBlock* header_;
        BasicBlock* body_;
        BasicBlock* exit_;
        BasicBlock* break_target_;
        BasicBlock* continue_target_;
    };
    std::vector<LoopContext> loop_stack_;

    // 延迟添加的块（用于保证块顺序等于执行顺序）
    std::vector<std::unique_ptr<BasicBlock>> pending_blocks_;

public:
    explicit IRBuilder(const std::string& filename = "");
    ~IRBuilder() = default;

    // 模块访问
    IRModule* module() const { return module_.get(); }

    // 当前基本块
    BasicBlock* current_block() const { return current_block_; }
    void set_current_block(BasicBlock* block);

    // 基本块操作
    BasicBlock* new_block(const std::string& label = "");
    BasicBlock* new_block_delayed(const std::string& label = "");
    void append_block(BasicBlock* block);
    void finish_block();

    // 作用域操作
    void enter_scope();
    void exit_scope();

    // 值栈操作
    void push_value(const Value& value);
    Value pop_value();
    Value& top_value();
    const Value& top_value() const;
    size_t value_stack_size() const { return value_stack_.size(); }

    // 指令生成 - 常量和变量
    Value emit_load_const(const Value& constant);
    Value emit_load_fast(const std::string& name);
    Value emit_store_fast(const std::string& name, const Value& value);
    Value emit_load_global(const std::string& name);
    Value emit_store_global(const std::string& name, const Value& value);

    // 指令生成 - 算术运算
    Value emit_binary_op(IROpcode op);
    Value emit_binary_add(const Value& left, const Value& right);
    Value emit_binary_sub(const Value& left, const Value& right);
    Value emit_binary_mul(const Value& left, const Value& right);
    Value emit_binary_div(const Value& left, const Value& right);
    Value emit_binary_floor_div(const Value& left, const Value& right);
    Value emit_binary_mod(const Value& left, const Value& right);
    Value emit_binary_power(const Value& left, const Value& right);

    // 指令生成 - 一元运算
    Value emit_unary_op(IROpcode op);
    Value emit_unary_negative(const Value& operand);
    Value emit_unary_positive(const Value& operand);
    Value emit_unary_not(const Value& operand);
    Value emit_unary_invert(const Value& operand);

    // 指令生成 - 比较运算
    Value emit_compare(CompareOp op);
    Value emit_compare_eq(const Value& left, const Value& right);
    Value emit_compare_ne(const Value& left, const Value& right);
    Value emit_compare_lt(const Value& left, const Value& right);
    Value emit_compare_le(const Value& left, const Value& right);
    Value emit_compare_gt(const Value& left, const Value& right);
    Value emit_compare_ge(const Value& left, const Value& right);

    // 指令生成 - 控制流
    BasicBlock* emit_jump(BasicBlock* target);
    BasicBlock* emit_jump_if_true(const Value& condition, BasicBlock* target);
    BasicBlock* emit_jump_if_false(const Value& condition, BasicBlock* target);
    Value emit_return(const Value& value);
    void emit_nop();

    // 指令生成 - 函数调用
    Value emit_call(const Value& function, const std::vector<Value>& args);
    Value emit_call(const Value& function);
    Value emit_load_method(const Value& object, const std::string& method);
    Value emit_method_call(const Value& method, const std::vector<Value>& args);

    // 指令生成 - 构建复合对象
    Value emit_build_tuple(const std::vector<Value>& elements);
    Value emit_build_list(const std::vector<Value>& elements);
    Value emit_build_set(const std::vector<Value>& elements);
    Value emit_build_map(const std::vector<Value>& keys, const std::vector<Value>& values);

    // 指令生成 - 序列操作
    std::vector<Value> emit_unpack_sequence(const Value& sequence, size_t count);
    Value emit_get_iter(const Value& iterable);
    Value emit_for_iter(BasicBlock* target);

    // 指令生成 - 属性操作
    Value emit_load_attr(const Value& object, const std::string& attr);
    Value emit_store_attr(const Value& object, const std::string& attr, const Value& value);

    // 指令生成 - 下标操作
    Value emit_load_subscript(const Value& container, const Value& index);
    Value emit_store_subscript(const Value& container, const Value& index, const Value& value);

    // PHI 节点
    Value emit_phi(const std::string& name, const std::unordered_map<BasicBlock*, Value>& incoming);

    // 从 AST 构建
    void build_from_module(const Module& module);
    void build_from_statement(const Stmt& stmt);
    Value build_from_expression(const Expr& expr);

    // 工具方法
    Value make_temporary();
    void add_jump_target(BasicBlock* target);
    BasicBlock* get_jump_target(size_t index);

    // 完成构建
    std::unique_ptr<IRModule> build();

private:
    // 内部辅助方法
    void build_function(const FunctionDef& func);
    void build_class(const ClassDef& cls);
    void build_if(const If& if_stmt);
    void build_while(const While& while_stmt);
    void build_for(const For& for_stmt);
    void build_return(const Return& ret);
    void build_assign(const Assign& assign);
    void build_aug_assign(const AugAssign& assign);
    void build_global(const Global& global);
    void build_nonlocal(const Nonlocal& nonlocal);
    void build_try(const Try& try_stmt);
    void build_with(const With& with_stmt);
    void build_raise(const Raise& raise);
    void build_assert(const Assert& assert_stmt);

    Value build_constant(const Constant& expr);
    Value build_name(const Name& expr);
    Value build_binop(const BinOp& expr);
    Value build_unaryop(const UnaryOp& expr);
    Value build_compare(const Compare& expr);
    Value build_call(const Call& expr);
    Value build_ifexp(const IfExp& expr);
    Value build_lambda(const Lambda& expr);
    Value build_attribute(const Attribute& expr);
    Value build_subscript(const Subscript& expr);
    Value build_list(const List& expr);
    Value build_tuple(const Tuple& expr);
    Value build_set(const Set& expr);
    Value build_dict(const Dict& expr);
    Value build_listcomp(const ListComp& expr);
    Value build_dictcomp(const DictComp& expr);
    Value build_setcomp(const SetComp& expr);
    Value build_genexp(const GeneratorExp& expr);
    Value build_yield(const Yield& expr);
    Value build_await(const Await& expr);
    Value build_boolop(const BoolOp& expr);
    // SSA 辅助方法（新增）
    Value lookup_variable(const std::string& name);
    void update_variable(const std::string& name, const Value& value);
    std::set<std::string> collect_loop_variables(const While& while_stmt);
};

} // namespace ir
} // namespace csgo

// ============================================================================
// Value hash 函数特化（用于 unordered_map）
// ============================================================================
namespace std {
    template<>
    struct hash<csgo::ir::Value> {
        size_t operator()(const csgo::ir::Value& value) const {
            return std::hash<std::string>{}(value.to_string());
        }
    };
    template<>
    struct hash<csgo::ir::IROpcode> {
        size_t operator()(const csgo::ir::IROpcode& opcode) const {
            return std::hash<int>{}(static_cast<int>(opcode));
        }
    };
}

#endif // CSGO_SSA_IR_H