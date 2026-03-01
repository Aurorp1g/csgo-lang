/**
 * @file type_checker.cpp
 * @brief CSGO 编程语言类型检查器实现文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件实现了 CSGO 语言的类型检查器（Type Checker）功能。
 * 基于 CPython 的类型注解和类型提示设计，
 * 实现了静态类型推断和类型检查功能。
 *
 * @section implementation 实现细节
 * - Type 类：类型表示和操作
 * - TypeChecker 类：类型检查主逻辑
 * - 类型推断算法
 * - 类型兼容性检查
 * - 函数调用类型检查
 *
 * @section reference 参考
 * - CPython Python/typing.c: 类型检查实现
 * - PEP 484: Type Hints
 *
 * @see type_checker.h 类型检查器头文件
 * @see SymbolTable 符号表
 */

#include "semantic/type_checker.h"
#include <sstream>
#include <algorithm>

namespace csgo {

// ============================================================================
// Type 类实现
// ============================================================================

std::string Type::toString() const {
    switch (kind) {
        case BasicType::Unknown:
            return "unknown";
        case BasicType::None:
            return "None";
        case BasicType::Bool:
            return "bool";
        case BasicType::Int:
            return "int";
        case BasicType::Float:
            return "float";
        case BasicType::Complex:
            return "complex";
        case BasicType::String:
            return "str";
        case BasicType::Bytes:
            return "bytes";
        case BasicType::List:
            if (elementType) {
                return "list[" + elementType->toString() + "]";
            }
            return "list";
        case BasicType::Tuple:
            if (tupleTypes.empty()) {
                return "tuple";
            }
            if (tupleTypes.size() == 1) {
                return "tuple[" + tupleTypes[0].toString() + ",]";
            }
            {
                std::stringstream ss;
                ss << "tuple[";
                for (size_t i = 0; i < tupleTypes.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << tupleTypes[i].toString();
                }
                ss << "]";
                return ss.str();
            }
        case BasicType::Dict:
            if (dictTypes.find("__key__") != dictTypes.end() &&
                dictTypes.find("__value__") != dictTypes.end()) {
                return "dict[" + dictTypes.at("__key__").toString() + ", " +
                       dictTypes.at("__value__").toString() + "]";
            }
            return "dict";
        case BasicType::Set:
            if (elementType) {
                return "set[" + elementType->toString() + "]";
            }
            return "set";
        case BasicType::Function:
            return "function";
        case BasicType::Module:
            return "module";
        case BasicType::Object:
            return "object";
        case BasicType::Any:
            return "Any";
        default:
            return "<unknown>";
    }
}

bool Type::isCompatibleWith(const Type& other) const {
    if (kind == BasicType::Any || other.kind == BasicType::Any) {
        return true;
    }
    
    if (kind == other.kind) {
        return true;
    }
    
    // 数值类型兼容
    if (isNumeric() && other.isNumeric()) {
        return true;
    }
    
    // 整数和布尔值
    if ((kind == BasicType::Int && other.kind == BasicType::Bool) ||
        (kind == BasicType::Bool && other.kind == BasicType::Int)) {
        return true;
    }
    
    return false;
}

bool Type::supportsBinOp(ASTNodeType op, const Type& right) const {
    switch (op) {
        case ASTNodeType::Add:
            // 数值类型相加
            if (isNumeric() && right.isNumeric()) {
                return true;
            }
            // 字符串连接
            if (kind == BasicType::String && right.kind == BasicType::String) {
                return true;
            }
            // 列表连接
            if (kind == BasicType::List && right.kind == BasicType::List) {
                return true;
            }
            // 元组连接
            if (kind == BasicType::Tuple && right.kind == BasicType::Tuple) {
                return true;
            }
            return false;
        case ASTNodeType::Sub:
        case ASTNodeType::Mult:
        case ASTNodeType::Div:
            return isNumeric() && right.isNumeric();
        case ASTNodeType::FloorDiv:
        case ASTNodeType::Mod:
            return (kind == BasicType::Int || kind == BasicType::Float) &&
                   (right.kind == BasicType::Int || right.kind == BasicType::Float);
        case ASTNodeType::Pow:
            return isNumeric() && right.isNumeric();
        case ASTNodeType::LShift:
        case ASTNodeType::RShift:
            return kind == BasicType::Int && right.kind == BasicType::Int;
        case ASTNodeType::BitOr:
        case ASTNodeType::BitXor:
        case ASTNodeType::BitAnd:
            return (kind == BasicType::Int || kind == BasicType::Bool) &&
                   (right.kind == BasicType::Int || right.kind == BasicType::Bool);
        case ASTNodeType::MatMult:
            return (kind == BasicType::Int || kind == BasicType::Float) &&
                   (right.kind == BasicType::Int || right.kind == BasicType::Float);
        default:
            return false;
    }
}

bool Type::supportsUnaryOp(ASTNodeType op) const {
    switch (op) {
        case ASTNodeType::UAdd:
        case ASTNodeType::USub:
            return isNumeric();
        case ASTNodeType::Invert:
            return kind == BasicType::Int || kind == BasicType::Bool;
        case ASTNodeType::Not:
            return true; // 任何类型都可以取反
        default:
            return false;
    }
}

bool Type::supportsCompare(ASTNodeType op, const Type& right) const {
    switch (op) {
        case ASTNodeType::Eq:
        case ASTNodeType::NotEq:
            // 任何可比较类型都支持相等性比较
            return (isNumeric() && right.isNumeric()) ||
                   (kind == BasicType::String && right.kind == BasicType::String);
        case ASTNodeType::Lt:
        case ASTNodeType::LtE:
        case ASTNodeType::Gt:
        case ASTNodeType::GtE:
            // 数值类型和字符串支持有序比较
            return (isNumeric() && right.isNumeric()) ||
                   (kind == BasicType::String && right.kind == BasicType::String);
        case ASTNodeType::Is:
        case ASTNodeType::IsNot:
            return true; // 任何类型都可以做身份比较
        case ASTNodeType::In:
        case ASTNodeType::NotIn:
            return isIterable() || right.isIterable();
        default:
            return false;
    }
}

// ============================================================================
// TypeChecker 类实现
// ============================================================================

TypeChecker::TypeChecker(std::shared_ptr<SymbolTable> st)
    : symtable(std::move(st)) {}

// ============================================================================
// 模块检查
// ============================================================================

bool TypeChecker::checkModule(const Module& module) {
    for (const auto& stmt : module.body) {
        if (!checkStatement(*stmt)) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// 符号管理
// ============================================================================

void TypeChecker::defineSymbol(const std::string& name, const Type& type,
                              size_t line, size_t col) {
    SymbolInfo info(type, line, col);
    symbolTypes[name] = info;
}

std::optional<Type> TypeChecker::getSymbolType(const std::string& name) {
    auto it = symbolTypes.find(name);
    if (it != symbolTypes.end()) {
        it->second.markAsUsed();
        return it->second.type;
    }
    
    // 从符号表获取
    if (symtable) {
        auto result = symtable->lookup(name);
        if (result.second.has_value()) {
            // 根据作用域类型推断类型
            switch (result.second.value()) {
                case ScopeType::LOCAL:
                case ScopeType::GLOBAL_EXPLICIT:
                case ScopeType::GLOBAL_IMPLICIT:
                    return Type::object();
                case ScopeType::FREE:
                    return Type::object();
                case ScopeType::CELL:
                    return Type::object();
            }
        }
    }
    
    return std::nullopt;
}

void TypeChecker::useSymbol(const std::string& name) {
    auto it = symbolTypes.find(name);
    if (it != symbolTypes.end()) {
        it->second.markAsUsed();
    }
}

bool TypeChecker::isSymbolDefined(const std::string& name) {
    return symbolTypes.find(name) != symbolTypes.end();
}

void TypeChecker::reportUndefinedSymbol(const std::string& name, size_t line, size_t col) {
    TypeError error("undefined symbol: " + name, line, col);
    error.hint = "Did you forget to define this variable?";
    errors.push_back(error);
}

// ============================================================================
// 错误报告
// ============================================================================

void TypeChecker::reportError(const TypeError& error) {
    errors.push_back(error);
}

void TypeChecker::reportTypeMismatch(const Type& expected, const Type& actual,
                                    size_t line, size_t col) {
    std::stringstream ss;
    ss << "type mismatch: expected " << expected.toString()
       << ", got " << actual.toString();
    TypeError error(ss.str(), line, col);
    errors.push_back(error);
}

void TypeChecker::reportInvalidOperation(ASTNodeType op, const Type& left,
                                        const Type& right, size_t line, size_t col) {
    std::string opName;
    switch (op) {
        case ASTNodeType::Add: opName = "+"; break;
        case ASTNodeType::Sub: opName = "-"; break;
        case ASTNodeType::Mult: opName = "*"; break;
        case ASTNodeType::Div: opName = "/"; break;
        case ASTNodeType::FloorDiv: opName = "//"; break;
        case ASTNodeType::Mod: opName = "%"; break;
        case ASTNodeType::Pow: opName = "**"; break;
        default: opName = "<operator>"; break;
    }
    
    std::stringstream ss;
    ss << "unsupported operand type(s) for " << opName << ": "
       << "'" << left.toString() << "' and '" << right.toString() << "'";
    TypeError error(ss.str(), line, col);
    errors.push_back(error);
}

// ============================================================================
// 类型推断
// ============================================================================

Type TypeChecker::inferExpressionType(const Expr& expr) {
    switch (expr.type) {
        case ASTNodeType::Constant:
            return inferConstantType(static_cast<const Constant&>(expr));
        case ASTNodeType::Name:
            return inferNameType(static_cast<const Name&>(expr));
        case ASTNodeType::BinOp:
            return inferBinaryOpType(static_cast<const BinOp&>(expr));
        case ASTNodeType::UnaryOp:
            return inferUnaryOpType(static_cast<const UnaryOp&>(expr));
        case ASTNodeType::Compare:
            return inferCompareType(static_cast<const Compare&>(expr));
        case ASTNodeType::Call:
            return inferCallType(static_cast<const Call&>(expr));
        case ASTNodeType::Attribute:
            return inferAttributeType(static_cast<const Attribute&>(expr));
        case ASTNodeType::Subscript:
            return inferSubscriptType(static_cast<const Subscript&>(expr));
        case ASTNodeType::List:
            return inferContainerType(static_cast<const List&>(expr).elts);
        case ASTNodeType::ListComp:
            return inferListCompType(static_cast<const ListComp&>(expr));
        case ASTNodeType::Tuple:
            return inferContainerType(static_cast<const Tuple&>(expr).elts);
        case ASTNodeType::Dict:
            return inferDictType(static_cast<const Dict&>(expr).keys,
                                static_cast<const Dict&>(expr).values);
        case ASTNodeType::DictComp:
            return inferDictCompType(static_cast<const DictComp&>(expr));
        case ASTNodeType::Set:
            return inferContainerType(static_cast<const Set&>(expr).elts);
        case ASTNodeType::SetComp:
            return inferSetCompType(static_cast<const SetComp&>(expr));
        case ASTNodeType::GeneratorExp:
            return inferGeneratorExpType(static_cast<const GeneratorExp&>(expr));
        case ASTNodeType::IfExp: {
            const auto& ifexp = static_cast<const IfExp&>(expr);
            Type thenType = inferExpressionType(*ifexp.body);
            Type elseType = inferExpressionType(*ifexp.orelse);
            return getCommonType(thenType, elseType);
        }
        case ASTNodeType::Lambda:
            return Type::function();
        case ASTNodeType::BoolOp: {
            const auto& boolop = static_cast<const BoolOp&>(expr);
            for (const auto& value : boolop.values) {
                inferExpressionType(*value);
            }
            return Type::boolean();
        }
        default:
            return Type::unknown();
    }
}

Type TypeChecker::inferConstantType(const Constant& constant) {
    if (constant.isNone()) {
        return Type::none();
    }
    if (constant.isBool()) {
        return Type::boolean();
    }
    if (constant.isInt()) {
        return Type::integer();
    }
    if (constant.isFloat()) {
        return Type::floating();
    }
    if (constant.isString()) {
        return Type::string();
    }
    if (constant.isBytes()) {
        return Type::bytes();
    }
    return Type::unknown();
}

Type TypeChecker::inferNameType(const Name& name) {
    if (name.ctx == Name::Load) {
        // 特殊处理 self 参数（在方法中）
        if (name.id == "self") {
            return Type::object();
        }
        
        // 特殊处理内置类型名称（在类型注解中使用）
        if (name.id == "int") {
            return Type::integer();
        }
        if (name.id == "float") {
            return Type::floating();
        }
        if (name.id == "str") {
            return Type::string();
        }
        if (name.id == "bool") {
            return Type::boolean();
        }
        if (name.id == "list") {
            return Type::list();
        }
        if (name.id == "dict") {
            return Type::dict();
        }
        if (name.id == "tuple") {
            return Type::tuple();
        }
        if (name.id == "set") {
            return Type::set();
        }
        if (name.id == "None") {
            return Type::none();
        }
        
        // 特殊处理内置函数
        if (name.id == "range") {
            return Type::function();  // range 返回可迭代对象
        }
        if (name.id == "print") {
            return Type::function();  // print 是内置函数
        }
        if (name.id == "len") {
            return Type::function();  // len 是内置函数
        }
        if (name.id == "sum") {
            return Type::function();  // sum 是内置函数
        }
        if (name.id == "int") {
            return Type::function();  // int() 可作为函数调用
        }
        if (name.id == "float") {
            return Type::function();  // float() 可作为函数调用
        }
        if (name.id == "str") {
            return Type::function();  // str() 可作为函数调用
        }
        
        auto type = getSymbolType(name.id);
        if (type.has_value()) {
            return type.value();
        }
        // 报告未定义错误
        reportUndefinedSymbol(name.id, name.position.line, name.position.column);
        return Type::unknown();
    } else if (name.ctx == Name::Store) {
        // Store 上下文中，符号应该已由 checkAssignment 定义
        // 如果找不到，返回 unknown 但不报告错误
        return Type::unknown();
    }
    return Type::unknown();
}

Type TypeChecker::inferBinaryOpType(const BinOp& binop) {
    Type left = inferExpressionType(*binop.left);
    Type right = inferExpressionType(*binop.right);
    
    // 检查运算符兼容性
    if (!left.supportsBinOp(binop.op, right)) {
        reportInvalidOperation(binop.op, left, right,
                             binop.position.line, binop.position.column);
        return Type::unknown();
    }
    
    // 根据运算符推断返回类型
    switch (binop.op) {
        case ASTNodeType::Add:
            return getCommonType(left, right);
        case ASTNodeType::Sub:
            return getCommonType(left, right);
        case ASTNodeType::Mult:
            return getCommonType(left, right);
        case ASTNodeType::Div:
        case ASTNodeType::FloorDiv:
        case ASTNodeType::Mod:
            // 除法类操作：如果有 Float 操作数则返回 Float，否则返回 Float
            // 注意：在 Python 中，/ 总是返回 float，即使操作数是整数
            if (left.isNumeric() && right.isNumeric()) {
                // 如果任一操作数是 Float，则返回 Float
                if (left.kind == BasicType::Float || right.kind == BasicType::Float) {
                    return Type::floating();
                }
                // 否则（两个都是整数），除法返回 Float
                return Type::floating();
            }
            return Type::unknown();
        case ASTNodeType::Pow:
            return getCommonType(left, right);
        case ASTNodeType::LShift:
        case ASTNodeType::RShift:
            return Type::integer();
        case ASTNodeType::BitOr:
        case ASTNodeType::BitXor:
        case ASTNodeType::BitAnd:
            return Type::integer();
        case ASTNodeType::MatMult:
            return getCommonType(left, right);
        default:
            return Type::unknown();
    }
}

Type TypeChecker::inferUnaryOpType(const UnaryOp& unop) {
    Type operand = inferExpressionType(*unop.operand);
    
    if (!operand.supportsUnaryOp(unop.op)) {
        reportInvalidOperation(unop.op, operand, operand,
                             unop.position.line, unop.position.column);
        return Type::unknown();
    }
    
    switch (unop.op) {
        case ASTNodeType::UAdd:
        case ASTNodeType::USub:
            return operand;
        case ASTNodeType::Invert:
            return Type::integer();
        case ASTNodeType::Not:
            return Type::boolean();
        default:
            return Type::unknown();
    }
}

Type TypeChecker::inferCompareType(const Compare& compare) {
    Type left = inferExpressionType(*compare.left);
    
    for (const auto& comparator : compare.comparators) {
        Type right = inferExpressionType(*comparator);
        
        ASTNodeType op = compare.ops.empty() ? ASTNodeType::Eq : compare.ops[0];
        if (!left.supportsCompare(op, right)) {
            reportInvalidOperation(op, left, right,
                                 compare.position.line, compare.position.column);
        }
    }
    
    return Type::boolean();
}

Type TypeChecker::inferCallType(const Call& call) {
    Type funcType = inferExpressionType(*call.func);
    
    // 检查函数调用参数
    for (const auto& arg : call.args) {
        inferExpressionType(*arg);
    }
    
    for (const auto& keyword : call.keywords) {
        inferExpressionType(*keyword);
    }
    
    // 如果调用的是类（类型为 Object），则返回 Object 实例类型
    if (funcType.kind == BasicType::Object) {
        return Type::object();
    }
    
    // 特殊处理内置函数 range
    if (call.func->type == ASTNodeType::Name) {
        const auto& name = static_cast<const Name&>(*call.func);
        if (name.id == "range") {
            return Type::listOf(Type::integer());  // range 返回整数列表
        }
        if (name.id == "sum") {
            // sum 返回与输入相同的数值类型
            if (!call.args.empty()) {
                Type argType = inferExpressionType(*call.args[0]);
                if (argType.kind == BasicType::List || argType.kind == BasicType::Tuple) {
                    return Type::integer();  // 简化处理：假设 sum 返回整数
                }
            }
            return Type::integer();
        }
        if (name.id == "len") {
            return Type::integer();  // len 返回整数
        }
    }
    
    // 如果函数类型已知，尝试获取其返回类型
    if (call.func->type == ASTNodeType::Name) {
        const auto& name = static_cast<const Name&>(*call.func);
        auto it = symbolTypes.find(name.id + "_return_type");
        if (it != symbolTypes.end()) {
            return it->second.type;
        }
    }
    
    // 函数调用返回 unknown（因为函数可能返回任何类型）
    return Type::unknown();
}

Type TypeChecker::inferAttributeType(const Attribute& attr) {
    inferExpressionType(*attr.value);
    
    // 如果值类型是列表，且访问的是 append 方法，返回函数类型
    if (attr.attr == "append") {
        return Type::function();
    }
    
    // 属性类型未知（需要完整的类型系统）
    return Type::object();
}

Type TypeChecker::inferSubscriptType(const Subscript& subscript) {
    Type valueType = inferExpressionType(*subscript.value);
    Type sliceType = inferExpressionType(*subscript.slice);
    
    if (!isIndexable(valueType)) {
        TypeError error("subscript requires a sequence or mapping type",
                       subscript.position.line, subscript.position.column);
        error.hint = "Use [] on lists, tuples, dicts, or strings";
        errors.push_back(error);
        return Type::unknown();
    }
    
    if (valueType.kind == BasicType::Dict) {
        return valueType.dictTypes.at("__value__");
    }
    
    return getIndexType(valueType);
}

Type TypeChecker::inferListCompType(const ListComp& listComp) {
    // 列表推导式返回列表类型
    
    // 处理生成器
    for (const auto& gen : listComp.generators) {
        if (gen->iter) {
            inferExpressionType(*gen->iter);
        }
        
        // 处理目标变量（在 for 循环的 Store 上下文中定义）
        if (gen->target->type == ASTNodeType::Name) {
            const auto& name = static_cast<const Name&>(*gen->target);
            defineSymbol(name.id, Type::integer(),
                        gen->target->position.line, gen->target->position.column);
        } else {
            inferExpressionType(*gen->target);
        }
        
        // 处理 if 条件
        if (!gen->ifs.empty()) {
            for (const auto& cond : gen->ifs) {
                inferExpressionType(*cond);
            }
        }
    }
    
    // 推断元素类型
    if (listComp.elt) {
        inferExpressionType(*listComp.elt);
    }
    
    return Type::list();
}

Type TypeChecker::inferDictCompType(const DictComp& dictComp) {
    // 字典推导式返回字典类型
    
    // 处理生成器
    for (const auto& gen : dictComp.generators) {
        if (gen->iter) {
            inferExpressionType(*gen->iter);
        }
        
        // 处理目标变量（在 for 循环的 Store 上下文中定义）
        if (gen->target->type == ASTNodeType::Name) {
            const auto& name = static_cast<const Name&>(*gen->target);
            defineSymbol(name.id, Type::integer(),
                        gen->target->position.line, gen->target->position.column);
        } else {
            inferExpressionType(*gen->target);
        }
        
        // 处理 if 条件
        if (!gen->ifs.empty()) {
            for (const auto& cond : gen->ifs) {
                inferExpressionType(*cond);
            }
        }
    }
    
    // 推断键和值类型
    if (dictComp.key) {
        inferExpressionType(*dictComp.key);
    }
    if (dictComp.value) {
        inferExpressionType(*dictComp.value);
    }
    
    return Type::dict();
}

Type TypeChecker::inferSetCompType(const SetComp& setComp) {
    // 集合推导式返回集合类型
    
    // 处理生成器
    for (const auto& gen : setComp.generators) {
        if (gen->iter) {
            inferExpressionType(*gen->iter);
        }
        
        // 处理目标变量（在 for 循环的 Store 上下文中定义）
        if (gen->target->type == ASTNodeType::Name) {
            const auto& name = static_cast<const Name&>(*gen->target);
            defineSymbol(name.id, Type::integer(),
                        gen->target->position.line, gen->target->position.column);
        } else {
            inferExpressionType(*gen->target);
        }
        
        // 处理 if 条件
        if (!gen->ifs.empty()) {
            for (const auto& cond : gen->ifs) {
                inferExpressionType(*cond);
            }
        }
    }
    
    // 推断元素类型
    if (setComp.elt) {
        inferExpressionType(*setComp.elt);
    }
    
    return Type::set();
}

Type TypeChecker::inferGeneratorExpType(const GeneratorExp& genExp) {
    // 生成器表达式返回可迭代对象（简化处理，返回列表类型）
    
    // 处理生成器
    for (const auto& gen : genExp.generators) {
        if (gen->iter) {
            inferExpressionType(*gen->iter);
        }
        
        // 处理目标变量（在 for 循环的 Store 上下文中定义）
        if (gen->target->type == ASTNodeType::Name) {
            const auto& name = static_cast<const Name&>(*gen->target);
            defineSymbol(name.id, Type::integer(),
                        gen->target->position.line, gen->target->position.column);
        } else {
            inferExpressionType(*gen->target);
        }
        
        // 处理 if 条件
        if (!gen->ifs.empty()) {
            for (const auto& cond : gen->ifs) {
                inferExpressionType(*cond);
            }
        }
    }
    
    // 推断元素类型
    if (genExp.elt) {
        inferExpressionType(*genExp.elt);
    }
    
    return Type::list();
}

Type TypeChecker::inferContainerType(const std::vector<std::unique_ptr<Expr>>& elements) {
    if (elements.empty()) {
        return Type::list();
    }
    
    // 推断元素类型
    Type elemType = inferExpressionType(*elements[0]);
    for (size_t i = 1; i < elements.size(); ++i) {
        Type other = inferExpressionType(*elements[i]);
        elemType = getCommonType(elemType, other);
    }
    
    return Type::listOf(elemType);
}

Type TypeChecker::inferDictType(const std::vector<std::unique_ptr<Expr>>& keys,
                               const std::vector<std::unique_ptr<Expr>>& values) {
    if (keys.empty()) {
        return Type::dict();
    }
    
    Type keyType = keys[0] ? inferExpressionType(*keys[0]) : Type::any();
    Type valueType = values[0] ? inferExpressionType(*values[0]) : Type::any();
    
    for (size_t i = 1; i < keys.size(); ++i) {
        if (keys[i]) {
            keyType = getCommonType(keyType, inferExpressionType(*keys[i]));
        }
    }
    
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i]) {
            valueType = getCommonType(valueType, inferExpressionType(*values[i]));
        }
    }
    
