// CSGO Lexer Implementation - Based on CPython's tokenizer.c// tokenizer.cpp
#include "tokenizer.h"
#include <cctype>
#include <unordered_map>
#include <sstream>

namespace csgo {

// 关键字表
static const std::unordered_map<std::string_view, TokenType> keywords = {
    // Python 核心关键字
    {"False", TokenType::False_},
    {"None", TokenType::None_},
    {"True", TokenType::True_},
    {"and", TokenType::And},
    {"as", TokenType::As},
    {"assert", TokenType::Assert},
    {"async", TokenType::Async},
    {"await", TokenType::Await},
    {"break", TokenType::Break},
    {"class", TokenType::Class},
    {"continue", TokenType::Continue},
    {"def", TokenType::Def},
    {"del", TokenType::Del},
    {"elif", TokenType::Elif},
    {"else", TokenType::Else},
    {"except", TokenType::Except},
    {"finally", TokenType::Finally},
    {"for", TokenType::For},
    {"from", TokenType::From},
    {"global", TokenType::Global},
    {"if", TokenType::If},
    {"import", TokenType::Import},
    {"in", TokenType::In},
    {"is", TokenType::Is},
    {"lambda", TokenType::Lambda},
    {"nonlocal", TokenType::Nonlocal},
    {"not", TokenType::Not},
    {"or", TokenType::Or},
    {"pass", TokenType::Pass},
    {"raise", TokenType::Raise},
    {"return", TokenType::Return},
    {"try", TokenType::Try},
    {"while", TokenType::While},
    {"with", TokenType::With},
    {"yield", TokenType::Yield},
    
    // CSGO 扩展关键字
    {"match", TokenType::Match},
    {"case", TokenType::Case},
    {"type", TokenType::Type},
    {"go", TokenType::Go},           // 启动协程
    {"chan", TokenType::Chan},        // 通道
    {"select", TokenType::Select},    // 多路复用
    {"defer", TokenType::Defer},      // 延迟执行
    {"const", TokenType::Const},      // 常量
    {"let", TokenType::Let},          // 变量声明
    {"mut", TokenType::Mut},          // 可变
    {"struct", TokenType::Struct},    // 结构体
    {"impl", TokenType::Impl},        // 实现
    {"pub", TokenType::Pub},          // 公开
    {"use", TokenType::Use},          // 引入
    {"mod", TokenType::Mod},          // 模块
};

// Token 转字符串
std::string Token::toString() const {
    std::ostringstream oss;
    oss << tokenTypeName(type) << "(" << line << ":" << column << ")";
    if (!text.empty()) {
        oss << " \"" << text << "\"";
    }
    return oss.str();
}

// Tokenizer 实现

Tokenizer::Tokenizer(std::string source) 
    : source_(std::move(source))
    , filename_("<string>")
    , view_(source_)
{}

Tokenizer::Tokenizer(std::istream& input, std::string filename)
    : filename_(std::move(filename))
{
    // 读取整个流
    std::ostringstream oss;
    oss << input.rdbuf();
    source_ = oss.str();
    view_ = source_;
}

bool Tokenizer::isAtEnd() const {
    return pos_ >= source_.length();
}

bool Tokenizer::hasError() const {
    return hasError_;
}

const std::string& Tokenizer::errorMessage() const {
    return errorMessage_;
}

char Tokenizer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

char Tokenizer::peek(size_t offset) const {
    if (pos_ + offset >= source_.length()) return '\0';
    return source_[pos_ + offset];
}

char Tokenizer::advance() {
    if (isAtEnd()) return '\0';
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
        lineStart_ = pos_;
    } else {
        column_++;
    }
    return c;
}

bool Tokenizer::match(char expected) {
    if (peek() == expected) {
        advance();
        return true;
    }
    return false;
}

void Tokenizer::skipWhitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\f':
                advance();
                break;
            default:
                return;
        }
    }
}

void Tokenizer::skipComment() {
    if (peek() == '#') {
        while (peek() != '\n' && !isAtEnd()) {
            advance();
        }
    }
}

Token Tokenizer::makeToken(TokenType type, size_t start, size_t length) {
    return Token{
        type,
        std::string_view(source_.data() + start, length),
        std::monostate{},  // 默认值
        line_,
        column_ - length
    };
}

