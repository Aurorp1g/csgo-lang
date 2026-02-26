/**
 * @file ast_printer.cpp
 * @brief CSGO 编程语言 AST 节点打印实现文件
 *
 * @author Aurorp1g
 * @version 2.0
 * @date 2026
 *
 * @section description 描述
 * 本文件实现了 CSGO 语言抽象语法树（AST）节点的字符串序列化功能。
 * 提供 toString() 方法将 AST 节点转换为可读的字符串表示，
 * 主要用于调试、错误报告和AST可视化。
 *
 * @section features 功能特性
 * - 支持所有 AST 节点类型的字符串序列化
 * - 生成格式化的树形结构字符串（带缩进）
 * - 包含完整的节点位置信息（行号、列号）
 * - 支持嵌套节点的递归序列化
 * - 可配置缩进宽度
 *
 * @section usage 使用示例
 * @code
 * // 创建 AST 节点
 * auto constant = std::make_unique<csgo::Constant>(csgo::Position(1, 1), 42);
 *
 * // 转换为字符串表示（单行格式）
 * std::string repr = constant->toString();
 * // 输出: "Constant(42, line=1, col=1)"
 *
 * // 打印整个模块（带缩进格式）
 * std::cout << module->toString(0) << std::endl;
 * @endcode
 *
 * @see ast_node.h AST节点类型定义
 * @see Parser 语法分析器
 */

#include "ast/ast_node.h"
#include <sstream>
#include <iomanip>

