/**
 * @file tokenizer.cpp
 * @brief CSGO 编程语言词法分析器实现文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件实现了 CSGO 语言的词法分析器（Tokenizer）功能。
 * 基于 CPython 的 tokenizer.c 设计，实现了完整的词法分析逻辑。
 *
 * @section features 功能特性
 * - 关键字识别和映射
 * - 标识符、数字、字符串的扫描
 * - 操作符和分隔符解析
 * - f-string 表达式插值解析
 * - Python 风格缩进处理
 * - 完善的错误报告机制
 */

#include "lexer/tokenizer.h"
#include <cctype>
#include <unordered_map>
#include <sstream>
#include <stack>

namespace csgo {

/**
 * @brief 关键字映射表
 *
 * 将关键字字符串映射到对应的 TokenType。
 * 使用 unordered_map 实现 O(1) 查找。
 *
 * @note 包含 Python 核心关键字和 CSGO 扩展关键字
 */
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
    {"go", TokenType::Go},
    {"chan", TokenType::Chan},
    {"select", TokenType::Select},
    {"defer", TokenType::Defer},
    {"const", TokenType::Const},
    {"let", TokenType::Let},
    {"mut", TokenType::Mut},
    {"struct", TokenType::Struct},
    {"impl", TokenType::Impl},
    {"pub", TokenType::Pub},
    {"use", TokenType::Use},
    {"mod", TokenType::Mod},
};

// ============================================================================
// Token 实现
// ============================================================================

/**
 * @brief 将 Token 转换为字符串表示
 *
 * 格式化输出 Token 的类型、位置和文本内容。
 * 格式：TYPE(line:column) "text"
 *
 * @return 格式化的字符串
 */
std::string Token::toString() const {
    std::ostringstream oss;
    oss << tokenTypeName(type) << "(" << line << ":" << column << ")";
    if (!text.empty()) {
        oss << " \"" << text << "\"";
    }
    return oss.str();
}

// ============================================================================
// Tokenizer 构造函数
// ============================================================================

/**
 * @brief 从字符串构造 Tokenizer
 * @param source 源代码字符串（通过移动语义接管所有权）
 */
Tokenizer::Tokenizer(std::string source) 
    : source_(std::move(source))
    , filename_("<string>")
    , view_(source_)
{}

/**
 * @brief 从输入流构造 Tokenizer
 * @param input 输入流对象
 * @param filename 文件名（用于错误报告）
 */
Tokenizer::Tokenizer(std::istream& input, std::string filename)
    : filename_(std::move(filename))
{
    std::ostringstream oss;
    oss << input.rdbuf();
    source_ = oss.str();
    view_ = source_;
}

// ============================================================================
// 状态查询方法
// ============================================================================

/**
 * @brief 检查是否到达输入末尾
 * @return true 如果当前位置 >= 源字符串长度
 */
bool Tokenizer::isAtEnd() const {
    return pos_ >= source_.length();
}

/**
 * @brief 检查是否发生错误
 * @return true 如果有错误发生
 */
bool Tokenizer::hasError() const {
    return hasError_;
}

/**
 * @brief 获取错误消息
 * @return 错误消息字符串引用
 */
const std::string& Tokenizer::errorMessage() const {
    return errorMessage_;
}

// ============================================================================
// 字符读取辅助方法
// ============================================================================

/**
 * @brief 查看当前字符（不消耗）
 * @return 当前字符，到末尾返回 '\0'
 */
char Tokenizer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

/**
 * @brief 查看指定偏移的字符（不消耗）
 * @param offset 相对于当前位置的偏移量
 * @return 指定位置的字符，到末尾返回 '\0'
 */
char Tokenizer::peek(size_t offset) const {
    if (pos_ + offset >= source_.length()) return '\0';
    return source_[pos_ + offset];
}

/**
 * @brief 读取并推进当前位置
 *
 * 读取当前字符并移动到下一个位置。
 * 自动处理换行符，更新行列号。
 *
 * @return 读取的字符
 */
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

