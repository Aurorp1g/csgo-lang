#ifndef PARSER_H
#define PARSER_H

#include "lexer/tokenizer.h"
#include "ast/ast_node.h"
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <unordered_map>
#include <stack>

namespace csgo {
// ============================================================================
// Parser 类
// ============================================================================

/**
 * @class Parser
 * @brief CSGO 语言递归下降语法分析器
 *
 * 基于 CPython 3.8 Parser/parser.c 设计
 * 采用递归下降解析算法，实现完整的 Python 语法解析
 *
 * @section grammar 语法规则
 * - file_input: ENDMARKER* stmt* ENDMARKER
 * - eval_input: testlist NEWLINE* ENDMARKER
 * - func_type_input: ','.tfpdef [','annotation]* NEWLINE* ENDMARKER
 * - stmt: simple_stmt | compound_stmt
 * - simple_stmt: small_stmt (';' small_stmt)* NEWLINE
 * - compound_stmt: if_stmt | while_stmt | for_stmt | try_stmt | with_stmt
 *                | funcdef | classdef | decorated | async_stmt
 *
 * @section precedence 运算符优先级
 * 从低到高：
 * 1. lambda
 * 2. if-else
 * 3. or
 * 4. and
 * 5. not
 * 6. comparisons
 * 7. |
 * 8. ^
 * 9. &
 * 10. << >>
 * 11. + -
 * 12. * / // %
 * 13. +x -x ~x
 * 14. **
 * 15. await
 * 16. x.attr, x[index], x(...)
 */
class Parser {
public:
    /**
     * @brief 构造函数
     * @param tokens Token 序列
     */
    explicit Parser(const std::vector<Token>& tokens);
    
    /**
     * @brief 从字符串构造 Parser
     * @param source 源代码字符串
     */
    explicit Parser(const std::string& source);
    
    // 禁止拷贝
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    
    /**
     * @brief 解析模块
     * @return 解析后的模块 AST
     */
    std::unique_ptr<Module> parseModule();
    
    /**
     * @brief 检查是否有解析错误
     * @return true 如果有错误
     */
    bool hasError() const { return hasError_; }
    
    /**
     * @brief 获取错误消息
     * @return 错误消息字符串
     */
    const std::string& errorMessage() const { return errorMessage_; }
    
    /**
     * @brief 获取当前 Token
     * @return 当前 Token 引用
     */
    const Token& currentToken() const { 
        if (current_ < tokens_.size()) {
            return tokens_[current_];
        }
        return tokens_.back();  // 返回 EndMarker
    }
    
    /**
     * @brief 获取前一个 Token
     * @return 前一个 Token 引用
     */
    const Token& previousToken() const { 
        return current_ > 0 ? tokens_[current_ - 1] : tokens_[0]; 
    }
    
    /**
     * @brief 前进到下一个 Token
     */
    void advance() { 
        if (current_ < tokens_.size()) {
            current_++;
        }
    }
    
    /**
     * @brief 检查当前 Token 类型
     * @param type 要检查的 TokenType
     * @return true 如果匹配
     */
    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return tokens_[current_].type == type;
    }
    
    /**
     * @brief 检查当前 Token 是否为指定类型之一
     * @param types TokenType 列表
     * @return true 如果匹配任意类型
     */
    bool check(const std::vector<TokenType>& types) const {
        if (isAtEnd()) return false;
        for (auto type : types) {
            if (tokens_[current_].type == type) return true;
        }
        return false;
    }
    
    /**
     * @brief 消耗指定 Token
     * @param type 要消耗的 TokenType
     * @return true 如果成功消耗
     */
    bool consume(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }
    
    /**
     * @brief 期待指定 Token
     * @param type 要期待的 TokenType
     * @return true 如果匹配，否则报错
     */
    bool expect(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        reportError("Expected " + std::string(tokenTypeName(type)));
        return false;
    }
    
    /**
     * @brief 预查看当前 Token（不消耗）
     * @return 当前 Token 引用
     */
    const Token& peek() const;
    
    /**
     * @brief 获取前一个 Token
     * @return 前一个 Token 引用
     */
    const Token& previous() const;
    
