/**
 * @file ssa_ir.cpp
 * @brief CSGO 编程语言 SSA 中间表示（IR）实现文件
 *
 * @author CSGO Language Team
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件实现了 CSGO 语言的 SSA（Static Single Assignment）中间表示。
 * 基于 CPython 3.8 Python/compile.c 和 Python/peephole.c 设计，
 * 提供完整的 SSA IR 构建、操作和优化功能。
 *
 * @section implementation 实现细节
 * - Value 类：SSA 值的表示和管理
 * - Instruction 类：SSA 指令的实现
 * - BasicBlock 类：基本块的实现和控制流管理
 * - IRModule 类：IR 模块的整体管理
 * - IRBuilder 类：高层 AST 到 SSA IR 的转换
 *
 * @section reference 参考
 * - CPython Python/compile.c: 编译器核心实现
 * - CPython Python/peephole.c: peephole 优化器
 * - CPython Include/opcode.h: 字节码操作码定义
 *
 * @see ssa_ir.h SSA IR 头文件
 * @see optimizer.h 优化器头文件
 */

#include "ir/ssa_ir.h"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <sstream>

namespace csgo {
namespace ir {

// ============================================================================
// Value 类实现
// ============================================================================

Value::Value() : kind_(Kind::Undefined), ssa_version_(0) {}

Value::Value(Kind kind) : kind_(kind), ssa_version_(0) {
    switch (kind_) {
        case Kind::Undefined:
            data_ = nullptr;
            break;
        case Kind::Constant:
            data_ = nullptr;
            break;
        case Kind::Variable:
        case Kind::Global:
        case Kind::Closure:
        case Kind::FreeVar:
            data_.emplace<VariableName>();
            break;
        case Kind::Temporary:
        case Kind::Argument:
            data_ = int(0);
            break;
    }
}

Value Value::None() {
    Value v;
    v.kind_ = Kind::Constant;
    v.data_ = nullptr;
    v.name_ = "None";
    return v;
}

Value Value::Bool(bool value) {
    Value v;
    v.kind_ = Kind::Constant;
    v.data_ = value;
    v.name_ = value ? "True" : "False";
    return v;
}

Value Value::Int(int64_t value) {
    Value v;
    v.kind_ = Kind::Constant;
    v.data_ = value;
    v.name_ = std::to_string(value);
    return v;
}

Value Value::Float(double value) {
    Value v;
    v.kind_ = Kind::Constant;
    v.data_ = value;
    std::ostringstream oss;
    oss << value;
    v.name_ = oss.str();
    return v;
}

Value Value::String(const std::string& value) {
    Value v;
    v.kind_ = Kind::Constant;
    v.data_ = value;
    v.name_ = "\"" + value + "\"";
    v.type_ = Type::string();
    return v;
}

Value Value::Bytes(const std::vector<uint8_t>& value) {
    Value v;
    v.kind_ = Kind::Constant;
    v.data_ = value;
    v.name_ = "b\"...\"";
    v.type_ = Type::bytes();
    return v;
}

Value Value::Variable(const std::string& name, size_t version) {
    Value v;
    v.kind_ = Kind::Variable;
    v.name_ = name;
    v.ssa_version_ = version;
    v.type_ = Type::unknown();
    v.data_.emplace<VariableName>(name, version);
    return v;
}

Value Value::Temporary(size_t id) {
    Value v;
    v.kind_ = Kind::Temporary;
    v.name_ = "%t" + std::to_string(id);
    v.data_ = int(id);
    v.type_ = Type::unknown();
    return v;
}

Value Value::Argument(size_t index) {
    Value v;
    v.kind_ = Kind::Argument;
    v.name_ = "%arg" + std::to_string(index);
    v.data_ = int(index);
    v.type_ = Type::unknown();
    return v;
}

Value Value::Global(const std::string& name) {
    Value v;
    v.kind_ = Kind::Global;
    v.name_ = name;
    v.type_ = Type::unknown();
    v.data_.emplace<VariableName>(name, 0);
    return v;
}

Value Value::Closure(const std::string& name) {
    Value v;
    v.kind_ = Kind::Closure;
    v.name_ = name;
    v.type_ = Type::unknown();
    v.data_.emplace<VariableName>(name, 0);
    return v;
}

Value Value::FreeVar(const std::string& name) {
    Value v;
    v.kind_ = Kind::FreeVar;
    v.name_ = name;
    v.type_ = Type::unknown();
    v.data_.emplace<VariableName>(name, 0);
    return v;
}

std::string Value::get_ssa_name() const {
    if (kind_ == Kind::Variable) {
        return name_ + "." + std::to_string(ssa_version_);
    }
    return name_;
}

std::string Value::to_string() const {
    std::ostringstream oss;
    switch (kind_) {
        case Kind::Undefined:
            oss << "undef";
            break;
        case Kind::Constant:
            oss << name_;
            break;
        case Kind::Variable:
            oss << name_ << "." << ssa_version_;
            break;
        case Kind::Temporary:
            oss << "%t" << std::get<int>(data_);
            break;
        case Kind::Argument:
            oss << "%arg" << std::get<int>(data_);
            break;
        case Kind::Global:
            oss << "@" << name_;
            break;
        case Kind::Closure:
            oss << "&" << name_;
            break;
        case Kind::FreeVar:
            oss << "$" << name_;
            break;
    }
    return oss.str();
}

Value Value::copy_with_new_version(size_t new_version) const {
    Value v = *this;
    v.ssa_version_ = new_version;
    return v;
}

bool Value::operator==(const Value& other) const {
    if (kind_ != other.kind_) return false;
    return to_string() == other.to_string();
}

// ============================================================================
// Instruction 类实现
// ============================================================================

Instruction::Instruction(IROpcode opcode)
    : opcode_(opcode), target_(nullptr) {}

Instruction::Instruction(IROpcode opcode, const Value& result)
    : opcode_(opcode), result_(result), target_(nullptr) {}

Instruction::Instruction(IROpcode opcode, const std::vector<Value>& operands)
    : opcode_(opcode), operands_(operands), target_(nullptr) {}

Instruction Instruction::CreateLoadConst(const Value& constant) {
    return Instruction(IROpcode::LOAD_CONST, constant);
}

Instruction Instruction::CreateLoadFast(const std::string& name, size_t version) {
    Value result = Value::Variable(name, version);
    return Instruction(IROpcode::LOAD_FAST, result);
}

Instruction Instruction::CreateStoreFast(const std::string& name, size_t version, const Value& value) {
    Value result = Value::Variable(name, version);
    Instruction instr(IROpcode::STORE_FAST, result);
    instr.add_operand(value);
    return instr;
}

Instruction Instruction::CreateBinaryOp(IROpcode op, const Value& left, const Value& right) {
    Instruction instr(op);
    instr.add_operand(left);
    instr.add_operand(right);
    instr.result_ = Value::Temporary(0);
    return instr;
}

Instruction Instruction::CreateUnaryOp(IROpcode op, const Value& operand) {
    Instruction instr(op);
    instr.add_operand(operand);
    instr.result_ = Value::Temporary(0);
    return instr;
}

Instruction Instruction::CreateCompare(CompareOp op, const Value& left, const Value& right) {
    Instruction instr(IROpcode::COMPARE_EQ);
    instr.add_operand(left);
    instr.add_operand(right);
    instr.result_ = Value::Temporary(0);
    return instr;
}

Instruction Instruction::CreateJump(BasicBlock* target) {
    Instruction instr(IROpcode::JUMP);
    instr.target_ = target;
    return instr;
}

Instruction Instruction::CreateCondJump(BasicBlock* target, const Value& condition) {
    Instruction instr(IROpcode::JUMP_IF_FALSE);
    instr.target_ = target;
    instr.add_operand(condition);
    return instr;
}

Instruction Instruction::CreateReturn(const Value& value) {
    Instruction instr(IROpcode::RETURN_VALUE);
    instr.add_operand(value);
    return instr;
}

Instruction Instruction::CreateCall(const Value& function, const std::vector<Value>& args) {
    Instruction instr(IROpcode::CALL_FUNCTION);
    instr.result_ = Value::Temporary(0);
    instr.add_operand(function);
    for (const auto& arg : args) {
        instr.add_operand(arg);
    }
    return instr;
}

Instruction Instruction::CreatePhi(const Value& result,
                                   const std::unordered_map<BasicBlock*, Value>& incoming) {
    Instruction instr(IROpcode::PHI, result);
    for (const auto& pair : incoming) {
        instr.add_phi_operand(pair.first, pair.second);
    }
    return instr;
}

Instruction Instruction::CreateLoadAttr(const Value& object, const std::string& attr) {
    Instruction instr(IROpcode::LOAD_ATTR);
    instr.add_operand(object);
    instr.result_ = Value::Temporary(0);
    instr.set_metadata("attr", attr);
    return instr;
}

Instruction Instruction::CreateStoreAttr(const Value& object, const std::string& attr, const Value& value) {
    Instruction instr(IROpcode::STORE_ATTR);
    instr.add_operand(object);
    instr.add_operand(value);
    instr.set_metadata("attr", attr);
    return instr;
}

Instruction Instruction::CreateLoadSubscript(const Value& container, const Value& index) {
    Instruction instr(IROpcode::LOAD_SUBSCR);
    instr.add_operand(container);
    instr.add_operand(index);
    instr.result_ = Value::Temporary(0);
    return instr;
}

Instruction Instruction::CreateStoreSubscript(const Value& container, const Value& index, const Value& value) {
    Instruction instr(IROpcode::STORE_SUBSCR);
    instr.add_operand(container);
    instr.add_operand(index);
    instr.add_operand(value);
    return instr;
}

Instruction Instruction::CreateBuildTuple(const std::vector<Value>& elements) {
    Instruction instr(IROpcode::BUILD_TUPLE);
    for (const auto& elem : elements) {
        instr.add_operand(elem);
    }
    instr.result_ = Value::Temporary(0);
    return instr;
}

Instruction Instruction::CreateBuildList(const std::vector<Value>& elements) {
    Instruction instr(IROpcode::BUILD_LIST);
    for (const auto& elem : elements) {
        instr.add_operand(elem);
    }
    instr.result_ = Value::Temporary(0);
    return instr;
}

Instruction Instruction::CreateBuildMap(const std::vector<Value>& keys,
                                        const std::vector<Value>& values) {
    Instruction instr(IROpcode::BUILD_MAP);
    for (const auto& key : keys) {
        instr.add_operand(key);
    }
    for (const auto& value : values) {
        instr.add_operand(value);
    }
    instr.result_ = Value::Temporary(0);
    return instr;
}

Instruction Instruction::CreateUnpackSequence(const Value& sequence, size_t count) {
    Instruction instr(IROpcode::UNPACK_SEQUENCE);
    instr.add_operand(sequence);
    instr.set_metadata("count", std::to_string(count));
    return instr;
}

Instruction Instruction::CreateGetIter(const Value& iterable) {
    Instruction instr(IROpcode::GET_ITER);
    instr.add_operand(iterable);
    instr.result_ = Value::Temporary(0);
    return instr;
}

Instruction Instruction::CreateForIter(const Value& iterator) {
    Instruction instr(IROpcode::FOR_ITER);
    instr.add_operand(iterator);
    instr.result_ = Value::Temporary(0);
    return instr;
}

bool Instruction::is_terminator() const {
    switch (opcode_) {
        case IROpcode::RETURN_VALUE:
        case IROpcode::JUMP:
        case IROpcode::JUMP_IF_TRUE:
        case IROpcode::JUMP_IF_FALSE:
        case IROpcode::JUMP_IF_FALSE_OR_POP:
        case IROpcode::JUMP_IF_TRUE_OR_POP:
        case IROpcode::RAISE_VARARGS:
        case IROpcode::YIELD_VALUE:
        case IROpcode::BREAK_LOOP:
        case IROpcode::CONTINUE_LOOP:
            return true;
        default:
            return false;
    }
}

bool Instruction::is_branch() const {
    switch (opcode_) {
        case IROpcode::JUMP:
        case IROpcode::JUMP_IF_TRUE:
        case IROpcode::JUMP_IF_FALSE:
        case IROpcode::JUMP_IF_FALSE_OR_POP:
        case IROpcode::JUMP_IF_TRUE_OR_POP:
            return true;
        default:
            return false;
    }
}

bool Instruction::is_call() const {
    return opcode_ == IROpcode::CALL_FUNCTION ||
           opcode_ == IROpcode::CALL_FUNCTION_KW ||
           opcode_ == IROpcode::CALL_FUNCTION_EX ||
           opcode_ == IROpcode::CALL_METHOD;
}

bool Instruction::is_load() const {
    switch (opcode_) {
        case IROpcode::LOAD_CONST:
        case IROpcode::LOAD_FAST:
        case IROpcode::LOAD_GLOBAL:
        case IROpcode::LOAD_NAME:
        case IROpcode::LOAD_ATTR:
        case IROpcode::LOAD_SUBSCR:
        case IROpcode::LOAD_CLOSURE:
        case IROpcode::LOAD_DEREF:
            return true;
        default:
            return false;
    }
}

bool Instruction::is_store() const {
    switch (opcode_) {
        case IROpcode::STORE_FAST:
        case IROpcode::STORE_GLOBAL:
        case IROpcode::STORE_NAME:
        case IROpcode::STORE_ATTR:
        case IROpcode::STORE_SUBSCR:
        case IROpcode::STORE_DEREF:
            return true;
        default:
            return false;
    }
}

int Instruction::stack_effect() const {
    int effect = 0;
    switch (opcode_) {
        case IROpcode::LOAD_CONST:
            effect = 1;
            break;
        case IROpcode::LOAD_FAST:
        case IROpcode::LOAD_GLOBAL:
        case IROpcode::LOAD_NAME:
        case IROpcode::LOAD_ATTR:
        case IROpcode::LOAD_SUBSCR:
        case IROpcode::LOAD_CLOSURE:
        case IROpcode::LOAD_DEREF:
            effect = 1;
            break;
        case IROpcode::STORE_FAST:
        case IROpcode::STORE_GLOBAL:
        case IROpcode::STORE_NAME:
        case IROpcode::STORE_ATTR:
        case IROpcode::STORE_SUBSCR:
        case IROpcode::STORE_DEREF:
            effect = -1;
            break;
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
        case IROpcode::COMPARE_EQ:
        case IROpcode::COMPARE_NE:
        case IROpcode::COMPARE_LT:
        case IROpcode::COMPARE_LE:
        case IROpcode::COMPARE_GT:
        case IROpcode::COMPARE_GE:
        case IROpcode::COMPARE_IS:
        case IROpcode::COMPARE_IS_NOT:
        case IROpcode::COMPARE_IN:
        case IROpcode::COMPARE_NOT_IN:
            effect = -1;
            break;
        case IROpcode::RETURN_VALUE:
            effect = -1;
            break;
        case IROpcode::CALL_FUNCTION:
            effect = -static_cast<int>(operand_count() - 1) + 1;
            break;
        case IROpcode::BUILD_TUPLE:
        case IROpcode::BUILD_LIST:
        case IROpcode::BUILD_SET:
            effect = 1 - static_cast<int>(operand_count());
            break;
        case IROpcode::BUILD_MAP:
            effect = 1 - 2 * static_cast<int>(operand_count() / 2);
            break;
        case IROpcode::UNPACK_SEQUENCE:
            effect = static_cast<int>(operand_count()) - 1;
            break;
        case IROpcode::POP_TOP:
            effect = -1;
            break;
        case IROpcode::DUP_TOP:
            effect = 1;
            break;
        case IROpcode::ROT_TWO:
        case IROpcode::ROT_THREE:
        case IROpcode::ROT_FOUR:
            effect = 0;
            break;
        case IROpcode::GET_ITER:
            effect = 0;
            break;
        case IROpcode::FOR_ITER:
            effect = 1;
            break;
        default:
            effect = 0;
            break;
    }
    return effect;
}

std::string Instruction::to_string() const {
    std::ostringstream oss;
    oss << "  ";

    switch (opcode_) {
        case IROpcode::LOAD_CONST:
            oss << "LOAD_CONST ";
            for (size_t i = 0; i < operands_.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << operands_[i].to_string();
            }
            break;
        case IROpcode::LOAD_FAST:
            oss << "LOAD_FAST ";
            if (!operands_.empty()) {
                oss << operands_[0].to_string();
            } else if (has_result()) {
                oss << result_.to_string();
            }
            break;
        case IROpcode::STORE_FAST:
            oss << "STORE_FAST " << (operands_.empty() ? "" : operands_[0].to_string());
            break;
        case IROpcode::BINARY_ADD:
            oss << "BINARY_ADD ";
            for (size_t i = 0; i < operands_.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << operands_[i].to_string();
            }
            break;
        case IROpcode::BINARY_SUBTRACT:
            oss << "BINARY_SUBTRACT";
            break;
        case IROpcode::BINARY_MULTIPLY:
            oss << "BINARY_MULTIPLY";
            break;
        case IROpcode::RETURN_VALUE:
            oss << "RETURN_VALUE";
            break;
        case IROpcode::JUMP:
            oss << "JUMP ";
            if (target_) {
                oss << target_->label();
            }
            break;
        case IROpcode::JUMP_IF_FALSE:
            oss << "JUMP_IF_FALSE ";
            if (target_) {
                oss << target_->label();
            }
            break;
        case IROpcode::CALL_FUNCTION:
            oss << "CALL_FUNCTION";
            break;
        case IROpcode::BUILD_TUPLE:
            oss << "BUILD_TUPLE " << operand_count();
            break;
        case IROpcode::BUILD_LIST:
            oss << "BUILD_LIST " << operand_count();
            break;
        case IROpcode::PHI:
            oss << "PHI";
            if (has_result()) {
                oss << " " << result_.to_string();
            }
            break;
        default:
            oss << "OP_" << static_cast<int>(opcode_);
            break;
    }

    if (!comment_.empty()) {
        oss << "  ; " << comment_;
    }

    return oss.str();
}

std::string Instruction::to_string_with_result() const {
    std::ostringstream oss;
    if (has_result() && result_.kind() != Value::Kind::Undefined) {
        oss << result_.to_string() << " = ";
    }
    oss << to_string();
    return oss.str();
}

// ============================================================================
// BasicBlock 类实现
// ============================================================================

BasicBlock::BasicBlock(const std::string& label)
    : label_(label), index_(0),
      is_start_(false), is_exit_(false),
      stack_depth_(0), stack_offset_(0) {}

Instruction* BasicBlock::append_instruction(std::unique_ptr<Instruction> instr) {
    Instruction* ptr = instr.get();
    instructions_.push_back(std::move(instr));
    return ptr;
}

Instruction* BasicBlock::append_instruction(IROpcode opcode) {
    return append_instruction(std::make_unique<Instruction>(opcode));
}

void BasicBlock::insert_instruction(size_t position, std::unique_ptr<Instruction> instr) {
    if (position >= instructions_.size()) {
        append_instruction(std::move(instr));
    } else {
        instructions_.insert(instructions_.begin() + position, std::move(instr));
    }
}

void BasicBlock::remove_instruction(size_t position) {
    if (position < instructions_.size()) {
        instructions_.erase(instructions_.begin() + position);
    }
}

void BasicBlock::clear_instructions() {
    instructions_.clear();
}

void BasicBlock::remove_predecessor(BasicBlock* pred) {
    auto it = std::find(predecessors_.begin(), predecessors_.end(), pred);
    if (it != predecessors_.end()) {
        predecessors_.erase(it);
    }
}

void BasicBlock::remove_successor(BasicBlock* succ) {
    auto it = std::find(successors_.begin(), successors_.end(), succ);
    if (it != successors_.end()) {
        successors_.erase(it);
    }
}

bool BasicBlock::has_predecessor(BasicBlock* pred) const {
    return std::find(predecessors_.begin(), predecessors_.end(), pred) != predecessors_.end();
}

bool BasicBlock::has_successor(BasicBlock* succ) const {
    return std::find(successors_.begin(), successors_.end(), succ) != successors_.end();
}

void BasicBlock::add_defined_var(const std::string& name, const Value& value) {
    defined_vars_[name].push_back(value);
}

void BasicBlock::add_used_var(const std::string& name, const Value& value) {
    used_vars_[name] = value;
}

void BasicBlock::merge(BasicBlock* other) {
    if (!other) return;

    for (const auto& pair : other->defined_vars_) {
        for (const auto& val : pair.second) {
            defined_vars_[pair.first].push_back(val);
        }
    }

    for (const auto& pair : other->used_vars_) {
        used_vars_[pair.first] = pair.second;
    }
}

void BasicBlock::hoist_instructions(BasicBlock* target) {
    if (!target || target == this) return;

    for (const auto& instr_ptr : instructions_) {
        Instruction* instr = instr_ptr.get();
        if (instr->opcode() == IROpcode::PHI) {
            continue;
        }
        target->append_instruction(std::make_unique<Instruction>(*instr));
    }
}

std::string BasicBlock::to_string() const {
    std::ostringstream oss;

    if (!label_.empty()) {
        oss << label_ << ":\n";
    } else {
        oss << "block_" << index_ << ":\n";
    }

    if (!predecessors_.empty()) {
        oss << "  ; predecessors: ";
        for (size_t i = 0; i < predecessors_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << (predecessors_[i] ? predecessors_[i]->label() : "null");
        }
        oss << "\n";
    }

    for (const auto& instr : instructions_) {
        oss << instr->to_string_with_result() << "\n";
    }

    if (!successors_.empty()) {
        oss << "  ; successors: ";
        for (size_t i = 0; i < successors_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << (successors_[i] ? successors_[i]->label() : "null");
        }
        oss << "\n";
    }

    return oss.str();
}

std::string BasicBlock::to_string_detailed() const {
    std::ostringstream oss;
    oss << "BasicBlock " << label_ << " (index=" << index_ << ")\n";
    oss << "  Entry: " << (is_start_ ? "yes" : "no") << "\n";
    oss << "  Exit: " << (is_exit_ ? "yes" : "no") << "\n";
    oss << "  Stack depth: " << stack_depth_ << "\n";
    oss << "  Instructions: " << instruction_count() << "\n";
    oss << to_string();
    return oss.str();
}

// ============================================================================
// IRModule 类实现
// ============================================================================

IRModule::IRModule(const std::string& filename)
    : filename_(filename), entry_block_(nullptr), exit_block_(nullptr) {}

BasicBlock* IRModule::create_block(const std::string& label) {
    std::string block_label = label;
    if (block_label.empty()) {
        block_label = "block_" + std::to_string(blocks_.size());
    }
    auto block = std::make_unique<BasicBlock>(block_label);
    block->set_index(blocks_.size());
    BasicBlock* ptr = block.get();
    blocks_.push_back(std::move(block));
    if (entry_block_ == nullptr) {
        entry_block_ = ptr;
    }
    return ptr;
}

void IRModule::add_block(std::unique_ptr<BasicBlock> block) {
    block->set_index(blocks_.size());
    blocks_.push_back(std::move(block));
}

void IRModule::set_entry_block(BasicBlock* block) {
    entry_block_ = block;
    if (block) {
        block->set_entry(true);
    }
}

void IRModule::set_exit_block(BasicBlock* block) {
    exit_block_ = block;
    if (block) {
        block->set_exit(true);
    }
}

Value IRModule::add_global(const std::string& name) {
    if (!has_global(name)) {
        globals_[name] = Value::Global(name);
    }
    return globals_[name];
}

Value IRModule::get_global(const std::string& name) const {
    auto it = globals_.find(name);
    if (it != globals_.end()) {
        return it->second;
    }
    return Value();
}

bool IRModule::has_global(const std::string& name) const {
    return globals_.find(name) != globals_.end();
}

void IRModule::set_global(const std::string& name, const Value& value) {
    globals_[name] = value;
}

Value IRModule::add_local(const std::string& name) {
    if (!has_local(name)) {
        locals_[name] = Value::Variable(name, 0);
    }
    return locals_[name];
}

Value IRModule::get_local(const std::string& name) const {
    auto it = locals_.find(name);
    if (it != locals_.end()) {
        return it->second;
    }
    return Value();
}

bool IRModule::has_local(const std::string& name) const {
    return locals_.find(name) != locals_.end();
}

void IRModule::set_local(const std::string& name, const Value& value) {
    locals_[name] = value;
}

size_t IRModule::add_constant(const Value& constant) {
    for (size_t i = 0; i < consts_.size(); ++i) {
        if (consts_[i].to_string() == constant.to_string()) {
            return i;
        }
    }
    consts_.push_back(constant);
    return consts_.size() - 1;
}

size_t IRModule::get_constant_index(const Value& constant) const {
    for (size_t i = 0; i < consts_.size(); ++i) {
        if (consts_[i].to_string() == constant.to_string()) {
            return i;
        }
    }
    return consts_.size();
}

size_t IRModule::add_name(const std::string& name) {
    for (size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == name) {
            return i;
        }
    }
    names_.push_back(name);
    return names_.size() - 1;
}

size_t IRModule::get_name_index(const std::string& name) const {
    for (size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == name) {
            return i;
        }
    }
    return names_.size();
}