namespace csgo {

// 全局配置：缩进空格数
static constexpr int INDENT_SIZE = 2;

/**
 * @brief 生成缩进字符串
 * @param indent 缩进级别
 * @return 缩进字符串（空格）
 */
static std::string getIndent(int indent) {
    return std::string(indent * INDENT_SIZE, ' ');
}

/**
 * @brief 格式化列表输出
 * @param oss 输出流
 * @param items 节点列表
 * @param indent 当前缩进级别
 * @param fieldName 字段名称
 */
static void formatNodeList(std::ostringstream& oss, 
                           const std::vector<std::unique_ptr<ASTNode>>& items, 
                           int indent, 
                           const std::string& fieldName) {
    if (items.empty()) {
        oss << "[]";
        return;
    }
    oss << "[\n";
    for (size_t i = 0; i < items.size(); ++i) {
        oss << getIndent(indent + 1);
        if (items[i]) {
            oss << items[i]->toString(indent + 1);
        } else {
            oss << "None";
        }
        if (i < items.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent) << "]";
}

/**
 * @brief 格式化可选节点输出
 * @param oss 输出流
 * @param node 可选节点
 * @param indent 当前缩进级别
 */
static void formatOptionalNode(std::ostringstream& oss, 
                               const std::unique_ptr<ASTNode>& node, 
                               int indent) {
    if (node) {
        oss << "\n" << getIndent(indent + 1) << node->toString(indent + 1);
    } else {
        oss << " None";
    }
}

/**
 * @brief 获取 AST 节点类型的字符串名称
 * @param type AST 节点类型枚举值
 * @return 类型名称字符串，如 "Module", "FunctionDef" 等
 * @note 用于调试输出和日志记录
 */
const char* ASTNode::getTypeName(ASTNodeType type) {
    switch (type) {
        case ASTNodeType::Module: return "Module";
        case ASTNodeType::Import: return "Import";
        case ASTNodeType::ImportFrom: return "ImportFrom";
        case ASTNodeType::Alias: return "Alias";
        case ASTNodeType::FunctionDef: return "FunctionDef";
        case ASTNodeType::AsyncFunctionDef: return "AsyncFunctionDef";
        case ASTNodeType::ClassDef: return "ClassDef";
        case ASTNodeType::Return: return "Return";
        case ASTNodeType::Delete: return "Delete";
        case ASTNodeType::Assign: return "Assign";
        case ASTNodeType::AugAssign: return "AugAssign";
        case ASTNodeType::AnnAssign: return "AnnAssign";
        case ASTNodeType::For: return "For";
        case ASTNodeType::AsyncFor: return "AsyncFor";
        case ASTNodeType::While: return "While";
        case ASTNodeType::If: return "If";
        case ASTNodeType::With: return "With";
        case ASTNodeType::AsyncWith: return "AsyncWith";
        case ASTNodeType::Raise: return "Raise";
        case ASTNodeType::Try: return "Try";
        case ASTNodeType::Assert: return "Assert";
        case ASTNodeType::Global: return "Global";
        case ASTNodeType::Nonlocal: return "Nonlocal";
        case ASTNodeType::ExprStmt: return "ExprStmt";
        case ASTNodeType::Pass: return "Pass";
        case ASTNodeType::Break: return "Break";
        case ASTNodeType::Continue: return "Continue";
        case ASTNodeType::BoolOp: return "BoolOp";
        case ASTNodeType::BinOp: return "BinOp";
        case ASTNodeType::UnaryOp: return "UnaryOp";
        case ASTNodeType::Lambda: return "Lambda";
        case ASTNodeType::IfExp: return "IfExp";
        case ASTNodeType::Dict: return "Dict";
        case ASTNodeType::Set: return "Set";
        case ASTNodeType::ListComp: return "ListComp";
        case ASTNodeType::SetComp: return "SetComp";
        case ASTNodeType::DictComp: return "DictComp";
        case ASTNodeType::GeneratorExp: return "GeneratorExp";
        case ASTNodeType::Yield: return "Yield";
        case ASTNodeType::YieldFrom: return "YieldFrom";
        case ASTNodeType::Await: return "Await";
        case ASTNodeType::Compare: return "Compare";
        case ASTNodeType::Call: return "Call";
        case ASTNodeType::FormattedValue: return "FormattedValue";
        case ASTNodeType::JoinedStr: return "JoinedStr";
        case ASTNodeType::Constant: return "Constant";
        case ASTNodeType::List: return "List";
        case ASTNodeType::Attribute: return "Attribute";
        case ASTNodeType::Subscript: return "Subscript";
        case ASTNodeType::Slice: return "Slice";
        case ASTNodeType::Starred: return "Starred";
        case ASTNodeType::Tuple: return "Tuple";
        case ASTNodeType::Name: return "Name";
        case ASTNodeType::Load: return "Load";
        case ASTNodeType::Store: return "Store";
        case ASTNodeType::Del: return "Del";
        case ASTNodeType::arguments: return "arguments";
        case ASTNodeType::arg: return "arg";
        case ASTNodeType::except_handler: return "except_handler";
        case ASTNodeType::withitem: return "withitem";
        case ASTNodeType::comprehension: return "comprehension";
        case ASTNodeType::Num: return "Num";
        case ASTNodeType::Str: return "Str";
        case ASTNodeType::Bytes: return "Bytes";
        case ASTNodeType::NameConstant: return "NameConstant";
        case ASTNodeType::Ellipsis: return "Ellipsis";
        case ASTNodeType::Add: return "Add";
        case ASTNodeType::Sub: return "Sub";
        case ASTNodeType::Mult: return "Mult";
        case ASTNodeType::MatMult: return "MatMult";
        case ASTNodeType::Div: return "Div";
        case ASTNodeType::FloorDiv: return "FloorDiv";
        case ASTNodeType::Mod: return "Mod";
        case ASTNodeType::Pow: return "Pow";
        case ASTNodeType::LShift: return "LShift";
        case ASTNodeType::RShift: return "RShift";
        case ASTNodeType::BitOr: return "BitOr";
        case ASTNodeType::BitXor: return "BitXor";
        case ASTNodeType::BitAnd: return "BitAnd";
        case ASTNodeType::Eq: return "Eq";
        case ASTNodeType::NotEq: return "NotEq";
        case ASTNodeType::Lt: return "Lt";
        case ASTNodeType::LtE: return "LtE";
        case ASTNodeType::Gt: return "Gt";
        case ASTNodeType::GtE: return "GtE";
        case ASTNodeType::Is: return "Is";
        case ASTNodeType::IsNot: return "IsNot";
        case ASTNodeType::In: return "In";
        case ASTNodeType::NotIn: return "NotIn";
        case ASTNodeType::Invert: return "Invert";
        case ASTNodeType::Not: return "Not";
        case ASTNodeType::UAdd: return "UAdd";
        case ASTNodeType::USub: return "USub";
        case ASTNodeType::And: return "And";
        case ASTNodeType::Or: return "Or";
        case ASTNodeType::Expr: return "Expr";
        case ASTNodeType::Stmt: return "Stmt";
        case ASTNodeType::Invalid: return "Invalid";
        default: return "Unknown";
    }
}

/**
 * @brief 将 Constant 节点序列化为字符串（单行格式）
 * @return 格式化的字符串表示，格式如：Constant(42, line=1, col=1)
 * @details 支持所有常量类型：None, True, False, Ellipsis, 整数, 浮点数, 字符串, 字节串
 */
std::string Constant::toString() const {
    std::ostringstream oss;
    oss << "Constant(";
    
    if (isNone()) {
        oss << "None";
    } else if (isTrue()) {
        oss << "True";
    } else if (isFalse()) {
        oss << "False";
    } else if (std::holds_alternative<std::monostate>(value)) {
        oss << "...";
    } else if (isInt()) {
        oss << asInt();
    } else if (isFloat()) {
        oss << std::setprecision(15) << asFloat();
    } else if (isString()) {
        oss << "\"" << asString() << "\"";
    } else if (isBytes()) {
        auto bytes = std::get<std::vector<uint8_t>>(value);
        oss << "b\"";
        for (auto byte : bytes) {
            oss << std::hex << std::setfill('0') << std::setw(2) << (int)byte;
        }
        oss << "\"";
    } else {
        oss << "<unknown>";
    }
    
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

/**
 * @brief 将 Constant 节点序列化为字符串（带缩进格式）
 * @param indent 缩进级别
 * @return 格式化的字符串表示
 */
std::string Constant::toString(int indent) const {
    return getIndent(indent) + toString();
}

/**
 * @brief 将 Name 节点序列化为字符串（单行格式）
 * @return 格式化的字符串表示，格式如：Name(id='x', ctx=Load, line=1, col=1)
 * @details 包含变量标识符和上下文类型（Load/Store/Del）
 */
std::string Name::toString() const {
    std::ostringstream oss;
    oss << "Name(" << "id='" << id << "', ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Name::toString(int indent) const {
    return getIndent(indent) + toString();
}

std::string BinOp::toString() const {
    std::ostringstream oss;
    oss << "BinOp(";
    if (left) oss << left->toString();
    oss << " " << getTypeName(op) << ", ";
    if (right) oss << right->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string BinOp::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "BinOp(\n";
    oss << getIndent(indent + 1) << "op=" << getTypeName(op) << ",\n";
    oss << getIndent(indent + 1) << "left=";
    if (left) {
        oss << "\n" << left->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "right=";
    if (right) {
        oss << "\n" << right->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

/**
 * @brief 将 UnaryOp 节点序列化为字符串（单行格式）
 * @return 格式化的字符串表示，格式如：UnaryOp(USub, Name(...))
 * @details 包含一元运算符类型和操作数
 */
std::string UnaryOp::toString() const {
    std::ostringstream oss;
    oss << "UnaryOp(" << getTypeName(op);
    if (operand) oss << " " << operand->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string UnaryOp::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "UnaryOp(\n";
    oss << getIndent(indent + 1) << "op=" << getTypeName(op) << ",\n";
    oss << getIndent(indent + 1) << "operand=";
    if (operand) {
        oss << "\n" << operand->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Compare::toString() const {
    std::ostringstream oss;
    oss << "Compare(";
    if (left) oss << left->toString();
    oss << ", [";
    for (size_t i = 0; i < ops.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << getTypeName(ops[i]);
    }
    oss << "], [";
    for (size_t i = 0; i < comparators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (comparators[i]) oss << comparators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Compare::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Compare(\n";
    oss << getIndent(indent + 1) << "left=";
    if (left) {
        oss << "\n" << left->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "ops=[";
    for (size_t i = 0; i < ops.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << getTypeName(ops[i]);
    }
    oss << "],\n";
    oss << getIndent(indent + 1) << "comparators=[\n";
    for (size_t i = 0; i < comparators.size(); ++i) {
        oss << getIndent(indent + 2);
        if (comparators[i]) {
            oss << comparators[i]->toString();
        } else {
            oss << "None";
        }
        if (i < comparators.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string BoolOp::toString() const {
    std::ostringstream oss;
    oss << "BoolOp(" << getTypeName(op) << ", [";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        if (values[i]) oss << values[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string BoolOp::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "BoolOp(\n";
    oss << getIndent(indent + 1) << "op=" << getTypeName(op) << ",\n";
    oss << getIndent(indent + 1) << "values=[\n";
    for (size_t i = 0; i < values.size(); ++i) {
        oss << getIndent(indent + 2);
        if (values[i]) {
            oss << values[i]->toString();
        } else {
            oss << "None";
        }
        if (i < values.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Call::toString() const {
    std::ostringstream oss;
    oss << "Call(";
    if (func) oss << func->toString();
    oss << ", args=[";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) oss << ", ";
        if (args[i]) oss << args[i]->toString();
    }
    oss << "], keywords=[";
    for (size_t i = 0; i < keywords.size(); ++i) {
        if (i > 0) oss << ", ";
        if (keywords[i]) oss << keywords[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Call::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Call(\n";
    oss << getIndent(indent + 1) << "func=";
    if (func) {
        oss << "\n" << func->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "args=[\n";
    for (size_t i = 0; i < args.size(); ++i) {
        oss << getIndent(indent + 2);
        if (args[i]) {
            oss << args[i]->toString();
        } else {
            oss << "None";
        }
        if (i < args.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "keywords=[\n";
    for (size_t i = 0; i < keywords.size(); ++i) {
        oss << getIndent(indent + 2);
        if (keywords[i]) {
            oss << keywords[i]->toString();
        } else {
            oss << "None";
        }
        if (i < keywords.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string IfExp::toString() const {
    std::ostringstream oss;
    oss << "IfExp(test=";
    if (test) oss << test->toString();
    oss << ", body=";
    if (body) oss << body->toString();
    oss << ", orelse=";
    if (orelse) oss << orelse->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string IfExp::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "IfExp(\n";
    oss << getIndent(indent + 1) << "test=";
    if (test) {
        oss << "\n" << test->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=";
    if (body) {
        oss << "\n" << body->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "orelse=";
    if (orelse) {
        oss << "\n" << orelse->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Lambda::toString() const {
    std::ostringstream oss;
    oss << "Lambda(args=";
    if (args) oss << args->toString();
    oss << ", body=";
    if (body) oss << body->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Lambda::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Lambda(\n";
    oss << getIndent(indent + 1) << "args=";
    if (args) {
        oss << "\n" << args->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=";
    if (body) {
        oss << "\n" << body->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Dict::toString() const {
    std::ostringstream oss;
    oss << "Dict(keys=[";
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) oss << ", ";
        if (keys[i]) oss << keys[i]->toString(); else oss << "None";
    }
    oss << "], values=[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        if (values[i]) oss << values[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Dict::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Dict(\n";
    oss << getIndent(indent + 1) << "keys=[\n";
    for (size_t i = 0; i < keys.size(); ++i) {
        oss << getIndent(indent + 2);
        if (keys[i]) {
            oss << keys[i]->toString();
        } else {
            oss << "None";
        }
        if (i < keys.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "values=[\n";
    for (size_t i = 0; i < values.size(); ++i) {
        oss << getIndent(indent + 2);
        if (values[i]) {
            oss << values[i]->toString();
        } else {
            oss << "None";
        }
        if (i < values.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Set::toString() const {
    std::ostringstream oss;
    oss << "Set(elts=[";
    for (size_t i = 0; i < elts.size(); ++i) {
        if (i > 0) oss << ", ";
        if (elts[i]) oss << elts[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Set::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Set(\n";
    oss << getIndent(indent + 1) << "elts=[\n";
    for (size_t i = 0; i < elts.size(); ++i) {
        oss << getIndent(indent + 2);
        if (elts[i]) {
            oss << elts[i]->toString();
        } else {
            oss << "None";
        }
        if (i < elts.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string List::toString() const {
    std::ostringstream oss;
    oss << "List(elts=[";
    for (size_t i = 0; i < elts.size(); ++i) {
        if (i > 0) oss << ", ";
        if (elts[i]) oss << elts[i]->toString();
    }
    oss << "], ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string List::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "List(\n";
    oss << getIndent(indent + 1) << "elts=[\n";
    for (size_t i = 0; i < elts.size(); ++i) {
        oss << getIndent(indent + 2);
        if (elts[i]) {
            oss << elts[i]->toString();
        } else {
            oss << "None";
        }
        if (i < elts.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ",\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Tuple::toString() const {
    std::ostringstream oss;
    oss << "Tuple(elts=[";
    for (size_t i = 0; i < elts.size(); ++i) {
        if (i > 0) oss << ", ";
        if (elts[i]) oss << elts[i]->toString();
    }
    oss << "], ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Tuple::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Tuple(\n";
    oss << getIndent(indent + 1) << "elts=[\n";
    for (size_t i = 0; i < elts.size(); ++i) {
        oss << getIndent(indent + 2);
        if (elts[i]) {
            oss << elts[i]->toString();
        } else {
            oss << "None";
        }
        if (i < elts.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ",\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Subscript::toString() const {
    std::ostringstream oss;
    oss << "Subscript(value=";
    if (value) oss << value->toString();
    oss << ", slice=";
    if (slice) oss << slice->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Subscript::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Subscript(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "slice=";
    if (slice) {
        oss << "\n" << slice->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Attribute::toString() const {
    std::ostringstream oss;
    oss << "Attribute(value=";
    if (value) oss << value->toString();
    oss << ", attr='" << attr << "', line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Attribute::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Attribute(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "attr='" << attr << "',\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Starred::toString() const {
    std::ostringstream oss;
    oss << "Starred(value=";
    if (value) oss << value->toString();
    oss << ", ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Starred::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Starred(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ",\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Slice::toString() const {
    std::ostringstream oss;
    oss << "Slice(lower=";
    if (lower) oss << lower->toString(); else oss << "None";
    oss << ", upper=";
    if (upper) oss << upper->toString(); else oss << "None";
    oss << ", step=";
    if (step) oss << step->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Slice::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Slice(\n";
    oss << getIndent(indent + 1) << "lower=";
    if (lower) {
        oss << "\n" << lower->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "upper=";
    if (upper) {
        oss << "\n" << upper->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "step=";
    if (step) {
        oss << "\n" << step->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string ListComp::toString() const {
    std::ostringstream oss;
    oss << "ListComp(elt=";
    if (elt) oss << elt->toString();
    oss << ", generators=[";
    for (size_t i = 0; i < generators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (generators[i]) oss << generators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string ListComp::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "ListComp(\n";
    oss << getIndent(indent + 1) << "elt=";
    if (elt) {
        oss << "\n" << elt->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "generators=[\n";
    for (size_t i = 0; i < generators.size(); ++i) {
        oss << getIndent(indent + 2);
        if (generators[i]) {
            oss << generators[i]->toString();
        } else {
            oss << "None";
        }
        if (i < generators.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string SetComp::toString() const {
    std::ostringstream oss;
    oss << "SetComp(elt=";
    if (elt) oss << elt->toString();
    oss << ", generators=[";
    for (size_t i = 0; i < generators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (generators[i]) oss << generators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string SetComp::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "SetComp(\n";
    oss << getIndent(indent + 1) << "elt=";
    if (elt) {
        oss << "\n" << elt->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "generators=[\n";
    for (size_t i = 0; i < generators.size(); ++i) {
        oss << getIndent(indent + 2);
        if (generators[i]) {
            oss << generators[i]->toString();
        } else {
            oss << "None";
        }
        if (i < generators.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string DictComp::toString() const {
    std::ostringstream oss;
    oss << "DictComp(key=";
    if (key) oss << key->toString();
    oss << ", value=";
    if (value) oss << value->toString();
    oss << ", generators=[";
    for (size_t i = 0; i < generators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (generators[i]) oss << generators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string DictComp::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "DictComp(\n";
    oss << getIndent(indent + 1) << "key=";
    if (key) {
        oss << "\n" << key->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "generators=[\n";
    for (size_t i = 0; i < generators.size(); ++i) {
        oss << getIndent(indent + 2);
        if (generators[i]) {
            oss << generators[i]->toString();
        } else {
            oss << "None";
        }
        if (i < generators.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string GeneratorExp::toString() const {
    std::ostringstream oss;
    oss << "GeneratorExp(elt=";
    if (elt) oss << elt->toString();
    oss << ", generators=[";
    for (size_t i = 0; i < generators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (generators[i]) oss << generators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string GeneratorExp::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "GeneratorExp(\n";
    oss << getIndent(indent + 1) << "elt=";
    if (elt) {
        oss << "\n" << elt->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "generators=[\n";
    for (size_t i = 0; i < generators.size(); ++i) {
        oss << getIndent(indent + 2);
        if (generators[i]) {
            oss << generators[i]->toString();
        } else {
            oss << "None";
        }
        if (i < generators.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Yield::toString() const {
    std::ostringstream oss;
    oss << "Yield(value=";
    if (value) oss << value->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Yield::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Yield(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string YieldFrom::toString() const {
    std::ostringstream oss;
    oss << "YieldFrom(value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string YieldFrom::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "YieldFrom(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Await::toString() const {
    std::ostringstream oss;
    oss << "Await(value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Await::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Await(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string FormattedValue::toString() const {
    std::ostringstream oss;
    oss << "FormattedValue(value=";
    if (value) oss << value->toString();
    oss << ", conversion=" << conversion;
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string FormattedValue::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "FormattedValue(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "conversion=" << conversion << ",\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string JoinedStr::toString() const {
    std::ostringstream oss;
    oss << "JoinedStr(values=[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        if (values[i]) oss << values[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string JoinedStr::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "JoinedStr(\n";
    oss << getIndent(indent + 1) << "values=[\n";
    for (size_t i = 0; i < values.size(); ++i) {
        oss << getIndent(indent + 2);
        if (values[i]) {
            oss << values[i]->toString();
        } else {
            oss << "None";
        }
        if (i < values.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string FunctionDef::toString() const {
    std::ostringstream oss;
    oss << "FunctionDef(name='" << name << "', args=";
    if (args) oss << args->toString(); else oss << "None";
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], decorators=[";
    for (size_t i = 0; i < decorator_list.size(); ++i) {
        if (i > 0) oss << ", ";
        if (decorator_list[i]) oss << decorator_list[i]->toString();
    }
    oss << "], returns='" << returns << "', line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string FunctionDef::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "FunctionDef(\n";
    oss << getIndent(indent + 1) << "name='" << name << "',\n";
    oss << getIndent(indent + 1) << "args=";
    if (args) {
        oss << "\n" << args->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "decorators=[\n";
    for (size_t i = 0; i < decorator_list.size(); ++i) {
        oss << getIndent(indent + 2);
        if (decorator_list[i]) {
            oss << decorator_list[i]->toString();
        } else {
            oss << "None";
        }
        if (i < decorator_list.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "returns='" << returns << "',\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string AsyncFunctionDef::toString() const {
    std::ostringstream oss;
    oss << "AsyncFunctionDef(";
    oss << "name='" << name << "', args=";
    if (args) oss << args->toString(); else oss << "None";
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AsyncFunctionDef::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "AsyncFunctionDef(\n";
    oss << getIndent(indent + 1) << "name='" << name << "',\n";
    oss << getIndent(indent + 1) << "args=";
    if (args) {
        oss << "\n" << args->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string ClassDef::toString() const {
    std::ostringstream oss;
    oss << "ClassDef(name='" << name << "', bases=";
    if (bases) oss << bases->toString(); else oss << "None";
    oss << ", keywords=";
    if (keywords) oss << keywords->toString(); else oss << "None";
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], decorators=[";
    for (size_t i = 0; i < decorator_list.size(); ++i) {
        if (i > 0) oss << ", ";
        if (decorator_list[i]) oss << decorator_list[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string ClassDef::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "ClassDef(\n";
    oss << getIndent(indent + 1) << "name='" << name << "',\n";
    oss << getIndent(indent + 1) << "bases=";
    if (bases) {
        oss << "\n" << bases->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "keywords=";
    if (keywords) {
        oss << "\n" << keywords->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "decorators=[\n";
    for (size_t i = 0; i < decorator_list.size(); ++i) {
        oss << getIndent(indent + 2);
        if (decorator_list[i]) {
            oss << decorator_list[i]->toString();
        } else {
            oss << "None";
        }
        if (i < decorator_list.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Return::toString() const {
    std::ostringstream oss;
    oss << "Return(value=";
    if (value) oss << value->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Return::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Return(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Delete::toString() const {
    std::ostringstream oss;
    oss << "Delete(targets=[";
    for (size_t i = 0; i < targets.size(); ++i) {
        if (i > 0) oss << ", ";
        if (targets[i]) oss << targets[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Delete::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Delete(\n";
    oss << getIndent(indent + 1) << "targets=[\n";
    for (size_t i = 0; i < targets.size(); ++i) {
        oss << getIndent(indent + 2);
        if (targets[i]) {
            oss << targets[i]->toString();
        } else {
            oss << "None";
        }
        if (i < targets.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Assign::toString() const {
    std::ostringstream oss;
    oss << "Assign(targets=[";
    for (size_t i = 0; i < targets.size(); ++i) {
        if (i > 0) oss << ", ";
        if (targets[i]) oss << targets[i]->toString();
    }
    oss << "], value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Assign::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Assign(\n";
    oss << getIndent(indent + 1) << "targets=[\n";
    for (size_t i = 0; i < targets.size(); ++i) {
        oss << getIndent(indent + 2);
        if (targets[i]) {
            oss << targets[i]->toString();
        } else {
            oss << "None";
        }
        if (i < targets.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string AugAssign::toString() const {
    std::ostringstream oss;
    oss << "AugAssign(target=";
    if (target) oss << target->toString();
    oss << ", op=" << getTypeName(op) << ", value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AugAssign::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "AugAssign(\n";
    oss << getIndent(indent + 1) << "target=";
    if (target) {
        oss << "\n" << target->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "op=" << getTypeName(op) << ",\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string AnnAssign::toString() const {
    std::ostringstream oss;
    oss << "AnnAssign(target=";
    if (target) oss << target->toString();
    oss << ", annotation=";
    if (annotation) oss << annotation->toString();
    oss << ", value=";
    if (value) oss << value->toString(); else oss << "None";
    oss << ", simple=" << (simple ? "true" : "false");
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AnnAssign::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "AnnAssign(\n";
    oss << getIndent(indent + 1) << "target=";
    if (target) {
        oss << "\n" << target->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "annotation=";
    if (annotation) {
        oss << "\n" << annotation->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "simple=" << (simple ? "true" : "false") << ",\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string For::toString() const {
    std::ostringstream oss;
    oss << "For(target=";
    if (target) oss << target->toString();
    oss << ", iter=";
    if (iter) oss << iter->toString();
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string For::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "For(\n";
    oss << getIndent(indent + 1) << "target=";
    if (target) {
        oss << "\n" << target->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "iter=";
    if (iter) {
        oss << "\n" << iter->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "orelse=[\n";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (orelse[i]) {
            oss << orelse[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < orelse.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string AsyncFor::toString() const {
    std::ostringstream oss;
    oss << "AsyncFor(";
    oss << "target=";
    if (target) oss << target->toString();
    oss << ", iter=";
    if (iter) oss << iter->toString();
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AsyncFor::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "AsyncFor(\n";
    oss << getIndent(indent + 1) << "target=";
    if (target) {
        oss << "\n" << target->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "iter=";
    if (iter) {
        oss << "\n" << iter->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "orelse=[\n";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (orelse[i]) {
            oss << orelse[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < orelse.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string While::toString() const {
    std::ostringstream oss;
    oss << "While(test=";
    if (test) oss << test->toString();
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string While::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "While(\n";
    oss << getIndent(indent + 1) << "test=";
    if (test) {
        oss << "\n" << test->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "orelse=[\n";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (orelse[i]) {
            oss << orelse[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < orelse.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string If::toString() const {
    std::ostringstream oss;
    oss << "If(test=";
    if (test) oss << test->toString();
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string If::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "If(\n";
    oss << getIndent(indent + 1) << "test=";
    if (test) {
        oss << "\n" << test->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "orelse=[\n";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (orelse[i]) {
            oss << orelse[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < orelse.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string With::toString() const {
    std::ostringstream oss;
    oss << "With(items=[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << ", ";
        if (items[i]) oss << items[i]->toString();
    }
    oss << "], body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string With::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "With(\n";
    oss << getIndent(indent + 1) << "items=[\n";
    for (size_t i = 0; i < items.size(); ++i) {
        oss << getIndent(indent + 2);
        if (items[i]) {
            oss << items[i]->toString();
        } else {
            oss << "None";
        }
        if (i < items.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string AsyncWith::toString() const {
    std::ostringstream oss;
    oss << "AsyncWith(";
    oss << "items=[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << ", ";
        if (items[i]) oss << items[i]->toString();
    }
    oss << "], body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AsyncWith::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "AsyncWith(\n";
    oss << getIndent(indent + 1) << "items=[\n";
    for (size_t i = 0; i < items.size(); ++i) {
        oss << getIndent(indent + 2);
        if (items[i]) {
            oss << items[i]->toString();
        } else {
            oss << "None";
        }
        if (i < items.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Raise::toString() const {
    std::ostringstream oss;
    oss << "Raise(exc=";
    if (exc) oss << exc->toString(); else oss << "None";
    oss << ", cause=";
    if (cause) oss << cause->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Raise::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Raise(\n";
    oss << getIndent(indent + 1) << "exc=";
    if (exc) {
        oss << "\n" << exc->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "cause=";
    if (cause) {
        oss << "\n" << cause->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Try::toString() const {
    std::ostringstream oss;
    oss << "Try(body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], handlers=[";
    for (size_t i = 0; i < handlers.size(); ++i) {
        if (i > 0) oss << ", ";
        if (handlers[i]) oss << handlers[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], finalbody=[";
    for (size_t i = 0; i < finalbody.size(); ++i) {
        if (i > 0) oss << ", ";
        if (finalbody[i]) oss << finalbody[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Try::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Try(\n";
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "handlers=[\n";
    for (size_t i = 0; i < handlers.size(); ++i) {
        oss << getIndent(indent + 2);
        if (handlers[i]) {
            oss << handlers[i]->toString();
        } else {
            oss << "None";
        }
        if (i < handlers.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "orelse=[\n";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (orelse[i]) {
            oss << orelse[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < orelse.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "finalbody=[\n";
    for (size_t i = 0; i < finalbody.size(); ++i) {
        if (finalbody[i]) {
            oss << finalbody[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < finalbody.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Assert::toString() const {
    std::ostringstream oss;
    oss << "Assert(test=";
    if (test) oss << test->toString();
    oss << ", msg=";
    if (msg) oss << msg->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Assert::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Assert(\n";
    oss << getIndent(indent + 1) << "test=";
    if (test) {
        oss << "\n" << test->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "msg=";
    if (msg) {
        oss << "\n" << msg->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Global::toString() const {
    std::ostringstream oss;
    oss << "Global(names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "'" << names[i] << "'";
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Global::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Global(\n";
    oss << getIndent(indent + 1) << "names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "'" << names[i] << "'";
    }
    oss << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Nonlocal::toString() const {
    std::ostringstream oss;
    oss << "Nonlocal(names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "'" << names[i] << "'";
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Nonlocal::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Nonlocal(\n";
    oss << getIndent(indent + 1) << "names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "'" << names[i] << "'";
    }
    oss << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Import::toString() const {
    std::ostringstream oss;
    oss << "Import(names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        if (names[i]) oss << names[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Import::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Import(\n";
    oss << getIndent(indent + 1) << "names=[\n";
    for (size_t i = 0; i < names.size(); ++i) {
        oss << getIndent(indent + 2);
        if (names[i]) {
            oss << names[i]->toString();
        } else {
            oss << "None";
        }
        if (i < names.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string ImportFrom::toString() const {
    std::ostringstream oss;
    oss << "ImportFrom(module=";
    if (module) oss << module->toString(); else oss << "None";
    oss << ", names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        if (names[i]) oss << names[i]->toString();
    }
    oss << "], level=" << level;
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string ImportFrom::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "ImportFrom(\n";
    oss << getIndent(indent + 1) << "module=";
    if (module) {
        oss << "\n" << module->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "names=[\n";
    for (size_t i = 0; i < names.size(); ++i) {
        oss << getIndent(indent + 2);
        if (names[i]) {
            oss << names[i]->toString();
        } else {
            oss << "None";
        }
        if (i < names.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "level=" << level << ",\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Alias::toString() const {
    std::ostringstream oss;
    oss << "Alias(name=";
    if (name) oss << name->toString(); else oss << "None";
    oss << ", asname='" << asname << "'";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Alias::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Alias(\n";
    oss << getIndent(indent + 1) << "name=";
    if (name) {
        oss << "\n" << name->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "asname='" << asname << "',\n";
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string ExprStmt::toString() const {
    std::ostringstream oss;
    oss << "ExprStmt(value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string ExprStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "ExprStmt(\n";
    oss << getIndent(indent + 1) << "value=";
    if (value) {
        oss << "\n" << value->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "line=" << position.line << ", col=" << position.column << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Pass::toString() const {
    std::ostringstream oss;
    oss << "Pass(line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Pass::toString(int indent) const {
    return getIndent(indent) + toString();
}

std::string Break::toString() const {
    std::ostringstream oss;
    oss << "Break(line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Break::toString(int indent) const {
    return getIndent(indent) + toString();
}

std::string Continue::toString() const {
    std::ostringstream oss;
    oss << "Continue(line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Continue::toString(int indent) const {
    return getIndent(indent) + toString();
}

std::string arguments::toString() const {
    std::ostringstream oss;
    oss << "arguments(args=[";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) oss << ", ";
        if (args[i]) oss << args[i]->toString();
    }
    oss << "], vararg=";
    if (vararg) oss << vararg->toString(); else oss << "None";
    oss << ", kwonlyargs=[";
    for (size_t i = 0; i < kwonlyargs.size(); ++i) {
        if (i > 0) oss << ", ";
        if (kwonlyargs[i]) oss << kwonlyargs[i]->toString();
    }
    oss << "], kw_defaults=[";
    for (size_t i = 0; i < kw_defaults.size(); ++i) {
        if (i > 0) oss << ", ";
        if (kw_defaults[i]) oss << kw_defaults[i]->toString(); else oss << "None";
    }
    oss << "], kwarg=";
    if (kwarg) oss << kwarg->toString(); else oss << "None";
    oss << ", defaults=[";
    for (size_t i = 0; i < defaults.size(); ++i) {
        if (i > 0) oss << ", ";
        if (defaults[i]) oss << defaults[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string arguments::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "arguments(\n";
    oss << getIndent(indent + 1) << "args=[\n";
    for (size_t i = 0; i < args.size(); ++i) {
        oss << getIndent(indent + 2);
        if (args[i]) {
            oss << args[i]->toString();
        } else {
            oss << "None";
        }
        if (i < args.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "vararg=";
    if (vararg) {
        oss << "\n" << vararg->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "kwonlyargs=[\n";
    for (size_t i = 0; i < kwonlyargs.size(); ++i) {
        oss << getIndent(indent + 2);
        if (kwonlyargs[i]) {
            oss << kwonlyargs[i]->toString();
        } else {
            oss << "None";
        }
        if (i < kwonlyargs.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "kw_defaults=[\n";
    for (size_t i = 0; i < kw_defaults.size(); ++i) {
        oss << getIndent(indent + 2);
        if (kw_defaults[i]) {
            oss << kw_defaults[i]->toString();
        } else {
            oss << "None";
        }
        if (i < kw_defaults.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "kwarg=";
    if (kwarg) {
        oss << "\n" << kwarg->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "defaults=[\n";
    for (size_t i = 0; i < defaults.size(); ++i) {
        oss << getIndent(indent + 2);
        if (defaults[i]) {
            oss << defaults[i]->toString();
        } else {
            oss << "None";
        }
        if (i < defaults.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "]\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string arg::toString() const {
    std::ostringstream oss;
    oss << "arg(arg='" << name << "', annotation=";
    if (annotation) oss << annotation->toString(); else oss << "None";
    oss << ")";
    return oss.str();
}

std::string arg::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "arg(\n";
    oss << getIndent(indent + 1) << "arg='" << name << "',\n";
    oss << getIndent(indent + 1) << "annotation=";
    if (annotation) {
        oss << "\n" << annotation->toString(indent + 2) << "\n";
    } else {
        oss << "None\n";
    }
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string except_handler::toString() const {
    std::ostringstream oss;
    oss << "except_handler(type=";
    if (type) oss << type->toString(); else oss << "None";
    oss << ", name='" << name << "', body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string except_handler::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "except_handler(\n";
    oss << getIndent(indent + 1) << "type=";
    if (type) {
        oss << "\n" << type->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "name='" << name << "',\n";
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "]\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string withitem::toString() const {
    std::ostringstream oss;
    oss << "withitem(context_expr=";
    if (context_expr) oss << context_expr->toString(); else oss << "None";
    oss << ", optional_vars=";
    if (optional_vars) oss << optional_vars->toString(); else oss << "None";
    oss << ")";
    return oss.str();
}

std::string withitem::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "withitem(\n";
    oss << getIndent(indent + 1) << "context_expr=";
    if (context_expr) {
        oss << "\n" << context_expr->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "optional_vars=";
    if (optional_vars) {
        oss << "\n" << optional_vars->toString(indent + 2) << "\n";
    } else {
        oss << "None\n";
    }
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string comprehension::toString() const {
    std::ostringstream oss;
    oss << "comprehension(target=";
    if (target) oss << target->toString(); else oss << "None";
    oss << ", iter=";
    if (iter) oss << iter->toString(); else oss << "None";
    oss << ", ifs=[";
    for (size_t i = 0; i < ifs.size(); ++i) {
        if (i > 0) oss << ", ";
        if (ifs[i]) oss << ifs[i]->toString();
    }
    oss << "], is_async=" << (is_async ? "true" : "false");
    oss << ")";
    return oss.str();
}

std::string comprehension::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "comprehension(\n";
    oss << getIndent(indent + 1) << "target=";
    if (target) {
        oss << "\n" << target->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "iter=";
    if (iter) {
        oss << "\n" << iter->toString(indent + 2) << ",\n";
    } else {
        oss << "None,\n";
    }
    oss << getIndent(indent + 1) << "ifs=[\n";
    for (size_t i = 0; i < ifs.size(); ++i) {
        oss << getIndent(indent + 2);
        if (ifs[i]) {
            oss << ifs[i]->toString();
        } else {
            oss << "None";
        }
        if (i < ifs.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "is_async=" << (is_async ? "true" : "false") << "\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

std::string Module::toString() const {
    std::ostringstream oss;
    oss << "Module(body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], type_ignores=[";
    for (size_t i = 0; i < type_ignores.size(); ++i) {
        if (i > 0) oss << ", ";
        if (type_ignores[i]) oss << type_ignores[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string Module::toString(int indent) const {
    std::ostringstream oss;
    oss << getIndent(indent) << "Module(\n";
    oss << getIndent(indent + 1) << "body=[\n";
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]) {
            oss << body[i]->toString(indent + 2);
        } else {
            oss << getIndent(indent + 2) << "None";
        }
        if (i < body.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "],\n";
    oss << getIndent(indent + 1) << "type_ignores=[\n";
    for (size_t i = 0; i < type_ignores.size(); ++i) {
        oss << getIndent(indent + 2);
        if (type_ignores[i]) {
            oss << type_ignores[i]->toString();
        } else {
            oss << "None";
        }
        if (i < type_ignores.size() - 1) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << getIndent(indent + 1) << "]\n";
    oss << getIndent(indent) << ")";
    return oss.str();
}

} // namespace csgo