/**
 * @brief 尝试匹配并消费指定字符
 * @param expected 期望匹配的字符
 * @return true 如果匹配成功并消费，false 如果不匹配
 */
bool Tokenizer::match(char expected) {
    if (peek() == expected) {
        advance();
        return true;
    }
    return false;
}

// ============================================================================
// 空白和注释处理
// ============================================================================

/**
 * @brief 跳过空白字符
 *
 * 跳过空格、制表符、回车符、换页符。
 * 处理行继续符（backslash + newline）。
 */
void Tokenizer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f') {
            advance();
        } else if (c == '\\') {
            // 行继续符：反斜杠后面紧跟换行
            advance();
            if (peek() == '\n') {
                advance();
                line_++;
                column_ = 1;
                lineStart_ = pos_;
            } else {
                // 不是行继续符，回退位置
                pos_--;
                break;
            }
        } else {
            break;
        }
    }
}

/**
 * @brief 跳过注释
 *
 * 从 # 字符开始，跳过到行尾的所有字符。
 */
void Tokenizer::skipComment() {
    if (peek() == '#') {
        while (peek() != '\n' && !isAtEnd()) {
            advance();
        }
    }
}

// ============================================================================
// Token 工厂方法
// ============================================================================

/**
 * @brief 创建无值的 Token
 *
 * 用于创建不需要解析值的 Token，如操作符、分隔符等。
 *
 * @param type Token 类型
 * @param start 起始位置
 * @param length Token 长度
 * @return 构造的 Token
 */
Token Tokenizer::makeToken(TokenType type, size_t start, size_t length) {
    return Token{
        type,
        std::string(source_.data() + start, length),
        std::monostate{},
        line_,
        column_ - length
    };
}

/**
 * @brief 创建带值的 Token
 *
 * 用于创建需要解析值的 Token，如数字、字符串等。
 * 将 variant 值正确存储到 Token 的 value 字段中。
 *
 * @param type Token 类型
 * @param start 起始位置
 * @param length Token 长度
 * @param value 要存储的值
 * @return 构造的 Token
 */
