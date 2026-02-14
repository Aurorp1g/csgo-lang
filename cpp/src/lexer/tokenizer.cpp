// CSGO Lexer Implementation - Based on CPython's tokenizer.c// tokenizer.cpp
#include "tokenizer.h"
#include <cctype>
#include <unordered_map>
#include <sstream>
#include <stack>

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
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f') {
            advance();
        } else if (c == '\\') {
            // 行继续符
            advance();
            if (peek() == '\n') {
                advance();
                line_++;
                column_ = 1;
                lineStart_ = pos_;
            } else {
                // 不是行继续符，回退
                pos_--;
                break;
            }
        } else {
            break;
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
                           std::variant<int64_t, double, std::string, std::vector<uint8_t>> value) {
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
    } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        token.value = std::get<std::vector<uint8_t>>(value);
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
    
    // 如果在 f-string 中，继续解析 f-string 的各个部分
    if (fstringState_) {
        char c = peek();
        
        // 检查 f-string 是否结束
        bool shouldEnd = false;
        
        if (fstringState_->quoteType == FStringState::SINGLE && c == '\'') {
            shouldEnd = true;
        } else if (fstringState_->quoteType == FStringState::DOUBLE && c == '"') {
            shouldEnd = true;
        } else if (fstringState_->quoteType == FStringState::TRIPLE_SINGLE) {
            if (c == '\'' && peek(1) == '\'' && peek(2) == '\'') {
                shouldEnd = true;
            }
        } else if (fstringState_->quoteType == FStringState::TRIPLE_DOUBLE) {
            if (c == '"' && peek(1) == '"' && peek(2) == '"') {
                shouldEnd = true;
            }
        }
        
        if (shouldEnd) {
            // 结束 f-string
            size_t start = pos_;
            advance();  // 跳过第一个引号
            if (fstringState_->quoteType == FStringState::TRIPLE_SINGLE ||
                fstringState_->quoteType == FStringState::TRIPLE_DOUBLE) {
                advance(); advance();  // 跳过第二、三个引号
            }
            
            fstringState_.reset();
            return makeToken(TokenType::FStringEnd, start, pos_ - start);
        }
        
        // 根据当前位置决定解析哪个部分
        if (c == '{' && !fstringState_->isRaw) {
            if (peek(1) == '{') {
                // 转义的 {{，作为文本处理
                return scanFStringText();
            } else {
                // 表达式开始
                return scanFStringExpression();
            }
        } else if (c == '!' && !fstringState_->isRaw) {
            // 转换标记
            return scanFStringConversion();
        } else if (c == ':' && !fstringState_->isRaw) {
            // 格式说明符
            return scanFStringFormatSpec();
        } else {
            // 文本部分
            return scanFStringText();
        }
    }
    
    // 正常的 token 解析逻辑
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
    
    // ========== 修改的字符串检测逻辑 ==========
    
    // 1. 直接以引号开头的普通字符串
    if (c == '"' || c == '\'') {
        return scanString();
    }
    
    // 2. 检查带前缀的字符串 (r, b, f, 及其组合)
    bool isPrefixChar = (c == 'r' || c == 'R' || c == 'b' || c == 'B' || c == 'f' || c == 'F');
    
    if (isPrefixChar) {
        char next = peek(1);
        char next2 = peek(2);
        
        // 情况1: 单前缀后跟引号，如 r"hello"
        if (next == '"' || next == '\'') {
            return scanString();
        }
        
        // 情况2: 双前缀后跟引号，如 rb"hello" 或 fr"hello"
        bool isSecondPrefix = (next == 'r' || next == 'R' || 
                               next == 'b' || next == 'B' || 
                               next == 'f' || next == 'F');
        if (isSecondPrefix && (next2 == '"' || next2 == '\'')) {
            return scanString();
        }
        
        // 情况3: 检查是否是 f-string 的特殊情况
        if ((c == 'f' || c == 'F') && 
            (next == 'r' || next == 'R') && 
            (next2 == '"' || next2 == '\'')) {
            return scanString();  // fr"..."
        }
        
        if ((c == 'r' || c == 'R') && 
            (next == 'f' || next == 'F') && 
            (next2 == '"' || next2 == '\'')) {
            return scanString();  // rf"..."
        }
        
        // 如果不是字符串前缀，让后面的标识符检查处理
        // 这里不返回，继续执行
    }
    
    // ========== 结束字符串检测 ==========
    
    // 换行处理
    if (c == '\n') {
        advance();
        return handleIndentation();
    }
    
    // 标识符/关键字（包括单独的 r/b/f 作为标识符）
    if (isIdentifierStart(c)) {
        return scanIdentifier();
    }
    
    // 数字
    if (isDigit(c)) {
        return scanNumber();
    }
    
    // 操作符
    return scanOperator();
}

