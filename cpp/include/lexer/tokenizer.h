/**
 * @file tokenizer.h
 * @brief CSGO 编程语言词法分析器头文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件定义了 CSGO 语言的词法分析器（Tokenizer），负责将源代码字符串
 * 转换为 token 序列。实现了完整的词法分析功能，包括：
 * - 基本词法单元（标识符、数字、字符串、操作符）
 * - Python 风格字符串（前缀 r, b, f 及其组合）
 * - f-string 表达式插值
 * - Python/CSGO 关键字识别
 * - 缩进敏感语法（Indent/Dedent）
 * - 行继续符处理
 *
 * @section dependencies 依赖
 * - C++17 标准库
 * - 标准库组件：string, string_view, vector, variant 等
 */

#ifndef CSGO_TOKENIZER_H
#define CSGO_TOKENIZER_H

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <cstdint>
#include <fstream>
#include <iostream>

namespace csgo {

/**
 * @enum TokenType
 * @brief Token 类型枚举，定义所有可能的词法单元类型
 *
 * CSGO 语言的 token 类型包括：
 * - 基础类型：标识符、数字、字符串、换行
 * - 操作符：算术、逻辑、位运算、比较
 * - Python 关键字：async/await、class、def、import 等
 * - CSGO 扩展关键字：go、chan、select、defer 等
 * - 字符串变体：RawString、Bytes、FString 及其子类型
 */
enum class TokenType : uint8_t {
    EndMarker,      ///< 文件结束标记
    Name,           ///< 标识符（变量名、函数名等）
    Number,         ///< 数字字面量（整数、浮点数）
    String,         ///< 普通字符串字面量
    NewLine,        ///< 换行符
    Indent,         ///< 缩进增加（Python 风格块开始）
    Dedent,         ///< 缩进减少（Python 风格块结束）
    LeftParen,      ///< 左圆括号 (
    RightParen,     ///< 右圆括号 )
    LeftBracket,    ///< 左方括号 [
    RightBracket,   ///< 右方括号 ]
    Colon,          ///< 冒号 :
    Comma,          ///< 逗号 ,
    Semi,           ///< 分号 ;
    Plus,           ///< 加号 +
    Minus,          ///< 减号 -
    Star,           ///< 星号 *
    Slash,          ///< 斜杠 /
    VBar,           ///< 竖线 |
    Amper,          ///< 按位与 &
    Less,           ///< 小于 <
    Greater,        ///< 大于 >
    Equal,          ///< 等于 =
    Dot,            ///< 句点 .
    Percent,        ///< 百分号 %
    LeftBrace,      ///< 左花括号 {
    RightBrace,     ///< 右花括号 }
    EqEqual,        ///< 等于比较 ==
    NotEqual,       ///< 不等于 !=
    LessEqual,      ///< 小于等于 <=
    GreaterEqual,   ///< 大于等于 >=
    Tilde,          ///< 按位取反 ~
    Circumflex,     ///< 按位异或 ^
    LeftShift,      ///< 左移位 <<
    RightShift,     ///< 右移位 >>
    DoubleStar,     ///< 乘方 **
    PlusEqual,      ///< 加法赋值 +=
    MinusEqual,     ///< 减法赋值 -=
    StarEqual,      ///< 乘法赋值 *=
    SlashEqual,     ///< 除法赋值 /=
    PercentEqual,   ///< 取模赋值 %=
    AmperEqual,     ///< 按位与赋值 &=
    VBarEqual,      ///< 按位或赋值 |=
    CircumflexEqual,///< 按位异或赋值 ^=
    LeftShiftEqual, ///< 左移位赋值 <<=
    RightShiftEqual,///< 右移位赋值 >>=
    DoubleStarEqual,///< 乘方赋值 **=
    DoubleSlash,    ///< 整除 //
    DoubleSlashEqual,///< 整除赋值 //=
    At,             ///< @ 符号
    AtEqual,        ///< @= 符号
    Arrow,          ///< 箭头 ->
    Ellipsis,       ///< 省略号 ...
    ColonEqual,     ///< 海象运算符 :=
    Await,          ///< await 关键字
    Async,          ///< async 关键字
    TypeIgnore,     ///< 类型忽略注释
    TypeComment,    ///< 类型注释

    // 字符串类型变体
    RawString,      ///< 原始字符串 r"..."
    Bytes,          ///< 字节串 b"..."
    FString,        ///< f-string 开始标记
    FStringText,    ///< f-string 中的文本部分
    FStringExpr,    ///< f-string 表达式开始标记 {expr}
    FStringConv,    ///< f-string 转换标记 !r !s !a
    FStringSpec,    ///< f-string 格式说明符
    FStringEnd,     ///< f-string 结束标记