    return Type::dictOf(keyType, valueType);
}

// ============================================================================
// 辅助方法
// ============================================================================

bool TypeChecker::checkArithmeticTypes(const Type& left, const Type& right,
                                      ASTNodeType op, size_t line, size_t col) {
    if (!left.supportsBinOp(op, right)) {
        reportInvalidOperation(op, left, right, line, col);
        return false;
    }
    return true;
}

bool TypeChecker::checkComparisonTypes(const Type& left, const Type& right,
                                      ASTNodeType op, size_t line, size_t col) {
    if (!left.supportsCompare(op, right)) {
        reportInvalidOperation(op, left, right, line, col);
        return false;
    }
    return true;
}

bool TypeChecker::checkBooleanTypes(const Type& left, const Type& right,
                                   size_t line, size_t col) {
    return true; // 任何类型都可以用于布尔运算
}

Type TypeChecker::getCommonType(const Type& a, const Type& b) {
    if (a.kind == BasicType::Any || b.kind == BasicType::Any) {
        return Type::any();
    }
    
    if (a.kind == b.kind) {
        return a;
    }
    
    // 处理数值类型提升
    bool aNumeric = a.isNumeric();
    bool bNumeric = b.isNumeric();
    
    if (aNumeric && bNumeric) {
        // 优先级: complex > float > int > bool
        if (a.kind == BasicType::Complex || b.kind == BasicType::Complex) {
            return Type::complex();
        }
        if (a.kind == BasicType::Float || b.kind == BasicType::Float) {
            return Type::floating();
        }
        return Type::integer();
    }
    
    // 默认返回 object
    return Type::object();
}