// 扫描 f-string 的主入口
Token Tokenizer::scanFString() {
    size_t start = pos_;
    size_t startLine = line_;
    size_t startColumn = column_;
    bool isRaw = false;
    
    // 检查前缀组合
    while (true) {
        char c = peek();
        if (c == 'f' || c == 'F') {
            advance();  // 跳过 f
        } else if ((c == 'r' || c == 'R') && !isRaw) {
            isRaw = true;
            advance();  // 跳过 r
        } else {
            break;
        }
    }
    
    // 确定引号类型
    char quote = advance();
    FStringState::QuoteType quoteType;
    bool isTriple = false;
    
    if (quote == '\'') {
        if (peek() == '\'' && peek(1) == '\'') {
            quoteType = FStringState::TRIPLE_SINGLE;
            isTriple = true;
            advance(); advance();  // 跳过两个额外的引号
        } else {
            quoteType = FStringState::SINGLE;
        }
    } else if (quote == '"') {
        if (peek() == '"' && peek(1) == '"') {
            quoteType = FStringState::TRIPLE_DOUBLE;
            isTriple = true;
            advance(); advance();  // 跳过两个额外的引号
        } else {
            quoteType = FStringState::DOUBLE;
        }
    } else {
        return error("Invalid f-string quote character");
    }
    
    // 保存 f-string 状态
    fstringState_ = FStringState{
        quoteType,
        isRaw,
        0,
        start,
        startLine,
        startColumn
    };
    
    // 返回 f-string 开始标记
    return makeToken(TokenType::FString, start, pos_ - start);
}

// 扫描 f-string 文本部分
Token Tokenizer::scanFStringText() {
    if (!fstringState_) {
        return error("Internal error: No f-string state");
    }
    
    size_t start = pos_;
    size_t startLine = line_;
    size_t startColumn = column_;
    std::string text;
    
    while (!isAtEnd()) {
        char c = peek();
        
        // 检查是否结束
        if (c == '\'' || c == '"') {
            bool shouldEnd = false;
            
            if (fstringState_->quoteType == FStringState::SINGLE && c == '\'') {
                shouldEnd = true;
            } else if (fstringState_->quoteType == FStringState::DOUBLE && c == '"') {
                shouldEnd = true;
            } else if (fstringState_->quoteType == FStringState::TRIPLE_SINGLE) {
                if (c == '\'' && peek(1) == '\'' && peek(2) == '\'') {
                    shouldEnd = true;
                }
            } else if (fstringState_->quoteType == FStringState::TRIPLE_DOUBLE) {
                if (c == '"' && peek(1) == '"' && peek(2) == '"') {
                    shouldEnd = true;
                }
            }
            
            if (shouldEnd) {
                break;
            }
        }
        
        // 检查表达式开始
        if (c == '{' && !fstringState_->isRaw) {
            if (peek(1) == '{') {
                // 转义的 {{，输出单个 {
                text += '{';
                advance(); advance();  // 跳过两个 {
                continue;
            } else {
                // 真正的表达式开始
                break;
            }
        }
        
        // 检查表达式结束（不应该在文本中遇到）
        if (c == '}' && !fstringState_->isRaw) {
            if (peek(1) == '}') {
                // 转义的 }}，输出单个 }
                text += '}';
                advance(); advance();  // 跳过两个 }
                continue;
            } else {
                return error("f-string: single '}' is not allowed outside expression");
            }
        }
        
        // 处理转义字符
        if (c == '\\' && !fstringState_->isRaw) {
            advance();  // 跳过 \
            if (isAtEnd()) break;
            
            char escaped = advance();
            switch (escaped) {
                case 'n': text += '\n'; break;
                case 't': text += '\t'; break;
                case 'r': text += '\r'; break;
                case '\\': text += '\\'; break;
                case '\'': text += '\''; break;
                case '"': text += '"'; break;
                case '{': text += '{'; break;
                case '}': text += '}'; break;
                default: text += escaped; break;
            }
            continue;
        }
        
        // 普通字符
        text += c;
        advance();
    }
    
    if (text.empty()) {
        // 没有文本，继续扫描下一个部分
        return nextToken();
    }
    
    return makeToken(TokenType::FStringText, start, pos_ - start, text);
}

