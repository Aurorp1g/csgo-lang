/**
 * @file symbol_table.h
 * @brief CSGO 编程语言符号表头文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件定义了 CSGO 语言的符号表（Symbol Table）实现。
 * 基于 CPython 的符号表设计，实现了完整的作用域管理功能。
 *
 * @section design 设计原则
 * - 支持嵌套作用域（模块、函数、类）
 * - 符号定义标志位管理
 * - 自由变量和闭包支持
 * - 名称解析优先级：局部 > 自由 > 全局 > 内置
 *
 * @section features 功能特性
 * - 作用域链管理
 * - 符号定义标志位（DEF_LOCAL, DEF_GLOBAL, DEF_PARAM 等）
 * - 符号查找和绑定
 * - 自由变量追踪
 * - 嵌套作用域支持
 *
 * @section reference 参考
 * - CPython Python/symtable.c: 符号表实现
 * - CPython Include/symtable.h: 符号表头文件
 *
 * @see TypeChecker 类型检查器
 * @see Parser 语法分析器
 */

#ifndef CSGO_SYMBOL_TABLE_H
#define CSGO_SYMBOL_TABLE_H

#include "../ast/ast_node.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <memory>
#include <functional>

namespace csgo {

// ============================================================================
// 块类型枚举（基于 CPython _Py_block_ty）
// ============================================================================

/**
 * @enum BlockType
 * @brief 代码块类型枚举
 *
 * 对应 CPython 的 _Py_block_ty 枚举类型。
 * 用于区分模块级、函数级和类级别的代码块。
 */
enum class BlockType {
    ModuleBlock,   ///< 模块级块
    FunctionBlock, ///< 函数级块
    ClassBlock     ///< 类级块
};

// ============================================================================
// 符号定义标志（基于 CPython 的 DEF_* 宏）
// ============================================================================

/**
 * @enum SymbolFlags
 * @brief 符号定义标志枚举
 *
 * 对应 CPython 的符号标志位定义，用于标记符号的定义方式和使用情况。
 */
enum class SymbolFlags : int {
    DEF_GLOBAL        = 1 << 0,  ///< global 语句声明
    DEF_LOCAL         = 1 << 1,  ///< 代码块中的赋值
    DEF_PARAM         = 1 << 2,  ///< 形式参数
    DEF_NONLOCAL      = 1 << 3,  ///< nonlocal 语句声明
    USE               = 1 << 4,  ///< 名称被使用
    DEF_FREE          = 1 << 5,  ///< 被使用但未在嵌套块中定义
    DEF_FREE_CLASS    = 1 << 6,  ///< 来自类作用域的自由变量
    DEF_IMPORT        = 1 << 7,  ///< 通过 import 赋值
    DEF_ANNOT         = 1 << 8,  ///< 此名称被注解
    DEF_COMP_ITER     = 1 << 9   ///< 推导式迭代变量
};

/**
 * @brief 符号定义标志的位运算组合
 */
inline SymbolFlags operator|(SymbolFlags a, SymbolFlags b) {
    return static_cast<SymbolFlags>(static_cast<int>(a) | static_cast<int>(b));
}

inline SymbolFlags& operator|=(SymbolFlags& a, SymbolFlags b) {
    a = a | b;
    return a;
}

inline SymbolFlags operator&(SymbolFlags a, SymbolFlags b) {
    return static_cast<SymbolFlags>(static_cast<int>(a) & static_cast<int>(b));
}

// ============================================================================
// 作用域类型枚举（基于 CPython 的 SCOPE_MASK）
// ============================================================================

/**
 * @enum ScopeType
 * @brief 作用域类型枚举
 *
 * 对应 CPython 的作用域类型，用于确定符号的绑定方式。
 */
enum class ScopeType {
    LOCAL,          ///< 局部变量
    GLOBAL_EXPLICIT, ///< 显式 global 声明
    GLOBAL_IMPLICIT, ///< 隐式全局变量
    FREE,           ///< 自由变量
    CELL            ///< 单元变量（用于闭包）
};

// ============================================================================
// 符号表条目类（基于 CPython PySTEntryObject）
// ============================================================================

/**
 * @class SymbolTableEntry
 * @brief 符号表条目类
 *
 * 对应 CPython 的 PySTEntryObject 结构体。
 * 表示一个作用域块（如模块、函数、类）的符号表条目。
 *
 * 包含：
 * - 符号名称到标志的映射
 * - 变量名列表（参数）
 * - 子块列表
 * - 块类型和嵌套信息
 */
class SymbolTableEntry {
public:
    std::string name;                                     ///< 块名称
    BlockType type;                                       ///< 块类型
    std::unordered_map<std::string, int> symbols;        ///< 符号名称 -> 标志位
    std::vector<std::string> varnames;                  ///< 变量名列表（参数）
    std::vector<std::shared_ptr<SymbolTableEntry>> children; ///< 子块列表
    