bool TypeChecker::isIndexable(const Type& t) {
    return t.kind == BasicType::List || t.kind == BasicType::Tuple ||
           t.kind == BasicType::Dict || t.kind == BasicType::String ||
           t.kind == BasicType::Bytes;
}

Type TypeChecker::getIndexType(const Type& t) {
    switch (t.kind) {
        case BasicType::List:
        case BasicType::Tuple:
            if (t.elementType) {
                return *t.elementType;
            }
            return Type::any();
        case BasicType::Dict:
            return t.dictTypes.at("__value__");
        case BasicType::String:
        case BasicType::Bytes:
            return t.kind == BasicType::String ? Type::string() : Type::bytes();
        default:
            return Type::any();
    }
}

// ============================================================================
// 语句检查
// ============================================================================

bool TypeChecker::checkStatement(const Stmt& stmt) {
    switch (stmt.type) {
        case ASTNodeType::ExprStmt:
            return checkExpressionStatement(static_cast<const ExprStmt&>(stmt));
        case ASTNodeType::Assign:
            return checkAssignment(static_cast<const Assign&>(stmt));
        case ASTNodeType::AnnAssign:
            return checkAnnotatedAssignment(static_cast<const AnnAssign&>(stmt));
        case ASTNodeType::FunctionDef:
            return checkFunctionDefinition(static_cast<const FunctionDef&>(stmt));
        case ASTNodeType::ClassDef:
            return checkClassDefinition(static_cast<const ClassDef&>(stmt));
        case ASTNodeType::Return:
            return checkReturn(static_cast<const Return&>(stmt));
        case ASTNodeType::If:
            return checkIfStatement(static_cast<const If&>(stmt));
        case ASTNodeType::For:
            return checkForLoop(static_cast<const For&>(stmt));
        case ASTNodeType::While:
            return checkWhileLoop(static_cast<const While&>(stmt));
        case ASTNodeType::Try:
            return checkTryStatement(static_cast<const Try&>(stmt));
        default:
            return true;
    }
}

