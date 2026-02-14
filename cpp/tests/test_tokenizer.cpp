// cpp/tests/lexer/test_tokenizer.cpp
#include "lexer/tokenizer.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <vector>

using namespace csgo;

// 辅助函数：检查 token 类型
void checkToken(const Token& token, TokenType expectedType, 
                const std::string& expectedText = "",
                size_t expectedLine = 0) {
    if (token.type != expectedType) {
        std::cerr << "FAIL: Expected " << tokenTypeName(expectedType) 
                  << ", got " << tokenTypeName(token.type) << std::endl;
        std::cerr << "  Token: " << token << std::endl;
        assert(false);
    }
    
    if (!expectedText.empty() && token.text != expectedText) {
        std::cerr << "FAIL: Expected text '" << expectedText 
                  << "', got '" << token.text << "'" << std::endl;
        assert(false);
    }
    
    if (expectedLine > 0 && token.line != expectedLine) {
        std::cerr << "FAIL: Expected line " << expectedLine 
                  << ", got " << token.line << std::endl;
        assert(false);
    }
}

// 测试1：空输入
void testEmpty() {
    std::cout << "Test: Empty input... ";
    Tokenizer tokenizer("");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 1);
    checkToken(tokens[0], TokenType::EndMarker);
    std::cout << "PASSED" << std::endl;
}

// 测试2：简单标识符
void testIdentifier() {
    std::cout << "Test: Identifier... ";
    Tokenizer tokenizer("hello");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 2);
    checkToken(tokens[0], TokenType::Name, "hello");
    checkToken(tokens[1], TokenType::EndMarker);
    std::cout << "PASSED" << std::endl;
}

// 测试3：数字（整数）
void testInteger() {
    std::cout << "Test: Integer... ";
    Tokenizer tokenizer("42");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 2);
    checkToken(tokens[0], TokenType::Number, "42");
    checkToken(tokens[1], TokenType::EndMarker);
    
    // 检查值
    assert(std::holds_alternative<int64_t>(tokens[0].value));
    assert(std::get<int64_t>(tokens[0].value) == 42);
    std::cout << "PASSED" << std::endl;
}

// 测试4：数字（浮点）
void testFloat() {
    std::cout << "Test: Float... ";
    Tokenizer tokenizer("3.14");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 2);
    checkToken(tokens[0], TokenType::Number, "3.14");
    
    assert(std::holds_alternative<double>(tokens[0].value));
    assert(std::get<double>(tokens[0].value) == 3.14);
    std::cout << "PASSED" << std::endl;
}

// 测试5：字符串
void testString() {
    std::cout << "Test: String... ";
    Tokenizer tokenizer("\"hello world\"");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 2);
    checkToken(tokens[0], TokenType::String, "\"hello world\"");
    
    assert(std::holds_alternative<std::string>(tokens[0].value));
    assert(std::get<std::string>(tokens[0].value) == "hello world");
    std::cout << "PASSED" << std::endl;
}

// 测试6：操作符
void testOperators() {
    std::cout << "Test: Operators... ";
    Tokenizer tokenizer("+ - * / == != <= >=");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 9);
    checkToken(tokens[0], TokenType::Plus);
    checkToken(tokens[1], TokenType::Minus);
    checkToken(tokens[2], TokenType::Star);
    checkToken(tokens[3], TokenType::Slash);
    checkToken(tokens[4], TokenType::EqEqual);
    checkToken(tokens[5], TokenType::NotEqual);
    checkToken(tokens[6], TokenType::LessEqual);
    checkToken(tokens[7], TokenType::GreaterEqual);
    checkToken(tokens[8], TokenType::EndMarker);
    std::cout << "PASSED" << std::endl;
}

// 测试7：缩进（Python 风格）
void testIndentation() {
    std::cout << "Test: Indentation... ";
    std::string code = R"(
if x:
    print(x)
    if y:
        print(y)
print(done)
)";
    Tokenizer tokenizer(code);
    auto tokens = tokenizer.tokenize();
    
    // 检查是否有 INDENT 和 DEDENT
    bool hasIndent = false;
    bool hasDedent = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Indent) hasIndent = true;
        if (t.type == TokenType::Dedent) hasDedent = true;
    }
    
    assert(hasIndent);
    assert(hasDedent);
    std::cout << "PASSED" << std::endl;
}