    /**
     * @brief 匹配 Token 类型并前进
     * @param types 要匹配的 TokenType 列表
     * @return true 如果匹配成功
     */
    bool match(const std::vector<TokenType>& types);
    
    /**
     * @brief 匹配 Token 类型并前进
     * @param types 要匹配的 TokenType 列表
     * @return 匹配的 TokenType 或 EndMarker
     */
    TokenType match_types(const std::vector<TokenType>& types);
    
    /**
     * @brief 检查是否到达 Token 序列末尾
     * @return true 如果已到达末尾
     */
    bool isAtEnd() const {
        return current_ >= tokens_.size() || 
            tokens_[current_].type == TokenType::EndMarker;
    }

private:
    std::vector<Token> tokens_;    ///< Token 序列
    std::string source_;            ///< 源字符串（保持所有权以支持 string_view）
    size_t current_;               ///< 当前 Token 索引
    bool hasError_;               ///< 是否有解析错误
    std::string errorMessage_;     ///< 错误消息
    
    size_t currentLine_;          ///< 当前行号
    size_t currentColumn_;        ///< 当前列号
    
    /**
     * @brief 报告解析错误
     * @param message 错误消息
     */
    void reportError(const std::string& message);
    
    /**
     * @brief 获取当前位置信息
     * @return 位置信息结构体
     */
    Position getPosition() const {
        if (current_ < tokens_.size()) {
            return Position(tokens_[current_].line, tokens_[current_].column);
        }
        return Position(currentLine_, currentColumn_);
    }
    
    // =========================================================================
    // 文件和模块解析
    // =========================================================================
    
    /**
     * @brief 解析模块入口
     * @details 解析 file_input 规则：
     * - file_input: ENDMARKER* stmt* ENDMARKER
     * - 解析所有语句直到文件结束
     * @return 解析后的 Module 节点
     */
    std::unique_ptr<Module> parseFileInput();
    
    /**
     * @brief 解析 eval 模式的表达式
     * @details 解析 eval_input 规则：
     * - eval_input: testlist NEWLINE* ENDMARKER
     * - 用于 eval() 函数和交互模式
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseEvalInput();
    
    /**
     * @brief 解析函数类型注解
     * @details 解析 func_type_input 规则：
     * - func_type_input: ','.tfpdef [','annotation]* NEWLINE* ENDMARKER
     * - 用于类型注解语法
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseFuncTypeInput();
    
    // =========================================================================
    // 语句解析
    // =========================================================================
    
    /**
     * @brief 解析语句入口
     * @details 解析 stmt 规则：
     * - stmt: simple_stmt | compound_stmt
     * @return 解析后的语句节点
     */
    std::unique_ptr<Stmt> parseStatement();
    
    /**
     * @brief 解析简单语句
     * @details 解析 simple_stmt 规则：
     * - simple_stmt: small_stmt (';' small_stmt)* NEWLINE
     * @return 解析后的语句节点
     */
    std::unique_ptr<Stmt> parseSimpleStatement();
    
    /**
     * @brief 解析复合语句
     * @details 解析 compound_stmt 规则：
     * - compound_stmt: if_stmt | while_stmt | for_stmt | try_stmt | with_stmt
     *                 | funcdef | classdef | decorated | async_stmt
     * @return 解析后的语句节点
     */
    std::unique_ptr<Stmt> parseCompoundStatement();
    
    /**
     * @brief 解析 if 语句
     * @details 解析 if_stmt 规则：
     * - if_stmt: 'if' test ':' suite ('elif' test ':' suite)* ['else' ':' suite]
     * - 支持多分支 if-elif-else 结构
     * @return 解析后的 If 节点
     */
    std::unique_ptr<If> parseIfStatement();
    
    /**
     * @brief 解析 while 循环语句
     * @details 解析 while_stmt 规则：
     * - while_stmt: 'while' test ':' suite ['else' ':' suite]
     * - 支持可选的 else 分支
     * @return 解析后的 While 节点
     */
    std::unique_ptr<While> parseWhileStatement();
    
    /**
     * @brief 解析 for 循环语句
     * @details 解析 for_stmt 规则：
     * - for_stmt: 'for' target 'in' testlist ':' suite ['else' ':' suite]
     * - 支持迭代器和可迭代对象
     * - 支持可选的 else 分支
     * @return 解析后的 For 节点
     */
    std::unique_ptr<For> parseForStatement();
    