bool TypeChecker::checkExpressionStatement(const ExprStmt& exprStmt) {
    inferExpressionType(*exprStmt.value);
    return !hasErrors();
}

bool TypeChecker::checkAssignment(const Assign& assign) {
    Type valueType = inferExpressionType(*assign.value);
    
    for (const auto& target : assign.targets) {
        if (target->type == ASTNodeType::Name) {
            const auto& name = static_cast<const Name&>(*target);
            if (name.ctx == Name::Store) {
                defineSymbol(name.id, valueType, 
                            target->position.line, target->position.column);
            }
        }
        inferExpressionType(*target);
    }
    
    return !hasErrors();
}

bool TypeChecker::checkAnnotatedAssignment(const AnnAssign& assign) {
    if (assign.annotation) {
        inferExpressionType(*assign.annotation);
    }
    
    if (assign.value) {
        Type valueType = inferExpressionType(*assign.value);
        
        if (assign.target && assign.target->type == ASTNodeType::Name) {
            const auto& name = static_cast<const Name&>(*assign.target);
            defineSymbol(name.id, valueType,
                        assign.target->position.line, assign.target->position.column);
        }
    }
    
    return !hasErrors();
}

bool TypeChecker::checkFunctionDefinition(const FunctionDef& func) {
    // 函数名在定义时检查
    defineSymbol(func.name, Type::function(),
                func.position.line, func.position.column);
    
    // 处理函数参数
    if (func.args) {
        for (const auto& arg : func.args->args) {
            if (arg && arg->type == ASTNodeType::arg) {
                const auto& param = static_cast<const csgo::arg&>(*arg);
                
                // 根据类型注解确定参数类型
                Type paramType = Type::object();
                if (param.annotation) {
                    paramType = inferExpressionType(*param.annotation);
                }
                
                size_t line = param.position.line > 0 ? param.position.line : func.position.line;
                size_t col = param.position.column > 0 ? param.position.column : func.position.column;
                defineSymbol(param.name, paramType, line, col);
            }
        }
    }
    
    // 处理返回类型注解（如果有）- 优先使用注解
    Type returnType = Type::unknown();
    bool hasReturnAnnotation = false;
    
    if (!func.returns.empty()) {
        // 解析返回类型注解
        if (func.returns == "int") {
            returnType = Type::integer();
            hasReturnAnnotation = true;
        } else if (func.returns == "float") {
            returnType = Type::floating();
            hasReturnAnnotation = true;
        } else if (func.returns == "str") {
            returnType = Type::string();
            hasReturnAnnotation = true;
        } else if (func.returns == "bool") {
            returnType = Type::boolean();
            hasReturnAnnotation = true;
        } else if (func.returns == "list") {
            returnType = Type::list();
            hasReturnAnnotation = true;
        }
    }
    
    // 如果没有返回类型注解，从 return 语句推断
    if (!hasReturnAnnotation) {
        for (const auto& stmt : func.body) {
            if (stmt->type == ASTNodeType::Return) {
                const auto& ret = static_cast<const Return&>(*stmt);
                if (ret.value) {
                    returnType = inferExpressionType(*ret.value);
                    break;  // 使用第一个 return 语句的类型
                }
            }
        }
    }
    
    // 存储函数返回类型
    if (returnType.kind != BasicType::Unknown) {
        symbolTypes[func.name + "_return_type"] = SymbolInfo(returnType,
            func.position.line, func.position.column);
    }
    
    // 检查函数体
    for (const auto& stmt : func.body) {
        if (!checkStatement(*stmt)) {
            return false;
        }
    }
    
    return !hasErrors();
}