    Error,          ///< 词法错误标记

    // 字面量关键字
    False_,         ///< False 布尔假值
    None_,          ///< None 空值
    True_,          ///< True 布尔真值

    // 逻辑运算符
    And,            ///< 逻辑与 and
    Or,             ///< 逻辑或 or
    Not,            ///< 逻辑非 not

    // Python 关键字
    As,             ///< as 别名定义
    Assert,         ///< assert 断言
    Break,          ///< break 跳出循环
    Class,          ///< class 类定义
    Continue,       ///< continue 继续循环
    Def,            ///< def 函数定义
    Del,            ///< del 删除对象
    Elif,           ///< elif 条件分支
    Else,           ///< else 条件分支
    Except,         ///< except 异常处理
    Finally,        ///< finally 异常处理
    For,            ///< for 循环
    From,           ///< from 导入
    Global,         ///< global 全局变量
    If,             ///< if 条件
    Import,         ///< import 导入模块
    In,             ///< in 成员测试
    Is,             ///< is 身份测试
    Lambda,         ///< lambda 匿名函数
    Nonlocal,       ///< nonlocal 非局部变量
    Pass,           ///< pass 空语句
    Raise,          ///< raise 抛出异常
    Return,         ///< return 返回值
    Try,            ///< try 异常捕获
    While,          ///< while 循环
    With,           ///< with 上下文管理
    Yield,          ///< yield 生成器

    // CSGO 语言扩展关键字
    Match,          ///< match 模式匹配
    Case,           ///< case 分支情况
    Type,           ///< type 类型定义
    Go,             ///< go 启动协程
    Chan,           ///< chan 通道类型
    Select,         ///< select 多路复用
    Defer,          ///< defer 延迟执行
    Const,          ///< const 常量声明
    Let,            ///< let 变量声明
    Mut,            ///< mut 可变声明
    Struct,         ///< struct 结构体定义
    Impl,           ///< impl 接口实现
    Pub,            ///< pub 公开访问修饰符
    Use,            ///< use 模块引入
    Mod,            ///< mod 模块定义
};

/**
 * @struct Token
 * @brief Token 结构体，表示一个词法单元
 *
 * Token 是词法分析的最小单位，包含：
 * - 类型信息（type）
 * - 文本内容（text）- 使用 string_view 实现零拷贝
 * - 解析后的值（value）- 支持多种类型
 * - 位置信息（line, column）- 用于错误报告和调试
 */
struct Token {
    TokenType type;                           ///< Token 的类型
    std::string_view text;                    ///< 指向源字符串的视图（零拷贝）
    
    /**
     * @brief Token 的解析值
     *
     * 使用 variant 存储不同类型的值：
     * - std::monostate: 无值（如操作符）
     * - int64_t: 整数字面量
     * - double: 浮点数字面量
     * - std::string: 字符串值
     * - std::vector<uint8_t>: 字节串值
     */
    std::variant<
        std::monostate,
        int64_t,
        double,
        std::string,
        std::vector<uint8_t>
    > value;
    
    size_t line;                              ///< 所在行号（从 1 开始）
    size_t column;                            ///< 所在列号（从 1 开始）

    /**
     * @brief 检查 Token 是否为指定类型
     * @param t 要检查的 TokenType
     * @return true 如果类型匹配，false 否则
     */
    bool is(TokenType t) const { return type == t; }

    /**
     * @brief 将 Token 转换为字符串表示
     * @return 格式化的字符串，包含类型、位置和文本内容
     */
    std::string toString() const;
};

/**
 * @class Tokenizer
 * @brief CSGO 语言词法分析器类
 *
 * 词法分析器负责将源代码字符串转换为 Token 序列。
 * 采用 RAII 模式管理资源，支持从字符串或流构造。
 *
 * @section features 功能特性
 * - 支持完整的 Python 风格语法
 * - 支持 CSGO 语言扩展（协程、通道等）
 * - 完善的错误处理和报告
 * - f-string 表达式解析
 * - Python 风格缩进处理
 *
 * @section usage 使用示例
 * @code
 * // 从字符串构造
 * csgo::Tokenizer tokenizer("x = 42");
 * auto tokens = tokenizer.tokenize();
 *
 * // 逐个获取 Token
 * csgo::Tokenizer tokenizer("print('hello')");
 * while (!tokenizer.isAtEnd()) {
 *     csgo::Token token = tokenizer.nextToken();
 *     // 处理 token...
 * }
 * @endcode
 */
class Tokenizer {
public:
    /**
     * @brief 从字符串构造 Tokenizer
     * @param source 源代码字符串（将被移动）
     */
    explicit Tokenizer(std::string source);

