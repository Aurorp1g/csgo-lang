/**
 * @file ast_node.h
 * @brief CSGO 编程语言抽象语法树（AST）定义头文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件定义了 CSGO 语言的抽象语法树（AST）节点类型。
 * 基于 CPython 3.8 Python/ast.c 和 Grammar/Grammar 设计，
 * 采用递归下降解析算法，支持完整的 Python 语法结构。
 *
 * @section features 功能特性
 * - 完整的表达式节点类型（常量、名称、二元运算、函数调用等）
 * - 完整的语句节点类型（函数定义、类定义、控制流等）
 * - 支持列表、元组、字典、集合等容器类型
 * - 支持列表推导式、字典推导式、生成器表达式
 * - 支持 f-string 和格式化字符串
 * - 支持协程、async/await 异步编程
 * - 支持 Python 风格缩进和作用域
 *
 * @section usage 使用示例
 * @code
 * // 创建变量引用
 * auto var = std::make_unique<csgo::Name>("x", csgo::Name::Load);
 *
 * // 创建常量
 * auto num = std::make_unique<csgo::Constant>(csgo::Position(1, 1), 42);
 *
 * // 创建二元运算：x + 10
 * auto binop = std::make_unique<csgo::BinOp>(
 *     std::move(var),
 *     csgo::ASTNodeType::Add,
 *     std::make_unique<csgo::Constant>(csgo::Position(1, 5), 10)
 * );
 * @endcode
 *
 * @see Parser 语法分析器
 * @see Tokenizer 词法分析器
 */

#ifndef CSGO_AST_NODE_H
#define CSGO_AST_NODE_H

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <unordered_map>
#include <stack>

namespace csgo {

// ============================================================================
// 前向声明
// ============================================================================

/**
 * @class Expr
 * @brief 表达式节点基类
 *
 * 表达式是计算产生值的语法结构。
 * 所有表达式类型都继承自此类。
 */
class Expr;

/**
 * @class Stmt
 * @brief 语句节点基类
 *
 * 语句是执行某个操作的语法结构，不产生值。
 * 所有语句类型都继承自此类。
 */
class Stmt;

/**
 * @class Module
 * @brief 模块节点类
 *
 * 表示整个模块的 AST 根节点。
 */
class Module;

/**
 * @class ASTNode
 * @brief AST 节点基类
 *
 * 所有 AST 节点的基类，提供通用的位置信息和类型信息。
 * 采用CRTP（Curiously Recurring Template Pattern）模式设计，
 * 支持类型安全的节点访问和遍历。
 *
 * @see Expr 表达式节点基类
 * @see Stmt 语句节点基类
 */
class ASTNode;

/**
 * @class withitem
 * @brief with 语句项节点
 *
 * 对应 CPython 的 ast.withitem 节点。
 * 表示 with 语句中的一个上下文项：expression [as target]
 */
class withitem;

/**
 * @class except_handler
 * @brief except 异常处理器节点
 *
 * 对应 CPython 的 ast.except_handler 节点。
 * 表示 except 子句：except [type] [as name]: body
 */
class except_handler;

/**
 * @class arguments
 * @brief 函数参数列表节点
 *
 * 对应 CPython 的 ast.arguments 节点。
 * 表示函数的完整参数列表，包括位置参数、关键字参数、*args 和 **kwargs。
 */
class arguments;

// ============================================================================
// AST 节点类型枚举
// ============================================================================

/**
 * @enum ASTNodeType
 * @brief AST 节点类型枚举
 *
 * 定义所有可能的 AST 节点类型，对应 CPython 的 AST node_type。
 * 用于区分不同类型的语法结构，支持类型检查和访问者模式。
 */
enum class ASTNodeType {
    // =========================================================================
    // 模块级别节点类型
    // =========================================================================
    Module,            ///< 模块节点，整个程序的根节点

    // =========================================================================
    // 导入语句类型节点（新增）
    // =========================================================================
    Import,            ///< import 导入语句
    ImportFrom,        ///< from...import 导入语句
    Alias,             ///< 导入别名

    // =========================================================================
    // 语句类型节点
    // =========================================================================
    FunctionDef,       ///< 函数定义语句：def func_name(args): ...
    AsyncFunctionDef,  ///< 异步函数定义：async def func_name(args): ...
    ClassDef,          ///< 类定义语句：class ClassName: ...
    Return,            ///< return 返回语句
    Delete,            ///< del 删除语句
    Assign,            ///< 赋值语句：x = value
    AugAssign,         ///< 增强赋值语句：x += 1
    AnnAssign,         ///< 类型注解赋值：x: int = 10
    For,               ///< for 循环语句
    AsyncFor,          ///< 异步 for 循环：async for
    While,             ///< while 循环语句
    If,                ///< if 条件语句
    With,              ///< with 上下文管理语句
    AsyncWith,         ///< 异步 with 语句：async with
    Raise,             ///< raise 异常抛出语句
    Try,               ///< try 异常捕获语句
    Assert,            ///< assert 断言语句
    Global,            ///< global 全局变量声明
    Nonlocal,          ///< nonlocal 非局部变量声明
    ExprStmt,          ///< 表达式语句
    Pass,              ///< pass 空语句
    Break,             ///< break 跳出循环
    Continue,          ///< continue 继续循环
    
    // =========================================================================
    // 表达式类型节点
    // =========================================================================
    BoolOp,            ///< 布尔运算表达式：and, or
    BinOp,             ///< 二元运算表达式：+, -, *, /, //, %, ** 等
    UnaryOp,           ///< 一元运算表达式：-, +, not, ~
    Lambda,            ///< lambda 匿名函数表达式
    IfExp,             ///< 条件表达式（三元运算符）：x if cond else y
    Dict,              ///< 字典字面量：{key: value, ...}
    Set,               ///< 集合字面量：{elem, ...}
    ListComp,          ///< 列表推导式：[x for x in ... if cond]
    SetComp,           ///< 集合推导式：{x for x in ...}
    DictComp,          ///< 字典推导式：{k: v for k, v in ...}
    GeneratorExp,      ///< 生成器表达式：(x for x in ...)
    Yield,             ///< yield 表达式
    YieldFrom,         ///< yield from 表达式
    Await,             ///< await 表达式
    Compare,           ///< 比较表达式：1 < x < 10
    Call,              ///< 函数调用表达式：func(args)
    FormattedValue,    ///< f-string 中的值表达式：{expr}
    JoinedStr,         ///< f-string 字符串连接
    Constant,          ///< 字面量常量值
    List,              ///< 列表字面量：[a, b, c]
    Attribute,         ///< 属性访问表达式：obj.attr
    
    // =========================================================================
    // 切片和下标访问节点类型
    // =========================================================================
    Subscript,         ///< 下标访问表达式：obj[key]
    Slice,             ///< 切片表达式：obj[start:stop:step]
    Starred,           ///< 星号解包表达式：*args
    Tuple,             ///< 元组字面量：(a, b, c)
    
    // =========================================================================
    // 名字和上下文节点类型
    // =========================================================================
    Name,              ///< 名称引用（变量、函数名等）
    Load,              ///< 加载上下文（读取变量）
    Store,             ///< 存储上下文（赋值）
    Del,               ///< 删除上下文（del 语句）
    
    // =========================================================================
    // 辅助类型节点
    // =========================================================================
    arguments,         ///< 函数参数列表
    arg,               ///< 单个函数参数
    except_handler,    ///< except 异常处理器
    withitem,          ///< with 语句项
    comprehension,     ///< 推导式迭代器
    
    // =========================================================================
    // 字面量类型节点（旧式兼容）
    // =========================================================================
    Num,               ///< 数字字面量（整数和浮点数）
    Str,               ///< 字符串字面量
    Bytes,             ///< 字节串字面量
    NameConstant,      ///< 名称常量：None, True, False
    Ellipsis,          ///< 省略号：...
    
    // =========================================================================
    // 二元运算符节点类型
    // =========================================================================
    Add,               ///< 加法：+
    Sub,               ///< 减法：-
    Mult,              ///< 乘法：*
    MatMult,           ///< 矩阵乘法：@
    Div,               ///< 除法：/
    FloorDiv,          ///< 整除：//
    Mod,               ///< 取模：%
    Pow,               ///< 幂运算：**
    LShift,            ///< 左移：<<
    RShift,            ///< 右移：>>
    BitOr,             ///< 按位或：|
    BitXor,            ///< 按位异或：^
    BitAnd,            ///< 按位与：&
    
    // =========================================================================
    // 比较运算符节点类型
    // =========================================================================
    Eq,                ///< 等于：==
    NotEq,             ///< 不等于：!=
    Lt,                ///< 小于：<
    LtE,               ///< 小于等于：<=
    Gt,                ///< 大于：>
    GtE,               ///< 大于等于：>=
    Is,                ///< 身份比较：is
    IsNot,             ///< 身份不等：is not
    In,                ///< 成员测试：in
    NotIn,             ///< 成员不测试：not in
    
    // =========================================================================
    // 一元运算符节点类型
    // =========================================================================
    Invert,            ///< 按位取反：~
    Not,               ///< 逻辑非：not
    UAdd,              ///< 一元加：+
    USub,              ///< 一元减：-
    
    // =========================================================================
    // 布尔运算符节点类型
    // =========================================================================
    And,               ///< 逻辑与：and
    Or,                ///< 逻辑或：or
    
    // =========================================================================
    // 上下文节点类型
    // =========================================================================
    Expr,              ///< 表达式上下文
    Stmt,              ///< 语句上下文

    Invalid,           ///< 无效节点类型
};

// ============================================================================
// 位置信息结构体
// ============================================================================

/**
 * @struct Position
 * @brief AST 节点位置信息结构体
 *
 * 记录节点在源代码中的位置，用于错误报告和调试。
 * 包含起始位置和结束位置，支持单行和多行节点的位置追踪。
 *
 * @section usage 使用示例
 * @code
 * Position pos(1, 5, 1, 10);  // 第1行第5列到第1行第10列
 * Position single(5, 3);       // 第5行第3列（单位置）
 * @endcode
 *
 * @note 行号和列号都从 1 开始计数
 */
struct Position {
    size_t line;      ///< 起始行号 (从 1 开始)
    size_t column;    ///< 起始列号 (从 1 开始)
    size_t endLine;   ///< 结束行号
    size_t endColumn; ///< 结束列号
    