Token Tokenizer::makeToken(TokenType type, size_t start, size_t length,
                           std::variant<int64_t, double, std::string> value) {
    Token token{
        type,
        std::string_view(source_.data() + start, length),
        std::monostate{},  // 先初始化为默认值
        line_,
        column_ - length
    };
    // 正确地将值存储到variant中
    if (std::holds_alternative<int64_t>(value)) {
        token.value = std::get<int64_t>(value);
    } else if (std::holds_alternative<double>(value)) {
        token.value = std::get<double>(value);
    } else if (std::holds_alternative<std::string>(value)) {
        token.value = std::get<std::string>(value);
    }
    return token;
}

Token Tokenizer::error(const std::string& message) {
    reportError(message);
    return makeToken(TokenType::Error, pos_, 0);
}

void Tokenizer::reportError(const std::string& message) {
    hasError_ = true;
    std::ostringstream oss;
    oss << filename_ << ":" << line_ << ":" << column_ << ": " << message;
    errorMessage_ = oss.str();
    std::cerr << errorMessage_ << std::endl;
}

// 静态工具方法
bool Tokenizer::isIdentifierStart(char c) {
    return (c >= 'a' && c <= 'z') || 
           (c >= 'A' && c <= 'Z') || 
           c == '_' ||
           (static_cast<unsigned char>(c) >= 128);
}

bool Tokenizer::isIdentifierChar(char c) {
    return isIdentifierStart(c) || (c >= '0' && c <= '9');
}

bool Tokenizer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Tokenizer::isHexDigit(char c) {
    return isDigit(c) || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

bool Tokenizer::isOctDigit(char c) {
    return c >= '0' && c <= '7';
}

bool Tokenizer::isBinDigit(char c) {
    return c == '0' || c == '1';
}

TokenType Tokenizer::checkKeyword(std::string_view text) {
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return it->second;
    }
    return TokenType::Name;
}

// 核心分词逻辑
Token Tokenizer::nextToken() {
    // 先返回待处理的缩进/dedent
    if (!pendingIndents_.empty()) {
        Token token = pendingIndents_.back();
        pendingIndents_.pop_back();
        return token;
    }
    
    skipWhitespace();
    skipComment();
    
    if (isAtEnd()) {
        // 文件结束，处理剩余的 dedent
        while (indentStack_.size() > 1) {
            indentStack_.pop_back();
            pendingIndents_.push_back(
                makeToken(TokenType::Dedent, pos_, 0));
        }
        if (!pendingIndents_.empty()) {
            Token token = pendingIndents_.back();
            pendingIndents_.pop_back();
            return token;
        }
        return makeToken(TokenType::EndMarker, pos_, 0);
    }
    
    char c = peek();
    size_t start = pos_;
    
    // 换行处理（缩进逻辑）
    if (c == '\n') {
        advance();
        return handleIndentation();
    }
    
    // 标识符/关键字
    if (isIdentifierStart(c)) {
        return scanIdentifier();
    }
    
    // 数字
    if (isDigit(c)) {
        return scanNumber();
    }
    
    // 字符串
    if (c == '"' || c == '\'') {
        return scanString();
    }
    
    // 操作符
    return scanOperator();
}

Token Tokenizer::handleIndentation() {
    // 计算缩进级别
    size_t col = 0;
    while (true) {
        char c = peek();
        if (c == ' ') {
            col++;
            advance();
        } else if (c == '\t') {
            col = (col / tabSize_ + 1) * tabSize_;
            advance();
        } else if (c == '\f') {
            col = 0;
            advance();
        } else {
            break;
        }
    }
    
    // 跳过空行和注释行
    if (peek() == '\n' || peek() == '#' || peek() == '\0') {
        return nextToken();  // 递归获取下一个有效 token
    }
    
    size_t currentIndent = indentStack_.back();
    
    if (col > currentIndent) {
        // 缩进
        if (indentStack_.size() >= 100) {
            return error("Too many indentation levels");
        }
        indentStack_.push_back(col);
        return makeToken(TokenType::Indent, pos_ - col, col);
    } else if (col < currentIndent) {
        // Dedent，可能多个
        while (col < indentStack_.back()) {
            indentStack_.pop_back();
            pendingIndents_.push_back(
                makeToken(TokenType::Dedent, pos_, 0));
        }
        if (col != indentStack_.back()) {
            return error("Inconsistent dedent");
        }
        // 返回第一个 dedent
        Token token = pendingIndents_.back();
        pendingIndents_.pop_back();
        return token;
    }
    
    // 缩进级别相同，继续获取下一个 token
    return nextToken();
}

