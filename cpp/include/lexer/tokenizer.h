// tokenizer.h
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

// Token 类型枚举（强类型）
enum class TokenType : uint8_t {
    EndMarker,      // 文件结束
    Name,           // 标识符
    Number,         // 数字
    String,         // 字符串
    NewLine,        // 换行
    Indent,         // 缩进
    Dedent,         // 取消缩进
    LeftParen,      // (
    RightParen,     // )
    LeftBracket,    // [
    RightBracket,   // ]
    Colon,          // :
    Comma,          // ,
    Semi,           // ;
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    VBar,           // |
    Amper,          // &
    Less,           // <
    Greater,        // >
    Equal,          // =
    Dot,            // .
    Percent,        // %
    LeftBrace,      // {
    RightBrace,     // }
    EqEqual,        // ==
    NotEqual,       // !=
    LessEqual,      // <=
    GreaterEqual,   // >=
    Tilde,          // ~
    Circumflex,     // ^
    LeftShift,      // <<
    RightShift,     // >>
    DoubleStar,     // **
    PlusEqual,      // +=
    MinusEqual,     // -=
    StarEqual,      // *=
    SlashEqual,     // /=
    PercentEqual,   // %=
    AmperEqual,     // &=
    VBarEqual,      // |=
    CircumflexEqual,// ^=
    LeftShiftEqual, // <<=
    RightShiftEqual,// >>=
    DoubleStarEqual,// **=
    DoubleSlash,    // //
    DoubleSlashEqual,// //=
    At,             // @
    AtEqual,        // @=
    Arrow,          // ->
    Ellipsis,       // ...
    ColonEqual,     // :=
    Await,          // await
    Async,          // async
    TypeIgnore,     // type: ignore
    TypeComment,    // type: ...

    // 字符串类型
    RawString,      // 原始字符串
    Bytes,          // 字节串
    FString,        // f-string 开始标记
    FStringText,    // f-string 中的文本部分
    FStringExpr,    // f-string 表达式开始
    FStringConv,    // 转换标记 !r !s !a
    FStringSpec,    // 格式说明符
    FStringEnd,     // f-string 结束标记

    Error,          // 错误

    // 字面量关键字
    False_,         // False
    None_,          // None
    True_,          // True

    // 逻辑运算符
    And,            // and
    Or,             // or
    Not,            // not

    // Python 关键字
    As,             // as
    Assert,         // assert
    Break,          // break
    Class,          // class
    Continue,       // continue
    Def,            // def
    Del,            // del
    Elif,           // elif
    Else,           // else
    Except,         // except
    Finally,        // finally
    For,            // for
    From,           // from
    Global,         // global
    If,             // if
    Import,         // import
    In,             // in
    Is,             // is
    Lambda,         // lambda
    Nonlocal,       // nonlocal
    Pass,           // pass
    Raise,          // raise
    Return,         // return
    Try,            // try
    While,          // while
    With,           // with
    Yield,          // yield

    // CSGO 扩展
    Match,          // match
    Case,           // case
    Type,           // type
    Go,             // go
    Chan,           // chan
    Select,         // select
    Defer,          // defer
    Const,          // const
    Let,            // let
    Mut,            // mut
    Struct,         // struct
    Impl,           // impl
    Pub,            // pub
    Use,            // use
    Mod,            // mod
};

// Token 结构体
struct Token {
    TokenType type;
    std::string_view text;      // 指向源字符串的视图（零拷贝）
    std::variant<
        std::monostate,         // 无额外数据
        int64_t,                // 整数值
        double,                 // 浮点值
        std::string,            // 字符串值（需拷贝）
        std::vector<uint8_t>    // 字节值
    > value;
    size_t line;                // 行号（从1开始）
    size_t column;              // 列号（从1开始）
    
    // 辅助方法
    bool is(TokenType t) const { return type == t; }
    std::string toString() const;
};

// Tokenizer 类（RAII 管理）
class Tokenizer {
public:
    // 构造函数
    explicit Tokenizer(std::string source);
    explicit Tokenizer(std::istream& input, std::string filename = "<input>");
    
    // 禁用拷贝，允许移动
    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    Tokenizer(Tokenizer&&) noexcept = default;
    Tokenizer& operator=(Tokenizer&&) noexcept = default;
    
    // 析构函数
    ~Tokenizer() = default;
    
    // 核心接口
    Token nextToken();              // 获取下一个 token
    std::vector<Token> tokenize();  // 一次性分词所有
    
    // 状态查询
    bool isAtEnd() const;
    bool hasError() const;
    const std::string& errorMessage() const;
    
    // 配置
    void setTabSize(size_t size) { tabSize_ = size; }
    void enableTypeComments(bool enable) { typeComments_ = enable; }

private:
    // 源数据
    std::string source_;            // 源字符串（拥有所有权）
    std::string filename_;
    std::string_view view_;         // 当前处理视图
    
    // 位置状态
    size_t pos_ = 0;                // 当前字符位置
    size_t line_ = 1;               // 当前行号
    size_t column_ = 1;             // 当前列号
    size_t lineStart_ = 0;          // 当前行起始位置
    
    // 缩进状态
    std::vector<size_t> indentStack_{0};  // 缩进栈
    std::vector<Token> pendingIndents_;   // 待处理的缩进/dedent
    
    // 括号嵌套
    std::vector<char> parenStack_;
    
    // 配置
    size_t tabSize_ = 8;
    bool typeComments_ = false;
    
    // 错误状态
    bool hasError_ = false;
    std::string errorMessage_;
    
    // 辅助方法
    char peek() const;
    char peek(size_t offset) const;
    char advance();
    bool match(char expected);
    void skipWhitespace();
    void skipComment();
    
    // Token 解析方法
    Token makeToken(TokenType type, size_t start, size_t length);
    Token makeToken(TokenType type, size_t start, size_t length, 
                    std::variant<int64_t, double, std::string, std::vector<uint8_t>> value);

    Token scanIdentifier();
    Token scanNumber();
    Token scanString();
    Token scanRawString();
    Token scanBytes();

    // f-string 相关方法
    Token scanFString();                    // 主入口
    Token scanFStringText();                 // 扫描文本部分
    Token scanFStringExpression();            // 扫描表达式
    Token scanFStringConversion();            // 扫描转换标记
    Token scanFStringFormatSpec();            // 扫描格式说明符
    
    // f-string 状态
    struct FStringState {
        enum QuoteType { SINGLE, DOUBLE, TRIPLE_SINGLE, TRIPLE_DOUBLE };
        QuoteType quoteType;
        bool isRaw;
        int nestedBraceLevel;
        size_t startPos;
        size_t startLine;
        size_t startColumn;
    };
    
    std::optional<FStringState> fstringState_;  // 当前 f-string 状态
    Token scanOperator();
    
    // 处理缩进
    Token handleIndentation();
    
    // 错误处理
    Token error(const std::string& message);
    void reportError(const std::string& message);
    
    // 工具
    static bool isIdentifierStart(char c);
    static bool isIdentifierChar(char c);
    static bool isDigit(char c);
    static bool isHexDigit(char c);
    static bool isOctDigit(char c);
    static bool isBinDigit(char c);
    
    // 关键字检查
    static TokenType checkKeyword(std::string_view text);
};

// 辅助函数
const char* tokenTypeName(TokenType type);
std::ostream& operator<<(std::ostream& os, const Token& token);
static uint8_t hexToDigit(char c);
static uint8_t octToDigit(char c);

} // namespace csgo

#endif // CSGO_TOKENIZER_H