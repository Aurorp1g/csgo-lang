/**
 * @file type_checker.h
 * @brief CSGO 编程语言类型检查器头文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件定义了 CSGO 语言的类型检查器（Type Checker）。
 * 基于 CPython 的类型注解和类型提示设计，
 * 实现了静态类型推断和类型检查功能。
 *
 * @section design 设计原则
 * - 支持 Python 风格的类型注解
 * - 基本类型：int, float, str, bool, None, bytes
 * - 容器类型：list, tuple, dict, set
 * - 泛型类型支持
 * - 函数类型支持
 *
 * @section features 功能特性
 * - 基础类型系统
 * - 泛型容器类型（list[T], dict[K,V], tuple[T,...]）
 * - 函数类型注解
 * - 类型推断
 * - 类型兼容性检查
 *
 * @section reference 参考
 * - CPython Python/typing.c: 类型检查实现
 * - PEP 484: Type Hints
 * - PEP 526: Syntax for Variable Annotations
 *
 * @see SymbolTable 符号表
 * @see SSAIR SSA中间表示
 */

#ifndef CSGO_TYPE_CHECKER_H
#define CSGO_TYPE_CHECKER_H

#include "symbol_table.h"
#include "../ast/ast_node.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace csgo {

enum class BasicType {
    Unknown,
    None,
    Bool,
    Int,
    Float,
    Complex,
    String,
    Bytes,
    List,
    Tuple,
    Dict,
    Set,
    Function,
    Module,
    Object,
    Any
};

class Type {
public:
    BasicType kind;
    std::unique_ptr<Type> elementType;
    std::unordered_map<std::string, Type> dictTypes;
    std::vector<Type> tupleTypes;
    std::vector<Type> paramTypes;
    std::unique_ptr<Type> returnType;
    
    Type() : kind(BasicType::Unknown) {}
    explicit Type(BasicType basic) : kind(basic) {}
    
    // 添加拷贝构造函数
    Type(const Type& other) 
        : kind(other.kind), 
          dictTypes(other.dictTypes),
          tupleTypes(other.tupleTypes),
          paramTypes(other.paramTypes) {
        if (other.elementType) {
            elementType = std::make_unique<Type>(*other.elementType);
        }
        if (other.returnType) {
            returnType = std::make_unique<Type>(*other.returnType);
        }
    }

    // 添加赋值运算符
    Type& operator=(const Type& other) {
        if (this != &other) {
            kind = other.kind;
            dictTypes = other.dictTypes;
            tupleTypes = other.tupleTypes;
            paramTypes = other.paramTypes;
            
            if (other.elementType) {
                elementType = std::make_unique<Type>(*other.elementType);
            } else {
                elementType.reset();
            }
            if (other.returnType) {
                returnType = std::make_unique<Type>(*other.returnType);
            } else {
                returnType.reset();
            }
        }
        return *this;
    }
    
    static Type unknown() { return Type(BasicType::Unknown); }
    static Type none() { return Type(BasicType::None); }
    static Type boolean() { return Type(BasicType::Bool); }
    static Type integer() { return Type(BasicType::Int); }
    static Type floating() { return Type(BasicType::Float); }
    static Type complex() { return Type(BasicType::Complex); }
    static Type string() { return Type(BasicType::String); }
    static Type bytes() { return Type(BasicType::Bytes); }
    static Type list() { return Type(BasicType::List); }
    static Type listOf(const Type& elem) { 
        Type t(BasicType::List);
        t.elementType = std::make_unique<Type>(elem);
        return t;
    }
    static Type tuple() { return Type(BasicType::Tuple); }
    static Type tupleOf(const std::vector<Type>& types) {
        Type t(BasicType::Tuple);
        t.tupleTypes = types;
        return t;
    }
    static Type dict() { return Type(BasicType::Dict); }
    static Type dictOf(const Type& key, const Type& value) {
        Type t(BasicType::Dict);
        t.dictTypes["__key__"] = key;
        t.dictTypes["__value__"] = value;
        return t;
    }
    static Type set() { return Type(BasicType::Set); }
    static Type setOf(const Type& elem) {
        Type t(BasicType::Set);
        t.elementType = std::make_unique<Type>(elem);
        return t;
    }
    static Type function() { return Type(BasicType::Function); }
    static Type functionWith(const std::vector<Type>& params, const Type& ret) {
        Type t(BasicType::Function);
        t.paramTypes = params;
        t.returnType = std::make_unique<Type>(ret);
        return t;
    }
    static Type module() { return Type(BasicType::Module); }
    static Type object() { return Type(BasicType::Object); }
    static Type any() { return Type(BasicType::Any); }
    
    bool isUnknown() const { return kind == BasicType::Unknown; }
    bool isNumeric() const {
        // Bool 是 Int 的子类型（在 Python 中 True == 1, False == 0）
        return kind == BasicType::Bool || kind == BasicType::Int || 
               kind == BasicType::Float || kind == BasicType::Complex;
    }
    bool isIterable() const {
        return kind == BasicType::List || kind == BasicType::Tuple ||
               kind == BasicType::Dict || kind == BasicType::Set ||
               kind == BasicType::String || kind == BasicType::Bytes;
    }
    
