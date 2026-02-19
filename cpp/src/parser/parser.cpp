#include "parser/parser.h"
#include <utility>
#include <sstream>
#include <algorithm>

namespace csgo {

Parser::Parser(const std::vector<Token>& tokens) 
    : tokens_(tokens), current_(0), hasError_(false), errorMessage_(""),
      currentLine_(1), currentColumn_(1) {
}

Parser::Parser(const std::string& source) 
    : source_(source), current_(0), hasError_(false), errorMessage_(""),
      currentLine_(1), currentColumn_(1) {
    Tokenizer tokenizer(source_);
    tokens_ = tokenizer.tokenize();
}

const Token& Parser::peek() const {
    size_t next = current_ + 1;
    if (next < tokens_.size()) {
        return tokens_[next];
    }
    return tokens_.back();
}

const Token& Parser::previous() const {
    if (current_ > 0) {
        return tokens_[current_ - 1];
    }
    return tokens_[0];
}

bool Parser::match(const std::vector<TokenType>& types) {
    for (auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

TokenType Parser::match_types(const std::vector<TokenType>& types) {
    for (auto type : types) {
        if (check(type)) {
            advance();
            return type;
        }
    }
    return TokenType::EndMarker;
}

std::unique_ptr<Module> Parser::parseModule() {
    return parseFileInput();
}

void Parser::reportError(const std::string& message) {
    hasError_ = true;
    std::ostringstream oss;
    oss << "Parse error at line " << currentToken().line 
        << ", column " << currentToken().column << ": " << message;
    errorMessage_ = oss.str();
}

std::unique_ptr<Module> Parser::parseFileInput() {
    auto module = std::make_unique<Module>();
    Position startPos = getPosition();
    
    while (!isAtEnd() && !check(TokenType::EndMarker)) {
        if (check(TokenType::NewLine)) {
            advance();
            continue;
        }
        
        auto stmt = parseStatement();
        if (stmt) {
            module->body.push_back(std::move(stmt));
        } else {
            break;
        }
    }
    
    module->position = startPos;
    return module;
}

std::unique_ptr<Expr> Parser::parseEvalInput() {
    auto testList = parseTestList();
    
    while (check(TokenType::NewLine)) {
        advance();
    }
    
    expect(TokenType::EndMarker);
    
    if (testList.size() == 1) {
        return std::move(testList[0]);
    }
    
    auto tuple = std::make_unique<Tuple>();
    tuple->elts = std::move(testList);
    tuple->ctx = Tuple::Load;
    tuple->position = getPosition();
    return tuple;
}

std::unique_ptr<Expr> Parser::parseFuncTypeInput() {
    auto args = parseTypedArgsList();
    
    while (check(TokenType::NewLine)) {
        advance();
    }
    
    expect(TokenType::EndMarker);
    
    // 创建一个元组表达式来表示参数列表
    // 这样可以保留参数信息，同时返回一个有效的Expr
    auto tuple = std::make_unique<Tuple>();
    tuple->position = getPosition();
    tuple->ctx = Tuple::Load;
    
    // 如果args中有任何参数，我们可以将其转换为表达式列表
    // 但现在我们创建一个空的元组，因为arguments不能直接转换为表达式列表
    
    return tuple;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    // 检查是否是简单语句关键字
    if (check(TokenType::Pass)) {
        auto passStmt = std::make_unique<Pass>();
        passStmt->position = getPosition();
        advance();
        return passStmt;
    } else if (check(TokenType::Break)) {
        return parseBreakStatement();
    } else if (check(TokenType::Continue)) {
        return parseContinueStatement();
    } else if (check(TokenType::Return)) {
        return parseReturnStatement();
    } else if (check(TokenType::Del)) {
        return parseDeleteStatement();
    } else if (check(TokenType::Global)) {
        return parseGlobalStatement();
    } else if (check(TokenType::Nonlocal)) {
        return parseNonlocalStatement();
    } else if (check(TokenType::Assert)) {
        return parseAssertStatement();
    } else if (check(TokenType::Raise)) {
        return parseRaiseStatement();
    } else if (check(TokenType::Import)) {
        return parseImportStatement();
    } else if (check(TokenType::From)) {
        return parseImportStatement();
    }
    
    // 检查是否是复合语句关键字
    if (check(TokenType::If) || check(TokenType::While) || check(TokenType::For) ||
        check(TokenType::Try) || check(TokenType::With) || check(TokenType::Def) ||
        check(TokenType::Class) || check(TokenType::Async) || check(TokenType::At)) {
        return parseCompoundStatement();
    }
    
    // 其他情况都当作简单语句处理（包括表达式语句）
    return parseSimpleStatement();
}

std::unique_ptr<Stmt> Parser::parseSimpleStatement() {
    auto firstStmt = parseExpressionStatement();
    
    while (check(TokenType::Semi)) {
        advance();
        if (check(TokenType::NewLine) || check(TokenType::Semi) || check(TokenType::EndMarker)) {
            break;
        }
        auto nextStmt = parseExpressionStatement();
        if (nextStmt) {
            // 多语句的情况需要创建特殊处理
            break;
        }
    }
    
    if (check(TokenType::NewLine)) {
        advance();
    }
    
    return firstStmt;
}

std::unique_ptr<Stmt> Parser::parseCompoundStatement() {
    if (check(TokenType::If)) {
        return parseIfStatement();
    } else if (check(TokenType::While)) {
        return parseWhileStatement();
    } else if (check(TokenType::For)) {
        return parseForStatement();
    } else if (check(TokenType::Try)) {
        return parseTryStatement();
    } else if (check(TokenType::With)) {
        return parseWithStatement();
    } else if (check(TokenType::Def)) {
        return parseFunctionDefinition();
    } else if (check(TokenType::Class)) {
        return parseClassDefinition();
    } else if (check(TokenType::Async)) {
        // 处理 async 语句
        advance();
        if (check(TokenType::Def)) {
            return parseFunctionDefinition();
        } else if (check(TokenType::For)) {
            return parseForStatement();
        } else if (check(TokenType::With)) {
            return parseWithStatement();
        }
    }
    
    // 检查装饰器
    if (check(TokenType::At)) {
        auto decorators = parseDecorators();
        if (check(TokenType::Def)) {
            auto funcDef = parseFunctionDefinition();
            funcDef->decorator_list = std::move(decorators);
            return funcDef;
        } else if (check(TokenType::Class)) {
            auto classDef = parseClassDefinition();
            classDef->decorator_list = std::move(decorators);
            return classDef;
        }
    }
    
    reportError("Expected compound statement");
    return nullptr;
}

std::unique_ptr<If> Parser::parseIfStatement() {
    auto ifNode = std::make_unique<If>();
    Position startPos = getPosition();
    
    expect(TokenType::If);
    ifNode->test = parseTest();
    expect(TokenType::Colon);
    ifNode->body = parseSuite();
    
    // 处理 elif 和 else
    while (check(TokenType::Elif)) {
        advance();
        auto elifTest = parseTest();
        expect(TokenType::Colon);
        auto elifBody = parseSuite();
        
        auto elifIf = std::make_unique<If>();
        elifIf->test = std::move(elifTest);
        elifIf->body = std::move(elifBody);
        ifNode->orelse.push_back(std::move(elifIf));
    }
    
    if (check(TokenType::Else)) {
        advance();
        expect(TokenType::Colon);
        ifNode->orelse = parseSuite();
    }
    
    ifNode->position = startPos;
    return ifNode;
}

std::unique_ptr<While> Parser::parseWhileStatement() {
    auto whileNode = std::make_unique<While>();
    Position startPos = getPosition();
    
    expect(TokenType::While);
    whileNode->test = parseTest();
    expect(TokenType::Colon);
    whileNode->body = parseSuite();
    
    if (check(TokenType::Else)) {
        advance();
        expect(TokenType::Colon);
        whileNode->orelse = parseSuite();
    }
    
    whileNode->position = startPos;
    return whileNode;
}

std::unique_ptr<For> Parser::parseForStatement() {
    auto forNode = std::make_unique<For>();
    Position startPos = getPosition();
    
    bool isAsync = false;
    if (check(TokenType::Async)) {
        isAsync = true;
        advance();
    }
    
    expect(TokenType::For);
    forNode->target = parseExpression();
    expect(TokenType::In);
    
    // 将解析得到的测试列表转换为单个表达式
    auto testList = parseTestList();
    if (testList.size() == 1) {
        forNode->iter = std::move(testList[0]);
    } else {
        // 如果有多个元素，创建一个Tuple表达式
        auto tuple = std::make_unique<Tuple>();
        tuple->elts = std::move(testList);
        tuple->ctx = Tuple::Load;
        tuple->position = getPosition();
        forNode->iter = std::move(tuple);
    }
    
    expect(TokenType::Colon);
    forNode->body = parseSuite();
    
    if (check(TokenType::Else)) {
        advance();
        expect(TokenType::Colon);
        forNode->orelse = parseSuite();
    }
    
    forNode->position = startPos;
    return forNode;
}

std::unique_ptr<Try> Parser::parseTryStatement() {
    auto tryNode = std::make_unique<Try>();
    Position startPos = getPosition();
    
    expect(TokenType::Try);
    expect(TokenType::Colon);
    tryNode->body = parseSuite();
    
    bool hasExcept = false;
    bool hasFinally = false;
    
    while (check(TokenType::Except)) {
        hasExcept = true;
        advance();
        
        auto handler = std::make_unique<except_handler>();
        
        if (!check(TokenType::Colon)) {
            if (check(TokenType::Name)) {
                handler->name = std::string(previousToken().text);
            }
            if (check(TokenType::Comma)) {
                advance();
                handler->type = parseTest();
            } else if (!check(TokenType::Colon)) {
                handler->type = parseTest();
            }
        }
        
        expect(TokenType::Colon);
        handler->body = parseSuite();
        tryNode->handlers.push_back(std::move(handler));
    }
    
    if (check(TokenType::Else)) {
        advance();
        expect(TokenType::Colon);
        tryNode->orelse = parseSuite();
    }
    
    if (check(TokenType::Finally)) {
        hasFinally = true;
        advance();
        expect(TokenType::Colon);
        tryNode->finalbody = parseSuite();
    }
    
    if (!hasExcept && !hasFinally) {
        reportError("try statement must have at least one except or finally clause");
    }
    
    tryNode->position = startPos;
    return tryNode;
}

std::unique_ptr<With> Parser::parseWithStatement() {
    auto withNode = std::make_unique<With>();
    Position startPos = getPosition();
    
    bool isAsync = false;
    if (check(TokenType::Async)) {
        isAsync = true;
        advance();
    }
    
    expect(TokenType::With);
    
    // 解析 with_item
    do {
        auto item = std::make_unique<withitem>();
        item->context_expr = parseTest();
        
        if (check(TokenType::As)) {
            advance();
            item->optional_vars = parseExpression();
        }
        
        withNode->items.push_back(std::move(item));
    } while (check(TokenType::Comma) && (advance(), true));
    
    expect(TokenType::Colon);
    withNode->body = parseSuite();
    
    withNode->position = startPos;
    return withNode;
}

std::unique_ptr<FunctionDef> Parser::parseFunctionDefinition() {
    auto funcDef = std::make_unique<FunctionDef>();
    Position startPos = getPosition();
    
    bool isAsync = false;
    if (check(TokenType::Async)) {
        isAsync = true;
        advance();
    }
    
    expect(TokenType::Def);
    
    if (check(TokenType::Name)) {
        funcDef->name = std::string(currentToken().text);
        advance();
    } else {
        reportError("Expected function name");
    }
    
    // 解析参数列表
    auto params = parseParameters();
    
    // 直接使用解析得到的参数对象
    if (params) {
        funcDef->args = std::move(params);
    } else {
        // 如果没有参数，创建一个空的 arguments 对象
        funcDef->args = std::make_unique<arguments>();
    }
    
    if (check(TokenType::Arrow)) {
        advance();
        auto returnType = parseTest();
        funcDef->returns = ""; // 简化处理
    }
    
    expect(TokenType::Colon);
    funcDef->body = parseSuite();
    
    funcDef->position = startPos;
    return funcDef;
}

std::unique_ptr<ClassDef> Parser::parseClassDefinition() {
    auto classDef = std::make_unique<ClassDef>();
    Position startPos = getPosition();
    
    expect(TokenType::Class);
    
    if (check(TokenType::Name)) {
        classDef->name = std::string(currentToken().text);
        advance();
    } else {
        reportError("Expected class name");
    }
    
    if (check(TokenType::LeftParen)) {
        advance();
        if (!check(TokenType::RightParen)) {
            auto testList = parseTestList();
            
            // 将测试列表包装成Tuple表达式
            auto tuple = std::make_unique<Tuple>();
            tuple->elts = std::move(testList);
            tuple->ctx = Tuple::Load;
            tuple->position = getPosition();
            
            classDef->bases = std::move(tuple);
        }
        expect(TokenType::RightParen);
    }
    
    expect(TokenType::Colon);
    classDef->body = parseSuite();
    
    classDef->position = startPos;
    return classDef;
}

std::vector<std::unique_ptr<Expr>> Parser::parseDecorators() {
    std::vector<std::unique_ptr<Expr>> decorators;
    
    while (check(TokenType::At)) {
        advance();
        auto decorator = parseExpression();
        decorators.push_back(std::move(decorator));
        
        if (check(TokenType::NewLine)) {
            advance();
        }
    }
    
    return decorators;
}

std::vector<std::unique_ptr<Stmt>> Parser::parseSuite() {
    std::vector<std::unique_ptr<Stmt>> body;
    
    if (check(TokenType::NewLine) || check(TokenType::Indent)) {
        if (check(TokenType::NewLine)) advance();
        if (check(TokenType::Indent)) advance();
        
        while (!check(TokenType::Dedent) && !check(TokenType::EndMarker)) {
            // 跳过空行
            while (check(TokenType::NewLine)) advance();
            
            if (check(TokenType::Dedent) || check(TokenType::EndMarker)) break;
            
            size_t posBefore = current_;
            auto stmt = parseStatement();
            
            if (!stmt) {
                // 无法解析，强制前进避免死循环
                if (current_ == posBefore && !isAtEnd()) {
                    advance();
                }
                break;
            }
            
            body.push_back(std::move(stmt));
            
            // 确保位置前进
            if (current_ == posBefore) {
                if (isAtEnd()) break;
                // 强制前进
                advance();
            }
        }
        
        if (check(TokenType::Dedent)) advance();
    } else {
        auto stmt = parseStatement();
        if (stmt) body.push_back(std::move(stmt));
    }
    
    return body;
}

std::unique_ptr<Assert> Parser::parseAssertStatement() {
    auto assertNode = std::make_unique<Assert>();
    Position startPos = getPosition();
    
    expect(TokenType::Assert);
    assertNode->test = parseTest();
    
    if (check(TokenType::Comma)) {
        advance();
        assertNode->msg = parseTest();
    }
    
    assertNode->position = startPos;
    return assertNode;
}

std::unique_ptr<Return> Parser::parseReturnStatement() {
    auto returnNode = std::make_unique<Return>();
    Position startPos = getPosition();
    
    expect(TokenType::Return);
    
    if (!check(TokenType::NewLine) && !check(TokenType::Semi) && !isAtEnd()) {
        auto testList = parseTestList();
        if (testList.size() == 1) {
            returnNode->value = std::move(testList[0]);
        } else if (testList.size() > 1) {
            // 如果有多个表达式，将它们包装成一个Tuple
            auto tuple = std::make_unique<Tuple>();
            tuple->elts = std::move(testList);
            tuple->ctx = Tuple::Load;
            tuple->position = getPosition();
            returnNode->value = std::move(tuple);
        }
    }
    
    returnNode->position = startPos;
    return returnNode;
}

std::unique_ptr<Raise> Parser::parseRaiseStatement() {
    auto raiseNode = std::make_unique<Raise>();
    Position startPos = getPosition();
    
    expect(TokenType::Raise);
    
    if (!check(TokenType::NewLine) && !check(TokenType::Semi) && !isAtEnd()) {
        raiseNode->exc = parseTest();
        
        if (check(TokenType::From)) {
            advance();
            raiseNode->cause = parseTest();
        }
    }
    
    raiseNode->position = startPos;
    return raiseNode;
}

std::unique_ptr<Break> Parser::parseBreakStatement() {
    auto breakNode = std::make_unique<Break>();
    Position startPos = getPosition();
    
    expect(TokenType::Break);
    breakNode->position = startPos;
    
    // 消费可选的换行符或分号
    if (check(TokenType::NewLine) || check(TokenType::Semi)) {
        advance();
    }
    
    return breakNode;
}

std::unique_ptr<Continue> Parser::parseContinueStatement() {
    auto continueNode = std::make_unique<Continue>();
    Position startPos = getPosition();
    
    expect(TokenType::Continue);
    continueNode->position = startPos;
    
    // 消费可选的换行符或分号
    if (check(TokenType::NewLine) || check(TokenType::Semi)) {
        advance();
    }
    
    return continueNode;
}

std::unique_ptr<Delete> Parser::parseDeleteStatement() {
    auto deleteNode = std::make_unique<Delete>();
    Position startPos = getPosition();
    
    expect(TokenType::Del);
    
    do {
        auto target = parseExpression();
        deleteNode->targets.push_back(std::move(target));
    } while (check(TokenType::Comma) && (advance(), true));
    
    deleteNode->position = startPos;
    return deleteNode;
}

std::unique_ptr<Global> Parser::parseGlobalStatement() {
    auto globalNode = std::make_unique<Global>();
    Position startPos = getPosition();
    
    expect(TokenType::Global);
    
    if (check(TokenType::Name)) {
        globalNode->names.push_back(std::string(currentToken().text));
        advance();
    }
    
    while (check(TokenType::Comma)) {
        advance();
        if (check(TokenType::Name)) {
            globalNode->names.push_back(std::string(currentToken().text));
            advance();
        }
    }
    
    globalNode->position = startPos;
    return globalNode;
}

std::unique_ptr<Nonlocal> Parser::parseNonlocalStatement() {
    auto nonlocalNode = std::make_unique<Nonlocal>();
    Position startPos = getPosition();
    
    expect(TokenType::Nonlocal);
    
    if (check(TokenType::Name)) {
        nonlocalNode->names.push_back(std::string(currentToken().text));
        advance();
    }
    
    while (check(TokenType::Comma)) {
        advance();
        if (check(TokenType::Name)) {
            nonlocalNode->names.push_back(std::string(currentToken().text));
            advance();
        }
    }
    
    nonlocalNode->position = startPos;
    return nonlocalNode;
}

std::unique_ptr<Stmt> Parser::parseImportStatement() {
    if (check(TokenType::From)) {
        return parseImportFrom();
    } else if (check(TokenType::Import)) {
        return parseImportName();
    }
    
    reportError("Expected import statement");
    return nullptr;
}

std::unique_ptr<Stmt> Parser::parseImportName() {
    auto importNode = std::make_unique<Import>();
    Position startPos = getPosition();
    
    expect(TokenType::Import);
    
    // 解析 dotted_as_names
    do {
        std::string moduleName;
        
        // 解析 dotted_name
        if (check(TokenType::Name)) {
            moduleName = std::string(currentToken().text);
            advance();
            
            while (check(TokenType::Dot)) {
                advance();
                if (check(TokenType::Name)) {
                    moduleName += ".";
                    moduleName += std::string(currentToken().text);
                    advance();
                }
            }
        }
        
        // 检查别名
        std::string alias;
        if (check(TokenType::As)) {
            advance();
            if (check(TokenType::Name)) {
                alias = std::string(currentToken().text);
                advance();
            }
        }
        
        // 创建 Name 表达式对象而不是直接添加字符串
        auto nameExpr = std::make_unique<Name>(moduleName, Name::Load);
        nameExpr->position = startPos;  // 设置位置信息
        importNode->names.push_back(std::move(nameExpr));
    } while (check(TokenType::Comma) && (advance(), true));
    
    importNode->position = startPos;
    return importNode;
}

std::unique_ptr<Stmt> Parser::parseImportFrom() {
    auto importFromNode = std::make_unique<ImportFrom>();
    Position startPos = getPosition();
    
    expect(TokenType::From);
    
    // 解析模块路径
    std::string moduleName;
    int dotCount = 0;
    
    while (check(TokenType::Dot)) {
        dotCount++;
        advance();
    }
    
    if (check(TokenType::Name)) {
        moduleName = std::string(currentToken().text);
        advance();
        
        while (check(TokenType::Dot)) {
            advance();
            if (check(TokenType::Name)) {
                moduleName += ".";
                moduleName += std::string(currentToken().text);
                advance();
            }
        }
    }
    
    // 修复：将模块名字符串转换为Name表达式
    if (!moduleName.empty()) {
        importFromNode->module = std::make_unique<Name>(moduleName, Name::Load);
    } else if (dotCount > 0) {
        // 如果只有相对导入（例如 from .. import），也需要处理
        // 创建一个表示相对导入的表达式（暂时用空名称表示）
        importFromNode->module = nullptr;  // 相对导入时 module 为空
    }
    
    expect(TokenType::Import);
    
    // 解析导入的名称
    if (check(TokenType::Star)) {
        advance();
        // 导入所有
    } else if (check(TokenType::LeftParen)) {
        advance();
        if (!check(TokenType::RightParen)) {
            do {
                if (check(TokenType::Name)) {
                    // 创建 Name 表达式而不是直接添加字符串
                    auto nameExpr = std::make_unique<Name>(std::string(currentToken().text), Name::Load);
                    nameExpr->position = startPos;
                    importFromNode->names.push_back(std::move(nameExpr));
                    advance();
                    
                    if (check(TokenType::As)) {
                        advance();
                        if (check(TokenType::Name)) {
                            // 别名处理
                            advance();
                        }
                    }
                }
            } while (check(TokenType::Comma) && (advance(), true));
        }
        expect(TokenType::RightParen);
    } else {
        do {
            if (check(TokenType::Name)) {
                // 创建 Name 表达式而不是直接添加字符串
                auto nameExpr = std::make_unique<Name>(std::string(currentToken().text), Name::Load);
                nameExpr->position = startPos;
                importFromNode->names.push_back(std::move(nameExpr));
                advance();
                
                if (check(TokenType::As)) {
                    advance();
                    if (check(TokenType::Name)) {
                        // 别名处理
                        advance();
                    }
                }
            }
        } while (check(TokenType::Comma) && (advance(), true));
    }
    
    importFromNode->position = startPos;
    return importFromNode;
}

std::unique_ptr<Stmt> Parser::parseExpressionStatement() {
    auto exprStmt = std::make_unique<ExprStmt>();
    Position startPos = getPosition();
    
    // 首先解析可能的测试表达式（用于赋值等）
    auto firstExpr = parseTest();
    
    // 处理不同类型的语句
    if (check(TokenType::ColonEqual)) {
        // 海象运算符 :=
    } else if (check(TokenType::Equal)) {
        // 赋值语句
        advance();
        auto value = parseTestList();
        
        auto assign = std::make_unique<Assign>();
        assign->targets.push_back(std::move(firstExpr));
        if (!value.empty()) {
            assign->value = std::move(value[0]);
        }
        assign->position = startPos;
        return assign;
    } else if (check(TokenType::PlusEqual) || check(TokenType::MinusEqual) ||
               check(TokenType::StarEqual) || check(TokenType::SlashEqual) ||
               check(TokenType::PercentEqual) || check(TokenType::DoubleStarEqual) ||
               check(TokenType::VBarEqual) || check(TokenType::AmperEqual) ||
               check(TokenType::CircumflexEqual) || check(TokenType::LeftShiftEqual) ||
               check(TokenType::RightShiftEqual) || check(TokenType::DoubleSlashEqual)) {
        // 增强赋值
        auto op = currentToken().type;
        advance();
        auto value = parseTest();
        
        auto augAssign = std::make_unique<AugAssign>();
        augAssign->target = std::move(firstExpr);
        augAssign->op = tokenToASTBinOp(op);
        augAssign->value = std::move(value);
        augAssign->position = startPos;
        return augAssign;
    } else if (check(TokenType::Colon)) {
        // 类型注解赋值
        advance();
        
        auto annAssign = std::make_unique<AnnAssign>();
        annAssign->target = std::move(firstExpr);
        
        if (!check(TokenType::NewLine) && !check(TokenType::Semi) && !isAtEnd()) {
            annAssign->annotation = parseTest();
        }
        
        if (check(TokenType::Equal)) {
            advance();
            auto valueList = parseTestList();
            if (!valueList.empty()) {
                annAssign->value = std::move(valueList[0]);
            }
        }
        
        annAssign->position = startPos;
        return annAssign;
    }
    
    // 简单的表达式语句
    exprStmt->value = std::move(firstExpr);
    exprStmt->position = startPos;
    return exprStmt;
}

std::vector<std::unique_ptr<Expr>> Parser::parseTestList() {
    std::vector<std::unique_ptr<Expr>> tests;
    
    auto first = parseTest();
    tests.push_back(std::move(first));
    
    while (check(TokenType::Comma)) {
        advance();
        if (check(TokenType::Comma) || check(TokenType::RightParen) || 
            check(TokenType::RightBracket) || check(TokenType::NewLine)) {
            break;
        }
        tests.push_back(parseTest());
    }
    
    return tests;
}

std::unique_ptr<Expr> Parser::parseTest() {
    if (check(TokenType::Lambda)) {
        return parseLambda();
    }
    
    auto orTest = parseOrTest();
    
    if (check(TokenType::If)) {
        advance();
        auto test = parseTest();
        expect(TokenType::Else);
        auto orElse = parseTest();
        
        auto ifExp = std::make_unique<IfExp>();
        ifExp->test = std::move(test);
        ifExp->body = std::move(orTest);
        ifExp->orelse = std::move(orElse);
        return ifExp;
    }
    
    return orTest;
}

std::unique_ptr<Expr> Parser::parseOrTest() {
    auto left = parseAndTest();
    
    while (check(TokenType::Or)) {
        advance();
        auto right = parseAndTest();
        
        auto boolOp = std::make_unique<BoolOp>();
        boolOp->op = ASTNodeType::Or;
        boolOp->values.push_back(std::move(left));
        boolOp->values.push_back(std::move(right));
        left = std::move(boolOp);
    }
    
    return left;
}

std::unique_ptr<Expr> Parser::parseAndTest() {
    auto left = parseNotTest();
    
    while (check(TokenType::And)) {
        advance();
        auto right = parseNotTest();
        
        auto boolOp = std::make_unique<BoolOp>();
        boolOp->op = ASTNodeType::And;
        boolOp->values.push_back(std::move(left));
        boolOp->values.push_back(std::move(right));
        left = std::move(boolOp);
    }
    
    return left;
}

std::unique_ptr<Expr> Parser::parseNotTest() {
    if (check(TokenType::Not)) {
        advance();
        auto operand = parseNotTest();
        
        auto unaryOp = std::make_unique<UnaryOp>();
        unaryOp->op = ASTNodeType::Not;
        unaryOp->operand = std::move(operand);
        return unaryOp;
    }
    
    return parseComparison();
}

std::unique_ptr<Expr> Parser::parseComparison() {
    auto left = parseExpression();
    std::vector<ASTNodeType> ops;
    std::vector<std::unique_ptr<Expr>> comparators;
    
    auto compOps = parseComparisonOperator();
    while (!compOps.empty()) {
        auto op = compOps[0];
        compOps.erase(compOps.begin());
        
        auto right = parseExpression();
        ops.push_back(op);
        comparators.push_back(std::move(right));
        
        compOps = parseComparisonOperator();
    }
    
    if (!ops.empty()) {
        auto compare = std::make_unique<Compare>();
        compare->left = std::move(left);
        compare->ops = std::move(ops);
        compare->comparators = std::move(comparators);
        return compare;
    }
    
    return left;
}

std::vector<ASTNodeType> Parser::parseComparisonOperator() {
    std::vector<ASTNodeType> ops;
    
    if (check(TokenType::Less)) {
        ops.push_back(ASTNodeType::Lt);
        advance();
    } else if (check(TokenType::Greater)) {
        ops.push_back(ASTNodeType::Gt);
        advance();
    } else if (check(TokenType::EqEqual)) {
        ops.push_back(ASTNodeType::Eq);
        advance();
    } else if (check(TokenType::GreaterEqual)) {
        ops.push_back(ASTNodeType::GtE);
        advance();
    } else if (check(TokenType::LessEqual)) {
        ops.push_back(ASTNodeType::LtE);
        advance();
    } else if (check(TokenType::NotEqual)) {
        ops.push_back(ASTNodeType::NotEq);
        advance();
    } else if (check(TokenType::In)) {
        ops.push_back(ASTNodeType::In);
        advance();
    } else if (check(TokenType::Is)) {
        advance();
        if (check(TokenType::Not)) {
            advance();
            ops.push_back(ASTNodeType::IsNot);
        } else {
            ops.push_back(ASTNodeType::Is);
        }
    } else if (check(TokenType::Not)) {
        advance();
        if (check(TokenType::In)) {
            advance();
            ops.push_back(ASTNodeType::NotIn);
        }
    }
    
    return ops;
}

std::unique_ptr<Expr> Parser::parseExpression() {
    auto left = parseXorExpr();
    
    while (check(TokenType::VBar)) {
        advance();
        auto right = parseXorExpr();
        
        auto binOp = std::make_unique<BinOp>();
        binOp->op = ASTNodeType::BitOr;
        binOp->left = std::move(left);
        binOp->right = std::move(right);
        left = std::move(binOp);
    }
    
    return left;
}

std::unique_ptr<Expr> Parser::parseXorExpr() {
    auto left = parseAndExpr();
    
    while (check(TokenType::Circumflex)) {
        advance();
        auto right = parseAndExpr();
        
        auto binOp = std::make_unique<BinOp>();
        binOp->op = ASTNodeType::BitXor;
        binOp->left = std::move(left);
        binOp->right = std::move(right);
        left = std::move(binOp);
    }
    
    return left;
}

std::unique_ptr<Expr> Parser::parseAndExpr() {
    auto left = parseShiftExpr();
    
    while (check(TokenType::Amper)) {
        advance();
        auto right = parseShiftExpr();
        
        auto binOp = std::make_unique<BinOp>();
        binOp->op = ASTNodeType::BitAnd;
        binOp->left = std::move(left);
        binOp->right = std::move(right);
        left = std::move(binOp);
    }
    
    return left;
}

std::unique_ptr<Expr> Parser::parseShiftExpr() {
    auto left = parseArithmeticExpr();
    
    while (check(TokenType::LeftShift) || check(TokenType::RightShift)) {
        auto op = currentToken().type;
        advance();
        auto right = parseArithmeticExpr();
        
        auto binOp = std::make_unique<BinOp>();
        binOp->op = tokenToASTBinOp(op);
        binOp->left = std::move(left);
        binOp->right = std::move(right);
        left = std::move(binOp);
    }
    
    return left;
}

std::unique_ptr<Expr> Parser::parseArithmeticExpr() {
    auto left = parseTerm();
    
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        auto op = currentToken().type;
        advance();
        auto right = parseTerm();
        
        auto binOp = std::make_unique<BinOp>();
        binOp->op = tokenToASTBinOp(op);
        binOp->left = std::move(left);
        binOp->right = std::move(right);
        left = std::move(binOp);
    }
    
    return left;
}

std::unique_ptr<Expr> Parser::parseTerm() {
    auto left = parseFactor();
    
    while (check(TokenType::Star) || check(TokenType::Slash) || 
           check(TokenType::Percent) || check(TokenType::DoubleSlash)) {
        auto op = currentToken().type;
        advance();
        auto right = parseFactor();
        
        auto binOp = std::make_unique<BinOp>();
        binOp->op = tokenToASTBinOp(op);
        binOp->left = std::move(left);
        binOp->right = std::move(right);
        left = std::move(binOp);
    }
    
    return left;
}

std::unique_ptr<Expr> Parser::parseFactor() {
    if (check(TokenType::Plus) || check(TokenType::Minus) || check(TokenType::Tilde)) {
        auto op = currentToken().type;
        advance();
        auto operand = parseFactor();
        
        auto unaryOp = std::make_unique<UnaryOp>();
        unaryOp->op = tokenToASTUnaryOp(op);
        unaryOp->operand = std::move(operand);
        return unaryOp;
    }
    
    return parsePower();
}

std::unique_ptr<Expr> Parser::parsePower() {
    auto atomExpr = parseAtomExpression();
    
    if (check(TokenType::DoubleStar)) {
        advance();
        auto operand = parseFactor();
        
        auto binOp = std::make_unique<BinOp>();
        binOp->op = ASTNodeType::Pow;
        binOp->left = std::move(atomExpr);
        binOp->right = std::move(operand);
        return binOp;
    }
    
    return atomExpr;
}

std::unique_ptr<Expr> Parser::parseAtomExpression() {
    Position startPos = getPosition();
    
    if (check(TokenType::Await)) {
        advance();
        auto operand = parseAtomExpression();
        
        auto awaitExpr = std::make_unique<Await>();
        awaitExpr->value = std::move(operand);
        awaitExpr->position = startPos;
        return awaitExpr;
    }
    
    auto atom = parseAtom();
    
    while (check(TokenType::LeftParen) || check(TokenType::LeftBracket) || check(TokenType::Dot)) {
        auto trailer = parseTrailer(std::move(atom));
        if (trailer) {
            atom = std::move(trailer);
        }
    }
    
    return atom;
}

std::unique_ptr<Expr> Parser::parseTrailer(std::unique_ptr<Expr> expr) {
    Position startPos = getPosition();
    
    if (check(TokenType::LeftParen)) {
        // 函数调用
        advance();
        
        auto call = std::make_unique<Call>();
        call->func = std::move(expr);
        
        if (!check(TokenType::RightParen)) {
            call->args = parseTestList();
        }
        
        expect(TokenType::RightParen);
        call->position = startPos;
        return call;
    } else if (check(TokenType::LeftBracket)) {
        // 下标访问
        advance();
        
        auto subscript = std::make_unique<Subscript>();
        subscript->value = std::move(expr);
        
        if (!check(TokenType::RightBracket)) {
            subscript->slice = parseSubscript();  // ← 关键修复
        }
        
        expect(TokenType::RightBracket);
        subscript->position = startPos;
        return subscript;
    } else if (check(TokenType::Dot)) {
        // 属性访问
        advance();
        
        if (check(TokenType::Name)) {
            auto attr = std::make_unique<Attribute>();
            attr->value = std::move(expr);
            attr->attr = std::string(currentToken().text);
            attr->position = startPos;
            advance();
            return attr;
        }
    }
    
    return nullptr;
}

std::unique_ptr<Expr> Parser::parseAtom() {
    Position startPos = getPosition();
    
    if (check(TokenType::Number)) {
        auto num = std::make_unique<Constant>();
        num->position = startPos;
        
        if (std::holds_alternative<int64_t>(currentToken().value)) {
            num->value = std::get<int64_t>(currentToken().value);
        } else if (std::holds_alternative<double>(currentToken().value)) {
            num->value = std::get<double>(currentToken().value);
        }
        
        advance();
        return num;
    } else if (check(TokenType::String) || check(TokenType::RawString) || check(TokenType::Bytes)) {
        auto str = std::make_unique<Constant>();
        str->position = startPos;
        
        if (std::holds_alternative<std::string>(currentToken().value)) {
            str->value = std::get<std::string>(currentToken().value);
        }
        
        advance();
        return str;
    } else if (check(TokenType::Ellipsis)) {
        auto ellipsis = std::make_unique<Constant>();
        ellipsis->position = startPos;
        ellipsis->value = std::monostate{};
        advance();
        return ellipsis;
    } else if (check(TokenType::None_)) {
        auto none = std::make_unique<Constant>();
        none->position = startPos;
        none->value = nullptr;
        advance();
        return none;
    } else if (check(TokenType::True_)) {
        auto trueVal = std::make_unique<Constant>();
        trueVal->position = startPos;
        trueVal->value = true;
        advance();
        return trueVal;
    } else if (check(TokenType::False_)) {
        auto falseVal = std::make_unique<Constant>();
        falseVal->position = startPos;
        falseVal->value = false;
        advance();
        return falseVal;
    } else if (check(TokenType::Name)) {
        auto name = std::make_unique<Name>();
        name->position = startPos;
        name->id = std::string(currentToken().text);
        name->ctx = Name::Load;
        advance();
        return name;
    } else if (check(TokenType::LeftParen)) {
        advance();
        
        if (check(TokenType::RightParen)) {
            // 空元组 ()
            advance();
            auto tuple = std::make_unique<Tuple>();
            tuple->position = startPos;
            return tuple;
        }
        
        // 先解析第一个表达式
        auto first = parseTest();
        
        // 检查是否是生成器表达式 (x for x in ...)
        if (check(TokenType::For)) {
            auto genexp = std::make_unique<GeneratorExp>();
            genexp->position = startPos;
            genexp->elt = std::move(first);
            genexp->generators = parseComprehension();
            expect(TokenType::RightParen);
            return genexp;
        }
        
        // 检查是否是多元素元组 (x, y, z)
        if (check(TokenType::Comma)) {
            auto tuple = std::make_unique<Tuple>();
            tuple->position = startPos;
            tuple->elts.push_back(std::move(first));
            
            while (check(TokenType::Comma)) {
                advance();
                if (check(TokenType::RightParen)) break;
                tuple->elts.push_back(parseTest());
            }
            
            expect(TokenType::RightParen);
            return tuple;
        }
        
        // 单元素分组 (x) - 就是 x 本身
        expect(TokenType::RightParen);
        return first;
    } else if (check(TokenType::LeftBracket)) {
        // 列表
        advance();
        
        if (check(TokenType::RightBracket)) {
            // 空列表 []
            advance();
            auto list = std::make_unique<List>();
            list->position = startPos;
            return list;
        }
        
        // 先解析第一个表达式
        auto first = parseTest();
        
        // 检查是否是列表推导式 [x for x in ...]
        if (check(TokenType::For)) {
            auto listcomp = std::make_unique<ListComp>();
            listcomp->position = startPos;
            listcomp->elt = std::move(first);
            listcomp->generators = parseComprehension();
            expect(TokenType::RightBracket);
            return listcomp;
        }
        
        // 普通列表 [x, y, z]
        auto list = std::make_unique<List>();
        list->position = startPos;
        list->elts.push_back(std::move(first));
        
        while (check(TokenType::Comma)) {
            advance();
            if (check(TokenType::RightBracket) || check(TokenType::EndMarker)) {
                break;
            }
            list->elts.push_back(parseTest());
        }
        
        expect(TokenType::RightBracket);
        return list;
    } else if (check(TokenType::LeftBrace)) {
        advance();
        
        if (check(TokenType::RightBrace)) {
            // 空字典 {}
            advance();
            auto dict = std::make_unique<Dict>();
            dict->position = startPos;
            return dict;
        }
        
        // 先解析第一个表达式
        auto first = parseTest();
        
        // 检查是否是集合推导式 {x for x in ...}
        if (check(TokenType::For)) {
            auto setcomp = std::make_unique<SetComp>();
            setcomp->position = startPos;
            setcomp->elt = std::move(first);
            setcomp->generators = parseComprehension();
            expect(TokenType::RightBrace);
            return setcomp;
        }
        
        // 检查是否是字典推导式 {k: v for k, v in ...} 或普通字典 {k: v, ...}
        if (check(TokenType::Colon)) {
            advance(); // 消费 :
            auto value = parseTest();
            
            // 检查是否是字典推导式
            if (check(TokenType::For)) {
                auto dictcomp = std::make_unique<DictComp>();
                dictcomp->position = startPos;
                dictcomp->key = std::move(first);
                dictcomp->value = std::move(value);
                dictcomp->generators = parseComprehension();
                expect(TokenType::RightBrace);
                return dictcomp;
            }
            
            // 普通字典 {key: value, ...}
            auto dict = std::make_unique<Dict>();
            dict->position = startPos;
            dict->keys.push_back(std::move(first));
            dict->values.push_back(std::move(value));
            
            while (check(TokenType::Comma)) {
                advance();
                if (check(TokenType::RightBrace)) break;
                
                auto key = parseTest();
                expect(TokenType::Colon);
                auto val = parseTest();
                dict->keys.push_back(std::move(key));
                dict->values.push_back(std::move(val));
            }
            
            expect(TokenType::RightBrace);
            return dict;
        }
        
        // 普通集合 {x, y, z}
        auto set = std::make_unique<Set>();
        set->position = startPos;
        set->elts.push_back(std::move(first));
        
        while (check(TokenType::Comma)) {
            advance();
            if (check(TokenType::RightBrace)) break;
            set->elts.push_back(parseTest());
        }
        
        expect(TokenType::RightBrace);
        return set;
    }
    
    reportError("Expected expression");
    return nullptr;
}

std::unique_ptr<Expr> Parser::parseLambda() {
    auto lambda = std::make_unique<Lambda>();
    Position startPos = getPosition();
    
    expect(TokenType::Lambda);
    
    if (!check(TokenType::Colon)) {
        // 解析参数列表
        auto args = parseVarArgsList();
        
        // 将arguments对象包装成一个表达式
        // 创建一个占位符表达式，包含参数信息
        auto argsExpr = std::make_unique<Tuple>();
        argsExpr->position = startPos;
        argsExpr->ctx = Tuple::Load;
        
        // 如果有参数，则将它们转换为元组元素
        if (args && !args->args.empty()) {
            for (auto& arg : args->args) {
                // 将arg转换为Name表达式
                if (auto argPtr = dynamic_cast<csgo::arg*>(arg.get())) {
                    auto nameExpr = std::make_unique<Name>();
                    nameExpr->id = argPtr->name; // 使用name而非arg作为参数名称
                    nameExpr->ctx = Name::Store; // 修改为正确的上下文类型
                    nameExpr->position = argPtr->position;
                    argsExpr->elts.push_back(std::move(nameExpr));
                }
            }
        }
        
        lambda->args = std::move(argsExpr);
    }
    
    expect(TokenType::Colon);
    lambda->body = parseTest();
    
    lambda->position = startPos;
    return lambda;
}

std::unique_ptr<Expr> Parser::parseListDisplay() {
    Position startPos = getPosition();
    
    // 先解析第一个元素
    auto first = parseTest();
    
    // 检查是否是列表推导式 [x for x in ...]
    if (check(TokenType::For)) {
        auto listcomp = std::make_unique<ListComp>();
        listcomp->position = startPos;
        listcomp->elt = std::move(first);
        listcomp->generators = parseComprehension();
        expect(TokenType::RightBracket);
        return listcomp;
    }
    
    // 普通列表 [x, y, z]
    auto list = std::make_unique<List>();
    list->position = startPos;
    list->elts.push_back(std::move(first));
    
    while (check(TokenType::Comma)) {
        advance();
        if (check(TokenType::RightBracket) || check(TokenType::EndMarker)) {
            break;
        }
        list->elts.push_back(parseTest());
    }
    
    expect(TokenType::RightBracket);
    return list;
}

std::unique_ptr<Expr> Parser::parseSetDisplay() {
    auto set = std::make_unique<Set>();
    Position startPos = getPosition();
    
    auto first = parseTest();
    set->elts.push_back(std::move(first));
    
    while (check(TokenType::Comma)) {
        advance();
        if (check(TokenType::RightBrace) || check(TokenType::EndMarker)) {
            break;
        }
        set->elts.push_back(parseTest());
    }
    
    expect(TokenType::RightBrace);
    set->position = startPos;
    return set;
}
 
std::unique_ptr<Expr> Parser::parseDictDisplay() {
    auto dict = std::make_unique<Dict>();
    Position startPos = getPosition();
    
    if (check(TokenType::DoubleStar)) {
        // 字典解包 **
        // 简化处理
    }
    
    if (check(TokenType::RightBrace)) {
        advance();
        dict->position = startPos;
        return dict;
    }
    
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        auto key = parseTest();
        expect(TokenType::Colon);
        auto value = parseTest();
        
        dict->keys.push_back(std::move(key));
        dict->values.push_back(std::move(value));
        
        if (check(TokenType::Comma)) {
            advance();
        } else {
            break;
        }
    }
    
    expect(TokenType::RightBrace);
    dict->position = startPos;
    return dict;
}
 
std::unique_ptr<Expr> Parser::parseGeneratorExpression() {
    auto gen = std::make_unique<GeneratorExp>();
    Position startPos = getPosition();
    
    // 生成器表达式的特殊语法 (x for x in iterable)
    // 这里简化处理
    
    auto element = parseTest();
    gen->elt = std::move(element);
    
    expect(TokenType::For);
    gen->generators = parseComprehension();
    
    gen->position = startPos;
    return gen;
}
 
std::vector<std::unique_ptr<Expr>> Parser::parseComprehension() {
    std::vector<std::unique_ptr<Expr>> generators;
    
    while (check(TokenType::For)) {
        auto comp = std::make_unique<comprehension>();
        Position compPos = getPosition();
        
        expect(TokenType::For);
        comp->target = parseExpression();
        expect(TokenType::In);
        
        // 迭代对象使用 parseTest，但遇到 if/for 需要停止
        comp->iter = parseComprehensionIter();
        
        // 解析可选的 if 子句 - 关键修复：使用 parseOrTest 而不是 parseTest
        while (check(TokenType::If)) {
            advance();
            comp->ifs.push_back(parseOrTest());  // ← 改为 parseOrTest
        }
        
        comp->position = compPos;
        generators.push_back(std::move(comp));
    }
    
    return generators;
}

// 专门解析迭代对象，遇到 if/for 停止
std::unique_ptr<Expr> Parser::parseComprehensionIter() {
    // 保存当前位置，用于回溯（如果需要）
    if (check(TokenType::If) || check(TokenType::For)) {
        reportError("Expected expression after 'in'");
        return nullptr;
    }
    
    // 使用 parseOrTest 或 parseTest，但需要限制范围
    // 实际上应该解析到 if/for 前停止
    return parseOrTest();
}
 
void Parser::parseListComprehension(std::unique_ptr<Expr> elt, 
                                     std::vector<std::unique_ptr<Expr>>& generators) {
    // 解析列表推导式语法：
    // [element for target in iterable if condition]
    // 支持多个 for 和 if 子句
    
    while (check(TokenType::For)) {
        auto comp = std::make_unique<comprehension>();
        Position compPos = getPosition();
        
        // 解析 'for' 关键字后的目标
        expect(TokenType::For);
        comp->target = parseExpression();
        
        // 解析 'in' 关键字后的迭代表达式
        expect(TokenType::In);
        auto iterList = parseTestList();
        if (iterList.size() == 1) {
            comp->iter = std::move(iterList[0]);
        } else if (iterList.size() > 1) {
            // 如果有多个元素，创建一个Tuple表达式
            auto tuple = std::make_unique<Tuple>();
            tuple->elts = std::move(iterList);
            tuple->ctx = Tuple::Load;
            tuple->position = getPosition();
            comp->iter = std::move(tuple);
        } else {
            // 如果列表为空，创建一个空的占位表达式并报告错误
            reportError("Expected iterable expression after 'in'");
        }
        
        // 解析可选的 'if' 条件子句
        while (check(TokenType::If)) {
            advance();
            comp->ifs.push_back(parseTest());
        }
        
        comp->position = compPos;
        generators.push_back(std::move(comp));
    }
}

std::unique_ptr<arguments> Parser::parseParameters() {
    auto args = std::make_unique<arguments>();
    
    expect(TokenType::LeftParen);
    
    if (!check(TokenType::RightParen)) {
        args = parseTypedArgsList();
    }
    
    expect(TokenType::RightParen);
    return args;
}

std::unique_ptr<arguments> Parser::parseTypedArgsList() {
    auto args = std::make_unique<arguments>();
    
    // 解析参数列表
    while (!check(TokenType::RightParen) && !isAtEnd()) {
        if (check(TokenType::DoubleStar)) {
            // **kwargs
            advance();
            auto kwarg = parseTypedParameter();
            args->kwarg = std::move(kwarg);
            break;
        } else if (check(TokenType::Star)) {
            // *args
            advance();
            if (!check(TokenType::Comma) && !check(TokenType::RightParen)) {
                auto vararg = parseTypedParameter();
                args->vararg = std::move(vararg);
            }
        } else {
            auto param = parseTypedParameter();
            args->args.push_back(std::move(param));
            
            if (check(TokenType::Equal)) {
                // 有默认值
                advance();
                // 默认值解析
            }
        }
        
        if (check(TokenType::Comma)) {
            advance();
        } else {
            break;
        }
    }
    
    return args;
}

std::unique_ptr<arguments> Parser::parseVarArgsList() {
    auto args = std::make_unique<arguments>();
    
    // 解析参数列表（不带类型注解）
    while (!check(TokenType::RightParen) && !isAtEnd()) {
        if (check(TokenType::DoubleStar)) {
            advance();
            auto kwarg = std::make_unique<arg>();
            if (check(TokenType::Name)) {
                kwarg->name = std::string(currentToken().text);
                advance();
            }
            args->kwarg = std::move(kwarg);
            break;
        } else if (check(TokenType::Star)) {
            advance();
            if (!check(TokenType::Comma) && !check(TokenType::RightParen)) {
                auto vararg = std::make_unique<arg>();
                if (check(TokenType::Name)) {
                    vararg->name = std::string(currentToken().text);
                    advance();
                }
                args->vararg = std::move(vararg);
            }
        } else {
            auto param = std::make_unique<arg>();
            if (check(TokenType::Name)) {
                param->name = std::string(currentToken().text);
                advance();
            }
            args->args.push_back(std::move(param));
        }
        
        if (check(TokenType::Comma)) {
            advance();
        } else {
            break;
        }
    }
    
    return args;
}

std::unique_ptr<arg> Parser::parseTypedParameter() {
    auto param = std::make_unique<arg>();
    
    if (check(TokenType::Star)) {
        param->name = "*";
        advance();
    } else if (check(TokenType::DoubleStar)) {
        param->name = "**";
        advance();
    } else if (check(TokenType::Name)) {
        param->name = std::string(currentToken().text);
        advance();
    }
    
    if (check(TokenType::Colon)) {
        // 有类型注解
        advance();
        param->annotation = parseTest();
    }
    
    return param;
}

void Parser::skipIndentation() {
    // 跳过空白字符
    while (check(TokenType::NewLine) || check(TokenType::Indent) || check(TokenType::Dedent)) {
        advance();
    }
}

ASTNodeType Parser::tokenToASTBinOp(TokenType type) {
    switch (type) {
        // 普通二元操作符
        case TokenType::Plus: return ASTNodeType::Add;
        case TokenType::Minus: return ASTNodeType::Sub;
        case TokenType::Star: return ASTNodeType::Mult;
        case TokenType::Slash: return ASTNodeType::Div;
        case TokenType::DoubleSlash: return ASTNodeType::FloorDiv;
        case TokenType::Percent: return ASTNodeType::Mod;
        case TokenType::DoubleStar: return ASTNodeType::Pow;
        case TokenType::LeftShift: return ASTNodeType::LShift;
        case TokenType::RightShift: return ASTNodeType::RShift;
        case TokenType::VBar: return ASTNodeType::BitOr;
        case TokenType::Amper: return ASTNodeType::BitAnd;
        case TokenType::Circumflex: return ASTNodeType::BitXor;
        
        // 增强赋值操作符 - 新增！
        case TokenType::PlusEqual: return ASTNodeType::Add;
        case TokenType::MinusEqual: return ASTNodeType::Sub;
        case TokenType::StarEqual: return ASTNodeType::Mult;
        case TokenType::SlashEqual: return ASTNodeType::Div;
        case TokenType::DoubleSlashEqual: return ASTNodeType::FloorDiv;
        case TokenType::PercentEqual: return ASTNodeType::Mod;
        case TokenType::DoubleStarEqual: return ASTNodeType::Pow;
        case TokenType::LeftShiftEqual: return ASTNodeType::LShift;
        case TokenType::RightShiftEqual: return ASTNodeType::RShift;
        case TokenType::VBarEqual: return ASTNodeType::BitOr;
        case TokenType::AmperEqual: return ASTNodeType::BitAnd;
        case TokenType::CircumflexEqual: return ASTNodeType::BitXor;
        
        default: return ASTNodeType::Invalid;
    }
}

ASTNodeType Parser::tokenToASTUnaryOp(TokenType type) {
    switch (type) {
        case TokenType::Plus: return ASTNodeType::UAdd;
        case TokenType::Minus: return ASTNodeType::USub;
        case TokenType::Tilde: return ASTNodeType::Invert;
        case TokenType::Not: return ASTNodeType::Not;
        default: return ASTNodeType::Invalid;
    }
}

ASTNodeType Parser::tokenToASTCompareOp(TokenType type) {
    switch (type) {
        case TokenType::Less: return ASTNodeType::Lt;
        case TokenType::Greater: return ASTNodeType::Gt;
        case TokenType::Equal: return ASTNodeType::Eq;
        case TokenType::LessEqual: return ASTNodeType::LtE;
        case TokenType::GreaterEqual: return ASTNodeType::GtE;
        case TokenType::NotEqual: return ASTNodeType::NotEq;
        case TokenType::In: return ASTNodeType::In;
        case TokenType::NotIn: return ASTNodeType::NotIn;
        case TokenType::Is: return ASTNodeType::Is;
        case TokenType::IsNot: return ASTNodeType::IsNot;
        default: return ASTNodeType::Invalid;
    }
}

ASTNodeType Parser::tokenToASTBoolOp(TokenType type) {
    switch (type) {
        case TokenType::And: return ASTNodeType::And;
        case TokenType::Or: return ASTNodeType::Or;
        default: return ASTNodeType::Invalid;
    }
}

std::unique_ptr<arg> Parser::parseParameter() {
    auto param = std::make_unique<arg>();
    
    if (check(TokenType::Star)) {
        param->name = "*";
        advance();
        if (check(TokenType::Name) && !check(TokenType::Equal)) {
            param->name = std::string(currentToken().text);
            advance();
        }
    } else if (check(TokenType::DoubleStar)) {
        param->name = "**";
        advance();
        if (check(TokenType::Name)) {
            param->name = std::string(currentToken().text);
            advance();
        }
    } else if (check(TokenType::Name)) {
        param->name = std::string(currentToken().text);
        advance();
    }
    
    if (check(TokenType::Equal)) {
        advance();
        // 注意：参数默认值不由单个arg对象持有，而是由包含它的arguments对象的defaults列表管理
        // 这里只是解析默认值，实际的存储需要在更高层完成
        auto defaultValue = parseTest();
    }
    
    return param;
}

std::pair<std::vector<std::unique_ptr<Expr>>, 
          std::vector<std::unique_ptr<Expr>>> Parser::parseArgumentList() {
    std::vector<std::unique_ptr<Expr>> args;
    std::vector<std::unique_ptr<Expr>> kwargs;
    
    while (!check(TokenType::RightParen) && !isAtEnd()) {
        if (check(TokenType::DoubleStar)) {
            // **kwargs
            advance();
            auto kwarg = parseTest();
            kwargs.push_back(std::move(kwarg));
        } else if (check(TokenType::Star)) {
            // *args
            advance();
            auto arg = parseTest();
            args.push_back(std::move(arg));
        } else {
            auto arg = parseArgument();
            args.push_back(std::move(arg));
        }
        
        if (check(TokenType::Comma)) {
            advance();
        } else {
            break;
        }
    }
    
    return {std::move(args), std::move(kwargs)};
}

std::unique_ptr<Expr> Parser::parseArgument() {
    if (check(TokenType::Star)) {
        // *test
        advance();
        auto starred = std::make_unique<Starred>();
        starred->value = parseTest();
        starred->position = getPosition();
        return starred;
    } else if (check(TokenType::DoubleStar)) {
        // **test
        advance();
        return parseTest();
    }
    
    auto arg = parseTest();
    
    // 检查是否有 = 赋值（关键字参数）
    if (check(TokenType::Equal)) {
        // 这是关键字参数
        advance();
        auto value = parseTest();
        return value;
    }
    
    return arg;
}

std::vector<std::unique_ptr<Expr>> Parser::parseExpressionList() {
    std::vector<std::unique_ptr<Expr>> exprs;
    
    auto first = parseExpression();
    exprs.push_back(std::move(first));
    
    while (check(TokenType::Comma)) {
        advance();
        if (check(TokenType::Comma) || check(TokenType::EndMarker)) {
            break;
        }
        exprs.push_back(parseExpression());
    }
    
    return exprs;
}

std::unique_ptr<Expr> Parser::parseTestListComp() {
    auto first = parseTest();
    
    // 检查是否有推导式或生成器
    if (check(TokenType::For) || check(TokenType::If)) {
        auto gen = std::make_unique<GeneratorExp>();
        gen->elt = std::move(first);
        gen->generators = parseComprehension();
        return gen;
    }
    
    // 检查是否是元组
    std::vector<std::unique_ptr<Expr>> elts;
    elts.push_back(std::move(first));
    
    while (check(TokenType::Comma)) {
        advance();
        if (check(TokenType::Comma) || check(TokenType::RightParen)) {
            break;
        }
        elts.push_back(parseTest());
    }
    
    if (elts.size() == 1) {
        return std::move(elts[0]);
    }
    
    auto tuple = std::make_unique<Tuple>();
    tuple->elts = std::move(elts);
    tuple->position = getPosition();
    return tuple;
}

std::vector<std::unique_ptr<Expr>> Parser::parseSubscriptList() {
    std::vector<std::unique_ptr<Expr>> subscripts;
    
    subscripts.push_back(parseSubscript());
    
    while (check(TokenType::Comma)) {
        advance();
        if (check(TokenType::Comma) || check(TokenType::RightBracket)) {
            break;
        }
        subscripts.push_back(parseSubscript());
    }
    
    return subscripts;
}

std::unique_ptr<Expr> Parser::parseSubscript() {
    Position startPos = getPosition();
    
    // 切片语法
    if (check(TokenType::Colon)) {
        auto slice = std::make_unique<Slice>();
        slice->position = startPos;
        
        advance();
        if (!check(TokenType::Colon) && !check(TokenType::RightBracket) && 
            !check(TokenType::Comma) && !isAtEnd()) {
            slice->lower = parseTest();
        }
        
        if (check(TokenType::Colon)) {
            advance();
            if (!check(TokenType::RightBracket) && !check(TokenType::Comma) && !isAtEnd()) {
                slice->upper = parseTest();
            }
            
            if (check(TokenType::Colon)) {
                advance();
                slice->step = parseTest();
            }
        }
        
        return slice;
    }
    
    auto test = parseTest();
    
    if (check(TokenType::Colon)) {
        auto slice = std::make_unique<Slice>();
        slice->position = startPos;
        slice->lower = std::move(test);
        
        advance();
        if (!check(TokenType::RightBracket) && !check(TokenType::Comma) && !isAtEnd()) {
            slice->upper = parseTest();
        }
        
        if (check(TokenType::Colon)) {
            advance();
            slice->step = parseTest();
        }
        
        return slice;
    }
    
    return test;
}

std::unique_ptr<Expr> Parser::parseSliceOp() {
    if (check(TokenType::Colon)) {
        advance();
        
        if (!check(TokenType::RightBracket) && !check(TokenType::Comma) && !isAtEnd()) {
            return parseTest();
        }
        
        auto slice = std::make_unique<Slice>();
        return slice;
    }
    
    return nullptr;
}

std::unique_ptr<Expr> Parser::parseDictOrSetMaker() {
    // 检查是否是集合（没有冒号）
    if (!check(TokenType::Colon)) {
        return parseSetDisplay();
    }
    
    return parseDictDisplay();
}

std::unique_ptr<Expr> Parser::parseStringConcat() {
    auto first = parseAtom();
    
    while (check(TokenType::String) || check(TokenType::RawString) || 
           check(TokenType::FString) || check(TokenType::Bytes)) {
        advance();
    }
    
    return first;
}

std::unique_ptr<Expr> Parser::parseFString() {
    // f-string 解析
    auto fstring = std::make_unique<JoinedStr>();
    fstring->position = getPosition();
    
    // 简化处理 - 实际需要解析 f-string 内容
    
    return fstring;
}

std::vector<std::unique_ptr<Expr>> Parser::parseFStringContent() {
    std::vector<std::unique_ptr<Expr>> content;
    
    while (!check(TokenType::FStringEnd) && !isAtEnd()) {
        if (check(TokenType::FStringText)) {
            advance();
        } else if (check(TokenType::FStringExpr)) {
            auto fv = std::make_unique<FormattedValue>();
            fv->value = parseTest();
            content.push_back(std::move(fv));
            
            if (check(TokenType::FStringConv)) {
                advance();
            }
            if (check(TokenType::FStringSpec)) {
                advance();
            }
            expect(TokenType::RightBrace);
        } else {
            break;
        }
    }
    
    return content;
}

}  // namespace csgo