size_t IRModule::add_varname(const std::string& name) {
    for (size_t i = 0; i < varnames_.size(); ++i) {
        if (varnames_[i] == name) {
            return i;
        }
    }
    varnames_.push_back(name);
    return varnames_.size() - 1;
}

size_t IRModule::get_next_ssa_version(const std::string& name) {
    return next_ssa_version_[name]++;
}

Value IRModule::create_ssa_version(const std::string& name) {
    size_t version = get_next_ssa_version(name);
    return Value::Variable(name, version);
}

IRModule* IRModule::add_nested_function(const std::string& name) {
    auto func = std::make_unique<IRModule>(name);
    IRModule* ptr = func.get();
    nested_functions_[name] = std::move(func);
    return ptr;
}

std::string IRModule::to_string() const {
    std::ostringstream oss;
    oss << "; CSGO SSA IR Module\n";
    oss << "; Filename: " << filename_ << "\n";
    oss << "; Module: " << module_name_ << "\n";
    oss << "; Constants: " << consts_.size() << "\n";
    oss << "; Names: " << names_.size() << "\n";
    oss << "; Varnames: " << varnames_.size() << "\n";
    oss << "\n";

    for (const auto& block : blocks_) {
        oss << block->to_string() << "\n";
    }

    return oss.str();
}