Token Tokenizer::makeToken(TokenType type, size_t start, size_t length,
                           std::variant<int64_t, double, std::string, std::vector<uint8_t>> value) {
    Token token{
        type,
        std::string(source_.data() + start, length),
        std::monostate{},
        line_,
        column_ - length
    };
    // 根据 value 的实际类型存储到 variant 中
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

// ============================================================================
// 错误处理
// ============================================================================

/**
 * @brief 创建错误 Token
 *
 * 设置错误状态并返回 Error 类型的 Token。
 *
 * @param message 错误消息
 * @return Error 类型的 Token
 */
Token Tokenizer::error(const std::string& message) {
    reportError(message);
    return makeToken(TokenType::Error, pos_, 0);
}

/**
 * @brief 报告错误
 *
 * 设置错误状态，格式化错误消息并输出到标准错误。
 *
 * @param message 错误消息
 */
void Tokenizer::reportError(const std::string& message) {
    hasError_ = true;
    std::ostringstream oss;
    oss << filename_ << ":" << line_ << ":" << column_ << ": " << message;
    errorMessage_ = oss.str();
    std::cerr << errorMessage_ << std::endl;
}

// ============================================================================
// 字符分类工具方法
// ============================================================================

/**
 * @brief 检查是否为标识符起始字符
 *
 * 标识符起始字符包括：
 * - 大写字母 A-Z
 * - 小写字母 a-z
 * - 下划线 _
 * - ASCII 128 以上的字符（Unicode 标识符）
 *
 * @param c 要检查的字符
 * @return true 如果是标识符起始字符
 */
bool Tokenizer::isIdentifierStart(char c) {
    return (c >= 'a' && c <= 'z') || 
           (c >= 'A' && c <= 'Z') || 
           c == '_' ||
           (static_cast<unsigned char>(c) >= 128);
}

/**
 * @brief 检查是否为标识符字符
 *
 * 标识符字符包括标识符起始字符加上数字 0-9。
 *
 * @param c 要检查的字符
 * @return true 如果是标识符字符
 */
bool Tokenizer::isIdentifierChar(char c) {
    return isIdentifierStart(c) || (c >= '0' && c <= '9');
}

/**
 * @brief 检查是否为十进制数字
 * @param c 要检查的字符
 * @return true 如果是 0-9
 */
bool Tokenizer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

/**
 * @brief 检查是否为十六进制数字
 * @param c 要检查的字符
 * @return true 如果是 0-9, a-f, A-F
 */
bool Tokenizer::isHexDigit(char c) {
    return isDigit(c) || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

/**
 * @brief 检查是否为八进制数字
 * @param c 要检查的字符
 * @return true 如果是 0-7
 */
bool Tokenizer::isOctDigit(char c) {
    return c >= '0' && c <= '7';
}

/**
 * @brief 检查是否为二进制数字
 * @param c 要检查的字符
 * @return true 如果是 0 或 1
 */
bool Tokenizer::isBinDigit(char c) {
    return c == '0' || c == '1';
}

/**
 * @brief 检查关键字
 *
 * 在关键字表中查找文本，返回匹配的 TokenType。
 * 如果未找到，返回 Name 类型。
 *
 * @param text 要检查的文本视图
 * @return 匹配的 TokenType 或 Name
 */
TokenType Tokenizer::checkKeyword(std::string_view text) {
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return it->second;
    }
    return TokenType::Name;
}

// ============================================================================
// 核心分词逻辑
// ============================================================================

/**
 * @brief 获取下一个 Token
 *
 * Tokenizer 的核心方法，负责解析源代码的下一个词法单元。
 * 处理：
 * - 空白和注释跳过
 * - 缩进处理（Indent/Dedent）
 * - 字符串解析（包括各种前缀）
 * - 标识符和关键字
 * - 数字
 * - 操作符
 *
 * @return 解析得到的 Token
 */
Token Tokenizer::nextToken() {
    // 1. 先返回待处理的缩进/dedent
    if (!pendingIndents_.empty()) {
        Token token = pendingIndents_.back();
        pendingIndents_.pop_back();
        return token;
    }
    
    // 2. 如果在 f-string 中，继续解析 f-string 的各个部分
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
            size_t start = pos_;
            advance();
            if (fstringState_->quoteType == FStringState::TRIPLE_SINGLE ||
                fstringState_->quoteType == FStringState::TRIPLE_DOUBLE) {
                advance(); advance();
            }
            
            fstringState_.reset();
            return makeToken(TokenType::FStringEnd, start, pos_ - start);
        }
        
        // 根据当前位置决定解析哪个部分
        if (c == '{' && !fstringState_->isRaw) {
            if (peek(1) == '{') {
                return scanFStringText();
            } else {
                return scanFStringExpression();
            }
        } else if (c == '!' && !fstringState_->isRaw) {
            return scanFStringConversion();
        } else if (c == ':' && !fstringState_->isRaw) {
            return scanFStringFormatSpec();
        } else {
            return scanFStringText();
        }
    }
    
    // 3. 正常的 token 解析逻辑
    skipWhitespace();
    skipComment();
    
    // 4. 检查是否到达文件末尾
    if (isAtEnd()) {
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
    
    // 5. 检测字符串（前缀 r, b, f 及其组合）
    if (c == '"' || c == '\'') {
        return scanString();
    }
    
    bool isPrefixChar = (c == 'r' || c == 'R' || c == 'b' || c == 'B' || c == 'f' || c == 'F');
    
    if (isPrefixChar) {
        char next = peek(1);
        char next2 = peek(2);
        
        // 单前缀后跟引号
        if (next == '"' || next == '\'') {
            return scanString();
        }
        
        // 双前缀后跟引号
        bool isSecondPrefix = (next == 'r' || next == 'R' || 
                               next == 'b' || next == 'B' || 
                               next == 'f' || next == 'F');
        if (isSecondPrefix && (next2 == '"' || next2 == '\'')) {
            return scanString();
        }
        
        // fr"..." 或 rf"..."
        if ((c == 'f' || c == 'F') && 
            (next == 'r' || next == 'R') && 
            (next2 == '"' || next2 == '\'')) {
            return scanString();
        }
        
        if ((c == 'r' || c == 'R') && 
            (next == 'f' || next == 'F') && 
            (next2 == '"' || next2 == '\'')) {
            return scanString();
        }
    }
    
    // 6. 换行处理
    if (c == '\n') {
        advance();
        return handleNewline();
    }
    
    // 7. 标识符/关键字
    if (isIdentifierStart(c)) {
        return scanIdentifier();
    }
    
    // 8. 数字
    if (isDigit(c)) {
        return scanNumber();
    }
    
    // 9. 操作符
    return scanOperator();
}