    // 指令位置（global/nonlocal 语句位置）
    std::vector<std::tuple<std::string, size_t, size_t>> directives;
    
    // 块属性标志
    bool isNested;              ///< 是否嵌套
    bool hasFreeVariables;     ///< 是否有自由变量
    bool hasChildFree;          ///< 子块是否有自由变量
    bool isGenerator;           ///< 是否是生成器
    bool isCoroutine;           ///< 是否是协程
    bool isComprehension;       ///< 是否是推导式
    bool returnsValue;          ///< 是否使用带参数的 return
    bool needsClassClosure;     ///< 是否需要类闭包
    bool isCompIterTarget;      ///< 是否正在访问推导式目标
    int compIterExpr;           ///< 推导式迭代表达式计数
    bool isVarargs;             ///< 是否有 *args 参数
    bool isVarkeywords;          ///< 是否有 **kwargs 参数
    
    // 位置信息
    size_t lineno;              ///< 起始行号
    size_t colOffset;           ///< 起始列偏移
    
    // 构造函数
    SymbolTableEntry(const std::string& entryName, BlockType blockType, 
                     size_t line = 0, size_t col = 0)
        : name(entryName), type(blockType),
          isNested(false), hasFreeVariables(false), hasChildFree(false),
          isGenerator(false), isCoroutine(false), isComprehension(false),
          returnsValue(false), needsClassClosure(false),
          isCompIterTarget(false), compIterExpr(0),
          isVarargs(false), isVarkeywords(false),
          lineno(line), colOffset(col) {}
    
    /**
     * @brief 获取符号的作用域类型
     * @param symbolName 符号名称
     * @return 作用域类型
     */
    std::optional<ScopeType> getScope(const std::string& symbolName) const;
    
    /**
     * @brief 获取符号的标志值
     * @param symbolName 符号名称
     * @return 标志值，如果符号不存在返回 0
     */
    int getFlags(const std::string& symbolName) const;
    
    /**
     * @brief 添加符号
     * @param symbolName 符号名称
     * @param flags 标志值
     */
    void addSymbol(const std::string& symbolName, int flags);
    
    /**
     * @brief 添加变量名
     * @param varName 变量名
     */
    void addVarname(const std::string& varName);
    
    /**
     * @brief 添加指令位置
     * @param directiveName 指令名称
     * @param line 行号
     * @param col 列偏移
     */
    void addDirective(const std::string& directiveName, size_t line, size_t col);
    
    /**
     * @brief 获取字符串表示
     * @return 格式化字符串
     */
    std::string toString() const;
};

// ============================================================================
// 符号表主类（基于 CPython struct symtable）
// ============================================================================

/**
 * @class SymbolTable
 * @brief 符号表类
 *
 * 对应 CPython 的 struct symtable。
 * 管理整个编译单元的符号表，包含多个作用域块的符号信息。
 *
 * 主要功能：
 * - 维护当前作用域和作用域栈
 * - 管理符号的收集和分析
 * - 支持嵌套作用域的符号查找
 */
class SymbolTable {
public:
    std::string filename;                                       ///< 文件名
    std::shared_ptr<SymbolTableEntry> currentEntry;           ///< 当前符号表条目
    std::shared_ptr<SymbolTableEntry> topEntry;              ///< 顶层符号表条目（模块）
    