bool TypeChecker::checkClassDefinition(const ClassDef& cls) {
    // 类名在定义时检查
    defineSymbol(cls.name, Type::object(),
                cls.position.line, cls.position.column);
    
    // 检查类体
    for (const auto& stmt : cls.body) {
        if (!checkStatement(*stmt)) {
            return false;
        }
    }
    
    return !hasErrors();
}

bool TypeChecker::checkTryStatement(const Try& tryStmt) {
    // 检查 try 块
    for (const auto& stmt : tryStmt.body) {
        if (!checkStatement(*stmt)) {
            return false;
        }
    }
    
    // 检查 except 处理器
    for (const auto& handler : tryStmt.handlers) {
        // 处理异常变量名（如果有）
        if (!handler->name.empty()) {
            defineSymbol(handler->name, Type::object(),
                        handler->position.line, handler->position.column);
        }
        
        // 检查处理器体
        for (const auto& stmt : handler->body) {
            if (!checkStatement(*stmt)) {
                return false;
            }
        }
    }
    
    // 检查 else 块
    for (const auto& stmt : tryStmt.orelse) {
        if (!checkStatement(*stmt)) {
            return false;
        }
    }
    
    // 检查 finally 块
    for (const auto& stmt : tryStmt.finalbody) {
        if (!checkStatement(*stmt)) {
            return false;
        }
    }
    
    return !hasErrors();
}