// ============================================================================
// 标识符扫描
// ============================================================================

/**
 * @brief 扫描标识符
 *
 * 扫描连续的标识符字符，识别关键字或普通标识符。
 * 格式：[a-zA-Z_][a-zA-Z0-9_]*
 *
 * @return Name 或关键字类型的 Token
 */
Token Tokenizer::scanIdentifier() {
    size_t start = pos_;
    while (isIdentifierChar(peek())) {
        advance();
    }
    
    std::string text(source_.data() + start, pos_ - start);
    TokenType type = checkKeyword(text);
    
    return makeToken(type, start, pos_ - start, text);
}

// ============================================================================
// 数字扫描
// ============================================================================

/**
 * @brief 扫描数字字面量
 *
 * 支持以下数字格式：
 * - 十进制整数：123, 456789
 * - 下划线分隔符：1_000_000
 * - 八进制：0o755
 * - 十六进制：0xFF
 * - 二进制：0b1010
 * - 浮点数：3.14, 1.5e10
 *
 * @return Number 类型的 Token，值存储为 int64_t 或 double
 */
Token Tokenizer::scanNumber() {
    size_t start = pos_;
    bool hasDot = false;
    bool hasExp = false;
    bool hasHex = false;
    bool hasBin = false;
    bool hasOct = false;
    
    // 处理前缀
    if (peek() == '0') {
        char next = peek(1);
        if (next == 'x' || next == 'X') {
            hasHex = true;
            advance(); advance();
            while (isHexDigit(peek()) || peek() == '_') {
                if (peek() != '_') advance();
                else advance();
            }
        } else if (next == 'o' || next == 'O') {
            hasOct = true;
            advance(); advance();
            while (isOctDigit(peek()) || peek() == '_') {
                if (peek() != '_') advance();
                else advance();
            }
        } else if (next == 'b' || next == 'B') {
            hasBin = true;
            advance(); advance();
            while (isBinDigit(peek()) || peek() == '_') {
                if (peek() != '_') advance();
                else advance();
            }
        }
    }
    
    // 处理整数部分
    while (isDigit(peek()) || peek() == '_') {
        if (peek() != '_') advance();
        else advance();
    }
    
    // 处理小数部分
    if (peek() == '.') {
        hasDot = true;
        advance();
        while (isDigit(peek()) || peek() == '_') {
            if (peek() != '_') advance();
            else advance();
        }
    }
    
    // 处理指数部分
    if ((peek() == 'e' || peek() == 'E') && !hasHex && !hasBin && !hasOct) {
        hasExp = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        while (isDigit(peek()) || peek() == '_') {
            if (peek() != '_') advance();
            else advance();
        }
    }
    
    std::string_view text(source_.data() + start, pos_ - start);
    
    // 解析数值
    if (hasHex) {
        uint64_t val = 0;
        bool inUnderscore = false;
        for (size_t i = 2; i < text.size(); i++) {
            char c = text[i];
            if (c == '_') {
                inUnderscore = true;
                continue;
            }
            val = (val << 4) | hexToDigit(c);
        }
        return makeToken(TokenType::Number, start, pos_ - start, static_cast<int64_t>(val));
    } else if (hasBin) {
        uint64_t val = 0;
        for (size_t i = 2; i < text.size(); i++) {
            char c = text[i];
            if (c == '_') continue;
            val = (val << 1) | (c - '0');
        }
        return makeToken(TokenType::Number, start, pos_ - start, static_cast<int64_t>(val));
    } else if (hasOct) {
        uint64_t val = 0;
        for (size_t i = 2; i < text.size(); i++) {
            char c = text[i];
            if (c == '_') continue;
            val = (val << 3) | octToDigit(c);
        }
        return makeToken(TokenType::Number, start, pos_ - start, static_cast<int64_t>(val));
    } else if (hasDot || hasExp) {
        std::string str;
        for (char c : text) {
            if (c != '_') str += c;
        }
        double val = std::stod(str);
        return makeToken(TokenType::Number, start, pos_ - start, val);
    } else {
        std::string str;
        for (char c : text) {
            if (c != '_') str += c;
        }
        int64_t val = std::stoll(str);
        return makeToken(TokenType::Number, start, pos_ - start, val);
    }
}