std::string IRModule::to_string_detailed() const {
    std::ostringstream oss;
    oss << "IRModule: " << module_name_ << "\n";
    oss << "  Filename: " << filename_ << "\n";
    oss << "  Blocks: " << blocks_.size() << "\n";
    oss << "  Constants: " << consts_.size() << "\n";
    oss << "  Names: " << names_.size() << "\n";
    oss << "  Varnames: " << varnames_.size() << "\n";
    oss << "  Nested functions: " << nested_functions_.size() << "\n";
    oss << "\n";

    for (const auto& pair : globals_) {
        oss << "  Global: " << pair.first << " = " << pair.second.to_string() << "\n";
    }

    for (const auto& block : blocks_) {
        oss << block->to_string_detailed() << "\n";
    }

    return oss.str();
}

// ============================================================================
// IRBuilder 类实现
// ============================================================================

IRBuilder::IRBuilder(const std::string& filename)
    : module_(std::make_unique<IRModule>(filename)),
      current_block_(nullptr),
      exit_block_(nullptr) {
    module_->set_filename(filename);
    module_->set_module_name("__main__");

    enter_scope();

    BasicBlock* entry = module_->create_block("entry");
    set_current_block(entry);
}

void IRBuilder::set_current_block(BasicBlock* block) {
    current_block_ = block;
    if (block) {
        // 检查是否已经在栈中，避免重复
        auto it = std::find(block_stack_.begin(), block_stack_.end(), block);
        if (it == block_stack_.end()) {
            block_stack_.push_back(block);
        }
    }
}