bool TypeChecker::checkReturn(const Return& ret) {
    if (ret.value) {
        inferExpressionType(*ret.value);
    }
    return !hasErrors();
}

bool TypeChecker::checkIfStatement(const If& ifStmt) {
    Type testType = inferExpressionType(*ifStmt.test);
    
    if (!testType.isCompatibleWith(Type::boolean()) &&
        testType.kind != BasicType::Any) {
        TypeError error("if condition must be boolean",
                       ifStmt.test->position.line, ifStmt.test->position.column);
        errors.push_back(error);
    }
    
    for (const auto& stmt : ifStmt.body) {
        if (!checkStatement(*stmt)) return false;
    }
    
    for (const auto& stmt : ifStmt.orelse) {
        if (!checkStatement(*stmt)) return false;
    }
    
    return !hasErrors();
}

bool TypeChecker::checkForLoop(const For& forStmt) {
    Type iterType = inferExpressionType(*forStmt.iter);
    
    if (!iterType.isIterable() && iterType.kind != BasicType::Any) {
        TypeError error("iterable required for for loop",
                       forStmt.iter->position.line, forStmt.iter->position.column);
        errors.push_back(error);
    }
    
    if (forStmt.target->type == ASTNodeType::Name) {
        const auto& name = static_cast<const Name&>(*forStmt.target);
        // 直接定义循环变量，不依赖于 ctx
        defineSymbol(name.id, Type::any(),
                    forStmt.target->position.line, forStmt.target->position.column);
        // 对于 for 循环中的变量，不调用 inferExpressionType，因为它的上下文是 Store
    } else {
        inferExpressionType(*forStmt.target);
    }
    
    for (const auto& stmt : forStmt.body) {
        if (!checkStatement(*stmt)) return false;
    }
    
    for (const auto& stmt : forStmt.orelse) {
        if (!checkStatement(*stmt)) return false;
    }
    
    return !hasErrors();
}