// ============================================================================
// 字符串扫描
// ============================================================================

/**
 * @brief 扫描字符串字面量
 *
 * 支持以下字符串类型：
 * - 普通字符串："..." 或 '...'
 * - 原始字符串：r"..." 或 R"..."
 * - 字节串：b"..." 或 B"..."
 * - f-string：f"..." 或 F"..."
 * - 三引号字符串："""...""" 或 '''...'''
 * - 组合前缀：rb, br, rf, fr, bf, fb 等
 *
 * @return 对应类型的 String Token
 */
Token Tokenizer::scanString() {
    size_t start = pos_;
    
    // 解析前缀
    bool isFString = false;
    bool isRawString = false;
    bool isBytesString = false;
    
    while (peek() == 'f' || peek() == 'F' || 
           peek() == 'r' || peek() == 'R' || 
           peek() == 'b' || peek() == 'B') {
        char prefix = advance();
        if (prefix == 'f' || prefix == 'F') isFString = true;
        else if (prefix == 'r' || prefix == 'R') isRawString = true;
        else if (prefix == 'b' || prefix == 'B') isBytesString = true;
        
        if (peek() == '"' || peek() == '\'') break;
    }
    
    // 解析引号
    char quote = advance();
    bool triple = false;
    
    if (peek() == quote && peek(1) == quote) {
        triple = true;
        advance();
        advance();
    }
    
    std::string value;
    
    // 扫描字符串内容
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
        
        // 转义序列处理
        if (c == '\\' && !isRawString) {
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
                        value += static_cast<char>((hexToDigit(h1) << 4) | hexToDigit(h2));
                    } else {
                        value += escaped;
                    }
                    break;
                }
                default: value += escaped; break;
            }
        } else {
            value += c;
        }
    }
    
    // 确定 Token 类型
    TokenType tokenType = TokenType::String;
    if (isFString) tokenType = TokenType::FString;
    else if (isRawString) tokenType = TokenType::RawString;
    else if (isBytesString) tokenType = TokenType::Bytes;
    
    // Bytes 类型需要转换为 vector<uint8_t>
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

// ============================================================================
// 原始字符串扫描（保留兼容）
// ============================================================================

/**
 * @brief 扫描原始字符串
 *
 * 原始字符串不处理转义序列，反斜杠被视为普通字符。
 *
 * @return RawString 类型的 Token
 */