// 扫描 f-string 表达式
Token Tokenizer::scanFStringExpression() {
    if (!fstringState_) {
        return error("Internal error: No f-string state");
    }
    
    size_t start = pos_;
    size_t startLine = line_;
    size_t startColumn = column_;
    
    if (peek() != '{') {
        return error("Expected '{' at start of f-string expression");
    }
    
    advance();  // 跳过 {
    fstringState_->nestedBraceLevel = 0;
    
    // 解析表达式，直到遇到匹配的 }
    std::string expr;
    std::stack<char> brackets;
    
    while (!isAtEnd()) {
        char c = peek();
        
        if (c == '{') {
            if (!fstringState_->isRaw) {
                // 嵌套的表达式开始
                brackets.push('{');
                fstringState_->nestedBraceLevel++;
            }
            expr += c;
            advance();
        }
        else if (c == '}') {
            if (fstringState_->nestedBraceLevel > 0) {
                // 结束嵌套的表达式
                if (!brackets.empty() && brackets.top() == '{') {
                    brackets.pop();
                    fstringState_->nestedBraceLevel--;
                }
                expr += c;
                advance();
            } else {
                // 表达式真正的结束
                advance();  // 跳过 }
                break;
            }
        }
        else if (c == '(' || c == '[') {
            brackets.push(c);
            expr += c;
            advance();
        }
        else if (c == ')' || c == ']') {
            if (!brackets.empty()) {
                char expected = (c == ')') ? '(' : '[';
                if (brackets.top() == expected) {
                    brackets.pop();
                }
            }
            expr += c;
            advance();
        }
        else if (c == '\'' || c == '"') {
            // 表达式中的字符串字面量
            expr += c;
            advance();
            char quote = c;
            
            // 简单处理字符串（完整实现应该调用 scanString）
            while (!isAtEnd() && peek() != quote) {
                if (peek() == '\\') {
                    expr += '\\';
                    advance();
                }
                expr += peek();
                advance();
            }
            
            if (!isAtEnd()) {
                expr += quote;
                advance();
            }
        }
        else {
            expr += c;
            advance();
        }
    }
    
    if (expr.empty()) {
        return error("Empty expression in f-string");
    }
    
    // 创建表达式 token
    Token exprToken = makeToken(TokenType::FStringExpr, start, pos_ - start, expr);
    
    // 检查后面是否有转换标记或格式说明符
    char next = peek();
    if (next == '!') {
        // 有转换标记，让 scanFStringConversion 处理
        return exprToken;
    } else if (next == ':') {
        // 有格式说明符，让 scanFStringFormatSpec 处理
        return exprToken;
    }
    
    // 没有转换和格式，表达式结束
    return exprToken;
}

// 扫描转换标记 (!r, !s, !a)
Token Tokenizer::scanFStringConversion() {
    if (!fstringState_) {
        return error("Internal error: No f-string state");
    }
    
    size_t start = pos_;
    
    if (peek() != '!') {
        return error("Expected '!' for f-string conversion");
    }
    
    advance();  // 跳过 !
    
    if (isAtEnd()) {
        return error("Unexpected end of f-string conversion");
    }
    
    char conv = advance();
    std::string convStr;
    
    switch (conv) {
        case 'r':
            convStr = "!r";
            break;
        case 's':
            convStr = "!s";
            break;
        case 'a':
            convStr = "!a";
            break;
        default:
            return error("Invalid conversion character in f-string: expected 'r', 's', or 'a'");
    }
    
    Token convToken = makeToken(TokenType::FStringConv, start, pos_ - start, convStr);
    
    // 检查后面是否有格式说明符
    if (peek() == ':') {
        return convToken;  // 让 scanFStringFormatSpec 处理
    }
    
    // 没有格式说明符，转换标记结束
    return convToken;
}