bool TypeChecker::checkWhileLoop(const While& whileStmt) {
    Type testType = inferExpressionType(*whileStmt.test);
    
    if (!testType.isCompatibleWith(Type::boolean()) &&
        testType.kind != BasicType::Any) {
        TypeError error("while condition must be boolean",
                       whileStmt.test->position.line, whileStmt.test->position.column);
        errors.push_back(error);
    }
    
    for (const auto& stmt : whileStmt.body) {
        if (!checkStatement(*stmt)) return false;
    }
    
    for (const auto& stmt : whileStmt.orelse) {
        if (!checkStatement(*stmt)) return false;
    }
    
    return !hasErrors();
}

// ============================================================================
// SemanticAnalyzer 类实现
// ============================================================================

SemanticAnalyzer::SemanticAnalyzer(const std::string& filename)
    : symtable(std::make_shared<SymbolTable>(filename)),
      typeChecker(nullptr) {}

bool SemanticAnalyzer::analyze(std::unique_ptr<Module> module) {
    // 第一阶段：构建符号表
    SymbolTableBuilder builder(symtable->filename);
    auto newSymtable = builder.build(*module);  // 使用解引用，不转移所有权
    
    if (!newSymtable) {
        errors.push_back("Failed to build symbol table");
        return false;
    }
    
    symtable = newSymtable;
    
    // 第二阶段：类型检查
    typeChecker = std::make_unique<TypeChecker>(symtable);
    
    // 重新遍历进行类型检查
    // 注意：这里需要再次创建 AST 或保存副本
    // 在实际实现中，应该在构建符号表时就进行类型检查
    
    return !hasErrors();
}

std::vector<std::string> SemanticAnalyzer::getErrorMessages() const {
    std::vector<std::string> messages;
    
    for (const auto& err : errors) {
        messages.push_back(err);
    }
    
    if (typeChecker) {
        for (const auto& err : typeChecker->getErrors()) {
            messages.push_back(err.toString());
        }
    }
    
    return messages;
}

} // namespace csgo