Token Tokenizer::scanIdentifier() {
    size_t start = pos_;
    while (isIdentifierChar(peek())) {
        advance();
    }
    std::string_view text(source_.data() + start, pos_ - start);
    TokenType type = checkKeyword(text);
    return makeToken(type, start, pos_ - start);
}

Token Tokenizer::scanNumber() {
    size_t start = pos_;
    bool isFloat = false;
    
    // 处理不同进制
    if (peek() == '0') {
        advance();
        char next = peek();
        if (next == 'x' || next == 'X') {
            // 十六进制
            advance();
            if (!isHexDigit(peek())) {
                return error("Invalid hexadecimal literal");
            }
            while (isHexDigit(peek())) {
                advance();
            }
            // 解析值
            std::string hexStr(source_, start + 2, pos_ - start - 2);
            int64_t value = std::stoll(hexStr, nullptr, 16);
            return makeToken(TokenType::Number, start, pos_ - start, value);
        } else if (next == 'o' || next == 'O') {
            // 八进制
            advance();
            while (isOctDigit(peek())) advance();
        } else if (next == 'b' || next == 'B') {
            // 二进制
            advance();
            while (isBinDigit(peek())) advance();
        }
    }
    
    // 十进制
    while (isDigit(peek())) {
        advance();
    }
    
    // 小数部分
    if (peek() == '.' && isDigit(peek(1))) {
        isFloat = true;
        advance();  // 跳过 '.'
        while (isDigit(peek())) advance();
    }
    
    // 指数部分
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (!isDigit(peek())) {
            return error("Invalid exponent");
        }
        while (isDigit(peek())) advance();
    }
    
    // 解析值
    std::string numStr(source_, start, pos_ - start);
    if (isFloat) {
        double value = std::stod(numStr);
        return makeToken(TokenType::Number, start, pos_ - start, value);
    } else {
        int64_t value = std::stoll(numStr);
        return makeToken(TokenType::Number, start, pos_ - start, value);
    }
}

Token Tokenizer::scanString() {
    size_t start = pos_;
    char quote = advance();  // ' 或 "
    bool triple = false;
    
    // 检查三引号
    if (peek() == quote && peek(1) == quote) {
        triple = true;
        advance();
        advance();
    }
    
    std::string value;
    while (!isAtEnd()) {
        char c = advance();
        
        if (c == quote) {
            if (triple) {
                if (peek() == quote && peek(1) == quote) {
                    advance();
                    advance();
                    break;
                }
            } else {
                break;
            }
        }
        
        if (c == '\\') {
            // 转义序列
            if (isAtEnd()) break;
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                default: value += escaped; break;
            }
        } else {
            value += c;
        }
    }
    
    return makeToken(TokenType::String, start, pos_ - start, std::move(value));
}