// 扫描格式说明符
Token Tokenizer::scanFStringFormatSpec() {
    if (!fstringState_) {
        return error("Internal error: No f-string state");
    }
    
    size_t start = pos_;
    
    if (peek() != ':') {
        return error("Expected ':' for f-string format specifier");
    }
    
    advance();  // 跳过 :
    
    std::string spec;
    bool inBraces = false;
    
    while (!isAtEnd()) {
        char c = peek();
        
        if (c == '{' && !fstringState_->isRaw) {
            // 格式说明符中不支持嵌套表达式
            return error("f-string: nested expressions inside format specifier are not allowed");
        }
        
        if (c == '}') {
            // 格式说明符结束，先跳过 }
            advance();
            break;
        }
        
        spec += c;
        advance();
    }
    
    return makeToken(TokenType::FStringSpec, start, pos_ - start, spec);
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
            while (isHexDigit(peek()) || peek() == '_') {
                advance();
            }
            // 解析值（需要移除下划线）
            std::string hexStr;
            for (size_t i = start + 2; i < pos_; ++i) {
                if (source_[i] != '_') hexStr += source_[i];
            }
            int64_t value = std::stoll(hexStr, nullptr, 16);
            return makeToken(TokenType::Number, start, pos_ - start, value);
        } else if (next == 'o' || next == 'O') {
            // 八进制
            advance();
            while (isOctDigit(peek()) || peek() == '_') advance();
        } else if (next == 'b' || next == 'B') {
            // 二进制
            advance();
            while (isBinDigit(peek()) || peek() == '_') advance();
        }
    }
    
    // 十进制
    while (isDigit(peek()) || peek() == '_') {
        advance();
    }
    
    // 小数部分
    if (peek() == '.' && isDigit(peek(1))) {
        isFloat = true;
        advance();  // 跳过 '.'
        while (isDigit(peek()) || peek() == '_') advance();
    }
    
    // 指数部分
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (!isDigit(peek())) {
            return error("Invalid exponent");
        }
        while (isDigit(peek()) || peek() == '_') advance();
    }
    
    // 解析值（需要移除下划线）
    std::string numStr;
    for (size_t i = start; i < pos_; ++i) {
        if (source_[i] != '_') numStr += source_[i];
    }
    
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
    
    // 检查前缀
    bool isFString = false;
    bool isRawString = false;
    bool isBytesString = false;
    
    // 处理前缀 (f, r, b, fr, rf, fb, bf, 等)
    while (peek() == 'f' || peek() == 'F' || peek() == 'r' || peek() == 'R' || peek() == 'b' || peek() == 'B') {
        char prefix = advance();
        if (prefix == 'f' || prefix == 'F') isFString = true;
        else if (prefix == 'r' || prefix == 'R') isRawString = true;
        else if (prefix == 'b' || prefix == 'B') isBytesString = true;
        
        // 检查是否是引号，如果不是则继续检查前缀
        if (peek() == '"' || peek() == '\'') break;
    }
    
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
        
        if (c == '\\' && !isRawString) {
            // 转义序列（非原始字符串）
            if (isAtEnd()) break;
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case 'x':
                case 'X': {
                    if (isHexDigit(peek()) && isHexDigit(peek(1))) {
                        char h1 = advance();
                        char h2 = advance();
                        uint8_t high = hexToDigit(h1);
                        uint8_t low = hexToDigit(h2);
                        value += static_cast<char>((high << 4) | low);
                    } else {
                        value += escaped;  // 保留未识别的转义
                    }
                    break;
                }
                default: value += escaped; break;
            }
        } else {
            value += c;
        }
    }
    
    // 确定token类型
    TokenType tokenType = TokenType::String;
    if (isFString) tokenType = TokenType::FString;
    else if (isRawString) tokenType = TokenType::RawString;
    else if (isBytesString) tokenType = TokenType::Bytes;
    
    // 如果是字节串，将 string 转换为 vector<uint8_t>
    if (tokenType == TokenType::Bytes) {
        std::vector<uint8_t> bytesValue;
        bytesValue.reserve(value.size());
        for (char c : value) {
            bytesValue.push_back(static_cast<uint8_t>(c));
        }
        return makeToken(tokenType, start, pos_ - start, std::move(bytesValue));
    }
    
    return makeToken(tokenType, start, pos_ - start, std::move(value));
}