Token Tokenizer::scanRawString() {
    size_t start = pos_;
    size_t startLine = line_;
    size_t startColumn = column_;
    bool isRaw = false;
    
    // 检查前缀
    if (peek() == 'r' || peek() == 'R') {
        isRaw = true;
        advance();
    }
    
    // 确保后面是引号
    char quote = peek();
    if (quote != '\'' && quote != '"') {
        pos_ = start;
        return scanIdentifier();
    }
    
    advance();
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
                value += c;
            } else {
                break;
            }
        } else if (c == '\\' && !isRaw) {
            if (isAtEnd()) break;
            char escaped = advance();
            switch (escaped) {
                case '\n': break;  // 忽略行继续符
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '\'': value += '\''; break;
                case '"': value += '"'; break;
                default: value += escaped; break;
            }
        } else {
            value += c;
        }
    }
    
    if (isAtEnd()) {
        pos_ = start;
        return error("Unterminated raw string literal");
    }
    
    // 检查是否成功读取结束引号
    if (!triple && pos_ > start && source_[pos_ - 1] != quote) {
        pos_ = start;
        return error("Unterminated raw string literal");
    }
    
    return makeToken(TokenType::RawString, start, pos_ - start, std::move(value));
}

// ============================================================================
// 字节串扫描（保留兼容）
// ============================================================================

/**
 * @brief 扫描字节串
 *
 * 字节串字面量以 b 或 B 前缀开头。
 *
 * @return Bytes 类型的 Token，值为 vector<uint8_t>
 */