    /**
     * @brief 默认构造函数
     * 初始化为无效位置 (0, 0, 0, 0)
     */
    Position() : line(0), column(0), endLine(0), endColumn(0) {}
    
    /**
     * @brief 单位置构造函数
     * @param l 起始行号
     * @param c 起始列号
     * 起始位置和结束位置相同
     */
    Position(size_t l, size_t c) : line(l), column(c), endLine(l), endColumn(c) {}
    
    /**
     * @brief 完整位置构造函数
     * @param l 起始行号
     * @param c 起始列号
     * @param el 结束行号
     * @param ec 结束列号
     */
    Position(size_t l, size_t c, size_t el, size_t ec) 
        : line(l), column(c), endLine(el), endColumn(ec) {}
};

// ============================================================================
// AST 基类
// ============================================================================

/**
 * @class ASTNode
 * @brief AST 节点基类
 *
 * 所有 AST 节点的基类，提供通用的位置信息和类型信息。
 * 采用CRTP（Curiously Recurring Template Pattern）模式设计，
 * 支持类型安全的节点访问和遍历。
 *
 * @section design 设计说明
 * - 使用虚函数实现多态
 * - 提供位置信息用于错误报告
 * - 支持 toString() 方法用于调试和序列化
 *
 * @see Expr 表达式节点基类
 * @see Stmt 语句节点基类
 */
class ASTNode {
public:
    ASTNodeType type;      ///< 节点类型
    Position position;     ///< 节点在源代码中的位置
    
    /**
     * @brief 构造函数
     * @param t 节点类型
     */
    ASTNode(ASTNodeType t) : type(t), position() {}
    
    /**
     * @brief 带位置信息的构造函数
     * @param t 节点类型
     * @param pos 位置信息
     */
    ASTNode(ASTNodeType t, const Position& pos) : type(t), position(pos) {}
    
    /**
     * @brief 虚析构函数
     */
    virtual ~ASTNode() = default;
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串，用于调试和序列化
     */
    virtual std::string toString() const = 0;
    
    /**
     * @brief 获取节点类型名称
     * @param type AST 节点类型
     * @return 类型名称字符串
     */
    static const char* getTypeName(ASTNodeType type);
};

// ============================================================================
// 表达式节点类
// ============================================================================

/**
 * @class Expr
 * @brief 表达式节点基类
 *
 * 表达式是计算产生值的语法结构。
 * 所有表达式类型都继承自此类，如常量、变量引用、函数调用等。
 *
 * @section usage 使用示例
 * @code
 * // 表达式节点基类用于类型擦除和统一处理
 * std::vector<std::unique_ptr<Expr>> expressions;
 * expressions.push_back(std::make_unique<Constant>(42));
 * expressions.push_back(std::make_unique<Name>("x", Name::Load));
 * @endcode
 *
 * @see Constant 常量表达式
 * @see Name 名称表达式
 * @see Call 函数调用表达式
 */
class Expr : public ASTNode {
public:
    /**
     * @brief 构造函数
     * @param t 节点类型
     */
    Expr(ASTNodeType t) : ASTNode(t) {}
    
    /**
     * @brief 带位置信息的构造函数
     * @param t 节点类型
     * @param pos 位置信息
     */
    Expr(ASTNodeType t, const Position& pos) : ASTNode(t, pos) {}
};

// ============================================================================
// 常量和字面量表达式
// ============================================================================

/**
 * @class Constant
 * @brief 常量表达式节点
 *
 * 对应 CPython 的 ast.Constant 节点。
 * 表示字面量值，支持多种类型：数字、字符串、字节串、None、True、False、...（Ellipsis）。
 *
 * @section value_types 支持的值类型
 * - std::monostate: 表示省略号 ...
 * - bool: 表示 True/False
 * - std::nullptr_t: 表示 None
 * - int64_t: 表示整数
 * - double: 表示浮点数
 * - std::string: 表示字符串
 * - std::vector<uint8_t>: 表示字节串
 *
 * @section usage 使用示例
 * @code
 * auto num = std::make_unique<Constant>(pos, 42);
 * auto str = std::make_unique<Constant>(pos, "hello");
 * auto none = std::make_unique<Constant>(pos);
 * none->value = nullptr;
 * @endcode
 */
class Constant : public Expr {
public:
    /**
     * @brief 存储的值，使用 std::variant 支持多类型
     */
    std::variant<
        std::monostate,          ///< 省略号 ...
        bool,                    ///< True/False
        std::nullptr_t,          ///< None
        int64_t,                 ///< 整数
        double,                  ///< 浮点数
        std::string,             ///< 字符串
        std::vector<uint8_t>     ///< 字节串
    > value;
    
    // =========================================================================
    // 类型查询辅助方法
    // =========================================================================
    
    /**
     * @brief 检查是否为 None
     * @return 如果值是 None 返回 true
     */
    bool isNone() const { return std::holds_alternative<std::nullptr_t>(value); }
    
    /**
     * @brief 检查是否为 True
     * @return 如果值是 True 返回 true
     */
    bool isTrue() const { return std::holds_alternative<bool>(value) && std::get<bool>(value); }
    
    /**
     * @brief 检查是否为 False
     * @return 如果值是 False 返回 true
     */
    bool isFalse() const { return std::holds_alternative<bool>(value) && !std::get<bool>(value); }
    
    /**
     * @brief 检查是否为布尔值
     * @return 如果值是 bool 类型返回 true
     */
    bool isBool() const { return std::holds_alternative<bool>(value); }
    
    /**
     * @brief 检查是否为整数
     * @return 如果值是 int64_t 返回 true
     */
    bool isInt() const { return std::holds_alternative<int64_t>(value); }
    
    /**
     * @brief 检查是否为浮点数
     * @return 如果值是 double 返回 true
     */
    bool isFloat() const { return std::holds_alternative<double>(value); }
    
    /**
     * @brief 检查是否为字符串
     * @return 如果值是 std::string 返回 true
     */
    bool isString() const { return std::holds_alternative<std::string>(value); }
    
    /**
     * @brief 检查是否为字节串
     * @return 如果值是 std::vector<uint8_t> 返回 true
     */
    bool isBytes() const { return std::holds_alternative<std::vector<uint8_t>>(value); }
    
    // =========================================================================
    // 值获取辅助方法
    // =========================================================================
    
    /**
     * @brief 获取整数值
     * @return 存储的 int64_t 值
     * @pre isInt() 必须为 true
     */
    int64_t asInt() const { return std::get<int64_t>(value); }
    
    /**
     * @brief 获取浮点数值
     * @return 存储的 double 值
     * @pre isFloat() 必须为 true
     */
    double asFloat() const { return std::get<double>(value); }
    
    /**
     * @brief 获取字符串值
     * @return 存储的 std::string 值
     * @pre isString() 必须为 true
     */
    std::string asString() const { return std::get<std::string>(value); }
    
    /**
     * @brief 默认构造函数
     */
    Constant() : Expr(ASTNodeType::Constant) {}
    
    /**
     * @brief 带位置信息的构造函数
     * @param pos 位置信息
     */
    Constant(const Position& pos) : Expr(ASTNodeType::Constant, pos) {}
    
    /**
     * @brief 整数常量构造函数
     * @param pos 位置信息
     * @param val 整数值
     */
    Constant(const Position& pos, int64_t val) : Expr(ASTNodeType::Constant, pos), value(val) {}
    
    /**
     * @brief 浮点数常量构造函数
     * @param pos 位置信息
     * @param val 浮点数值
     */
    Constant(const Position& pos, double val) : Expr(ASTNodeType::Constant, pos), value(val) {}
    
    /**
     * @brief 字符串常量构造函数
     * @param pos 位置信息
     * @param val 字符串值
     */
    Constant(const Position& pos, const std::string& val) : Expr(ASTNodeType::Constant, pos), value(val) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 名称和标识符表达式
// ============================================================================

/**
 * @class Name
 * @brief 名称表达式节点（变量、函数名引用）
 *
 * 对应 CPython 的 ast.Name 节点。
 * 表示对变量、函数名或其他标识符的引用。
 *
 * @section context 上下文类型
 * - Load: 加载上下文，读取变量值
 * - Store: 存储上下文，赋值给变量
 * - Del: 删除上下文，删除变量
 *
 * @section usage 使用示例
 * @code
 * // 读取变量
 * auto var = std::make_unique<Name>("x", Name::Load);
 *
 * // 赋值变量
 * auto target = std::make_unique<Name>("y", Name::Store);
 *
 * // 删除变量
 * auto del_target = std::make_unique<Name>("z", Name::Del);
 * @endcode
 */
class Name : public Expr {
public:
    std::string id;                      ///< 名称标识符
    enum Context { Load, Store, Del } ctx;///< 上下文类型
    
    /**
     * @brief 默认构造函数
     */
    Name() : Expr(ASTNodeType::Name), id(), ctx(Load) {}
    
    /**
     * @brief 构造函数
     * @param name 名称标识符
     * @param context 上下文类型
     */
    Name(const std::string& name, Context context) 
        : Expr(ASTNodeType::Name), id(name), ctx(context) {}
    
    /**
     * @brief 带位置信息的构造函数
     * @param pos 位置信息
     * @param name 名称标识符
     * @param context 上下文类型
     */
    Name(const Position& pos, const std::string& name, Context context) 
        : Expr(ASTNodeType::Name, pos), id(name), ctx(context) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 二元和一元运算表达式
// ============================================================================

/**
 * @class BinOp
 * @brief 二元操作表达式节点
 *
 * 对应 CPython 的 ast.BinOp 节点。
 * 支持所有二元运算符：+ - * / // % ** << >> & | ^ @
 *
 * @section operators 支持的运算符
 * - Add: 加法 (+)
 * - Sub: 减法 (-)
 * - Mult: 乘法 (*)
 * - MatMult: 矩阵乘法 (@)
 * - Div: 除法 (/)
 * - FloorDiv: 整除 (//)
 * - Mod: 取模 (%)
 * - Pow: 幂运算 (**)
 * - LShift: 左移 (<<)
 * - RShift: 右移 (>>)
 * - BitOr: 按位或 (|)
 * - BitXor: 按位异或 (^)
 * - BitAnd: 按位与 (&)
 *
 * @section usage 使用示例
 * @code
 * // x + 10
 * auto add = std::make_unique<BinOp>(
 *     std::make_unique<Name>("x", Name::Load),
 *     ASTNodeType::Add,
 *     std::make_unique<Constant>(pos, 10)
 * );
 * @endcode
 */
class BinOp : public Expr {
public:
    std::unique_ptr<Expr> left;       ///< 左操作数
    ASTNodeType op;                   ///< 运算符类型
    std::unique_ptr<Expr> right;      ///< 右操作数
    