    /**
     * @brief 解析 try 异常处理语句
     * @details 解析 try_stmt 规则：
     * - try_stmt: ('try' ':' suite 
     *            ((except_clause ':' suite)+ ['else' ':' suite] ['finally' ':' suite]
     *             | ('finally' ':' suite)))
     * - 支持多个 except 子句
     * - 支持可选的 else 和 finally 分支
     * @return 解析后的 Try 节点
     */
    std::unique_ptr<Try> parseTryStatement();
    
    /**
     * @brief 解析 with 上下文管理语句
     * @details 解析 with_stmt 规则：
     * - with_stmt: 'with' (with_item ',')+ ':' suite | 'with' with_item ':' suite
     * - 支持多个上下文项
     * @return 解析后的 With 节点
     */
    std::unique_ptr<With> parseWithStatement();
    
    /**
     * @brief 解析函数定义语句
     * @details 解析 funcdef 规则：
     * - funcdef: 'def' NAME parameters ['->' test] ':' suite
     * - 支持返回类型注解
     * @return 解析后的 FunctionDef 节点
     */
    std::unique_ptr<FunctionDef> parseFunctionDefinition();
    
    /**
     * @brief 解析类定义语句
     * @details 解析 classdef 规则：
     * - classdef: 'class' NAME ['(' [arglist] ')' ] ':' suite
     * - 支持继承和构造函数参数
     * @return 解析后的 ClassDef 节点
     */
    std::unique_ptr<ClassDef> parseClassDefinition();
    
    /**
     * @brief 解析装饰器列表
     * @details 解析 decorated 规则：
     * - decorated: '@' dotted_name [ '(' [arglist] ')' ] NEWLINE
     * - 返回装饰器表达式列表
     * @return 装饰器表达式列表
     */
    std::vector<std::unique_ptr<Expr>> parseDecorators();
    
    /**
     * @brief 解析 suite（语句块）
     * @details 解析 suite 规则：
     * - suite: simple_stmt | NEWLINE INDENT stmt+ DEDENT
     * - 处理 Python 风格的缩进块
     * @return 语句块中的语句列表
     */
    std::vector<std::unique_ptr<Stmt>> parseSuite();
    
    /**
     * @brief 解析 assert 断言语句
     * @details 解析 assert_stmt 规则：
     * - assert_stmt: 'assert' test [',' test]
     * - 支持可选的断言消息
     * @return 解析后的 Assert 节点
     */
    std::unique_ptr<Assert> parseAssertStatement();
    
    /**
     * @brief 解析 return 返回语句
     * @details 解析 return_stmt 规则：
     * - return_stmt: 'return' [testlist]
     * - 支持可选的返回值表达式
     * @return 解析后的 Return 节点
     */
    std::unique_ptr<Return> parseReturnStatement();
    
    /**
     * @brief 解析 raise 异常抛出语句
     * @details 解析 raise_stmt 规则：
     * - raise_stmt: 'raise' [test ['from' test]]
     * - 支持可选的异常原因链
     * @return 解析后的 Raise 节点
     */
    std::unique_ptr<Raise> parseRaiseStatement();
    
    /**
     * @brief 解析 break 中断循环语句
     * @details 解析 break_stmt 规则：
     * - break_stmt: 'break'
     * @return 解析后的 Break 节点
     */
    std::unique_ptr<Break> parseBreakStatement();
    
    /**
     * @brief 解析 continue 继续循环语句
     * @details 解析 continue_stmt 规则：
     * - continue_stmt: 'continue'
     * @return 解析后的 Continue 节点
     */
    std::unique_ptr<Continue> parseContinueStatement();
    
    /**
     * @brief 解析 del 删除语句
     * @details 解析 del_stmt 规则：
     * - del_stmt: 'del' exprlist
     * @return 解析后的 Delete 节点
     */
    std::unique_ptr<Delete> parseDeleteStatement();
    
    /**
     * @brief 解析 global 全局声明语句
     * @details 解析 global_stmt 规则：
     * - global_stmt: 'global' (NAME (',' NAME)*)
     * @return 解析后的 Global 节点
     */
    std::unique_ptr<Global> parseGlobalStatement();
    