Token Tokenizer::scanBytes() {
    size_t start = pos_;
    size_t startLine = line_;
    size_t startColumn = column_;
    bool isRaw = false;
    
    // 检查前缀
    if (peek() == 'b' || peek() == 'B') {
        advance();
    }
    
    // 检查原始前缀
    if (peek() == 'r' || peek() == 'R') {
        isRaw = true;
        advance();
    }
    
    // 确保后面是引号
    char quote = peek();
    if (quote != '\'' && quote != '"') {
        pos_ = start;
        return scanIdentifier();
    }
    
    advance();
    bool triple = false;
    
    if (peek() == quote && peek(1) == quote) {
        triple = true;
        advance();
        advance();
    }
    
    std::vector<uint8_t> value;
    
    while (!isAtEnd()) {
        char c = peek();
        
        // 检查结束
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
        // 转义序列
        else if (c == '\\' && !isRaw) {
            advance();
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
    
    return makeToken(TokenType::Bytes, start, pos_ - start, value);
}

// ============================================================================
// f-string 解析方法
// ============================================================================

/**
 * @brief 扫描 f-string 主入口
 *
 * 识别 f-string 前缀，初始化 f-string 状态。
 *
 * @return FString 类型的 Token
 */
Token Tokenizer::scanFString() {
    size_t start = pos_;
    size_t startLine = line_;
    size_t startColumn = column_;
    bool isRaw = false;
    
    // 检查前缀组合
    while (true) {
        char c = peek();
        if (c == 'f' || c == 'F') {
            advance();
        } else if ((c == 'r' || c == 'R') && !isRaw) {
            isRaw = true;
            advance();
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
            advance();
            advance();
        } else {
            quoteType = FStringState::SINGLE;
        }
    } else if (quote == '"') {
        if (peek() == '"' && peek(1) == '"') {
            quoteType = FStringState::TRIPLE_DOUBLE;
            isTriple = true;
            advance();
            advance();
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
    
    return makeToken(TokenType::FString, start, pos_ - start);
}

/**
 * @brief 扫描 f-string 文本部分
 *
 * 扫描 f-string 中不属于表达式的文本内容。
 * 处理转义序列和转义的 {{ }}。
 *
 * @return FStringText 类型的 Token
 */
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
                text += '{';
                advance();
                advance();
                continue;
            } else {
                break;
            }
        }
        
        // 检查表达式结束
        if (c == '}' && !fstringState_->isRaw) {
            if (peek(1) == '}') {
                text += '}';
                advance();
                advance();
                continue;
            } else {
                return error("f-string: single '}' is not allowed outside expression");
            }
        }
        
        // 处理转义字符
        if (c == '\\' && !fstringState_->isRaw) {
            advance();
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
        
        text += c;
        advance();
    }
    
    if (text.empty()) {
        return nextToken();
    }
    
    return makeToken(TokenType::FStringText, start, pos_ - start, text);
}

/**
 * @brief 扫描 f-string 表达式
 *
 * 扫描 {expr} 中的表达式内容。
 * 支持嵌套花括号和括号。
 *
 * @return FStringExpr 类型的 Token
 */
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
    
    advance();
    fstringState_->nestedBraceLevel = 0;
    
    std::string expr;
    std::stack<char> brackets;
    
    while (!isAtEnd()) {
        char c = peek();
        
        if (c == '{') {
            if (!fstringState_->isRaw) {
                brackets.push('{');
                fstringState_->nestedBraceLevel++;
            }
            expr += c;
            advance();
        }
        else if (c == '}') {
            if (fstringState_->nestedBraceLevel > 0) {
                if (!brackets.empty() && brackets.top() == '{') {
                    brackets.pop();
                    fstringState_->nestedBraceLevel--;
                }
                expr += c;
                advance();
            } else {
                advance();
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
            expr += c;
            advance();
            char quote = c;
            
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
    
    Token exprToken = makeToken(TokenType::FStringExpr, start, pos_ - start, expr);
    
    // 检查转换标记或格式说明符
    char next = peek();
    if (next == '!') {
        return exprToken;
    } else if (next == ':') {
        return exprToken;
    }
    
    return exprToken;
}

/**
 * @brief 扫描 f-string 转换标记
 *
 * 扫描 !r, !s, !a 转换标记。
 *
 * @return FStringConv 类型的 Token
 */
Token Tokenizer::scanFStringConversion() {
    if (!fstringState_) {
        return error("Internal error: No f-string state");
    }
    
    size_t start = pos_;
    
    if (peek() != '!') {
        return error("Expected '!' for f-string conversion");
    }
    
    advance();
    
    if (isAtEnd()) {
        return error("Unexpected end of f-string conversion");
    }
    
    char conv = advance();
    std::string convStr;
    
    switch (conv) {
        case 'r': convStr = "!r"; break;
        case 's': convStr = "!s"; break;
        case 'a': convStr = "!a"; break;
        default: return error("Invalid conversion character in f-string: expected 'r', 's', or 'a'");
    }
    
    Token convToken = makeToken(TokenType::FStringConv, start, pos_ - start, convStr);
    
    if (peek() == ':') {
        return convToken;
    }
    
    return convToken;
}

/**
 * @brief 扫描 f-string 格式说明符
 *
 * 扫描 : 后面的格式说明符内容。
 *
 * @return FStringSpec 类型的 Token
 */
Token Tokenizer::scanFStringFormatSpec() {
    if (!fstringState_) {
        return error("Internal error: No f-string state");
    }
    
    size_t start = pos_;
    
    if (peek() != ':') {
        return error("Expected ':' for f-string format specifier");
    }
    
    advance();
    
    std::string spec;
    bool inBraces = false;
    
    while (!isAtEnd()) {
        char c = peek();
        
        if (c == '{' && !fstringState_->isRaw) {
            return error("f-string: nested expressions inside format specifier are not allowed");
        }
        
        if (c == '}') {
            advance();
            break;
        }
        
        spec += c;
        advance();
    }
    
    return makeToken(TokenType::FStringSpec, start, pos_ - start, spec);
}

// ============================================================================
// 操作符扫描
// ============================================================================

/**
 * @brief 扫描操作符和分隔符
 *
 * 识别单字符、双字符和三字符操作符。
 *
 * @return 对应类型的 Token
 */
Token Tokenizer::scanOperator() {
    size_t start = pos_;
    char c1 = advance();
    char c2 = peek();
    char c3 = peek(1);
    
    // 三字符操作符
    if (c1 == '.' && c2 == '.' && c3 == '.') {
        advance();
        advance();
        return makeToken(TokenType::Ellipsis, start, 3);
    }
    
    // 双字符操作符
    switch (c1) {
        case '+':
            if (c2 == '=') { advance(); return makeToken(TokenType::PlusEqual, start, 2); }
            if (c2 == '+') { advance(); return makeToken(TokenType::Error, start, 2); }
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
            if (c2 == '&') { advance(); return makeToken(TokenType::Error, start, 2); }
            break;
        case '|':
            if (c2 == '=') { advance(); return makeToken(TokenType::VBarEqual, start, 2); }
            if (c2 == '|') { advance(); return makeToken(TokenType::Error, start, 2); }
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

// ============================================================================
// 缩进处理
// ============================================================================

/**
 * @brief 处理缩进变化
 *
 * Python 风格的缩进处理，维护缩进栈，
 * 生成 Indent 和 Dedent Token。
 *
 * @return Indent、Dedent 或 NewLine Token
 */
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

// ============================================================================
// 一次性分词接口
// ============================================================================

/**
 * @brief 对整个输入进行词法分析
 *
 * 循环调用 nextToken() 直到 EndMarker。
 *
 * @return Token 向量，包含所有解析结果
 */
std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    
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
    
    if (tokens.empty() || tokens.back().type != TokenType::EndMarker) {
        tokens.push_back(makeToken(TokenType::EndMarker, pos_, 0));
    }
    
    return tokens;
}

// ============================================================================
// 辅助函数实现
// ============================================================================

/**
 * @brief 获取 TokenType 的字符串名称
 * @param type Token 类型
 * @return 类型名称字符串
 */
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
        case TokenType::NotIn: return "NOTIN";
        case TokenType::Is: return "IS";
        case TokenType::IsNot: return "ISNOT";
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

/**
 * @brief 处理换行后的缩进逻辑
 * @details 计算新行的缩进级别，生成 NewLine、Indent 或 Dedent token
 * @return NewLine、Indent 或 Dedent 类型的 Token
 */
Token Tokenizer::handleNewline() {
    // 计算新行的缩进
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
    
    // 跳过空行（另一个换行）和注释行
    if (peek() == '\n' || peek() == '#') {
        return nextToken();
    }
    
    // 关键修复：文件结束也要生成 NewLine
    // 先计算缩进变化，生成 pending tokens，然后返回 NewLine
    
    size_t currentIndent = indentStack_.back();
    
    if (col > currentIndent) {
        // 缩进增加：先返回 NewLine，下次返回 Indent
        indentStack_.push_back(col);
        pendingIndents_.push_back(makeToken(TokenType::Indent, pos_ - col, col));
        return makeToken(TokenType::NewLine, pos_ - col, 0);
    } else if (col < currentIndent) {
        // 缩进减少：先返回 NewLine，然后 pendingIndents 返回 Dedent(s)
        while (col < indentStack_.back()) {
            indentStack_.pop_back();
            pendingIndents_.push_back(makeToken(TokenType::Dedent, pos_, 0));
        }
        if (col != indentStack_.back()) {
            return error("Inconsistent dedent");
        }
        return makeToken(TokenType::NewLine, pos_ - col, 0);
    }
    
    // 缩进相同：返回 NewLine
    return makeToken(TokenType::NewLine, pos_ - col, 0);
}

/**
 * @brief Token 输出流重载
 */
std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << token.toString();
    return os;
}

/**
 * @brief 将十六进制字符转换为数值
 * @param c 十六进制字符
 * @return 对应数值，无效返回 0
 */
static uint8_t hexToDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return 0;
}

/**
 * @brief 将八进制字符转换为数值
 * @param c 八进制字符
 * @return 对应数值，无效返回 0
 */
static uint8_t octToDigit(char c) {
    if (c >= '0' && c <= '7') return c - '0';
    return 0;
}

} // namespace csgo