    /**
     * @brief 默认构造函数
     */
    BinOp() : Expr(ASTNodeType::BinOp), left(nullptr), op(ASTNodeType::Add), right(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param l 左操作数
     * @param oper 运算符类型
     * @param r 右操作数
     */
    BinOp(std::unique_ptr<Expr> l, ASTNodeType oper, std::unique_ptr<Expr> r)
        : Expr(ASTNodeType::BinOp), left(std::move(l)), op(oper), right(std::move(r)) {}
    
    /**
     * @brief 带位置信息的构造函数
     * @param pos 位置信息
     * @param l 左操作数
     * @param oper 运算符类型
     * @param r 右操作数
     */
    BinOp(const Position& pos, std::unique_ptr<Expr> l, ASTNodeType oper, std::unique_ptr<Expr> r)
        : Expr(ASTNodeType::BinOp, pos), left(std::move(l)), op(oper), right(std::move(r)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class UnaryOp
 * @brief 一元操作表达式节点
 *
 * 对应 CPython 的 ast.UnaryOp 节点。
 * 支持一元运算符：- + not ~
 *
 * @section operators 支持的运算符
 * - USub: 一元减 (-)
 * - UAdd: 一元加 (+)
 * - Not: 逻辑非 (not)
 * - Invert: 按位取反 (~)
 *
 * @section usage 使用示例
 * @code
 * // -x
 * auto neg = std::make_unique<UnaryOp>(
 *     ASTNodeType::USub,
 *     std::make_unique<Name>("x", Name::Load)
 * );
 *
 * // not flag
 * auto logical_not = std::make_unique<UnaryOp>(
 *     ASTNodeType::Not,
 *     std::make_unique<Name>("flag", Name::Load)
 * );
 * @endcode
 */
class UnaryOp : public Expr {
public:
    ASTNodeType op;                   ///< 运算符类型
    std::unique_ptr<Expr> operand;   ///< 操作数
    
    /**
     * @brief 默认构造函数
     */
    UnaryOp() : Expr(ASTNodeType::UnaryOp), op(ASTNodeType::USub), operand(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param oper 运算符类型
     * @param expr 操作数表达式
     */
    UnaryOp(ASTNodeType oper, std::unique_ptr<Expr> expr)
        : Expr(ASTNodeType::UnaryOp), op(oper), operand(std::move(expr)) {}
    
    /**
     * @brief 带位置信息的构造函数
     * @param pos 位置信息
     * @param oper 运算符类型
     * @param expr 操作数表达式
     */
    UnaryOp(const Position& pos, ASTNodeType oper, std::unique_ptr<Expr> expr)
        : Expr(ASTNodeType::UnaryOp, pos), op(oper), operand(std::move(expr)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Compare
 * @brief 比较表达式节点
 *
 * 对应 CPython 的 ast.Compare 节点。
 * 支持链式比较操作，如：1 < x < 10 会被解析为 (1 < x) and (x < 10)。
 *
 * @section operators 支持的比较运算符
 * - Eq: 等于 (==)
 * - NotEq: 不等于 (!=)
 * - Lt: 小于 (<)
 * - LtE: 小于等于 (<=)
 * - Gt: 大于 (>)
 * - GtE: 大于等于 (>=)
 * - Is: 身份比较 (is)
 * - IsNot: 身份不等 (is not)
 * - In: 成员测试 (in)
 * - NotIn: 成员不测试 (not in)
 *
 * @section usage 使用示例
 * @code
 * // 简单比较：x < 10
 * auto simple = std::make_unique<Compare>(
 *     std::make_unique<Name>("x", Name::Load),
 *     {ASTNodeType::Lt},
 *     {std::make_unique<Constant>(pos, 10)}
 * );
 *
 * // 链式比较：1 < x < 10
 * auto chain = std::make_unique<Compare>(
 *     std::make_unique<Constant>(pos, 1),
 *     {ASTNodeType::Lt, ASTNodeType::Lt},
 *     {
 *         std::make_unique<Name>("x", Name::Load),
 *         std::make_unique<Constant>(pos, 10)
 *     }
 * );
 * @endcode
 */
class Compare : public Expr {
public:
    std::unique_ptr<Expr> left;                     ///< 左操作数
    std::vector<ASTNodeType> ops;                   ///< 比较运算符列表
    std::vector<std::unique_ptr<Expr>> comparators; ///< 右操作数列表
    
    /**
     * @brief 默认构造函数
     */
    Compare() : Expr(ASTNodeType::Compare), left(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param l 左操作数
     * @param operators 比较运算符列表
     * @param comps 右操作数列表
     */
    Compare(std::unique_ptr<Expr> l, std::vector<ASTNodeType> operators,
            std::vector<std::unique_ptr<Expr>> comps)
        : Expr(ASTNodeType::Compare), left(std::move(l)), 
          ops(std::move(operators)), comparators(std::move(comps)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 布尔运算表达式
// ============================================================================

/**
 * @class BoolOp
 * @brief 布尔操作表达式节点
 *
 * 对应 CPython 的 ast.BoolOp 节点。
 * 表示布尔运算，支持 and 和 or 操作。
 *
 * @section operators 支持的运算符
 * - And: 逻辑与 (and)
 * - Or: 逻辑或 (or)
 *
 * @section usage 使用示例
 * @code
 * // a and b and c
 * auto and_op = std::make_unique<BoolOp>(
 *     ASTNodeType::And,
 *     {
 *         std::make_unique<Name>("a", Name::Load),
 *         std::make_unique<Name>("b", Name::Load),
 *         std::make_unique<Name>("c", Name::Load)
 *     }
 * );
 *
 * // x or y
 * auto or_op = std::make_unique<BoolOp>(
 *     ASTNodeType::Or,
 *     {
 *         std::make_unique<Name>("x", Name::Load),
 *         std::make_unique<Name>("y", Name::Load)
 *     }
 * );
 * @endcode
 */
class BoolOp : public Expr {
public:
    ASTNodeType op;                                   ///< 运算符类型 (And/Or)
    std::vector<std::unique_ptr<Expr>> values;       ///< 操作数列表
    
    /**
     * @brief 默认构造函数
     */
    BoolOp() : Expr(ASTNodeType::BoolOp), op(ASTNodeType::And) {}
    
    /**
     * @brief 构造函数
     * @param oper 运算符类型
     * @param vals 操作数列表
     */
    BoolOp(ASTNodeType oper, std::vector<std::unique_ptr<Expr>> vals)
        : Expr(ASTNodeType::BoolOp), op(oper), values(std::move(vals)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 函数调用和引用表达式
// ============================================================================

/**
 * @class Call
 * @brief 函数调用表达式节点
 *
 * 对应 CPython 的 ast.Call 节点。
 * 表示函数调用，包括被调用的函数对象、位置参数和关键字参数。
 *
 * @section usage 使用示例
 * @code
 * // print("hello")
 * auto call = std::make_unique<Call>(
 *     std::make_unique<Name>("print", Name::Load)
 * );
 * call->args.push_back(std::make_unique<Constant>(pos, "hello"));
 *
 * // max(x, y, key=abs)
 * auto max_call = std::make_unique<Call>(
 *     std::make_unique<Name>("max", Name::Load)
 * );
 * max_call->args.push_back(std::make_unique<Name>("x", Name::Load));
 * max_call->args.push_back(std::make_unique<Name>("y", Name::Load));
 * @endcode
 */
class Call : public Expr {
public:
    std::unique_ptr<Expr> func;                      ///< 被调用的函数/可调用对象
    std::vector<std::unique_ptr<Expr>> args;         ///< 位置参数列表
    std::vector<std::unique_ptr<Expr>> keywords;    ///< 关键字参数列表
    
    /**
     * @brief 默认构造函数
     */
    Call() : Expr(ASTNodeType::Call), func(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param function 被调用的函数表达式
     */
    Call(std::unique_ptr<Expr> function)
        : Expr(ASTNodeType::Call), func(std::move(function)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class IfExp
 * @brief 条件表达式节点（三元运算符）
 *
 * 对应 CPython 的 ast.IfExp 节点。
 * Python 的三元运算符格式：value1 if condition else value2
 *
 * @section usage 使用示例
 * @code
 * // "yes" if condition else "no"
 * auto ternary = std::make_unique<IfExp>(
 *     std::make_unique<Name>("condition", Name::Load),
 *     std::make_unique<Constant>(pos, "yes"),
 *     std::make_unique<Constant>(pos, "no")
 * );
 * @endcode
 */
class IfExp : public Expr {
public:
    std::unique_ptr<Expr> test;       ///< 条件表达式
    std::unique_ptr<Expr> body;       ///< 条件为真时返回的表达式
    std::unique_ptr<Expr> orelse;     ///< 条件为假时返回的表达式
    
    /**
     * @brief 默认构造函数
     */
    IfExp() : Expr(ASTNodeType::IfExp), test(nullptr), body(nullptr), orelse(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param cond 条件表达式
     * @param then 真值分支表达式
     * @param else_ 假值分支表达式
     */
    IfExp(std::unique_ptr<Expr> cond, std::unique_ptr<Expr> then, std::unique_ptr<Expr> else_)
        : Expr(ASTNodeType::IfExp), test(std::move(cond)), body(std::move(then)), orelse(std::move(else_)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Lambda
 * @brief Lambda 匿名函数表达式节点
 *
 * 对应 CPython 的 ast.Lambda 节点。
 * Python 的 lambda 表达式：lambda parameters: expression
 *
 * @section usage 使用示例
 * @code
 * // lambda x: x + 1
 * auto lambda = std::make_unique<Lambda>(
 *     std::make_unique<Name>("x", Name::Load),
 *     std::make_unique<BinOp>(
 *         std::make_unique<Name>("x", Name::Load),
 *         ASTNodeType::Add,
 *         std::make_unique<Constant>(pos, 1)
 *     )
 * );
 * @endcode
 */
class Lambda : public Expr {
public:
    std::unique_ptr<Expr> args;       ///< 参数列表（作为表达式节点）
    std::unique_ptr<Expr> body;       ///< 函数体表达式
    
    /**
     * @brief 默认构造函数
     */
    Lambda() : Expr(ASTNodeType::Lambda), args(nullptr), body(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param parameters 参数表达式
     * @param expr 函数体表达式
     */
    Lambda(std::unique_ptr<Expr> parameters, std::unique_ptr<Expr> expr)
        : Expr(ASTNodeType::Lambda), args(std::move(parameters)), body(std::move(expr)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 容器类型表达式
// ============================================================================

/**
 * @class Dict
 * @brief 字典字面量表达式节点
 *
 * 对应 CPython 的 ast.Dict 节点。
 * 字典是由键值对组成的映射结构：{key1: value1, key2: value2, ...}
 *
 * @section usage 使用示例
 * @code
 * // {"a": 1, "b": 2}
 * auto dict = std::make_unique<Dict>();
 * dict->keys.push_back(std::make_unique<Constant>(pos, "a"));
 * dict->values.push_back(std::make_unique<Constant>(pos, 1));
 * dict->keys.push_back(std::make_unique<Constant>(pos, "b"));
 * dict->values.push_back(std::make_unique<Constant>(pos, 2));
 * @endcode
 */
class Dict : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> keys;    ///< 键表达式列表
    std::vector<std::unique_ptr<Expr>> values; ///< 值表达式列表
    
    /**
     * @brief 默认构造函数
     */
    Dict() : Expr(ASTNodeType::Dict) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Set
 * @brief 集合字面量表达式节点
 *
 * 对应 CPython 的 ast.Set 节点。
 * 集合是由不重复元素组成的无序容器：{elem1, elem2, ...}
 *
 * @section usage 使用示例
 * @code
 * // {1, 2, 3}
 * auto set = std::make_unique<Set>();
 * set->elts.push_back(std::make_unique<Constant>(pos, 1));
 * set->elts.push_back(std::make_unique<Constant>(pos, 2));
 * set->elts.push_back(std::make_unique<Constant>(pos, 3));
 * @endcode
 */
class Set : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elts; ///< 元素表达式列表
    
    /**
     * @brief 默认构造函数
     */
    Set() : Expr(ASTNodeType::Set) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class List
 * @brief 列表字面量表达式节点
 *
 * 对应 CPython 的 ast.List 节点。
 * 列表是有序的可变容器：[elem1, elem2, ...]
 *
 * @section context 上下文类型
 * - Load: 加载上下文，用于读取列表
 * - Store: 存储上下文，用于解包赋值
 * - Del: 删除上下文，用于删除元素
 *
 * @section usage 使用示例
 * @code
 * // [1, 2, 3]
 * auto list = std::make_unique<List>();
 * list->elts.push_back(std::make_unique<Constant>(pos, 1));
 * list->elts.push_back(std::make_unique<Constant>(pos, 2));
 * list->elts.push_back(std::make_unique<Constant>(pos, 3));
 * @endcode
 */
class List : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elts;       ///< 元素表达式列表
    enum Context { Load, Store, Del } ctx;         ///< 上下文类型
    
    /**
     * @brief 默认构造函数
     */
    List() : Expr(ASTNodeType::List), ctx(Load) {}
    
    /**
     * @brief 构造函数
     * @param elements 元素列表（使用移动语义）
     */
    List(std::vector<std::unique_ptr<Expr>>&& elements)
        : Expr(ASTNodeType::List), elts(std::move(elements)), ctx(Load) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Tuple
 * @brief 元组字面量表达式节点
 *
 * 对应 CPython 的 ast.Tuple 节点。
 * 元组是有序的不可变容器：(elem1, elem2, ...) 或 elem1, elem2, ...
 *
 * @section usage 使用示例
 * @code
 * // (1, 2, 3)
 * auto tuple = std::make_unique<Tuple>();
 * tuple->elts.push_back(std::make_unique<Constant>(pos, 1));
 * tuple->elts.push_back(std::make_unique<Constant>(pos, 2));
 * tuple->elts.push_back(std::make_unique<Constant>(pos, 3));
 * @endcode
 */
class Tuple : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elts;       ///< 元素表达式列表
    enum Context { Load, Store, Del } ctx;         ///< 上下文类型
    
    /**
     * @brief 默认构造函数
     */
    Tuple() : Expr(ASTNodeType::Tuple), ctx(Load) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 切片和属性访问表达式
// ============================================================================

/**
 * @class Subscript
 * @brief 下标访问表达式节点
 *
 * 对应 CPython 的 ast.Subscript 节点。
 * 表示通过下标访问容器元素：obj[key]
 *
 * @section usage 使用示例
 * @code
 * // items[0]
 * auto subscript = std::make_unique<Subscript>(
 *     std::make_unique<Name>("items", Name::Load),
 *     std::make_unique<Constant>(pos, 0)
 * );
 *
 * // matrix[i][j]
 * auto nested = std::make_unique<Subscript>(
 *     std::make_unique<Subscript>(
 *         std::make_unique<Name>("matrix", Name::Load),
 *         std::make_unique<Name>("i", Name::Load)
 *     ),
 *     std::make_unique<Name>("j", Name::Load)
 * );
 * @endcode
 */
class Subscript : public Expr {
public:
    std::unique_ptr<Expr> value;     ///< 被下标访问的对象
    std::unique_ptr<Expr> slice;     ///< 下标表达式
    
    /**
     * @brief 默认构造函数
     */
    Subscript() : Expr(ASTNodeType::Subscript), value(nullptr), slice(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param val 被访问的对象
     * @param idx 下标表达式
     */
    Subscript(std::unique_ptr<Expr> val, std::unique_ptr<Expr> idx)
        : Expr(ASTNodeType::Subscript), value(std::move(val)), slice(std::move(idx)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Attribute
 * @brief 属性访问表达式节点
 *
 * 对应 CPython 的 ast.Attribute 节点。
 * 表示访问对象的属性：obj.attr
 *
 * @section usage 使用示例
 * @code
 * // obj.x
 * auto attr = std::make_unique<Attribute>(
 *     std::make_unique<Name>("obj", Name::Load),
 *     "x"
 * );
 *
 * // sys.stdout.write
 * auto method = std::make_unique<Attribute>(
 *     std::make_unique<Attribute>(
 *         std::make_unique<Name>("sys", Name::Load),
 *         "stdout"
 *     ),
 *     "write"
 * );
 * @endcode
 */
class Attribute : public Expr {
public:
    std::unique_ptr<Expr> value; ///< 对象表达式
    std::string attr;            ///< 属性名
    
    /**
     * @brief 默认构造函数
     */
    Attribute() : Expr(ASTNodeType::Attribute), value(nullptr), attr() {}
    
    /**
     * @brief 构造函数
     * @param val 对象表达式
     * @param attribute 属性名
     */
    Attribute(std::unique_ptr<Expr> val, const std::string& attribute)
        : Expr(ASTNodeType::Attribute), value(std::move(val)), attr(attribute) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Starred
 * @brief 星号解包表达式节点
 *
 * 对应 CPython 的 ast.Starred 节点。
 * 用于解包操作：*args 或 *iterable
 *
 * @section usage 使用示例
 * @code
 * // *rest 在函数参数中
 * auto starred = std::make_unique<Starred>(
 *     std::make_unique<Name>("rest", Name::Load),
 *     Starred::Load
 * );
 *
 * // [1, *items, 3] 解包列表
 * @endcode
 */
class Starred : public Expr {
public:
    std::unique_ptr<Expr> value;              ///< 被解包的值
    enum Context { Load, Store, Del } ctx;    ///< 上下文类型
    
    /**
     * @brief 默认构造函数
     */
    Starred() : Expr(ASTNodeType::Starred), value(nullptr), ctx(Load) {}
    
    /**
     * @brief 构造函数
     * @param val 被解包的表达式
     * @param context 上下文类型
     */
    Starred(std::unique_ptr<Expr> val, Context context)
        : Expr(ASTNodeType::Starred), value(std::move(val)), ctx(context) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Slice
 * @brief 切片表达式节点
 *
 * 对应 CPython 的 ast.Slice 节点。
 * 表示切片操作：obj[start:stop:step]
 *
 * @section usage 使用示例
 * @code
 * // items[1:10:2]
 * auto slice = std::make_unique<Slice>();
 * slice->lower = std::make_unique<Constant>(pos, 1);
 * slice->upper = std::make_unique<Constant>(pos, 10);
 * slice->step = std::make_unique<Constant>(pos, 2);
 * @endcode
 */
class Slice : public Expr {
public:
    std::unique_ptr<Expr> lower;   ///< 起始下标
    std::unique_ptr<Expr> upper;   ///< 结束下标
    std::unique_ptr<Expr> step;    ///< 步长
    
    /**
     * @brief 默认构造函数
     */
    Slice() : Expr(ASTNodeType::Slice) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class ListComp
 * @brief 列表推导式表达式节点
 *
 * 对应 CPython 的 ast.ListComp 节点。
 * 列表推导式是一种简洁的创建列表的语法：[x for x in items if condition]
 *
 * @section usage 使用示例
 * @code
 * // [x * 2 for x in range(10) if x % 2 == 0]
 * auto list_comp = std::make_unique<ListComp>();
 * list_comp->elt = std::make_unique<BinOp>(
 *     std::make_unique<Name>("x", Name::Load),
 *     ASTNodeType::Mult,
 *     std::make_unique<Constant>(pos, 2)
 * );
 * // 需要添加 generators
 * @endcode
 */
class ListComp : public Expr {
public:
    std::unique_ptr<Expr> elt;                          ///< 元素表达式
    std::vector<std::unique_ptr<Expr>> generators;    ///< 生成器/迭代器列表
    
    /**
     * @brief 默认构造函数
     */
    ListComp() : Expr(ASTNodeType::ListComp), elt(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class SetComp
 * @brief 集合推导式表达式节点
 *
 * 对应 CPython 的 ast.SetComp 节点。
 * 集合推导式是一种简洁的创建集合的语法：{x for x in items if condition}
 *
 * @section usage 使用示例
 * @code
 * // {x for x in range(10) if x % 2 == 0}
 * auto set_comp = std::make_unique<SetComp>();
 * set_comp->elt = std::make_unique<Name>("x", Name::Load);
 * @endcode
 */
class SetComp : public Expr {
public:
    std::unique_ptr<Expr> elt;                          ///< 元素表达式
    std::vector<std::unique_ptr<Expr>> generators;    ///< 生成器/迭代器列表
    
    /**
     * @brief 默认构造函数
     */
    SetComp() : Expr(ASTNodeType::SetComp), elt(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class DictComp
 * @brief 字典推导式表达式节点
 *
 * 对应 CPython 的 ast.DictComp 节点。
 * 字典推导式是一种简洁的创建字典的语法：{k: v for k, v in items if condition}
 *
 * @section usage 使用示例
 * @code
 * // {k: v for k, v in items.items() if v > 0}
 * auto dict_comp = std::make_unique<DictComp>();
 * dict_comp->key = std::make_unique<Name>("k", Name::Load);
 * dict_comp->value = std::make_unique<Name>("v", Name::Load);
 * @endcode
 */
class DictComp : public Expr {
public:
    std::unique_ptr<Expr> key;                          ///< 键表达式
    std::unique_ptr<Expr> value;                       ///< 值表达式
    std::vector<std::unique_ptr<Expr>> generators;    ///< 生成器/迭代器列表
    
    /**
     * @brief 默认构造函数
     */
    DictComp() : Expr(ASTNodeType::DictComp), key(nullptr), value(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class GeneratorExp
 * @brief 生成器表达式节点
 *
 * 对应 CPython 的 ast.GeneratorExp 节点。
 * 生成器表达式是一种惰性求值的可迭代对象：(x for x in items if condition)
 *
 * @section usage 使用示例
 * @code
 * // (x for x in range(1000) if x % 2 == 0)
 * auto gen = std::make_unique<GeneratorExp>();
 * gen->elt = std::make_unique<Name>("x", Name::Load);
 * @endcode
 *
 * @note 生成器表达式是惰性求值的，不会立即计算所有元素
 */
class GeneratorExp : public Expr {
public:
    std::unique_ptr<Expr> elt;                          ///< 元素表达式
    std::vector<std::unique_ptr<Expr>> generators;    ///< 生成器/迭代器列表
    
    /**
     * @brief 默认构造函数
     */
    GeneratorExp() : Expr(ASTNodeType::GeneratorExp), elt(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 异步和生成器表达式
// ============================================================================

/**
 * @class Yield
 * @brief yield 表达式节点
 *
 * 对应 CPython 的 ast.Yield 节点。
 * 用于生成器函数中产生值：yield value
 *
 * @section usage 使用示例
 * @code
 * // yield x
 * auto yield = std::make_unique<Yield>(
 *     std::make_unique<Name>("x", Name::Load)
 * );
 *
 * // yield (空值)
 * auto empty_yield = std::make_unique<Yield>();
 * @endcode
 */
class Yield : public Expr {
public:
    std::unique_ptr<Expr> value; ///< 产生的值（可选）
    
    /**
     * @brief 默认构造函数
     */
    Yield() : Expr(ASTNodeType::Yield), value(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param val 要产生的值
     */
    Yield(std::unique_ptr<Expr> val) : Expr(ASTNodeType::Yield), value(std::move(val)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class YieldFrom
 * @brief yield from 表达式节点
 *
 * 对应 CPython 的 ast.YieldFrom 节点。
 * 用于委托给另一个生成器：yield from iterable
 *
 * @section usage 使用示例
 * @code
 * // yield from items
 * auto yield_from = std::make_unique<YieldFrom>(
 *     std::make_unique<Name>("items", Name::Load)
 * );
 * @endcode
 */
class YieldFrom : public Expr {
public:
    std::unique_ptr<Expr> value; ///< 要迭代的对象
    
    /**
     * @brief 默认构造函数
     */
    YieldFrom() : Expr(ASTNodeType::YieldFrom), value(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param val 要迭代的对象
     */
    YieldFrom(std::unique_ptr<Expr> val) : Expr(ASTNodeType::YieldFrom), value(std::move(val)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Await
 * @brief await 表达式节点
 *
 * 对应 CPython 的 ast.Await 节点。
 * 用于异步函数中等待 coroutine 完成：await coroutine
 *
 * @section usage 使用示例
 * @code
 * // await result
 * auto await = std::make_unique<Await>(
 *     std::make_unique<Name>("result", Name::Load)
 * );
 * @endcode
 *
 * @note await 只能在 async 函数内部使用
 */
class Await : public Expr {
public:
    std::unique_ptr<Expr> value; ///< 等待的 coroutine
    
    /**
     * @brief 默认构造函数
     */
    Await() : Expr(ASTNodeType::Await), value(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param val 要等待的 coroutine
     */
    Await(std::unique_ptr<Expr> val) : Expr(ASTNodeType::Await), value(std::move(val)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// f-string 表达式
// ============================================================================

/**
 * @class FormattedValue
 * @brief f-string 值表达式节点
 *
 * 对应 CPython 的 ast.FormattedValue 节点。
 * 表示 f-string 中的值表达式：{expr} 或 {expr!r} 或 {expr:format}
 *
 * @section conversion 转换标记
 * - -1: 无转换（默认）
 * - 's': 使用 str() 转换
 * - 'r': 使用 repr() 转换
 * - 'a': 使用 ascii() 转换
 *
 * @section usage 使用示例
 * @code
 * // {x}
 * auto fv = std::make_unique<FormattedValue>(
 *     std::make_unique<Name>("x", Name::Load)
 * );
 *
 * // {x!r}
 * auto fv_r = std::make_unique<FormattedValue>(
 *     std::make_unique<Name>("x", Name::Load),
 *     'r'
 * );
 *
 * // {x:.2f}
 * auto fv_fmt = std::make_unique<FormattedValue>(
 *     std::make_unique<Name>("x", Name::Load)
 * );
 * fv_fmt->conversion = -1;  // 格式说明符在 format_spec 中
 * @endcode
 */
class FormattedValue : public Expr {
public:
    std::unique_ptr<Expr> value; ///< 值表达式
    int conversion;              ///< 转换标记：-1, 's', 'r', 'a'
    
    /**
     * @brief 默认构造函数
     */
    FormattedValue() : Expr(ASTNodeType::FormattedValue), value(nullptr), conversion(-1) {}
    
    /**
     * @brief 构造函数
     * @param val 值表达式
     * @param conv 转换标记
     */
    FormattedValue(std::unique_ptr<Expr> val, int conv = -1)
        : Expr(ASTNodeType::FormattedValue), value(std::move(val)), conversion(conv) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class JoinedStr
 * @brief f-string 字符串连接节点
 *
 * 对应 CPython 的 ast.JoinedStr 节点。
 * 表示 f-string 的整体，包含字符串片段和 FormattedValue 交替排列。
 *
 * @section usage 使用示例
 * @code
 * // f"hello {name}!"
 * auto jstr = std::make_unique<JoinedStr>();
 * jstr->values.push_back(std::make_unique<Constant>(pos, "hello "));
 * jstr->values.push_back(std::make_unique<FormattedValue>(
 *     std::make_unique<Name>("name", Name::Load)
 * ));
 * jstr->values.push_back(std::make_unique<Constant>(pos, "!"));
 * @endcode
 */
class JoinedStr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> values; ///< 字符串和 FormattedValue 交替列表
    
    /**
     * @brief 默认构造函数
     */
    JoinedStr() : Expr(ASTNodeType::JoinedStr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 语句节点基类
// ============================================================================

/**
 * @class Stmt
 * @brief 语句节点基类
 *
 * 语句是执行某个操作的语法结构，不产生值。
 * 所有语句类型都继承自此类，如函数定义、赋值、控制流等。
 *
 * @section statement_types 语句类型
 * - 执行语句（Expression Statement）
 * - 赋值语句（Assignment）
 * - 控制流（If, For, While, Try）
 * - 函数和类定义（FunctionDef, ClassDef）
 * - 异常处理（Raise, Try, Except）
 * - 导入导出（Import, ImportFrom）
 * - 其他（Pass, Break, Continue, Return, Yield）
 *
 * @see Expr 表达式节点基类
 */
class Stmt : public ASTNode {
public:
    /**
     * @brief 构造函数
     * @param t 节点类型
     */
    Stmt(ASTNodeType t) : ASTNode(t) {}
    
    /**
     * @brief 带位置信息的构造函数
     * @param t 节点类型
     * @param pos 位置信息
     */
    Stmt(ASTNodeType t, const Position& pos) : ASTNode(t, pos) {}
};

// ============================================================================
// 函数和类定义语句
// ============================================================================

/**
 * @class FunctionDef
 * @brief 函数定义语句节点
 *
 * 对应 CPython 的 ast.FunctionDef 节点。
 * 表示函数定义：def function_name(parameters) -> return_type: body
 *
 * @section usage 使用示例
 * @code
 * // def add(a, b):
 * //     return a + b
 * auto func = std::make_unique<FunctionDef>();
 * func->name = "add";
 * func->args = ... 参数列表 ...;
 * func->body = ... 函数体语句列表 ...;
 * @endcode
 */
class FunctionDef : public Stmt {
public:
    std::string name;                          ///< 函数名称
    std::unique_ptr<arguments> args;               ///< 参数列表
    std::vector<std::unique_ptr<Stmt>> body; ///< 函数体语句列表
    std::vector<std::unique_ptr<Expr>> decorator_list; ///< 装饰器列表
    std::string returns;                      ///< 返回类型注解（CSGO 扩展）
    
    /**
     * @brief 默认构造函数
     */
    FunctionDef() : Stmt(ASTNodeType::FunctionDef), name(), args(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class AsyncFunctionDef
 * @brief 异步函数定义语句节点
 *
 * 对应 CPython 的 ast.AsyncFunctionDef 节点。
 * 表示异步函数定义：async def function_name(parameters): body
 *
 * @section usage 使用示例
 * @code
 * // async def fetch_data(url):
 * //     response = await http_get(url)
 * //     return response.json()
 * auto async_func = std::make_unique<AsyncFunctionDef>();
 * async_func->name = "fetch_data";
 * async_func->body = ... 函数体语句列表 ...;
 * @endcode
 *
 * @note 异步函数内部可以使用 await 表达式
 */
class AsyncFunctionDef : public FunctionDef {
public:
    /**
     * @brief 默认构造函数
     */
    AsyncFunctionDef() : FunctionDef() { type = ASTNodeType::AsyncFunctionDef; }
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 类定义语句
// ============================================================================

/**
 * @class ClassDef
 * @brief 类定义语句节点
 *
 * 对应 CPython 的 ast.ClassDef 节点。
 * 表示类定义：class ClassName(Base1, Base2): body
 *
 * @section usage 使用示例
 * @code
 * // class MyClass(BaseClass):
 * //     def __init__(self):
 * //         self.value = 42
 * auto class_def = std::make_unique<ClassDef>();
 * class_def->name = "MyClass";
 * class_def->bases.push_back(std::make_unique<Name>("BaseClass", Name::Load));
 * class_def->body = ... 类体语句列表 ...;
 * @endcode
 */
class ClassDef : public Stmt {
public:
    std::string name;                              ///< 类名称
    std::unique_ptr<Expr> bases;                  ///< 基类列表
    std::unique_ptr<Expr> keywords;                ///< 关键字参数
    std::vector<std::unique_ptr<Stmt>> body;     ///< 类体语句列表
    std::vector<std::unique_ptr<Expr>> decorator_list; ///< 装饰器列表
    
    /**
     * @brief 默认构造函数
     */
    ClassDef() : Stmt(ASTNodeType::ClassDef), name(), bases(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 返回和删除语句
// ============================================================================

/**
 * @class Return
 * @brief return 返回语句节点
 *
 * 对应 CPython 的 ast.Return 节点。
 * 用于函数中返回值：return [value]
 *
 * @section usage 使用示例
 * @code
 * // return result
 * auto return_stmt = std::make_unique<Return>(
 *     std::make_unique<Name>("result", Name::Load)
 * );
 *
 * // return (无返回值)
 * auto empty_return = std::make_unique<Return>();
 * @endcode
 *
 * @note return 语句只能在函数内部使用
 */
class Return : public Stmt {
public:
    std::unique_ptr<Expr> value; ///< 返回值表达式（可选）
    
    /**
     * @brief 默认构造函数
     */
    Return() : Stmt(ASTNodeType::Return), value(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param val 返回值表达式
     */
    Return(std::unique_ptr<Expr> val) : Stmt(ASTNodeType::Return), value(std::move(val)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Delete
 * @brief del 删除语句节点
 *
 * 对应 CPython 的 ast.Delete 节点。
 * 用于删除变量或容器元素：del target
 *
 * @section usage 使用示例
 * @code
 * // del x
 * auto del_stmt = std::make_unique<Delete>();
 * del_stmt->targets.push_back(std::make_unique<Name>("x", Name::Del));
 *
 * // del items[0]
 * auto del_item = std::make_unique<Delete>();
 * del_item->targets.push_back(std::make_unique<Subscript>(
 *     std::make_unique<Name>("items", Name::Load),
 *     std::make_unique<Constant>(pos, 0)
 * ));
 * @endcode
 */
class Delete : public Stmt {
public:
    std::vector<std::unique_ptr<Expr>> targets; ///< 要删除的目标列表
    
    /**
     * @brief 默认构造函数
     */
    Delete() : Stmt(ASTNodeType::Delete) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 赋值语句
// ============================================================================

/**
 * @class Assign
 * @brief 赋值语句节点
 *
 * 对应 CPython 的 ast.Assign 节点。
 * 用于将右侧值赋给左侧目标：target1 = target2 = ... = value
 *
 * @section usage 使用示例
 * @code
 * // x = 10
 * auto assign = std::make_unique<Assign>();
 * assign->targets.push_back(std::make_unique<Name>("x", Name::Store));
 * assign->value = std::make_unique<Constant>(pos, 10);
 *
 * // a = b = c = 0
 * auto multi_assign = std::make_unique<Assign>();
 * multi_assign->targets.push_back(std::make_unique<Name>("a", Name::Store));
 * multi_assign->targets.push_back(std::make_unique<Name>("b", Name::Store));
 * multi_assign->targets.push_back(std::make_unique<Name>("c", Name::Store));
 * multi_assign->value = std::make_unique<Constant>(pos, 0);
 * @endcode
 */
class Assign : public Stmt {
public:
    std::vector<std::unique_ptr<Expr>> targets; ///< 赋值目标列表
    std::unique_ptr<Expr> value;               ///< 右侧值表达式
    
    /**
     * @brief 默认构造函数
     */
    Assign() : Stmt(ASTNodeType::Assign), value(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class AugAssign
 * @brief 增强赋值语句节点
 *
 * 对应 CPython 的 ast.AugAssign 节点。
 * 用于复合赋值操作：target operator= value（如 x += 1）
 *
 * @section operators 支持的运算符
 * - Add: +=
 * - Sub: -=
 * - Mult: *=
 * - MatMult: @=
 * - Div: /=
 * - FloorDiv: //=
 * - Mod: %=
 * - Pow: **=
 * - LShift: <<=
 * - RShift: >>=
 * - BitOr: |=
 * - BitXor: ^=
 * - BitAnd: &=
 *
 * @section usage 使用示例
 * @code
 * // x += 1
 * auto aug_assign = std::make_unique<AugAssign>();
 * aug_assign->target = std::make_unique<Name>("x", Name::Store);
 * aug_assign->op = ASTNodeType::Add;
 * aug_assign->value = std::make_unique<Constant>(pos, 1);
 * @endcode
 */
class AugAssign : public Stmt {
public:
    std::unique_ptr<Expr> target; ///< 赋值目标
    ASTNodeType op;                ///< 运算符类型
    std::unique_ptr<Expr> value;  ///< 值表达式
    
    /**
     * @brief 默认构造函数
     */
    AugAssign() : Stmt(ASTNodeType::AugAssign), target(nullptr), op(ASTNodeType::Add), value(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class AnnAssign
 * @brief 带类型注解的赋值语句节点
 *
 * 对应 CPython 的 ast.AnnAssign 节点。
 * 用于带类型注解的变量声明：target: annotation [= value]
 *
 * @section usage 使用示例
 * @code
 * // x: int
 * auto ann1 = std::make_unique<AnnAssign>();
 * ann1->target = std::make_unique<Name>("x", Name::Store);
 * ann1->annotation = std::make_unique<Name>("int", Name::Load);
 * ann1->simple = true;
 *
 * // x: int = 10
 * auto ann2 = std::make_unique<AnnAssign>();
 * ann2->target = std::make_unique<Name>("x", Name::Store);
 * ann2->annotation = std::make_unique<Name>("int", Name::Load);
 * ann2->value = std::make_unique<Constant>(pos, 10);
 * ann2->simple = true;
 * @endcode
 */
class AnnAssign : public Stmt {
public:
    std::unique_ptr<Expr> target;     ///< 赋值目标
    std::unique_ptr<Expr> annotation; ///< 类型注解表达式
    std::unique_ptr<Expr> value;     ///< 可选的初始值
    bool simple;                     ///< 是否为简单注解（仅声明无赋值）
    
    /**
     * @brief 默认构造函数
     */
    AnnAssign() : Stmt(ASTNodeType::AnnAssign), target(nullptr), 
                  annotation(nullptr), value(nullptr), simple(true) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 循环语句
// ============================================================================

/**
 * @class For
 * @brief for 循环语句节点
 *
 * 对应 CPython 的 ast.For 节点。
 * 用于迭代循环：for target in iter: body [else: orelse]
 *
 * @section usage 使用示例
 * @code
 * // for i in range(10):
 * //     print(i)
 * auto for_loop = std::make_unique<For>();
 * for_loop->target = std::make_unique<Name>("i", Name::Store);
 * for_loop->iter = std::make_unique<Call>(
 *     std::make_unique<Name>("range", Name::Load)
 * );
 * for_loop->iter->args.push_back(std::make_unique<Constant>(pos, 10));
 * // for_loop->body = ... 循环体 ...;
 *
 * // for-else 结构
 * auto for_else = std::make_unique<For>();
 * for_else->target =  ... ;
 * for_else->iter =  ... ;
 * // for_else->body = ... 循环体 ...;
 * // for_else->orelse = ... else 分支 ...;
 * @endcode
 */
class For : public Stmt {
public:
    std::unique_ptr<Expr> target;                ///< 迭代变量
    std::unique_ptr<Expr> iter;                  ///< 迭代对象
    std::vector<std::unique_ptr<Stmt>> body;    ///< 循环体语句列表
    std::vector<std::unique_ptr<Stmt>> orelse;  ///< else 子句语句列表
    
    /**
     * @brief 默认构造函数
     */
    For() : Stmt(ASTNodeType::For), target(nullptr), iter(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class AsyncFor
 * @brief 异步 for 循环语句节点
 *
 * 对应 CPython 的 ast.AsyncFor 节点。
 * 用于异步迭代：async for target in aiter: body [else: orelse]
 *
 * @section usage 使用示例
 * @code
 * // async for item in async_generator():
 * //     await process(item)
 * auto async_for = std::make_unique<AsyncFor>();
 * async_for->target = std::make_unique<Name>("item", Name::Store);
 * async_for->iter = std::make_unique<Name>("async_generator", Name::Load);
 * @endcode
 *
 * @note 异步 for 循环内部可以使用 await 表达式
 */
class AsyncFor : public For {
public:
    /**
     * @brief 默认构造函数
     */
    AsyncFor() { type = ASTNodeType::AsyncFor; }
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class While
 * @brief while 循环语句节点
 *
 * 对应 CPython 的 ast.While 节点。
 * 用于条件循环：while test: body [else: orelse]
 *
 * @section usage 使用示例
 * @code
 * // while n > 0:
 * //     n -= 1
 * auto while_loop = std::make_unique<While>();
 * while_loop->test = std::make_unique<Compare>(
 *     std::make_unique<Name>("n", Name::Load),
 *     {ASTNodeType::Gt},
 *     {std::make_unique<Constant>(pos, 0)}
 * );
 * // while_loop->body = ... 循环体 ...;
 * @endcode
 */
class While : public Stmt {
public:
    std::unique_ptr<Expr> test;                  ///< 循环条件表达式
    std::vector<std::unique_ptr<Stmt>> body;    ///< 循环体语句列表
    std::vector<std::unique_ptr<Stmt>> orelse;  ///< else 子句语句列表
    
    /**
     * @brief 默认构造函数
     */
    While() : Stmt(ASTNodeType::While), test(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class If
 * @brief if 条件语句节点
 *
 * 对应 CPython 的 ast.If 节点。
 * 用于条件分支：if test1: body1 [elif test2: body2] ... [else: orelse]
 *
 * @section usage 使用示例
 * @code
 * // if x > 0:
 * //     print("positive")
 * // elif x < 0:
 * //     print("negative")
 * // else:
 * //     print("zero")
 * auto if_stmt = std::make_unique<If>();
 * if_stmt->test = std::make_unique<Compare>(
 *     std::make_unique<Name>("x", Name::Load),
 *     {ASTNodeType::Gt},
 *     {std::make_unique<Constant>(pos, 0)}
 * );
 * // if_stmt->body = ... if 分支 ...;
 * @endcode
 */
class If : public Stmt {
public:
    std::unique_ptr<Expr> test;                 ///< 条件表达式
    std::vector<std::unique_ptr<Stmt>> body;    ///< if 分支语句列表
    std::vector<std::unique_ptr<Stmt>> orelse; ///< else 分支语句列表（包含 elif）
    
    /**
     * @brief 默认构造函数
     */
    If() : Stmt(ASTNodeType::If), test(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 上下文管理语句
// ============================================================================

/**
 * @class With
 * @brief with 上下文管理语句节点
 *
 * 对应 CPython 的 ast.With 节点。
 * 用于上下文管理：with expression [as target]: body
 *
 * @section usage 使用示例
 * @code
 * // with open("file.txt") as f:
 * //     f.read()
 * auto with_stmt = std::make_unique<With>();
 * with_stmt->items.push_back( ... withitem 表达式 ...);
 * // with_stmt->body = ... 语句块 ...;
 * @endcode
 *
 * @note with 语句确保资源在使用后被正确清理
 */
class With : public Stmt {
public:
    std::vector<std::unique_ptr<withitem>> items; ///< 上下文项列表
    std::vector<std::unique_ptr<Stmt>> body; ///< with 块语句列表
    
    /**
     * @brief 默认构造函数
     */
    With() : Stmt(ASTNodeType::With) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class AsyncWith
 * @brief 异步 with 上下文管理语句节点
 *
 * 对应 CPython 的 ast.AsyncWith 节点。
 * 用于异步上下文管理：async with expression [as target]: body
 *
 * @section usage 使用示例
 * @code
 * // async with async_session() as session:
 * //     await session.get_data()
 * auto async_with = std::make_unique<AsyncWith>();
 * @endcode
 *
 * @note 异步 with 语句内部可以使用 await 表达式
 */
class AsyncWith : public With {
public:
    /**
     * @brief 默认构造函数
     */
    AsyncWith() { type = ASTNodeType::AsyncWith; }
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 异常处理语句
// ============================================================================

/**
 * @class Raise
 * @brief raise 异常抛出语句节点
 *
 * 对应 CPython 的 ast.Raise 节点。
 * 用于抛出异常：raise exception [from cause]
 *
 * @section usage 使用示例
 * @code
 * // raise ValueError("invalid input")
 * auto raise_stmt = std::make_unique<Raise>();
 * raise_stmt->exc = std::make_unique<Call>(
 *     std::make_unique<Name>("ValueError", Name::Load)
 * );
 * raise_stmt->exc->args.push_back(std::make_unique<Constant>(pos, "invalid input"));
 *
 * // raise exception from cause
 * auto chained = std::make_unique<Raise>();
 * chained->exc = ... exception ...;
 * chained->cause = ... cause ...;
 * @endcode
 */
class Raise : public Stmt {
public:
    std::unique_ptr<Expr> exc;  ///< 异常表达式
    std::unique_ptr<Expr> cause; ///< 异常原因表达式（可选）
    
    /**
     * @brief 默认构造函数
     */
    Raise() : Stmt(ASTNodeType::Raise), exc(nullptr), cause(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Try
 * @brief try 异常处理语句节点
 *
 * 对应 CPython 的 ast.Try 节点。
 * 用于异常捕获：try: body except [type]: handler [else: orelse] [finally: finalbody]
 *
 * @section usage 使用示例
 * @code
 * // try:
 * //     risky_operation()
 * // except ValueError:
 * //     handle_error()
 * // else:
 * //     success()
 * // finally:
 * //     cleanup()
 * auto try_stmt = std::make_unique<Try>();
 * // try_stmt->body = ... try 块 ...;
 * // try_stmt->handlers = ... except 处理器 ...;
 * // try_stmt->orelse = ... else 块 ...;
 * // try_stmt->finalbody = ... finally 块 ...;
 * @endcode
 */
class Try : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> body;       ///< try 块语句列表
    std::vector<std::unique_ptr<except_handler>> handlers; ///< except 处理器列表
    std::vector<std::unique_ptr<Stmt>> orelse;     ///< else 块语句列表
    std::vector<std::unique_ptr<Stmt>> finalbody; ///< finally 块语句列表
    
    /**
     * @brief 默认构造函数
     */
    Try() : Stmt(ASTNodeType::Try) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Assert
 * @brief assert 断言语句节点
 *
 * 对应 CPython 的 ast.Assert 节点。
 * 用于调试断言：assert test [, msg]
 *
 * @section usage 使用示例
 * @code
 * // assert x > 0
 * auto assert_stmt = std::make_unique<Assert>();
 * assert_stmt->test = std::make_unique<Compare>(
 *     std::make_unique<Name>("x", Name::Load),
 *     {ASTNodeType::Gt},
 *     {std::make_unique<Constant>(pos, 0)}
 * );
 *
 * // assert condition, "error message"
 * auto with_msg = std::make_unique<Assert>();
 * with_msg->test = ... condition ...;
 * with_msg->msg = std::make_unique<Constant>(pos, "error message");
 * @endcode
 *
 * @note 断言在调试模式下检查条件，发布模式下可能被优化掉
 */
class Assert : public Stmt {
public:
    std::unique_ptr<Expr> test; ///< 断言条件表达式
    std::unique_ptr<Expr> msg;  ///< 可选的断言消息表达式
    
    /**
     * @brief 默认构造函数
     */
    Assert() : Stmt(ASTNodeType::Assert), test(nullptr), msg(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 变量声明语句
// ============================================================================

/**
 * @class Global
 * @brief global 全局变量声明语句节点
 *
 * 对应 CPython 的 ast.Global 节点。
 * 用于声明全局变量：global name1, name2, ...
 *
 * @section usage 使用示例
 * @code
 * // global counter
 * auto global_stmt = std::make_unique<Global>();
 * global_stmt->names.push_back("counter");
 * @endcode
 *
 * @note global 声明必须在函数或方法内部使用
 */
class Global : public Stmt {
public:
    std::vector<std::string> names; ///< 变量名列表
    
    /**
     * @brief 默认构造函数
     */
    Global() : Stmt(ASTNodeType::Global) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Nonlocal
 * @brief nonlocal 非局部变量声明语句节点
 *
 * 对应 CPython 的 ast.Nonlocal 节点。
 * 用于声明非局部变量：nonlocal name1, name2, ...
 *
 * @section usage 使用示例
 * @code
 * // nonlocal counter
 * auto nonlocal_stmt = std::make_unique<Nonlocal>();
 * nonlocal_stmt->names.push_back("counter");
 * @endcode
 *
 * @note nonlocal 声明必须在嵌套函数内部使用，用于访问外层函数的变量
 */
class Nonlocal : public Stmt {
public:
    std::vector<std::string> names; ///< 变量名列表
    
    /**
     * @brief 默认构造函数
     */
    Nonlocal() : Stmt(ASTNodeType::Nonlocal) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Alias
 * @brief 导入别名节点
 *
 * 对应 CPython 的 ast.alias 节点。
 * 表示导入时的别名关系：name as alias_name
 */
class Alias : public Expr {
public:
    std::string asname;                        ///< 别名（如 "np"）
    std::unique_ptr<Expr> name;                ///< 原始名称（如 "numpy"）
    
    /**
     * @brief 默认构造函数
     */
    Alias() : Expr(ASTNodeType::Alias), asname() {}  // 复用 Name 类型或添加 Alias 类型
    
    /**
     * @brief 构造函数
     * @param alias 别名
     * @param original 原始名称表达式
     */
    Alias(const std::string& alias, std::unique_ptr<Expr> original)
        : Expr(ASTNodeType::Alias), asname(alias), name(std::move(original)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};
// ============================================================================
// 导入语句
// ============================================================================

/**
 * @class Import
 * @brief import 导入语句节点
 *
 * 对应 CPython 的 ast.Import 节点。
 * 用于导入模块：import module_name [as alias], ...
 *
 * @section usage 使用示例
 * @code
 * // import os
 * auto import1 = std::make_unique<Import>();
 * import1->names.push_back(std::make_unique<Name>("os", Name::Load));
 *
 * // import numpy as np
 * auto import2 = std::make_unique<Import>();
 * import2->names.push_back(
 *     std::make_unique<Alias>("np", std::make_unique<Name>("numpy", Name::Load))
 * );
 *
 * // import os, sys
 * auto import3 = std::make_unique<Import>();
 * import3->names.push_back(std::make_unique<Name>("os", Name::Load));
 * import3->names.push_back(std::make_unique<Name>("sys", Name::Load));
 * @endcode
 */
class Import : public Stmt {
public:
    std::vector<std::unique_ptr<Expr>> names;  ///< 导入的模块/别名列表（使用 Name 或 Alias）
    
    /**
     * @brief 默认构造函数
     */
    Import() : Stmt(ASTNodeType::Import) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class ImportFrom
 * @brief from...import 导入语句节点
 *
 * 对应 CPython 的 ast.ImportFrom 节点。
 * 用于从模块导入特定名称：from module import name [as alias], ...
 *
 * @section usage 使用示例
 * @code
 * // from os import path
 * auto from1 = std::make_unique<ImportFrom>();
 * from1->module = std::make_unique<Name>("os", Name::Load);
 * from1->names.push_back(std::make_unique<Name>("path", Name::Load));
 *
 * // from math import sqrt as s
 * auto from2 = std::make_unique<ImportFrom>();
 * from2->module = std::make_unique<Name>("math", Name::Load);
 * from2->names.push_back(
 *     std::make_unique<Alias>("s", std::make_unique<Name>("sqrt", Name::Load))
 * );
 *
 * // from . import module (相对导入，level=1)
 * auto from3 = std::make_unique<ImportFrom>();
 * from3->level = 1;
 * from3->module = nullptr;  // 相对导入无模块名
 * from3->names.push_back(std::make_unique<Name>("module", Name::Load));
 * @endcode
 *
 * @note level 表示相对导入层级：0=绝对导入, 1=当前目录, 2=父目录, ...
 */
class ImportFrom : public Stmt {
public:
    std::unique_ptr<Expr> module;               ///< 源模块名（相对导入时可为空）
    std::vector<std::unique_ptr<Expr>> names;  ///< 导入的名称/别名列表
    int level;                                  ///< 相对导入层级（0表示绝对导入）
    
    /**
     * @brief 默认构造函数
     */
    ImportFrom() : Stmt(ASTNodeType::ImportFrom), level(0) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 简单语句
// ============================================================================

/**
 * @class ExprStmt
 * @brief 表达式语句节点
 *
 * 对应 CPython 的 ast.Expr 节点。
 * 当一个表达式单独作为语句时使用，如函数调用、打印语句等。
 *
 * @section usage 使用示例
 * @code
 * // print("hello")
 * auto expr_stmt = std::make_unique<ExprStmt>();
 * expr_stmt->value = std::make_unique<Call>(
 *     std::make_unique<Name>("print", Name::Load)
 * );
 * expr_stmt->value->args.push_back(std::make_unique<Constant>(pos, "hello"));
 * @endcode
 */
class ExprStmt : public Stmt {
public:
    std::unique_ptr<Expr> value; ///< 表达式
    
    /**
     * @brief 默认构造函数
     */
    ExprStmt() : Stmt(ASTNodeType::ExprStmt), value(nullptr) {}
    
    /**
     * @brief 构造函数
     * @param val 表达式
     */
    ExprStmt(std::unique_ptr<Expr> val) : Stmt(ASTNodeType::ExprStmt), value(std::move(val)) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Pass
 * @brief pass 空语句节点
 *
 * 对应 CPython 的 ast.Pass 节点。
 * 作为占位语句，什么都不做。
 *
 * @section usage 使用示例
 * @code
 * // if condition:
 * //     pass
 * auto pass_stmt = std::make_unique<Pass>();
 * @endcode
 *
 * @note pass 常用于语法需要但逻辑上什么都不做的场景
 */
class Pass : public Stmt {
public:
    /**
     * @brief 默认构造函数
     */
    Pass() : Stmt(ASTNodeType::Pass) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Break
 * @brief break 中断循环语句节点
 *
 * 对应 CPython 的 ast.Break 节点。
 * 用于立即退出循环（for 或 while）。
 *
 * @section usage 使用示例
 * @code
 * // for i in range(100):
 * //     if i == 10:
 * //         break
 * auto break_stmt = std::make_unique<Break>();
 * @endcode
 *
 * @note break 语句只能在循环内部使用
 */
class Break : public Stmt {
public:
    /**
     * @brief 默认构造函数
     */
    Break() : Stmt(ASTNodeType::Break) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class Continue
 * @brief continue 继续循环语句节点
 *
 * 对应 CPython 的 ast.Continue 节点。
 * 用于立即进入下一次循环迭代。
 *
 * @section usage 使用示例
 * @code
 * // for i in range(10):
 * //     if i % 2 == 0:
 * //         continue
 * //     print(i)
 * auto continue_stmt = std::make_unique<Continue>();
 * @endcode
 *
 * @note continue 语句只能在循环内部使用
 */
class Continue : public Stmt {
public:
    /**
     * @brief 默认构造函数
     */
    Continue() : Stmt(ASTNodeType::Continue) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 模块和参数节点
// ============================================================================

/**
 * @class arguments
 * @brief 函数参数列表节点
 *
 * 对应 CPython 的 ast.arguments 节点。
 * 表示函数的完整参数列表，包括位置参数、关键字参数、*args 和 **kwargs。
 *
 * @section usage 使用示例
 * @code
 * // def func(a, b=1, *args, c, **kwargs):
 * //     pass
 * auto args = std::make_unique<arguments>();
 * // 设置各种参数...
 * @endcode
 */
class arguments : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> args;      ///< 位置参数列表
    std::unique_ptr<ASTNode> vararg;                ///< 可变位置参数 (*args)
    std::vector<std::unique_ptr<ASTNode>> kwonlyargs; ///< 仅关键字参数列表
    std::vector<std::unique_ptr<Expr>> kw_defaults;  ///< 仅关键字参数默认值列表
    std::unique_ptr<ASTNode> kwarg;                 ///< 可变关键字参数 (**kwargs)
    std::vector<std::unique_ptr<Expr>> defaults;     ///< 位置参数默认值列表
    
    /**
     * @brief 默认构造函数
     */
    arguments() : ASTNode(ASTNodeType::arguments) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class arg
 * @brief 单个函数参数节点
 *
 * 对应 CPython 的 ast.arg 节点。
 * 表示函数的一个参数，包括参数名和可选的类型注解。
 *
 * @section usage 使用示例
 * @code
 * // def func(x: int):
 * //     pass
 * auto parameter = std::make_unique<arg>("x");
 * parameter->annotation = std::make_unique<Name>("int", Name::Load);
 * @endcode
 */
class arg : public ASTNode {
public:
    std::string name;                      ///< 参数名称
    std::unique_ptr<Expr> annotation;    ///< 类型注解表达式（可选）
    
    /**
     * @brief 默认构造函数
     */
    arg() : ASTNode(ASTNodeType::arg), name() {}
    
    /**
     * @brief 构造函数
     * @param arg 参数名
     */
    arg(const std::string& arg) : ASTNode(ASTNodeType::arg), name(arg) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class except_handler
 * @brief except 异常处理器节点
 *
 * 对应 CPython 的 ast.except_handler 节点。
 * 表示 except 子句：except [type [as name]]: body
 *
 * @section usage 使用示例
 * @code
 * // except ValueError as e:
 * //     print(e)
 * auto handler = std::make_unique<except_handler>();
 * handler->type = std::make_unique<Name>("ValueError", Name::Load);
 * handler->name = "e";
 * // handler->body = ... 处理语句 ...;
 * @endcode
 */
class except_handler : public ASTNode {
public:
    std::unique_ptr<Expr> type;     ///< 异常类型表达式（可选）
    std::string name;                ///< 异常变量名（as 子句）
    std::vector<std::unique_ptr<Stmt>> body; ///< 处理块语句列表
    
    /**
     * @brief 默认构造函数
     */
    except_handler() : ASTNode(ASTNodeType::except_handler), type(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class withitem
 * @brief with 语句项节点
 *
 * 对应 CPython 的 ast.withitem 节点。
 * 表示 with 语句中的一个上下文项：context_expr [as optional_vars]
 *
 * @section usage 使用示例
 * @code
 * // with open("file.txt") as f:
 * //     ...
 * auto item = std::make_unique<withitem>();
 * item->context_expr = ... open("file.txt") ...;
 * item->optional_vars = ... f ...;
 * @endcode
 */
class withitem : public ASTNode {
public:
    std::unique_ptr<Expr> context_expr; ///< 上下文表达式（如 open("file.txt")）
    std::unique_ptr<Expr> optional_vars; ///< 可选的目标变量（如 f）
    
    /**
     * @brief 默认构造函数
     */
    withitem() : ASTNode(ASTNodeType::withitem), context_expr(nullptr) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

/**
 * @class comprehension
 * @brief 推导式生成器节点
 *
 * 对应 CPython 的 ast.comprehension 节点。
 * 表示推导式中的一个 for 子句：for target in iter [if condition]
 *
 * @section usage 使用示例
 * @code
 * // for x in items if x > 0
 * auto comp = std::make_unique<comprehension>();
 * comp->target = std::make_unique<Name>("x", Name::Store);
 * comp->iter = std::make_unique<Name>("items", Name::Load);
 * comp->ifs.push_back(... x > 0 条件 ...);
 * @endcode
 */
class comprehension : public Expr {
public:
    std::unique_ptr<Expr> target; ///< 迭代变量（如 x in ... 中的 x）
    std::unique_ptr<Expr> iter;   ///< 迭代对象（如 items）
    std::vector<std::unique_ptr<Expr>> ifs; ///< 过滤条件列表
    bool is_async;               ///< 是否为 async for
    
    /**
     * @brief 默认构造函数
     */
    comprehension() : Expr(ASTNodeType::comprehension), 
                      target(nullptr), iter(nullptr), is_async(false) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

// ============================================================================
// 模块节点
// ============================================================================

/**
 * @class Module
 * @brief 模块节点（AST 根节点）
 *
 * 对应 CPython 的 ast.Module 节点。
 * 表示整个源代码模块，是 AST 的根节点。
 *
 * @section usage 使用示例
 * @code
 * // 解析整个源代码文件
 * Parser parser("x = 1\nprint(x)");
 * auto module = parser.parseModule();
 *
 * // 遍历模块中的语句
 * for (const auto& stmt : module->body) {
 *     // 处理每个语句...
 * }
 * @endcode
 */
class Module : public ASTNode {
public:
    std::vector<std::unique_ptr<Stmt>> body;        ///< 模块体语句列表
    std::vector<std::unique_ptr<Expr>> type_ignores; ///< 类型忽略注释列表（type: ignore）
    
    /**
     * @brief 默认构造函数
     */
    Module() : ASTNode(ASTNodeType::Module) {}
    
    /**
     * @brief 获取节点的字符串表示
     * @return 格式化的字符串
     */
    std::string toString() const override;
};

}  // namespace csgo

#endif  // CSGO_AST_NODE_H