Token Tokenizer::scanOperator() {
    size_t start = pos_;
    char c1 = advance();
    char c2 = peek();
    char c3 = peek(1);
    
    // 三字符操作符
    if (c1 == '.' && c2 == '.' && c3 == '.') {
        advance(); advance();
        return makeToken(TokenType::Ellipsis, start, 3);
    }
    
    // 双字符操作符
    switch (c1) {
        case '+':
            if (c2 == '=') { advance(); return makeToken(TokenType::PlusEqual, start, 2); }
            if (c2 == '+') { advance(); return makeToken(TokenType::Error, start, 2); }  // ++ 不支持
            break;
        case '-':
            if (c2 == '=') { advance(); return makeToken(TokenType::MinusEqual, start, 2); }
            if (c2 == '>') { advance(); return makeToken(TokenType::Arrow, start, 2); }
            break;
        case '*':
            if (c2 == '=') { advance(); return makeToken(TokenType::StarEqual, start, 2); }
            if (c2 == '*') { 
                advance();
                if (peek() == '=') { advance(); return makeToken(TokenType::DoubleStarEqual, start, 3); }
                return makeToken(TokenType::DoubleStar, start, 2);
            }
            break;
        case '/':
            if (c2 == '=') { advance(); return makeToken(TokenType::SlashEqual, start, 2); }
            if (c2 == '/') {
                advance();
                if (peek() == '=') { advance(); return makeToken(TokenType::DoubleSlashEqual, start, 3); }
                return makeToken(TokenType::DoubleSlash, start, 2);
            }
            break;
        case '%':
            if (c2 == '=') { advance(); return makeToken(TokenType::PercentEqual, start, 2); }
            break;
        case '&':
            if (c2 == '=') { advance(); return makeToken(TokenType::AmperEqual, start, 2); }
            if (c2 == '&') { advance(); return makeToken(TokenType::Error, start, 2); }  // && 不支持
            break;
        case '|':
            if (c2 == '=') { advance(); return makeToken(TokenType::VBarEqual, start, 2); }
            if (c2 == '|') { advance(); return makeToken(TokenType::Error, start, 2); }  // || 不支持
            break;
        case '^':
            if (c2 == '=') { advance(); return makeToken(TokenType::CircumflexEqual, start, 2); }
            break;
        case '<':
            if (c2 == '=') { advance(); return makeToken(TokenType::LessEqual, start, 2); }
            if (c2 == '<') {
                advance();
                if (peek() == '=') { advance(); return makeToken(TokenType::LeftShiftEqual, start, 3); }
                return makeToken(TokenType::LeftShift, start, 2);
            }
            break;
        case '>':
            if (c2 == '=') { advance(); return makeToken(TokenType::GreaterEqual, start, 2); }
            if (c2 == '>') {
                advance();
                if (peek() == '=') { advance(); return makeToken(TokenType::RightShiftEqual, start, 3); }
                return makeToken(TokenType::RightShift, start, 2);
            }
            break;
        case '=':
            if (c2 == '=') { advance(); return makeToken(TokenType::EqEqual, start, 2); }
            break;
        case '!':
            if (c2 == '=') { advance(); return makeToken(TokenType::NotEqual, start, 2); }
            break;
        case ':':
            if (c2 == '=') { advance(); return makeToken(TokenType::ColonEqual, start, 2); }
            break;
        case '@':
            if (c2 == '=') { advance(); return makeToken(TokenType::AtEqual, start, 2); }
            break;
    }
    
    // 单字符操作符
    switch (c1) {
        case '(': return makeToken(TokenType::LeftParen, start, 1);
        case ')': return makeToken(TokenType::RightParen, start, 1);
        case '[': return makeToken(TokenType::LeftBracket, start, 1);
        case ']': return makeToken(TokenType::RightBracket, start, 1);
        case '{': return makeToken(TokenType::LeftBrace, start, 1);
        case '}': return makeToken(TokenType::RightBrace, start, 1);
        case ':': return makeToken(TokenType::Colon, start, 1);
        case ',': return makeToken(TokenType::Comma, start, 1);
        case ';': return makeToken(TokenType::Semi, start, 1);
        case '+': return makeToken(TokenType::Plus, start, 1);
        case '-': return makeToken(TokenType::Minus, start, 1);
        case '*': return makeToken(TokenType::Star, start, 1);
        case '/': return makeToken(TokenType::Slash, start, 1);
        case '|': return makeToken(TokenType::VBar, start, 1);
        case '&': return makeToken(TokenType::Amper, start, 1);
        case '<': return makeToken(TokenType::Less, start, 1);
        case '>': return makeToken(TokenType::Greater, start, 1);
        case '=': return makeToken(TokenType::Equal, start, 1);
        case '.': return makeToken(TokenType::Dot, start, 1);
        case '%': return makeToken(TokenType::Percent, start, 1);
        case '~': return makeToken(TokenType::Tilde, start, 1);
        case '^': return makeToken(TokenType::Circumflex, start, 1);
        case '@': return makeToken(TokenType::At, start, 1);
        default:  return error(std::string("Unexpected character: ") + c1);
    }
}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    
    // 确保至少有一个EndMarker token，即使输入为空
    if (isAtEnd() && pendingIndents_.empty()) {
        tokens.push_back(makeToken(TokenType::EndMarker, pos_, 0));
        return tokens;
    }
    
    while (!isAtEnd() || !pendingIndents_.empty()) {
        Token token = nextToken();
        tokens.push_back(token);
        if (token.type == TokenType::EndMarker) break;
        if (token.type == TokenType::Error && hasError_) break;
    }
    
    // 确保以EndMarker结束
    if (tokens.empty() || tokens.back().type != TokenType::EndMarker) {
        tokens.push_back(makeToken(TokenType::EndMarker, pos_, 0));
    }
    
    return tokens;
}