// 扫描原始字符串 (r"..." 或 R"...")
Token Tokenizer::scanRawString() {
    size_t start = pos_;
    size_t startLine = line_;
    size_t startColumn = column_;
    bool isRaw = false;
    
    // 检查前缀
    if (peek() == 'r' || peek() == 'R') {
        isRaw = true;
        advance();  // 跳过 r/R
    }
    
    // 确保后面是引号
    char quote = peek();
    if (quote != '\'' && quote != '"') {
        // 不是原始字符串，回退并作为标识符处理
        pos_ = start;
        return scanIdentifier();
    }
    
    advance();  // 跳过引号
    bool triple = false;
    
    // 检查三引号
    if (peek() == quote && peek(1) == quote) {
        triple = true;
        advance();  // 跳过第二个引号
        advance();  // 跳过第三个引号
    }
    
    std::string value;
    
    while (!isAtEnd()) {
        char c = peek();
        
        // 检查是否结束
        if (c == quote) {
            if (triple) {
                if (peek(1) == quote && peek(2) == quote) {
                    advance();  // 跳过当前引号
                    advance();  // 跳过第二个引号
                    advance();  // 跳过第三个引号
                    break;
                }
                // 在三引号中，单个引号是内容
                value += c;
                advance();
            } else {
                advance();  // 跳过结束引号
                break;
            }
        }
        else {
            // 原始字符串中，所有字符都原样保留，包括反斜杠
            value += c;
            advance();
        }
        
        // 处理换行（单引号原始字符串不允许换行）
        if (c == '\n' && !triple) {
            pos_ = start;  // 回退到开始位置
            return error("Unterminated raw string literal");
        }
    }
    
    if (isAtEnd()) {
        pos_ = start;  // 回退到开始位置
        return error("Unterminated raw string literal");
    }
    
    // 检查是否成功读取了结束引号
    // 对于非三引号的字符串，最后一个字符应该是引号
    if (!triple && pos_ > start && source_[pos_ - 1] != quote) {
        pos_ = start;  // 回退到开始位置
        return error("Unterminated raw string literal");
    }
    
    return makeToken(TokenType::RawString, start, pos_ - start, std::move(value));
}