    // 块映射：AST节点地址 -> 符号表条目
    std::unordered_map<const void*, std::shared_ptr<SymbolTableEntry>> blocks;
    
    // 作用域栈
    std::vector<std::shared_ptr<SymbolTableEntry>> scopeStack;
    
    // 全局符号集合
    std::shared_ptr<SymbolTableEntry> globalSymbols;
    
    // 当前私有名称（用于类作用域的名称修饰）
    std::string privateName;
    
    // 递归深度跟踪
    int recursionDepth;
    int recursionLimit;
    
    // 错误信息
    bool hasError;
    std::string errorMessage;
    
    // 构造函数
    explicit SymbolTable(const std::string& fileName);
    
    // =========================================================================
    // 作用域管理
    // =========================================================================
    
    /**
     * @brief 进入新的作用域块
     * @param name 块名称
     * @param type 块类型
     * @param astNode 关联的AST节点指针
     * @param lineno 行号
     * @param colOffset 列偏移
     * @return 是否成功
     */
    bool enterBlock(const std::string& name, BlockType type, 
                   const void* astNode, size_t lineno = 0, size_t colOffset = 0);
    
    /**
     * @brief 退出当前作用域块
     * @return 是否成功
     */
    bool exitBlock();
    
    /**
     * @brief 获取当前作用域
     * @return 当前符号表条目
     */
    std::shared_ptr<SymbolTableEntry> current() const;
    
    /**
     * @brief 获取顶层作用域
     * @return 模块级符号表条目
     */
    std::shared_ptr<SymbolTableEntry> top() const;

    
    /**
     * @brief 添加符号定义
     * @param name 符号名称
     * @param flags 标志值
     */
    void addSymbol(const std::string& name, int flags);
    
    /**
     * @brief 标记符号被使用
     * @param name 符号名称
     */
    void markUsed(const std::string& name);
    
    /**
     * @brief 查找符号
     * @param name 符号名称
     * @return 符号表条目和作用域类型
     */
    std::pair<std::shared_ptr<SymbolTableEntry>, std::optional<ScopeType>> 
    lookup(const std::string& name);
    
    /**
     * @brief 查找符号的作用域
     * @param name 符号名称
     * @return 作用域类型
     */
    std::optional<ScopeType> getScope(const std::string& name);
    
    // =========================================================================
    // 符号分析
    // =========================================================================
    
    /**
     * @brief 分析所有作用域的符号
     * @return 是否成功
     */
    bool analyze();
    
    /**
     * @brief 查找符号表条目
     * @param astNode AST节点指针
     * @return 符号表条目
     */
    std::shared_ptr<SymbolTableEntry> lookupEntry(const void* astNode);
    
    // =========================================================================
    // 错误处理
    // =========================================================================
    
    /**
     * @brief 报告错误
     * @param message 错误消息
     */
    void reportError(const std::string& message);
    
    /**
     * @brief 清除错误状态
     */
    void clearError();
};

// ============================================================================
// 符号表构建器类
// ============================================================================

/**
 * @class SymbolTableBuilder
 * @brief 符号表构建器
 *
 * 基于 CPython 的 PySymtable_Build 函数。
 * 通过遍历 AST 来构建符号表。
 *
 * 使用 Visitor 模式遍历 AST 节点，
 * 收集符号定义和使用信息。
 */
class SymbolTableBuilder {
public:
    std::shared_ptr<SymbolTable> symtable;
    
    // 构造函数
    explicit SymbolTableBuilder(const std::string& filename);
    