    /**
     * @brief 从输入流构造 Tokenizer
     * @param input 输入流对象
     * @param filename 文件名（用于错误报告，默认 "<input>"）
     */
    explicit Tokenizer(std::istream& input, std::string filename = "<input>");

    // 禁用拷贝语义，支持移动语义
    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    Tokenizer(Tokenizer&&) noexcept = default;
    Tokenizer& operator=(Tokenizer&&) noexcept = default;

    /**
     * @brief 默认析构函数
     */
    ~Tokenizer() = default;

    /**
     * @brief 获取下一个 Token
     * @return 解析得到的 Token
     * @note 对于多字符操作符、字符串等会进行完整解析
     */
    Token nextToken();

    /**
     * @brief 对整个输入进行词法分析
     * @return Token 向量，包含所有解析结果
     * @note 结果以 EndMarker 结束
     */
    std::vector<Token> tokenize();

    /**
     * @brief 检查是否到达输入末尾
     * @return true 如果已到达末尾，false 否则
     */
    bool isAtEnd() const;

    /**
     * @brief 检查是否发生错误
     * @return true 如果有错误发生，false 否则
     */
    bool hasError() const;

    /**
     * @brief 获取错误消息
     * @return 错误消息字符串
     */
    const std::string& errorMessage() const;

    /**
     * @brief 设置 Tab 宽度
     * @param size Tab 字符对应的空格数（默认 8）
     */
    void setTabSize(size_t size) { tabSize_ = size; }

    /**
     * @brief 启用/禁用类型注释
     * @param enable true 启用，false 禁用
     */
    void enableTypeComments(bool enable) { typeComments_ = enable; }

private:
    // ===== 源数据管理 =====
    std::string source_;        ///< 源字符串（拥有所有权）
    std::string filename_;       ///< 文件名（用于错误报告）
    std::string_view view_;      ///< 当前处理视图

    // ===== 位置状态 =====
    size_t pos_ = 0;            ///< 当前字符位置（字节偏移）
    size_t line_ = 1;           ///< 当前行号
    size_t column_ = 1;         ///< 当前列号
    size_t lineStart_ = 0;      ///< 当前行起始位置

    // ===== 缩进状态管理 =====
    std::vector<size_t> indentStack_{0};   ///< 缩进栈，记录各级缩进
    std::vector<Token> pendingIndents_;    ///< 待处理的缩进/dedent Token

    // ===== 括号嵌套追踪 =====
    std::vector<char> parenStack_;         ///< 括号类型栈

    // ===== 配置选项 =====
    size_t tabSize_ = 8;            ///< Tab 宽度
    bool typeComments_ = false;    ///< 是否启用类型注释

    // ===== 错误状态 =====
    bool hasError_ = false;         ///< 是否有错误发生
    std::string errorMessage_;      ///< 错误消息

    // ===== 字符读取辅助方法 =====
    /**
     * @brief 查看当前字符（不消耗）
     * @return 当前字符，到末尾返回 '\0'
     */
    char peek() const;

    /**
     * @brief 查看指定偏移的字符（不消耗）
     * @param offset 偏移量（相对于当前位置）
     * @return 指定位置的字符，到末尾返回 '\0'
     */
    char peek(size_t offset) const;

    /**
     * @brief 读取并推进当前位置
     * @return 读取的字符
     * @note 会自动更新行列号
     */
    char advance();

    /**
     * @brief 尝试匹配并消费指定字符
     * @param expected 期望的字符
     * @return true 如果匹配成功并消费，false 否则
     */
    bool match(char expected);

    // ===== 空白和注释处理 =====
    /**
     * @brief 跳过空白字符（不包括换行）
     */
    void skipWhitespace();

    /**
     * @brief 跳过注释（从 # 到行尾）
     */
    void skipComment();

    // ===== Token 工厂方法 =====
    /**
     * @brief 创建无值 Token
     * @param type Token 类型
     * @param start 起始位置
     * @param length 长度
     * @return 构造的 Token
     */
    Token makeToken(TokenType type, size_t start, size_t length);

    /**
     * @brief 创建带值 Token
     * @param type Token 类型
     * @param start 起始位置
     * @param length 长度
     * @param value 要存储的值
     * @return 构造的 Token
     */
    Token makeToken(TokenType type, size_t start, size_t length,
                    std::variant<int64_t, double, std::string, std::vector<uint8_t>> value);