// 扫描字节串 (b"..." 或 B"...")
Token Tokenizer::scanBytes() {
    size_t start = pos_;
    size_t startLine = line_;
    size_t startColumn = column_;
    bool isRaw = false;
    
    // 检查前缀
    if (peek() == 'b' || peek() == 'B') {
        advance();  // 跳过 b/B
    }
    
    // 检查是否有原始前缀 (br 或 rb)
    if (peek() == 'r' || peek() == 'R') {
        isRaw = true;
        advance();  // 跳过 r/R
    }
    
    // 确保后面是引号
    char quote = peek();
    if (quote != '\'' && quote != '"') {
        pos_ = start;
        return scanIdentifier();
    }
    
    advance();  // 跳过引号
    bool triple = false;
    
    // 检查三引号
    if (peek() == quote && peek(1) == quote) {
        triple = true;
        advance();  // 跳过第二个引号
        advance();  // 跳过第三个引号
    }
    
    std::vector<uint8_t> value;  // 明确使用 vector<uint8_t>
    
    while (!isAtEnd()) {
        char c = peek();
        
        // 检查是否结束
        if (c == quote) {
            if (triple) {
                if (peek(1) == quote && peek(2) == quote) {
                    advance();
                    advance();
                    advance();
                    break;
                }
                value.push_back(static_cast<uint8_t>(c));
                advance();
            } else {
                advance();
                break;
            }
        }
        // 处理转义序列
        else if (c == '\\' && !isRaw) {
            advance();  // 跳过 \
            if (isAtEnd()) break;
            
            char escaped = advance();
            
            switch (escaped) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case 'r': value.push_back('\r'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case '\\': value.push_back('\\'); break;
                case '\'': value.push_back('\''); break;
                case '"': value.push_back('"'); break;
                
                case 'x':
                case 'X': {
                    if (isHexDigit(peek()) && isHexDigit(peek(1))) {
                        char h1 = advance();
                        char h2 = advance();
                        
                        uint8_t high = hexToDigit(h1);
                        uint8_t low = hexToDigit(h2);
                        uint8_t val = (high << 4) | low;
                        
                        value.push_back(val);
                    } else {
                        pos_ = start;
                        return error("Invalid \\x escape sequence in bytes literal");
                    }
                    break;
                }
                
                default:
                    // 对于未知转义，保留反斜杠和字符
                    value.push_back('\\');
                    value.push_back(static_cast<uint8_t>(escaped));
                    break;
            }
        }
        else {
            value.push_back(static_cast<uint8_t>(c));
            advance();
        }
    }
    
    if (isAtEnd()) {
        pos_ = start;
        return error("Unterminated bytes literal");
    }
    
    // 确保返回 vector<uint8_t>
    return makeToken(TokenType::Bytes, start, pos_ - start, value);
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
        case TokenType::FString: return "FSTRING";
        case TokenType::FStringText: return "FSTRING_TEXT";
        case TokenType::FStringExpr: return "FSTRING_EXPR";
        case TokenType::FStringConv: return "FSTRING_CONV";
        case TokenType::FStringSpec: return "FSTRING_SPEC";
        case TokenType::FStringEnd: return "FSTRING_END";
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
        case TokenType::RawString: return "RAWSTRING";
        case TokenType::Bytes: return "BYTES";
        case TokenType::Error: return "ERRORTOKEN";
        case TokenType::False_: return "FALSE";
        case TokenType::None_: return "NONE";
        case TokenType::True_: return "TRUE";
        case TokenType::And: return "AND";
        case TokenType::Or: return "OR";
        case TokenType::Not: return "NOT";
        case TokenType::As: return "AS";
        case TokenType::Assert: return "ASSERT";
        case TokenType::Break: return "BREAK";
        case TokenType::Class: return "CLASS";
        case TokenType::Continue: return "CONTINUE";
        case TokenType::Def: return "DEF";
        case TokenType::Del: return "DEL";
        case TokenType::Elif: return "ELIF";
        case TokenType::Else: return "ELSE";
        case TokenType::Except: return "EXCEPT";
        case TokenType::Finally: return "FINALLY";
        case TokenType::For: return "FOR";
        case TokenType::From: return "FROM";
        case TokenType::Global: return "GLOBAL";
        case TokenType::If: return "IF";
        case TokenType::Import: return "IMPORT";
        case TokenType::In: return "IN";
        case TokenType::Is: return "IS";
        case TokenType::Lambda: return "LAMBDA";
        case TokenType::Nonlocal: return "NONLOCAL";
        case TokenType::Pass: return "PASS";
        case TokenType::Raise: return "RAISE";
        case TokenType::Return: return "RETURN";
        case TokenType::Try: return "TRY";
        case TokenType::While: return "WHILE";
        case TokenType::With: return "WITH";
        case TokenType::Yield: return "YIELD";
        case TokenType::Match: return "MATCH";
        case TokenType::Case: return "CASE";
        case TokenType::Type: return "TYPE";
        case TokenType::Go: return "GO";
        case TokenType::Chan: return "CHAN";
        case TokenType::Select: return "SELECT";
        case TokenType::Defer: return "DEFER";
        case TokenType::Const: return "CONST";
        case TokenType::Let: return "LET";
        case TokenType::Mut: return "MUT";
        case TokenType::Struct: return "STRUCT";
        case TokenType::Impl: return "IMPL";
        case TokenType::Pub: return "PUB";
        case TokenType::Use: return "USE";
        case TokenType::Mod: return "MOD";
        default: return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << token.toString();
    return os;
}


// 辅助函数：将十六进制字符转换为数字
static uint8_t hexToDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return 0;
}

// 辅助函数：将八进制字符转换为数字
static uint8_t octToDigit(char c) {
    if (c >= '0' && c <= '7') return c - '0';
    return 0;
}

} // namespace csgo