    // =========================================================================
    // 主入口
    // =========================================================================
    
    /**
     * @brief 从 AST 模块构建符号表
     * @param module AST模块节点（引用，不获取所有权）
     * @return 符号表指针，失败返回nullptr
     */
    std::shared_ptr<SymbolTable> build(Module& module);
    
    // =========================================================================
    // 模块级遍历
    // =========================================================================
    
    bool visitModule(const std::vector<std::unique_ptr<Stmt>>& body);
    
    // =========================================================================
    // 语句遍历
    // =========================================================================
    
    bool visitStatement(const Stmt& stmt);
    bool visitFunctionDef(const FunctionDef& func);
    bool visitAsyncFunctionDef(const FunctionDef& func);
    bool visitClassDef(const ClassDef& cls);
    bool visitReturn(const Return& ret);
    bool visitAssign(const Assign& assign);
    bool visitAugAssign(const AugAssign& assign);
    bool visitAnnAssign(const AnnAssign& assign);
    bool visitIf(const If& ifStmt);
    bool visitFor(const For& forStmt);
    bool visitAsyncFor(const For& forStmt);
    bool visitWhile(const While& whileStmt);
    bool visitWith(const With& withStmt);
    bool visitAsyncWith(const With& withStmt);
    bool visitTry(const Try& tryStmt);
    bool visitRaise(const Raise& raiseStmt);
    bool visitAssert(const Assert& assertStmt);
    bool visitDelete(const Delete& delStmt);
    bool visitGlobal(const Global& globalStmt);
    bool visitNonlocal(const Nonlocal& nonlocalStmt);
    bool visitImport(const Import& importStmt);
    bool visitImportFrom(const ImportFrom& importStmt);
    bool visitExpressionStmt(const ExprStmt& exprStmt);
    bool visitPass(const Pass& passStmt);
    bool visitBreak(const Break& breakStmt);
    bool visitContinue(const Continue& continueStmt);
    
    // =========================================================================
    // 表达式遍历
    // =========================================================================
    
    bool visitExpression(const Expr& expr);
    bool visitConstant(const Constant& constant);
    bool visitName(const Name& name);
    bool visitBinaryOp(const BinOp& binop);
    bool visitUnaryOp(const UnaryOp& unop);
    bool visitLambda(const Lambda& lambda);
    bool visitIfExp(const IfExp& ifexp);
    bool visitDict(const Dict& dict);
    bool visitSet(const Set& set);
    bool visitListComp(const ListComp& listcomp);
    bool visitSetComp(const SetComp& setcomp);
    bool visitDictComp(const DictComp& dictcomp);
    bool visitGeneratorExp(const GeneratorExp& genexp);
    bool visitYield(const Yield& yield);
    bool visitYieldFrom(const YieldFrom& yieldfrom);
    bool visitAwait(const Await& await);
    bool visitCompare(const Compare& compare);
    bool visitCall(const Call& call);
    bool visitAttribute(const Attribute& attr);
    bool visitSubscript(const Subscript& subscript);
    bool visitStarred(const Starred& starred);
    bool visitTuple(const Tuple& tuple);
    bool visitList(const List& list);
    bool visitBoolOp(const BoolOp& boolop);
    
    // =========================================================================
    // 辅助遍历
    // =========================================================================
    
    bool visitArguments(const arguments& args);
    bool visitComprehension(const comprehension& comp);
    bool visitExceptHandler(const except_handler& handler);
    bool visitWithitem(const withitem& item);
    
private:
    // 内部辅助方法
    void addDef(const std::string& name, int flags);
    void addUse(const std::string& name);
    bool handleComprehension(const Expr& expr, const std::vector<std::unique_ptr<comprehension>>& generators);
    
    // 全局/nonlocal 指令处理
    std::vector<std::tuple<std::string, size_t, size_t>> pendingDirectives;
};

} // namespace csgo

#endif // CSGO_SYMBOL_TABLE_H