    std::string toString() const;
    bool isCompatibleWith(const Type& other) const;
    bool supportsBinOp(ASTNodeType op, const Type& right) const;
    bool supportsUnaryOp(ASTNodeType op) const;
    bool supportsCompare(ASTNodeType op, const Type& right) const;
};

class TypeError {
public:
    std::string message;
    size_t line;
    size_t column;
    std::string hint;
    
    TypeError(const std::string& msg, size_t l = 0, size_t c = 0)
        : message(msg), line(l), column(c) {}
    
    TypeError& withHint(const std::string& h) {
        hint = h;
        return *this;
    }
    
    std::string toString() const {
        std::string result = "TypeError: " + message;
        if (line > 0) {
            result += " at line " + std::to_string(line);
            if (column > 0) {
                result += ", column " + std::to_string(column);
            }
        }
        if (!hint.empty()) {
            result += "\nHint: " + hint;
        }
        return result;
    }
};

class SymbolInfo {
public:
    Type type;
    bool isDefined;
    bool isUsed;
    size_t definedLine;
    size_t definedColumn;
    std::string description;
    
    SymbolInfo() 
        : type(Type::unknown()), isDefined(false), isUsed(false),
          definedLine(0), definedColumn(0) {}
    
    SymbolInfo(const Type& t, size_t line = 0, size_t col = 0)
        : type(t), isDefined(true), isUsed(false),
          definedLine(line), definedColumn(col) {}
    
    void markAsUsed() { isUsed = true; }
};

class TypeChecker {
public:
    std::shared_ptr<SymbolTable> symtable;
    std::unordered_map<std::string, SymbolInfo> symbolTypes;
    std::vector<TypeError> errors;
    std::vector<TypeError> warnings;
    
    explicit TypeChecker(std::shared_ptr<SymbolTable> st);
    
    bool checkModule(const Module& module);
    
    void defineSymbol(const std::string& name, const Type& type, 
                      size_t line = 0, size_t col = 0);
    std::optional<Type> getSymbolType(const std::string& name);
    void useSymbol(const std::string& name);
    bool isSymbolDefined(const std::string& name);
    void reportUndefinedSymbol(const std::string& name, size_t line, size_t col);
    
    void reportError(const TypeError& error);
    void reportTypeMismatch(const Type& expected, const Type& actual, 
                            size_t line, size_t col);
    void reportInvalidOperation(ASTNodeType op, const Type& left, 
                               const Type& right, size_t line, size_t col);
    
    const std::vector<TypeError>& getErrors() const { return errors; }
    const std::vector<TypeError>& getWarnings() const { return warnings; }
    bool hasErrors() const { return !errors.empty(); }
    void clearErrors() { errors.clear(); warnings.clear(); }
    
    Type inferExpressionType(const Expr& expr);
    Type inferConstantType(const Constant& constant);
    Type inferNameType(const Name& name);
    Type inferBinaryOpType(const BinOp& binop);
    Type inferUnaryOpType(const UnaryOp& unop);
    Type inferCompareType(const Compare& compare);
    Type inferCallType(const Call& call);
    Type inferAttributeType(const Attribute& attr);
    Type inferSubscriptType(const Subscript& subscript);
    Type inferListCompType(const ListComp& listComp);
    Type inferDictCompType(const DictComp& dictComp);
    Type inferSetCompType(const SetComp& setComp);
    Type inferGeneratorExpType(const GeneratorExp& genExp);
    Type inferContainerType(const std::vector<std::unique_ptr<Expr>>& elements);
    Type inferDictType(const std::vector<std::unique_ptr<Expr>>& keys,
                      const std::vector<std::unique_ptr<Expr>>& values);
    
private:
    bool checkStatement(const Stmt& stmt);
    bool checkExpressionStatement(const ExprStmt& exprStmt);
    bool checkAssignment(const Assign& assign);
    bool checkAnnotatedAssignment(const AnnAssign& assign);
    bool checkFunctionDefinition(const FunctionDef& func);
    bool checkClassDefinition(const ClassDef& cls);
    bool checkTryStatement(const Try& tryStmt);
    bool checkReturn(const Return& ret);
    bool checkIfStatement(const If& ifStmt);
    bool checkForLoop(const For& forStmt);
    bool checkWhileLoop(const While& whileStmt);
    
    bool checkArithmeticTypes(const Type& left, const Type& right, 
                             ASTNodeType op, size_t line, size_t col);
    bool checkComparisonTypes(const Type& left, const Type& right,
                             ASTNodeType op, size_t line, size_t col);
    bool checkBooleanTypes(const Type& left, const Type& right,
                           size_t line, size_t col);
    Type getCommonType(const Type& a, const Type& b);
    bool isIndexable(const Type& t);
    Type getIndexType(const Type& t);
};

class SemanticAnalyzer {
public:
    std::shared_ptr<SymbolTable> symtable;
    std::unique_ptr<TypeChecker> typeChecker;
    std::vector<std::string> errors;
    
    explicit SemanticAnalyzer(const std::string& filename);
    bool analyze(std::unique_ptr<Module> module);
    std::shared_ptr<SymbolTable> getSymbolTable() const { return symtable; }
    bool hasErrors() const { 
        return !errors.empty() || 
               (typeChecker && typeChecker->hasErrors());
    }
    std::vector<std::string> getErrorMessages() const;
};

} // namespace csgo

#endif // CSGO_TYPE_CHECKER_H