    // ===== 词法扫描方法 =====
    /**
     * @brief 扫描标识符
     * @return 标识符 Token
     */
    Token scanIdentifier();

    /**
     * @brief 扫描数字字面量
     * @return 数字 Token
     */
    Token scanNumber();

    /**
     * @brief 扫描字符串字面量
     * @return 字符串 Token（可能为 String、RawString、Bytes 或 FString）
     */
    Token scanString();

    /**
     * @brief 扫描原始字符串 r"..."
     * @return RawString Token
     * @deprecated 由 scanString 统一处理，此函数保留用于兼容
     */
    Token scanRawString();

    /**
     * @brief 扫描字节串 b"..."
     * @return Bytes Token
     * @deprecated 由 scanString 统一处理，此函数保留用于兼容
     */
    Token scanBytes();

    // ===== f-string 解析方法 =====
    /**
     * @brief 扫描 f-string 主入口
     * @return FString Token
     */
    Token scanFString();

    /**
     * @brief 扫描 f-string 文本部分
     * @return FStringText Token
     */
    Token scanFStringText();

    /**
     * @brief 扫描 f-string 表达式
     * @return FStringExpr Token
     */
    Token scanFStringExpression();

    /**
     * @brief 扫描 f-string 转换标记
     * @return FStringConv Token
     */
    Token scanFStringConversion();

    /**
     * @brief 扫描 f-string 格式说明符
     * @return FStringSpec Token
     */
    Token scanFStringFormatSpec();

    /**
     * @brief f-string 解析状态
     */
    struct FStringState {
        /**
         * @brief 引号类型枚举
         */
        enum QuoteType { SINGLE, DOUBLE, TRIPLE_SINGLE, TRIPLE_DOUBLE };
        
        QuoteType quoteType;       ///< 引号类型
        bool isRaw;                ///< 是否为原始 f-string
        int nestedBraceLevel;      ///< 花括号嵌套深度
        size_t startPos;           ///< f-string 开始位置
        size_t startLine;          ///< f-string 开始行
        size_t startColumn;        ///< f-string 开始列
    };

    std::optional<FStringState> fstringState_;   ///< 当前 f-string 状态

    /**
     * @brief 扫描操作符和分隔符
     * @return 对应的 Token
     */
    Token scanOperator();

    // ===== 缩进处理 =====
    /**
     * @brief 处理缩进变化（Indent/Dedent）
     * @return Indent 或 Dedent Token
     */
    Token handleIndentation();

    // ===== 错误处理 =====
    /**
     * @brief 创建错误 Token
     * @param message 错误消息
     * @return Error Token
     */
    Token error(const std::string& message);

    /**
     * @brief 报告错误
     * @param message 错误消息
     */
    void reportError(const std::string& message);

    // ===== 字符分类工具方法 =====
    /**
     * @brief 检查是否为标识符起始字符
     */
    static bool isIdentifierStart(char c);

    /**
     * @brief 检查是否为标识符字符
     */
    static bool isIdentifierChar(char c);

    /**
     * @brief 检查是否为十进制数字
     */
    static bool isDigit(char c);

    /**
     * @brief 检查是否为十六进制数字
     */
    static bool isHexDigit(char c);

    /**
     * @brief 检查是否为八进制数字
     */
    static bool isOctDigit(char c);

    /**
     * @brief 检查是否为二进制数字
     */
    static bool isBinDigit(char c);

    /**
     * @brief 检查是否为关键字
     * @param text 要检查的文本
     * @return 匹配的 TokenType 或 Name
     */
    static TokenType checkKeyword(std::string_view text);
};

/**
 * @brief 获取 TokenType 的字符串名称
 * @param type Token 类型
 * @return 类型名称字符串
 */
const char* tokenTypeName(TokenType type);

/**
 * @brief Token 输出流重载
 * @param os 输出流
 * @param token 要输出的 Token
 * @return 输出流引用
 */
std::ostream& operator<<(std::ostream& os, const Token& token);

/**
 * @brief 将十六进制字符转换为数值
 * @param c 十六进制字符（0-9, a-f, A-F）
 * @return 对应的数值（0-15），无效字符返回 0
 */
static uint8_t hexToDigit(char c);

/**
 * @brief 将八进制字符转换为数值
 * @param c 八进制字符（0-7）
 * @return 对应的数值（0-7），无效字符返回 0
 */
static uint8_t octToDigit(char c);

} // namespace csgo

#endif // CSGO_TOKENIZER_H