BasicBlock* IRBuilder::new_block(const std::string& label) {
    return module_->create_block(label);
}

void IRBuilder::append_block(BasicBlock* block) {
    if (!block) return;

    if (current_block_) {
        Instruction* last = current_block_->last_instruction();
        if (last && !last->is_terminator()) {
            if (current_block_->successors().empty()) {
                current_block_->append_instruction(
                    std::make_unique<Instruction>(IROpcode::JUMP));
            }
        }

        for (auto* succ : current_block_->successors()) {
            succ->add_predecessor(current_block_);
        }
    }

    set_current_block(block);
}

void IRBuilder::finish_block() {
    if (!current_block_) return;

    Instruction* last = current_block_->last_instruction();
    if (!last || !last->is_terminator()) {
        if (exit_block_) {
            current_block_->append_instruction(
                std::make_unique<Instruction>(IROpcode::JUMP));
            current_block_->last_instruction()->set_target(exit_block_);
            current_block_->add_successor(exit_block_);
            exit_block_->add_predecessor(current_block_);
        } else {
            current_block_->append_instruction(
                std::make_unique<Instruction>(IROpcode::RETURN_VALUE));
            current_block_->last_instruction()->add_operand(Value::None());
        }
    }

    if (!block_stack_.empty()) {
        block_stack_.pop_back();
        if (!block_stack_.empty()) {
            current_block_ = block_stack_.back();
        } else {
            current_block_ = nullptr;
        }
    }
}

void IRBuilder::enter_scope() {
    scope_stack_.push_back(Scope());
}

void IRBuilder::exit_scope() {
    if (!scope_stack_.empty()) {
        scope_stack_.pop_back();
    }
}

void IRBuilder::push_value(const Value& value) {
    value_stack_.push_back(value);
}

Value IRBuilder::pop_value() {
    if (value_stack_.empty()) {
        throw std::runtime_error("Value stack underflow");
    }
    Value value = value_stack_.back();
    value_stack_.pop_back();
    return value;
}