    /**
     * @brief 解析 nonlocal 非局部声明语句
     * @details 解析 nonlocal_stmt 规则：
     * - nonlocal_stmt: 'nonlocal' (NAME (',' NAME)*)
     * @return 解析后的 Nonlocal 节点
     */
    std::unique_ptr<Nonlocal> parseNonlocalStatement();
    
    /**
     * @brief 解析 import 导入语句
     * @details 解析 import_stmt 规则：
     * - import_stmt: import_name | import_from
     * - import_name: 'import' dotted_as_names
     * - import_from: 'from' ('.'* dotted_name | '.'+) 'import' ('*' | '(' import_as_names ')' | import_as_names)
     * @return 解析后的语句节点
     */
    std::unique_ptr<Stmt> parseImportStatement();
    
    /**
     * @brief 解析 import name 语句
     */
    std::unique_ptr<Stmt> parseImportName();
    
    /**
     * @brief 解析 from...import 语句
     */
    std::unique_ptr<Stmt> parseImportFrom();
    
    /**
     * @brief 解析表达式语句
     * @details 处理赋值、增强赋值、注解赋值等
     */
    std::unique_ptr<Stmt> parseExpressionStatement();
    
    // =========================================================================
    // 辅助函数
    // =========================================================================
    
    /**
     * @brief 设置目标表达式的上下文为 Store
     * @details 用于赋值语句的目标节点
     * @param expr 要设置上下文的表达式
     */
    void setTargetContext(Expr& expr);
    
    // =========================================================================
    // 表达式解析（优先级从低到高）
    // =========================================================================
    
    /**
     * @brief 解析测试表达式列表
     * @details 解析 testlist 规则：
     * - testlist: test (',' test)* [',']
     * - 用于函数参数、赋值语句右侧等
     * @return 表达式列表
     */
    std::vector<std::unique_ptr<Expr>> parseTestList();
    
    /**
     * @brief 解析测试表达式
     * @details 解析 test 规则：
     * - test: or_test ['if' or_test 'else' test] | lambdef
     * - 支持条件表达式和 lambda 表达式
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseTest();
    
    /**
     * @brief 解析或测试表达式
     * @details 解析 or_test 规则：
     * - or_test: and_test ('or' and_test)*
     * - 优先级低于 and_test
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseOrTest();
    
    /**
     * @brief 解析与测试表达式
     * @details 解析 and_test 规则：
     * - and_test: not_test ('and' not_test)*
     * - 优先级高于 or_test
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseAndTest();
    
    /**
     * @brief 解析非测试表达式
     * @details 解析 not_test 规则：
     * - not_test: 'not' not_test | comparison
     * - 支持逻辑非运算
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseNotTest();
    
    /**
     * @brief 解析比较表达式
     * @details 解析 comparison 规则：
     * - comparison: expr (comp_op expr)*
     * - 支持链式比较运算符
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseComparison();
    
    /**
     * @brief 解析比较运算符
     * @details 解析 comp_op 规则：
     * - comp_op: '<'|'>'|'=='|'>='|'<='|'!='|'in'|'not' 'in'|'is'|'is' 'not'
     * @return 比较运算符类型列表
     */
    std::vector<ASTNodeType> parseComparisonOperator();
    
    /**
     * @brief 解析位或表达式
     * @details 解析 expr 规则：
     * - expr: xor_expr ('|' xor_expr)*
     * - 最低优先级的位运算
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseExpression();
    
    /**
     * @brief 解析异或表达式
     * @details 解析 xor_expr 规则：
     * - xor_expr: and_expr ('^' and_expr)*
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseXorExpr();
    
    /**
     * @brief 解析按位与表达式
     * @details 解析 and_expr 规则：
     * - and_expr: shift_expr ('&' shift_expr)*
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseAndExpr();
    
    /**
     * @brief 解析移位表达式
     * @details 解析 shift_expr 规则：
     * - shift_expr: arith_expr ('<<'|'>>' arith_expr)*
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseShiftExpr();
    
    /**
     * @brief 解析算术表达式
     * @details 解析 arith_expr 规则：
     * - arith_expr: term (('+'|'-') term)*
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseArithmeticExpr();
    
    /**
     * @brief 解析 term
     * term: factor (('*'|'/'|'%'|'//') factor)*
     */
    std::unique_ptr<Expr> parseTerm();
    