// 测试8：关键字
void testKeywords() {
    std::cout << "Test: Keywords... ";
    Tokenizer tokenizer("async await");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 3);
    checkToken(tokens[0], TokenType::Async, "async");
    checkToken(tokens[1], TokenType::Await, "await");
    std::cout << "PASSED" << std::endl;
}

// 测试9：完整代码片段
void testFullSnippet() {
    std::cout << "Test: Full snippet... ";
    std::string code = R"(
def greet(name):
    print("Hello, " + name)
    return 42

greet("World")
)";
    Tokenizer tokenizer(code);
    auto tokens = tokenizer.tokenize();
    
    // 不应该有错误
    for (const auto& t : tokens) {
        if (t.type == TokenType::Error) {
            std::cerr << "Unexpected error token: " << t << std::endl;
            assert(false);
        }
    }
    
    // 检查关键 token 存在
    bool hasDef = false, hasReturn = false, hasString = false;
    for (const auto& t : tokens) {
        if (t.text == "def") hasDef = true;
        if (t.text == "return") hasReturn = true;
        if (t.type == TokenType::String) hasString = true;
    }
    
    assert(hasDef);
    assert(hasReturn);
    assert(hasString);
    std::cout << "PASSED" << std::endl;
}

// 测试10：错误处理
void testError() {
    std::cout << "Test: Error handling... ";
    Tokenizer tokenizer("@");  // @ 单独使用可能不被支持
    auto tokens = tokenizer.tokenize();
    
    // 检查是否有错误
    bool hasError = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Error) {
            hasError = true;
            break;
        }
    }
    
    // 或者检查 @ 被识别为 At
    bool hasAt = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::At) hasAt = true;
    }
    
    assert(hasError || hasAt);
    std::cout << "PASSED" << std::endl;
}

// ========== 新增测试 ==========

// 测试11：空字符串（与空输入不同）
void testEmptyString() {
    std::cout << "Test: Empty string (whitespace only)... ";
    Tokenizer tokenizer("   \n   \n");  // 只有空白和换行
    auto tokens = tokenizer.tokenize();
    // 应该只有 ENDMARKER（空白行被跳过）
    assert(tokens.back().type == TokenType::EndMarker);
    std::cout << "PASSED" << std::endl;
}

// 测试12：极大数字
void testLargeNumber() {
    std::cout << "Test: Large number (int64 max)... ";
    Tokenizer tokenizer("9223372036854775807");  // int64_max
    auto tokens = tokenizer.tokenize();
    assert(tokens[0].type == TokenType::Number);
    assert(std::holds_alternative<int64_t>(tokens[0].value));
    std::cout << "PASSED" << std::endl;
}

// 测试13：转义字符
void testEscapeSequences() {
    std::cout << "Test: Escape sequences... ";
    Tokenizer tokenizer("\"hello\\nworld\\t!\"");
    auto tokens = tokenizer.tokenize();
    assert(std::get<std::string>(tokens[0].value) == "hello\nworld\t!");
    std::cout << "PASSED" << std::endl;
}

// 测试14：连续操作符（边界情况）
void testConsecutiveOperators() {
    std::cout << "Test: Consecutive operators... ";
    Tokenizer tokenizer("+++---");  // 应该解析为 ++ + -- 或错误
    auto tokens = tokenizer.tokenize();
    // 至少不应该崩溃
    std::cout << "PASSED" << std::endl;
}

// 测试15：简单表达式 "1 + 2"（核心演示）
void testSimpleExpression() {
    std::cout << "Test: Simple expression '1 + 2'... ";
    Tokenizer tokenizer("1 + 2");
    auto tokens = tokenizer.tokenize();
    
    // 验证: [NUMBER(1), PLUS, NUMBER(2), ENDMARKER]
    assert(tokens.size() == 4);
    checkToken(tokens[0], TokenType::Number, "1");
    checkToken(tokens[1], TokenType::Plus, "+");
    checkToken(tokens[2], TokenType::Number, "2");
    checkToken(tokens[3], TokenType::EndMarker);
    
    // 验证值
    assert(std::get<int64_t>(tokens[0].value) == 1);
    assert(std::get<int64_t>(tokens[2].value) == 2);
    
    std::cout << "PASSED" << std::endl;
}