Value& IRBuilder::top_value() {
    if (value_stack_.empty()) {
        throw std::runtime_error("Value stack is empty");
    }
    return value_stack_.back();
}

Value IRBuilder::emit_load_const(const Value& constant) {
    size_t idx = module_->add_constant(constant);
    auto instr = std::make_unique<Instruction>(IROpcode::LOAD_CONST);
    Value result = Value::Temporary(temp_counter_++);
    instr->result() = result;
    instr->add_operand(constant);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_load_fast(const std::string& name) {
    if (scope_stack_.empty()) {
        throw std::runtime_error("No active scope");
    }
    auto& scope = scope_stack_.back();
    auto it = scope.locals_.find(name);
    if (it == scope.locals_.end()) {
        size_t version = module_->get_next_ssa_version(name);
        Value var = Value::Variable(name, version);
        scope.locals_[name] = var;
        auto instr = std::make_unique<Instruction>(IROpcode::LOAD_FAST, var);
        instr->add_operand(var);
        current_block_->append_instruction(std::move(instr));
        return var;
    }
    return it->second;
}

Value IRBuilder::emit_store_fast(const std::string& name, const Value& value) {
    if (scope_stack_.empty()) {
        throw std::runtime_error("No active scope");
    }
    auto& scope = scope_stack_.back();
    size_t version = module_->get_next_ssa_version(name);
    Value var = Value::Variable(name, version);
    scope.locals_[name] = var;
    auto instr = std::make_unique<Instruction>(IROpcode::STORE_FAST, var);
    instr->add_operand(value);
    current_block_->append_instruction(std::move(instr));
    return var;
}

Value IRBuilder::emit_load_global(const std::string& name) {
    if (!module_->has_global(name)) {
        module_->add_global(name);
    }
    Value global = module_->get_global(name);
    auto instr = std::make_unique<Instruction>(IROpcode::LOAD_GLOBAL, global);
    current_block_->append_instruction(std::move(instr));
    return global;
}

Value IRBuilder::emit_store_global(const std::string& name, const Value& value) {
    module_->set_global(name, value);
    auto instr = std::make_unique<Instruction>(IROpcode::STORE_GLOBAL);
    instr->add_operand(value);
    current_block_->append_instruction(std::move(instr));
    return value;
}

Value IRBuilder::emit_binary_op(IROpcode op) {
    if (value_stack_.size() < 2) {
        throw std::runtime_error("Not enough operands for binary operation");
    }
    Value right = pop_value();
    Value left = pop_value();
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(op, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    push_value(result);
    return result;
}

Value IRBuilder::emit_binary_add(const Value& left, const Value& right) {
    static size_t temp_counter = 0;
    Value result = Value::Temporary(temp_counter++);
    auto instr = std::make_unique<Instruction>(IROpcode::BINARY_ADD, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_binary_sub(const Value& left, const Value& right) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::BINARY_SUBTRACT, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_binary_mul(const Value& left, const Value& right) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::BINARY_MULTIPLY, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_binary_div(const Value& left, const Value& right) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::BINARY_TRUE_DIVIDE, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_binary_floor_div(const Value& left, const Value& right) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::BINARY_FLOOR_DIVIDE, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_binary_mod(const Value& left, const Value& right) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::BINARY_MODULO, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_binary_power(const Value& left, const Value& right) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::BINARY_POWER, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_unary_op(IROpcode op) {
    if (value_stack_.empty()) {
        throw std::runtime_error("Not enough operands for unary operation");
    }
    Value operand = pop_value();
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(op, result);
    instr->add_operand(operand);
    current_block_->append_instruction(std::move(instr));
    push_value(result);
    return result;
}

Value IRBuilder::emit_unary_negative(const Value& operand) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::UNARY_NEGATIVE, result);
    instr->add_operand(operand);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_unary_positive(const Value& operand) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::UNARY_POSITIVE, result);
    instr->add_operand(operand);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_unary_not(const Value& operand) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::UNARY_NOT, result);
    instr->add_operand(operand);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_unary_invert(const Value& operand) {
    Value result = module_->create_ssa_version("tmp");
    auto instr = std::make_unique<Instruction>(IROpcode::UNARY_INVERT, result);
    instr->add_operand(operand);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_compare(CompareOp op) {
    if (value_stack_.size() < 2) {
        throw std::runtime_error("Not enough operands for compare");
    }
    Value right = pop_value();
    Value left = pop_value();
    static size_t temp_counter = 0;
    Value result = Value::Temporary(temp_counter++);
    auto instr = std::make_unique<Instruction>(IROpcode::COMPARE_EQ, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    push_value(result);
    return result;
}

Value IRBuilder::emit_compare_eq(const Value& left, const Value& right) {
    static size_t temp_counter = 0;
    Value result = Value::Temporary(temp_counter++);
    auto instr = std::make_unique<Instruction>(IROpcode::COMPARE_EQ, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_compare_ne(const Value& left, const Value& right) {
    static size_t temp_counter = 0;
    Value result = Value::Temporary(temp_counter++);
    auto instr = std::make_unique<Instruction>(IROpcode::COMPARE_NE, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_compare_lt(const Value& left, const Value& right) {
    static size_t temp_counter = 0;
    Value result = Value::Temporary(temp_counter++);
    auto instr = std::make_unique<Instruction>(IROpcode::COMPARE_LT, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_phi(const std::string& name,
                          const std::unordered_map<BasicBlock*, Value>& incoming) {
    Value result = module_->create_ssa_version(name);
    auto instr = std::make_unique<Instruction>(IROpcode::PHI, result);
    for (const auto& pair : incoming) {
        instr->add_phi_operand(pair.first, pair.second);
    }
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_compare_le(const Value& left, const Value& right) {
    static size_t temp_counter = 0;
    Value result = Value::Temporary(temp_counter++);
    auto instr = std::make_unique<Instruction>(IROpcode::COMPARE_LE, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_compare_gt(const Value& left, const Value& right) {
    static size_t temp_counter = 0;
    Value result = Value::Temporary(temp_counter++);
    auto instr = std::make_unique<Instruction>(IROpcode::COMPARE_GT, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_compare_ge(const Value& left, const Value& right) {
    static size_t temp_counter = 0;
    Value result = Value::Temporary(temp_counter++);
    auto instr = std::make_unique<Instruction>(IROpcode::COMPARE_GE, result);
    instr->add_operand(left);
    instr->add_operand(right);
    current_block_->append_instruction(std::move(instr));
    return result;
}

BasicBlock* IRBuilder::emit_jump(BasicBlock* target) {
    if (!target) {
        throw std::runtime_error("Jump target is null");
    }
    auto instr = std::make_unique<Instruction>(IROpcode::JUMP);
    instr->set_target(target);
    current_block_->append_instruction(std::move(instr));
    current_block_->add_successor(target);
    target->add_predecessor(current_block_);
    return target;
}

BasicBlock* IRBuilder::emit_jump_if_true(const Value& condition, BasicBlock* target) {
    if (!target) {
        throw std::runtime_error("Jump target is null");
    }
    auto instr = std::make_unique<Instruction>(IROpcode::JUMP_IF_TRUE);
    instr->set_target(target);
    instr->add_operand(condition);
    current_block_->append_instruction(std::move(instr));
    current_block_->add_successor(target);
    target->add_predecessor(current_block_);
    return target;
}

BasicBlock* IRBuilder::emit_jump_if_false(const Value& condition, BasicBlock* target) {
    if (!target) {
        throw std::runtime_error("Jump target is null");
    }
    auto instr = std::make_unique<Instruction>(IROpcode::JUMP_IF_FALSE);
    instr->set_target(target);
    instr->add_operand(condition);
    current_block_->append_instruction(std::move(instr));
    current_block_->add_successor(target);
    target->add_predecessor(current_block_);
    return target;
}

Value IRBuilder::emit_return(const Value& value) {
    auto instr = std::make_unique<Instruction>(IROpcode::RETURN_VALUE);
    instr->add_operand(value);
    current_block_->append_instruction(std::move(instr));
    return value;
}

void IRBuilder::emit_nop() {
    current_block_->append_instruction(
        std::make_unique<Instruction>(IROpcode::POP_TOP));
}

Value IRBuilder::emit_call(const Value& function, const std::vector<Value>& args) {
    Value result = module_->create_ssa_version("call");
    auto instr = std::make_unique<Instruction>(IROpcode::CALL_FUNCTION, result);
    instr->add_operand(function);
    for (const auto& arg : args) {
        instr->add_operand(arg);
    }
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_call(const Value& function) {
    return emit_call(function, {});
}

Value IRBuilder::emit_load_method(const Value& object, const std::string& method) {
    Value result = module_->create_ssa_version("method");
    auto instr = std::make_unique<Instruction>(IROpcode::LOAD_METHOD, result);
    instr->add_operand(object);
    instr->set_metadata("method", method);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_method_call(const Value& method, const std::vector<Value>& args) {
    Value result = module_->create_ssa_version("call");
    auto instr = std::make_unique<Instruction>(IROpcode::CALL_METHOD, result);
    instr->add_operand(method);
    for (const auto& arg : args) {
        instr->add_operand(arg);
    }
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_build_tuple(const std::vector<Value>& elements) {
    Value result = module_->create_ssa_version("tuple");
    auto instr = std::make_unique<Instruction>(IROpcode::BUILD_TUPLE, result);
    for (const auto& elem : elements) {
        instr->add_operand(elem);
    }
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_build_list(const std::vector<Value>& elements) {
    Value result = module_->create_ssa_version("list");
    auto instr = std::make_unique<Instruction>(IROpcode::BUILD_LIST, result);
    for (const auto& elem : elements) {
        instr->add_operand(elem);
    }
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_build_set(const std::vector<Value>& elements) {
    Value result = module_->create_ssa_version("set");
    auto instr = std::make_unique<Instruction>(IROpcode::BUILD_SET, result);
    for (const auto& elem : elements) {
        instr->add_operand(elem);
    }
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_build_map(const std::vector<Value>& keys,
                                 const std::vector<Value>& values) {
    Value result = module_->create_ssa_version("dict");
    auto instr = std::make_unique<Instruction>(IROpcode::BUILD_MAP, result);
    for (const auto& key : keys) {
        instr->add_operand(key);
    }
    for (const auto& value : values) {
        instr->add_operand(value);
    }
    current_block_->append_instruction(std::move(instr));
    return result;
}

std::vector<Value> IRBuilder::emit_unpack_sequence(const Value& sequence, size_t count) {
    std::vector<Value> results;
    auto instr = std::make_unique<Instruction>(IROpcode::UNPACK_SEQUENCE);
    instr->add_operand(sequence);
    instr->set_metadata("count", std::to_string(count));
    current_block_->append_instruction(std::move(instr));

    for (size_t i = 0; i < count; ++i) {
        Value var = module_->create_ssa_version("unpack");
        results.push_back(var);
    }
    return results;
}

Value IRBuilder::emit_get_iter(const Value& iterable) {
    Value result = module_->create_ssa_version("iter");
    auto instr = std::make_unique<Instruction>(IROpcode::GET_ITER, result);
    instr->add_operand(iterable);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_load_attr(const Value& object, const std::string& attr) {
    Value result = module_->create_ssa_version("attr");
    auto instr = std::make_unique<Instruction>(IROpcode::LOAD_ATTR, result);
    instr->add_operand(object);
    instr->set_metadata("attr", attr);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_store_attr(const Value& object, const std::string& attr,
                                  const Value& value) {
    auto instr = std::make_unique<Instruction>(IROpcode::STORE_ATTR);
    instr->add_operand(object);
    instr->add_operand(value);
    instr->set_metadata("attr", attr);
    current_block_->append_instruction(std::move(instr));
    return value;
}

Value IRBuilder::emit_load_subscript(const Value& container, const Value& index) {
    Value result = module_->create_ssa_version("subscript");
    auto instr = std::make_unique<Instruction>(IROpcode::LOAD_SUBSCR, result);
    instr->add_operand(container);
    instr->add_operand(index);
    current_block_->append_instruction(std::move(instr));
    return result;
}

Value IRBuilder::emit_store_subscript(const Value& container, const Value& index,
                                       const Value& value) {
    auto instr = std::make_unique<Instruction>(IROpcode::STORE_SUBSCR);
    instr->add_operand(container);
    instr->add_operand(index);
    instr->add_operand(value);
    current_block_->append_instruction(std::move(instr));
    return value;
}

void IRBuilder::build_from_module(const Module& module) {
    enter_scope();

    module_->set_entry_block(current_block_);

    for (const auto& stmt : module.body) {
        build_from_statement(*stmt);
    }

    exit_scope();
}

void IRBuilder::build_from_statement(const Stmt& stmt) {
    switch (stmt.type) {
        case ASTNodeType::FunctionDef:
        case ASTNodeType::AsyncFunctionDef:
            build_function(static_cast<const FunctionDef&>(stmt));
            break;
        case ASTNodeType::ClassDef:
            build_class(static_cast<const ClassDef&>(stmt));
            break;
        case ASTNodeType::ExprStmt:
            build_from_expression(*static_cast<const ExprStmt&>(stmt).value);
            break;
        case ASTNodeType::Return:
            build_return(static_cast<const Return&>(stmt));
            break;
        case ASTNodeType::Assign:
            build_assign(static_cast<const Assign&>(stmt));
            break;
        case ASTNodeType::AugAssign:
            build_aug_assign(static_cast<const AugAssign&>(stmt));
            break;
        case ASTNodeType::If:
            build_if(static_cast<const If&>(stmt));
            break;
        case ASTNodeType::While:
            build_while(static_cast<const While&>(stmt));
            break;
        case ASTNodeType::For:
        case ASTNodeType::AsyncFor:
            build_for(static_cast<const For&>(stmt));
            break;
        case ASTNodeType::Global:
            build_global(static_cast<const Global&>(stmt));
            break;
        case ASTNodeType::Nonlocal:
            build_nonlocal(static_cast<const Nonlocal&>(stmt));
            break;
        case ASTNodeType::Try:
            build_try(static_cast<const Try&>(stmt));
            break;
        case ASTNodeType::With:
        case ASTNodeType::AsyncWith:
            build_with(static_cast<const With&>(stmt));
            break;
        case ASTNodeType::Raise:
            build_raise(static_cast<const Raise&>(stmt));
            break;
        case ASTNodeType::Assert:
            build_assert(static_cast<const Assert&>(stmt));
            break;
        case ASTNodeType::Pass:
            emit_nop();
            break;
        case ASTNodeType::Break:
            if (!loop_stack_.empty()) {
                emit_jump(loop_stack_.back().exit_);
            }
            break;
        case ASTNodeType::Continue:
            if (!loop_stack_.empty()) {
                emit_jump(loop_stack_.back().continue_target_);
            }
            break;
        default:
            break;
    }
}

Value IRBuilder::build_from_expression(const Expr& expr) {
    switch (expr.type) {
        case ASTNodeType::Constant:
            return build_constant(static_cast<const Constant&>(expr));
        case ASTNodeType::Name:
            return build_name(static_cast<const Name&>(expr));
        case ASTNodeType::BinOp:
            return build_binop(static_cast<const BinOp&>(expr));
        case ASTNodeType::UnaryOp:
            return build_unaryop(static_cast<const UnaryOp&>(expr));
        case ASTNodeType::Compare:
            return build_compare(static_cast<const Compare&>(expr));
        case ASTNodeType::Call:
            return build_call(static_cast<const Call&>(expr));
        case ASTNodeType::IfExp:
            return build_ifexp(static_cast<const IfExp&>(expr));
        case ASTNodeType::Lambda:
            return build_lambda(static_cast<const Lambda&>(expr));
        case ASTNodeType::Attribute:
            return build_attribute(static_cast<const Attribute&>(expr));
        case ASTNodeType::Subscript:
            return build_subscript(static_cast<const Subscript&>(expr));
        case ASTNodeType::List:
            return build_list(static_cast<const List&>(expr));
        case ASTNodeType::Tuple:
            return build_tuple(static_cast<const Tuple&>(expr));
        case ASTNodeType::Set:
            return build_set(static_cast<const Set&>(expr));
        case ASTNodeType::Dict:
            return build_dict(static_cast<const Dict&>(expr));
        case ASTNodeType::ListComp:
            return build_listcomp(static_cast<const ListComp&>(expr));
        case ASTNodeType::DictComp:
            return build_dictcomp(static_cast<const DictComp&>(expr));
        case ASTNodeType::SetComp:
            return build_setcomp(static_cast<const SetComp&>(expr));
        case ASTNodeType::GeneratorExp:
            return build_genexp(static_cast<const GeneratorExp&>(expr));
        case ASTNodeType::Yield:
            return build_yield(static_cast<const Yield&>(expr));
        case ASTNodeType::Await:
            return build_await(static_cast<const Await&>(expr));
        case ASTNodeType::BoolOp:
            return build_boolop(static_cast<const BoolOp&>(expr));
        default:
            return Value();
    }
}

Value IRBuilder::make_temporary() {
    return module_->create_ssa_version("tmp");
}

void IRBuilder::add_jump_target(BasicBlock* target) {
    if (target) {
        jump_targets_.push_back(target);
    }
}

BasicBlock* IRBuilder::get_jump_target(size_t index) {
    if (index < jump_targets_.size()) {
        return jump_targets_[index];
    }
    return nullptr;
}

std::unique_ptr<IRModule> IRBuilder::build() {
    // 先完成当前块
    finish_block();
    
    // 确保所有块都有 terminator，特别是空块
    for (auto& block : module_->blocks()) {
        if (block->instructions().empty() || 
            !block->last_instruction()->is_terminator()) {
            block->append_instruction(
                std::make_unique<Instruction>(IROpcode::RETURN_VALUE));
            block->last_instruction()->add_operand(Value::None());
        }
    }
    
    return std::move(module_);
}

void IRBuilder::build_function(const FunctionDef& func) {
    enter_scope();

    BasicBlock* func_entry = new_block("func_entry");
    BasicBlock* func_exit = new_block("func_exit");

    emit_jump(func_entry);

    append_block(func_entry);

    int arg_idx = 0;
    for (const auto& arg : func.args->args) {
        if (arg) {
            if (auto arg_ptr = dynamic_cast<csgo::arg*>(arg.get())) {
                emit_store_fast(arg_ptr->name, Value::Argument(arg_idx));
                arg_idx++;
            }
        }
    }

    for (const auto& stmt : func.body) {
        build_from_statement(*stmt);
    }

    if (!func_exit->instructions().empty() ||
        exit_block_ == nullptr) {
        if (current_block_ && current_block_ != func_exit) {
            emit_jump(func_exit);
        }
    }

    append_block(func_exit);
    emit_return(Value::None());

    exit_scope();
}

void IRBuilder::build_class(const ClassDef& cls) {
    emit_nop();
}

void IRBuilder::build_if(const If& if_stmt) {
    Value condition = build_from_expression(*if_stmt.test);

    BasicBlock* then_block = new_block("if_then");
    BasicBlock* else_block = if_stmt.orelse.empty() ? nullptr : new_block("if_else");
    BasicBlock* merge_block = new_block("if_merge");

    emit_jump_if_false(condition, then_block);

    if (else_block) {
        emit_jump(else_block);
    } else {
        emit_jump(merge_block);
    }

    append_block(then_block);
    for (const auto& stmt : if_stmt.body) {
        build_from_statement(*stmt);
    }
    emit_jump(merge_block);

    if (else_block) {
        append_block(else_block);
        for (const auto& stmt : if_stmt.orelse) {
            build_from_statement(*stmt);
        }
        emit_jump(merge_block);
    }

    append_block(merge_block);
}

void IRBuilder::build_while(const While& while_stmt) {
    BasicBlock* loop_header = new_block("while_header");
    BasicBlock* loop_body = new_block("while_body");
    BasicBlock* loop_exit = new_block("while_exit");

    emit_jump(loop_header);

    append_block(loop_header);

    if (while_stmt.test) {
        Value condition = build_from_expression(*while_stmt.test);
        emit_jump_if_false(condition, loop_exit);
    }

    emit_jump(loop_body);

    append_block(loop_body);

    loop_stack_.push_back({loop_header, loop_body, loop_exit, loop_exit, loop_header});

    for (const auto& stmt : while_stmt.body) {
        build_from_statement(*stmt);
    }

    loop_stack_.pop_back();

    emit_jump(loop_header);

    append_block(loop_exit);
}

void IRBuilder::build_for(const For& for_stmt) {
    Value iterable = build_from_expression(*for_stmt.iter);
    Value iter_var = emit_get_iter(iterable);

    BasicBlock* loop_header = new_block("for_header");
    BasicBlock* loop_body = new_block("for_body");
    BasicBlock* loop_exit = new_block("for_exit");

    emit_jump(loop_header);

    append_block(loop_header);

    loop_stack_.push_back({loop_header, loop_body, loop_exit, loop_exit, loop_header});

    if (for_stmt.target != nullptr) {
        Value elem = build_from_expression(*for_stmt.target);
        emit_store_fast("i", elem);
    }

    for (const auto& stmt : for_stmt.body) {
        build_from_statement(*stmt);
    }

    loop_stack_.pop_back();

    emit_jump(loop_header);

    append_block(loop_exit);
}

void IRBuilder::build_return(const Return& ret) {
    Value value;
    if (ret.value) {
        value = build_from_expression(*ret.value);
    } else {
        value = Value::None();
    }
    emit_return(value);
}

void IRBuilder::build_assign(const Assign& assign) {
    Value value = build_from_expression(*assign.value);

    for (const auto& target : assign.targets) {
        if (target->type == ASTNodeType::Name) {
            const Name& name = static_cast<const Name&>(*target);
            emit_store_fast(name.id, value);
        }
    }
}

void IRBuilder::build_aug_assign(const AugAssign& assign) {
    const Name& name = static_cast<const Name&>(*assign.target);
    Value left = emit_load_fast(name.id);
    Value right = build_from_expression(*assign.value);
    Value result;

    switch (assign.op) {
        case ASTNodeType::Add:
            result = emit_binary_add(left, right);
            break;
        case ASTNodeType::Sub:
            result = emit_binary_sub(left, right);
            break;
        case ASTNodeType::Mult:
            result = emit_binary_mul(left, right);
            break;
        case ASTNodeType::Div:
            result = emit_binary_div(left, right);
            break;
        default:
            result = make_temporary();
            break;
    }

    emit_store_fast(name.id, result);
}

void IRBuilder::build_global(const Global& global) {
    for (const auto& name : global.names) {
        emit_load_global(name);
    }
}

void IRBuilder::build_nonlocal(const Nonlocal& nonlocal) {
}

void IRBuilder::build_try(const Try& try_stmt) {
}

void IRBuilder::build_with(const With& with_stmt) {
}

void IRBuilder::build_raise(const Raise& raise) {
}

void IRBuilder::build_assert(const Assert& assert_stmt) {
}

Value IRBuilder::build_constant(const Constant& expr) {
    Value const_value;
    switch (expr.value.index()) {
        case 0:
            const_value = Value::None();
            break;
        case 1:
            const_value = Value::Bool(std::get<bool>(expr.value));
            break;
        case 2:
            const_value = Value::None();
            break;
        case 3:
            const_value = Value::Int(std::get<int64_t>(expr.value));
            break;
        case 4:
            const_value = Value::Float(std::get<double>(expr.value));
            break;
        case 5:
            const_value = Value::String(std::get<std::string>(expr.value));
            break;
        default:
            return Value();
    }
    return emit_load_const(const_value);
}

Value IRBuilder::build_name(const Name& expr) {
    if (expr.ctx == Name::Load) {
        return emit_load_fast(expr.id);
    } else {
        return make_temporary();
    }
}

Value IRBuilder::build_binop(const BinOp& expr) {
    Value left = build_from_expression(*expr.left);
    Value right = build_from_expression(*expr.right);

    switch (expr.op) {
        case ASTNodeType::Add:
            return emit_binary_add(left, right);
        case ASTNodeType::Sub:
            return emit_binary_sub(left, right);
        case ASTNodeType::Mult:
            return emit_binary_mul(left, right);
        case ASTNodeType::Div:
            return emit_binary_div(left, right);
        case ASTNodeType::FloorDiv:
            return emit_binary_floor_div(left, right);
        case ASTNodeType::Mod:
            return emit_binary_mod(left, right);
        case ASTNodeType::Pow:
            return emit_binary_power(left, right);
        default:
            return make_temporary();
    }
}

Value IRBuilder::build_unaryop(const UnaryOp& expr) {
    Value operand = build_from_expression(*expr.operand);

    switch (expr.op) {
        case ASTNodeType::USub:
            return emit_unary_negative(operand);
        case ASTNodeType::UAdd:
            return emit_unary_positive(operand);
        case ASTNodeType::Not:
            return emit_unary_not(operand);
        case ASTNodeType::Invert:
            return emit_unary_invert(operand);
        default:
            return make_temporary();
    }
}

Value IRBuilder::build_compare(const Compare& expr) {
    Value left = build_from_expression(*expr.left);
    Value right = build_from_expression(*expr.comparators[0]);

    if (expr.ops.size() == 1) {
        switch (expr.ops[0]) {
            case ASTNodeType::Eq:
                return emit_compare_eq(left, right);
            case ASTNodeType::NotEq:
                return emit_compare_ne(left, right);
            case ASTNodeType::Lt:
                return emit_compare_lt(left, right);
            case ASTNodeType::LtE:
                return emit_compare_le(left, right);
            case ASTNodeType::Gt:
                return emit_compare_gt(left, right);
            case ASTNodeType::GtE:
                return emit_compare_ge(left, right);
            default:
                return make_temporary();
        }
    }

    return make_temporary();
}

Value IRBuilder::build_call(const Call& expr) {
    Value func = build_from_expression(*expr.func);

    std::vector<Value> args;
    for (const auto& arg : expr.args) {
        args.push_back(build_from_expression(*arg));
    }

    return emit_call(func, args);
}

Value IRBuilder::build_ifexp(const IfExp& expr) {
    Value condition = build_from_expression(*expr.test);
    Value if_true = build_from_expression(*expr.body);
    Value if_false = build_from_expression(*expr.orelse);

    BasicBlock* then_block = new_block("ifexp_then");
    BasicBlock* else_block = new_block("ifexp_else");
    BasicBlock* merge_block = new_block("ifexp_merge");

    emit_jump_if_false(condition, else_block);
    emit_jump(then_block);

    append_block(then_block);
    push_value(if_true);
    emit_jump(merge_block);

    append_block(else_block);
    push_value(if_false);

    append_block(merge_block);

    return pop_value();
}

Value IRBuilder::build_lambda(const Lambda& expr) {
    return make_temporary();
}

Value IRBuilder::build_attribute(const Attribute& expr) {
    Value value = build_from_expression(*expr.value);
    return emit_load_attr(value, expr.attr);
}

Value IRBuilder::build_subscript(const Subscript& expr) {
    Value container = build_from_expression(*expr.value);
    Value index = build_from_expression(*expr.slice);
    return emit_load_subscript(container, index);
}

Value IRBuilder::build_list(const List& expr) {
    std::vector<Value> elements;
    for (const auto& elem : expr.elts) {
        elements.push_back(build_from_expression(*elem));
    }
    return emit_build_list(elements);
}

Value IRBuilder::build_tuple(const Tuple& expr) {
    std::vector<Value> elements;
    for (const auto& elem : expr.elts) {
        elements.push_back(build_from_expression(*elem));
    }
    return emit_build_tuple(elements);
}

Value IRBuilder::build_set(const Set& expr) {
    std::vector<Value> elements;
    for (const auto& elem : expr.elts) {
        elements.push_back(build_from_expression(*elem));
    }
    return emit_build_set(elements);
}

Value IRBuilder::build_dict(const Dict& expr) {
    std::vector<Value> keys;
    std::vector<Value> values;
    for (const auto& kv : expr.keys) {
        keys.push_back(build_from_expression(*kv));
    }
    for (const auto& kv : expr.values) {
        values.push_back(build_from_expression(*kv));
    }
    return emit_build_map(keys, values);
}

Value IRBuilder::build_listcomp(const ListComp& expr) {
    return make_temporary();
}

Value IRBuilder::build_dictcomp(const DictComp& expr) {
    return make_temporary();
}

Value IRBuilder::build_setcomp(const SetComp& expr) {
    return make_temporary();
}

Value IRBuilder::build_genexp(const GeneratorExp& expr) {
    return make_temporary();
}

Value IRBuilder::build_yield(const Yield& expr) {
    return make_temporary();
}

Value IRBuilder::build_await(const Await& expr) {
    return make_temporary();
}

Value IRBuilder::build_boolop(const BoolOp& expr) {
    if (expr.values.size() < 2) {
        // 当少于两个值时，无法构成有效的布尔操作
        return make_temporary();
    }

    // 获取第一个操作数
    Value result = build_from_expression(*expr.values[0]);
    
    // 遍历剩余的操作数并生成相应的代码
    for (size_t i = 1; i < expr.values.size(); ++i) {
        Value right = build_from_expression(*expr.values[i]);
        BasicBlock* right_block = new_block("bool_right");
        BasicBlock* end_block = new_block("bool_end");

        if (expr.op == ASTNodeType::And) {
            emit_jump_if_false(result, end_block);
            append_block(right_block);
            result = build_from_expression(*expr.values[i]);
        } else if (expr.op == ASTNodeType::Or) {
            emit_jump_if_true(result, end_block);
            append_block(right_block);
            result = build_from_expression(*expr.values[i]);
        }

        append_block(end_block);
    }
    return make_temporary();
}

} // namespace ir
} // namespace csgo