    /**
     * @brief 解析 factor
     * factor: ('+'|'-'|'~') factor | power
     */
    std::unique_ptr<Expr> parseFactor();
    
    /**
     * @brief 解析 power
     * power: atom_expr ['**' factor]
     */
    std::unique_ptr<Expr> parsePower();
    
    /**
     * @brief 解析 atom_expr
     * atom_expr: ['await'] atom trailer*
     */
    std::unique_ptr<Expr> parseAtomExpression();
    
    /**
     * @brief 解析 trailer
     * trailer: '(' [arglist] ')' | '[' subscriptlist ']' | '.' NAME
     */
    std::unique_ptr<Expr> parseTrailer(std::unique_ptr<Expr> expr);
    
    /**
     * @brief 解析 atom
     * atom: NAME | NUMBER | STRING+ | '...' | 'None' | 'True' | 'False'
     *     | '(' [yield_expr|testlist_comp] ')' 
     *     | '[' [listmaker] ']' 
     *     | '{' [dictsetmaker] '}'
     */
    std::unique_ptr<Expr> parseAtom();
    
    /**
     * @brief 解析 lambda 表达式
     * lambdef: 'lambda' [typedargslist] ':' test
     */
    std::unique_ptr<Expr> parseLambda();
    
    /**
     * @brief 解析列表/元组/生成器
     * listmaker: test ( list_iter | (',' test)* [','] )
     * list_iter: list_for | list_if
     * list_for: 'for' target_list 'in' testlist [comp_iter]
     * list_if: 'if' test [comp_iter]
     * comp_iter: comp_for | comp_if
     * comp_for: 'for' target_list 'in' or_test [comp_iter]
     * comp_if: 'if' test [comp_iter]
     */
    std::unique_ptr<Expr> parseListDisplay();
    std::unique_ptr<Expr> parseSetDisplay();
    std::unique_ptr<Expr> parseDictDisplay();
    std::unique_ptr<Expr> parseGeneratorExpression();
    
    /**
     * @brief 解析推导式
     * @return 解析后的推导式 AST 向量
     */
    std::vector<std::unique_ptr<comprehension>> parseComprehension();

    /**
     * @brief 解析推导式迭代对象
     * @return 解析后的表达式节点
     */
    std::unique_ptr<Expr> parseComprehensionIter();
    
    /**
     * @brief 解析列表推导式
     * @param elt 元素表达式
     * @param generators 生成器列表
     */
    void parseListComprehension(std::unique_ptr<Expr> elt, 
                                std::vector<std::unique_ptr<Expr>>& generators);
    
    // =========================================================================
    // 参数列表解析
    // =========================================================================
    
    /**
     * @brief 解析 parameters（函数参数）
     * parameters: '(' [typedargslist] ')'
     */
    std::unique_ptr<arguments> parseParameters();
    
    /**
     * @brief 解析 typedargslist
     * typedargslist: (tfpdef ['=' test] (',' tfpdef ['=' test])* [',' ['*' [tfpdef] (',' tfpdef ['=' test])* ['**' tfpdef] | '**' tfpdef]]
     *               | '*' [tfpdef] (',' tfpdef ['=' test])* ['**' tfpdef]
     *               | '**' tfpdef)
     */
    std::unique_ptr<arguments> parseTypedArgsList();
    
    /**
     * @brief 解析 varargslist
     * varargslist: (vfpdef ['=' test] (',' vfpdef ['=' test])* [',' ['*' [vfpdef] (',' vfpdef ['=' test])* ['**' vfpdef] | '**' vfpdef]]
     *             | '*' [vfpdef] (',' vfpdef ['=' test])* ['**' vfpdef]
     *             | '**' vfpdef)
     */
    std::unique_ptr<arguments> parseVarArgsList();
    
    /**
     * @brief 解析 tfpdef（带类型的参数）
     * tfpdef: NAME [':' test]
     */
    std::unique_ptr<arg> parseTypedParameter();
    