// 测试16：复杂表达式
void testComplexExpression() {
    std::cout << "Test: Complex expression '3.14 * (x + y)'... ";
    Tokenizer tokenizer("3.14 * (x + y)");
    auto tokens = tokenizer.tokenize();
    
    // 验证 token 序列
    assert(tokens.size() == 8);
    checkToken(tokens[0], TokenType::Number, "3.14");      // 3.14
    checkToken(tokens[1], TokenType::Star, "*");           // *
    checkToken(tokens[2], TokenType::LeftParen, "(");     // (
    checkToken(tokens[3], TokenType::Name, "x");           // x
    checkToken(tokens[4], TokenType::Plus, "+");          // +
    checkToken(tokens[5], TokenType::Name, "y");           // y
    checkToken(tokens[6], TokenType::RightParen, ")");    // )
    checkToken(tokens[7], TokenType::EndMarker);
    
    std::cout << "PASSED" << std::endl;
}

// 测试17：函数调用
void testFunctionCall() {
    std::cout << "Test: Function call 'print(\"Hello\")'... ";
    Tokenizer tokenizer(R"(print("Hello"))");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 5);
    checkToken(tokens[0], TokenType::Name, "print");
    checkToken(tokens[1], TokenType::LeftParen, "(");
    checkToken(tokens[2], TokenType::String, "\"Hello\"");
    checkToken(tokens[3], TokenType::RightParen, ")");
    checkToken(tokens[4], TokenType::EndMarker);
    
    std::cout << "PASSED" << std::endl;
}

// 测试18：赋值语句
void testAssignment() {
    std::cout << "Test: Assignment 'x = 10'... ";
    Tokenizer tokenizer("x = 10");
    auto tokens = tokenizer.tokenize();
    
    assert(tokens.size() == 4);
    checkToken(tokens[0], TokenType::Name, "x");
    checkToken(tokens[1], TokenType::Equal, "=");
    checkToken(tokens[2], TokenType::Number, "10");
    checkToken(tokens[3], TokenType::EndMarker);
    
    std::cout << "PASSED" << std::endl;
}

// 测试19：多行代码
void testMultiLine() {
    std::cout << "Test: Multi-line code... ";
    std::string code = R"(a = 1
b = 2
c = a + b)";
    Tokenizer tokenizer(code);
    auto tokens = tokenizer.tokenize();
    
    // 检查行号递增
    size_t prevLine = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::EndMarker) break;
        assert(t.line >= prevLine);
        prevLine = t.line;
    }
    
    std::cout << "PASSED" << std::endl;
}

// 测试20：所有关键字
void testAllKeywords() {
    std::cout << "Test: All keywords... ";
    Tokenizer tokenizer("async await");
    auto tokens = tokenizer.tokenize();
    
    // 目前只有 async 和 await 两个关键字
    // 后续可以扩展更多
    assert(tokens.size() >= 3);  // 至少有两个关键字 + ENDMARKER
    
    std::cout << "PASSED" << std::endl;
}

// 主函数
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "CSGO Tokenizer Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        // 原有测试
        testEmpty();
        testIdentifier();
        testInteger();
        testFloat();
        testString();
        testOperators();
        testIndentation();
        testKeywords();
        testFullSnippet();
        testError();
        
        // 新增测试
        testEmptyString();
        testLargeNumber();
        testEscapeSequences();
        testConsecutiveOperators();
        testSimpleExpression();      // 核心演示: "1 + 2"
        testComplexExpression();     // "3.14 * (x + y)"
        testFunctionCall();          // "print(\"Hello\")"
        testAssignment();            // "x = 10"
        testMultiLine();             // 多行代码
        testAllKeywords();           // 所有关键字
        
        std::cout << "========================================" << std::endl;
        std::cout << "All 20 tests PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test FAILED with exception: " << e.what() << std::endl;
        return 1;
    }
}