// 辅助函数
const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::EndMarker: return "ENDMARKER";
        case TokenType::Name: return "NAME";
        case TokenType::Number: return "NUMBER";
        case TokenType::String: return "STRING";
        case TokenType::NewLine: return "NEWLINE";
        case TokenType::Indent: return "INDENT";
        case TokenType::Dedent: return "DEDENT";
        case TokenType::LeftParen: return "LPAR";
        case TokenType::RightParen: return "RPAR";
        case TokenType::LeftBracket: return "LSQB";
        case TokenType::RightBracket: return "RSQB";
        case TokenType::Colon: return "COLON";
        case TokenType::Comma: return "COMMA";
        case TokenType::Semi: return "SEMI";
        case TokenType::Plus: return "PLUS";
        case TokenType::Minus: return "MINUS";
        case TokenType::Star: return "STAR";
        case TokenType::Slash: return "SLASH";
        case TokenType::VBar: return "VBAR";
        case TokenType::Amper: return "AMPER";
        case TokenType::Less: return "LESS";
        case TokenType::Greater: return "GREATER";
        case TokenType::Equal: return "EQUAL";
        case TokenType::Dot: return "DOT";
        case TokenType::Percent: return "PERCENT";
        case TokenType::LeftBrace: return "LBRACE";
        case TokenType::RightBrace: return "RBRACE";
        case TokenType::EqEqual: return "EQEQUAL";
        case TokenType::NotEqual: return "NOTEQUAL";
        case TokenType::LessEqual: return "LESSEQUAL";
        case TokenType::GreaterEqual: return "GREATEREQUAL";
        case TokenType::Tilde: return "TILDE";
        case TokenType::Circumflex: return "CIRCUMFLEX";
        case TokenType::LeftShift: return "LEFTSHIFT";
        case TokenType::RightShift: return "RIGHTSHIFT";
        case TokenType::DoubleStar: return "DOUBLESTAR";
        case TokenType::PlusEqual: return "PLUSEQUAL";
        case TokenType::MinusEqual: return "MINEQUAL";
        case TokenType::StarEqual: return "STAREQUAL";
        case TokenType::SlashEqual: return "SLASHEQUAL";
        case TokenType::PercentEqual: return "PERCENTEQUAL";
        case TokenType::AmperEqual: return "AMPEREQUAL";
        case TokenType::VBarEqual: return "VBAREQUAL";
        case TokenType::CircumflexEqual: return "CIRCUMFLEXEQUAL";
        case TokenType::LeftShiftEqual: return "LEFTSHIFTEQUAL";
        case TokenType::RightShiftEqual: return "RIGHTSHIFTEQUAL";
        case TokenType::DoubleStarEqual: return "DOUBLESTAREQUAL";
        case TokenType::DoubleSlash: return "DOUBLESLASH";
        case TokenType::DoubleSlashEqual: return "DOUBLESLASHEQUAL";
        case TokenType::At: return "AT";
        case TokenType::AtEqual: return "ATEQUAL";
        case TokenType::Arrow: return "RARROW";
        case TokenType::Ellipsis: return "ELLIPSIS";
        case TokenType::ColonEqual: return "COLONEQUAL";
        case TokenType::Await: return "AWAIT";
        case TokenType::Async: return "ASYNC";
        case TokenType::TypeIgnore: return "TYPE_IGNORE";
        case TokenType::TypeComment: return "TYPE_COMMENT";
        case TokenType::Error: return "ERRORTOKEN";
        default: return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << token.toString();
    return os;
}

} // namespace csgo