    /**
     * @brief 解析 vfpdef（不带类型的参数）
     * vfpdef: NAME
     */
    std::unique_ptr<arg> parseParameter();
    
    /**
     * @brief 解析 arglist（实参列表）
     * arglist: (argument ',')* (argument [',']
     *          | '*' test (',' argument)* [',' '**' test]
     *          | '**' test)
     */
    std::pair<std::vector<std::unique_ptr<Expr>>, 
              std::vector<std::unique_ptr<Expr>>> parseArgumentList();
    
    /**
     * @brief 解析 argument（实参）
     * argument: ( [test] '=' namedexpr_test | test [comp_for] | '*' test | '**' test )
     */
    std::unique_ptr<Expr> parseArgument();
    
    /**
     * @brief 解析 exprlist
     * exprlist: expr (',' expr)* [',']
     */
    std::vector<std::unique_ptr<Expr>> parseExpressionList();
    
    /**
     * @brief 解析 testlist_comp
     * testlist_comp: test ( comp_for | (',' test)* [','] )
     */
    std::unique_ptr<Expr> parseTestListComp();
    
    /**
     * @brief 解析 subscriptlist
     * subscriptlist: subscript (',' subscript)* [',']
     */
    std::vector<std::unique_ptr<Expr>> parseSubscriptList();
    
    /**
     * @brief 解析 subscript
     * subscript: test | [test] ':' [test] [sliceop]
     */
    std::unique_ptr<Expr> parseSubscript();
    
    /**
     * @brief 解析 sliceop
     * sliceop: ':' [test]
     */
    std::unique_ptr<Expr> parseSliceOp();
    
    /**
     * @brief 解析 dictsetmaker
     * dictsetmaker: ( (test ':' test (comp_for | (',' test ':' test)* [','])) 
     *              | (test (comp_for | (',' test)* [','])) )
     */
    std::unique_ptr<Expr> parseDictOrSetMaker();
    
    /**
     * @brief 解析字符串连接
     * string_concatenation: STRING+
     */
    std::unique_ptr<Expr> parseStringConcat();
    
    /**
     * @brief 解析 f-string
     */
    std::unique_ptr<Expr> parseFString();
    
    /**
     * @brief 解析 f-string 内容
     */
    std::vector<std::unique_ptr<Expr>> parseFStringContent();
    
    // =========================================================================
    // 辅助方法
    // =========================================================================
    
    /**
     * @brief 创建位置信息
     * @param startToken 起始 Token
     * @param endToken 结束 Token
     * @return 位置信息
     */
    Position makePosition(const Token& startToken, const Token& endToken) const {
        return Position(startToken.line, startToken.column,
                       endToken.line, endToken.column);
    }
    
    /**
     * @brief 创建带位置信息的 Constant
     */
    std::unique_ptr<Constant> makeConstant(const Token& token) {
        auto constant = std::make_unique<Constant>();
        constant->position = Position(token.line, token.column);
        // 值由 Token 的 value 字段设置
        return constant;
    }
    
    /**
     * @brief 创建带位置信息的 Name
     */
    std::unique_ptr<Name> makeName(const Token& token, Name::Context ctx) {
        return std::make_unique<Name>(
            Position(token.line, token.column),
            std::string(token.text),
            ctx
        );
    }
    
    /**
     * @brief 跳过换行符
     */
    void skipNewlines() {
        while (check(TokenType::NewLine)) {
            advance();
        }
    }
    
    /**
     * @brief 跳过缩进
     */
    void skipIndentation();
    
    /**
     * @brief 将二元运算符 TokenType 转换为 ASTNodeType
     */
    static ASTNodeType tokenToASTBinOp(TokenType type);

    /**
     * @brief 将一元运算符 TokenType 转换为 ASTNodeType
     */
    static ASTNodeType tokenToASTUnaryOp(TokenType type);

    /**
     * @brief 将比较运算符 TokenType 转换为 ASTNodeType
     */
    static ASTNodeType tokenToASTCompareOp(TokenType type);

    /**
     * @brief 将布尔运算符 TokenType 转换为 ASTNodeType
     */
    static ASTNodeType tokenToASTBoolOp(TokenType type);
    };

}// namespace csgo

#endif // PARSER_H