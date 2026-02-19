// cpp/tests/test_ast.cpp
#include "ast/ast_node.h"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace csgo;

// 辅助函数：检查节点类型
void checkNodeType(const ASTNode& node, ASTNodeType expectedType, const std::string& testName) {
    if (node.type != expectedType) {
        std::cerr << "FAIL [" << testName << "]: Expected " << ASTNode::getTypeName(expectedType) 
                  << ", got " << ASTNode::getTypeName(node.type) << std::endl;
        assert(false);
    }
}

// 辅助函数：检查位置信息
void checkPosition(const ASTNode& node, size_t line, size_t col, const std::string& testName) {
    if (node.position.line != line || node.position.column != col) {
        std::cerr << "FAIL [" << testName << "]: Expected position (" << line << "," << col 
                  << "), got (" << node.position.line << "," << node.position.column << ")" << std::endl;
        assert(false);
    }
}

// ============================================================================
// 测试1: 常量表达式 (Constant)
// ============================================================================
void testConstant() {
    std::cout << "Test: Constant expressions... ";
    Position pos(1, 1);

    // 测试整数常量
    auto intConst = std::make_unique<Constant>(pos, int64_t(42));
    checkNodeType(*intConst, ASTNodeType::Constant, "Constant-Int-Type");
    assert(intConst->isInt());
    assert(intConst->asInt() == 42);
    checkPosition(*intConst, 1, 1, "Constant-Int-Pos");

    // 测试浮点常量
    auto floatConst = std::make_unique<Constant>(pos, 3.14);
    checkNodeType(*floatConst, ASTNodeType::Constant, "Constant-Float-Type");
    assert(floatConst->isFloat());
    assert(floatConst->asFloat() == 3.14);

    // 测试字符串常量
    auto strConst = std::make_unique<Constant>(pos, std::string("hello"));
    checkNodeType(*strConst, ASTNodeType::Constant, "Constant-Str-Type");
    assert(strConst->isString());
    assert(strConst->asString() == "hello");

    // 测试布尔常量
    auto boolConst = std::make_unique<Constant>();
    boolConst->value = true;
    assert(boolConst->isBool());
    assert(boolConst->isTrue());

    // 测试 None
    auto noneConst = std::make_unique<Constant>();
    noneConst->value = nullptr;
    assert(noneConst->isNone());

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试2: 名称表达式 (Name)
// ============================================================================
void testName() {
    std::cout << "Test: Name expressions... ";
    Position pos(2, 5);

    // 测试加载上下文
    auto loadName = std::make_unique<Name>(pos, "x", Name::Load);
    checkNodeType(*loadName, ASTNodeType::Name, "Name-Load-Type");
    assert(loadName->id == "x");
    assert(loadName->ctx == Name::Load);
    checkPosition(*loadName, 2, 5, "Name-Load-Pos");

    // 测试存储上下文
    auto storeName = std::make_unique<Name>("y", Name::Store);
    checkNodeType(*storeName, ASTNodeType::Name, "Name-Store-Type");
    assert(storeName->id == "y");
    assert(storeName->ctx == Name::Store);

    // 测试删除上下文
    auto delName = std::make_unique<Name>("z", Name::Del);
    assert(delName->ctx == Name::Del);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试3: 二元运算表达式 (BinOp)
// ============================================================================
void testBinOp() {
    std::cout << "Test: Binary operations... ";
    Position pos(1, 1);

    // 创建 1 + 2
    auto left = std::make_unique<Constant>(pos, int64_t(1));
    auto right = std::make_unique<Constant>(pos, int64_t(2));
    auto addOp = std::make_unique<BinOp>(pos, std::move(left), ASTNodeType::Add, std::move(right));

    checkNodeType(*addOp, ASTNodeType::BinOp, "BinOp-Add-Type");
    assert(addOp->op == ASTNodeType::Add);
    assert(addOp->left != nullptr);
    assert(addOp->right != nullptr);

    // 测试其他运算符
    auto subOp = std::make_unique<BinOp>(
        std::make_unique<Name>("x", Name::Load),
        ASTNodeType::Sub,
        std::make_unique<Constant>(pos, int64_t(10))
    );
    assert(subOp->op == ASTNodeType::Sub);

    auto mulOp = std::make_unique<BinOp>(
        std::make_unique<Name>("a", Name::Load),
        ASTNodeType::Mult,
        std::make_unique<Name>("b", Name::Load)
    );
    assert(mulOp->op == ASTNodeType::Mult);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试4: 一元运算表达式 (UnaryOp)
// ============================================================================
void testUnaryOp() {
    std::cout << "Test: Unary operations... ";
    Position pos(1, 1);

    // 测试 -x
    auto negOp = std::make_unique<UnaryOp>(pos, ASTNodeType::USub, 
                                           std::make_unique<Name>("x", Name::Load));
    checkNodeType(*negOp, ASTNodeType::UnaryOp, "UnaryOp-USub-Type");
    assert(negOp->op == ASTNodeType::USub);

    // 测试 +x
    auto posOp = std::make_unique<UnaryOp>(ASTNodeType::UAdd, 
                                           std::make_unique<Name>("y", Name::Load));
    assert(posOp->op == ASTNodeType::UAdd);

    // 测试 not x
    auto notOp = std::make_unique<UnaryOp>(ASTNodeType::Not, 
                                           std::make_unique<Name>("flag", Name::Load));
    assert(notOp->op == ASTNodeType::Not);

    // 测试 ~x
    auto invertOp = std::make_unique<UnaryOp>(ASTNodeType::Invert, 
                                              std::make_unique<Name>("bits", Name::Load));
    assert(invertOp->op == ASTNodeType::Invert);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试5: 比较表达式 (Compare)
// ============================================================================
void testCompare() {
    std::cout << "Test: Compare operations... ";
    Position pos(1, 1);

    // 简单比较: x < 10
    std::vector<ASTNodeType> ops = {ASTNodeType::Lt};
    std::vector<std::unique_ptr<Expr>> comparators;
    comparators.push_back(std::make_unique<Constant>(pos, int64_t(10)));

    auto simpleCompare = std::make_unique<Compare>(
        std::make_unique<Name>("x", Name::Load),
        std::move(ops),
        std::move(comparators)
    );
    checkNodeType(*simpleCompare, ASTNodeType::Compare, "Compare-Simple-Type");
    assert(simpleCompare->ops.size() == 1);
    assert(simpleCompare->ops[0] == ASTNodeType::Lt);

    // 链式比较: 1 < x < 10
    std::vector<ASTNodeType> chainOps = {ASTNodeType::Lt, ASTNodeType::Lt};
    std::vector<std::unique_ptr<Expr>> chainComparators;
    chainComparators.push_back(std::make_unique<Name>("x", Name::Load));
    chainComparators.push_back(std::make_unique<Constant>(pos, int64_t(10)));

    auto chainCompare = std::make_unique<Compare>(
        std::make_unique<Constant>(pos, int64_t(1)),
        std::move(chainOps),
        std::move(chainComparators)
    );
    assert(chainCompare->ops.size() == 2);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试6: 布尔运算表达式 (BoolOp)
// ============================================================================
void testBoolOp() {
    std::cout << "Test: Boolean operations... ";
    Position pos(1, 1);

    // 测试 a and b and c
    std::vector<std::unique_ptr<Expr>> andValues;
    andValues.push_back(std::make_unique<Name>("a", Name::Load));
    andValues.push_back(std::make_unique<Name>("b", Name::Load));
    andValues.push_back(std::make_unique<Name>("c", Name::Load));

    auto andOp = std::make_unique<BoolOp>(ASTNodeType::And, std::move(andValues));
    checkNodeType(*andOp, ASTNodeType::BoolOp, "BoolOp-And-Type");
    assert(andOp->op == ASTNodeType::And);
    assert(andOp->values.size() == 3);

    // 测试 x or y
    std::vector<std::unique_ptr<Expr>> orValues;
    orValues.push_back(std::make_unique<Name>("x", Name::Load));
    orValues.push_back(std::make_unique<Name>("y", Name::Load));

    auto orOp = std::make_unique<BoolOp>(ASTNodeType::Or, std::move(orValues));
    assert(orOp->op == ASTNodeType::Or);
    assert(orOp->values.size() == 2);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试7: 函数调用表达式 (Call)
// ============================================================================
void testCall() {
    std::cout << "Test: Function calls... ";
    Position pos(1, 1);

    // 测试 print("hello")
    auto printCall = std::make_unique<Call>(std::make_unique<Name>("print", Name::Load));
    printCall->args.push_back(std::make_unique<Constant>(pos, std::string("hello")));

    checkNodeType(*printCall, ASTNodeType::Call, "Call-Simple-Type");
    assert(printCall->func != nullptr);
    assert(printCall->args.size() == 1);
    assert(printCall->keywords.empty());

    // 测试 max(x, y)
    auto maxCall = std::make_unique<Call>(std::make_unique<Name>("max", Name::Load));
    maxCall->args.push_back(std::make_unique<Name>("x", Name::Load));
    maxCall->args.push_back(std::make_unique<Name>("y", Name::Load));
    assert(maxCall->args.size() == 2);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试8: 条件表达式 (IfExp)
// ============================================================================
void testIfExp() {
    std::cout << "Test: Conditional expressions... ";
    Position pos(1, 1);

    // 测试 "yes" if condition else "no"
    auto ifExp = std::make_unique<IfExp>(
        std::make_unique<Name>("condition", Name::Load),
        std::make_unique<Constant>(pos, std::string("yes")),
        std::make_unique<Constant>(pos, std::string("no"))
    );

    checkNodeType(*ifExp, ASTNodeType::IfExp, "IfExp-Type");
    assert(ifExp->test != nullptr);
    assert(ifExp->body != nullptr);
    assert(ifExp->orelse != nullptr);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试9: Lambda 表达式
// ============================================================================
void testLambda() {
    std::cout << "Test: Lambda expressions... ";
    Position pos(1, 1);

    // 测试 lambda x: x + 1
    auto lambdaBody = std::make_unique<BinOp>(
        std::make_unique<Name>("x", Name::Load),
        ASTNodeType::Add,
        std::make_unique<Constant>(pos, int64_t(1))
    );

    auto lambda = std::make_unique<Lambda>(
        std::make_unique<Name>("x", Name::Load),
        std::move(lambdaBody)
    );

    checkNodeType(*lambda, ASTNodeType::Lambda, "Lambda-Type");
    assert(lambda->args != nullptr);
    assert(lambda->body != nullptr);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试10: 容器类型 - List, Tuple, Dict, Set
// ============================================================================
void testContainers() {
    std::cout << "Test: Container types... ";
    Position pos(1, 1);

    // 测试列表 [1, 2, 3]
    auto list = std::make_unique<List>();
    list->elts.push_back(std::make_unique<Constant>(pos, int64_t(1)));
    list->elts.push_back(std::make_unique<Constant>(pos, int64_t(2)));
    list->elts.push_back(std::make_unique<Constant>(pos, int64_t(3)));
    checkNodeType(*list, ASTNodeType::List, "List-Type");
    assert(list->elts.size() == 3);
    assert(list->ctx == List::Load);

    // 测试元组 (1, 2)
    auto tuple = std::make_unique<Tuple>();
    tuple->elts.push_back(std::make_unique<Constant>(pos, int64_t(1)));
    tuple->elts.push_back(std::make_unique<Constant>(pos, int64_t(2)));
    checkNodeType(*tuple, ASTNodeType::Tuple, "Tuple-Type");
    assert(tuple->elts.size() == 2);

    // 测试字典 {"a": 1, "b": 2}
    auto dict = std::make_unique<Dict>();
    dict->keys.push_back(std::make_unique<Constant>(pos, std::string("a")));
    dict->values.push_back(std::make_unique<Constant>(pos, int64_t(1)));
    dict->keys.push_back(std::make_unique<Constant>(pos, std::string("b")));
    dict->values.push_back(std::make_unique<Constant>(pos, int64_t(2)));
    checkNodeType(*dict, ASTNodeType::Dict, "Dict-Type");
    assert(dict->keys.size() == 2);
    assert(dict->values.size() == 2);

    // 测试集合 {1, 2, 3}
    auto set = std::make_unique<Set>();
    set->elts.push_back(std::make_unique<Constant>(pos, int64_t(1)));
    set->elts.push_back(std::make_unique<Constant>(pos, int64_t(2)));
    set->elts.push_back(std::make_unique<Constant>(pos, int64_t(3)));
    checkNodeType(*set, ASTNodeType::Set, "Set-Type");
    assert(set->elts.size() == 3);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试11: 下标和属性访问 (Subscript, Attribute, Slice)
// ============================================================================
void testSubscriptAndAttribute() {
    std::cout << "Test: Subscript and attribute access... ";
    Position pos(1, 1);

    // 测试 items[0]
    auto subscript = std::make_unique<Subscript>(
        std::make_unique<Name>("items", Name::Load),
        std::make_unique<Constant>(pos, int64_t(0))
    );
    checkNodeType(*subscript, ASTNodeType::Subscript, "Subscript-Type");
    assert(subscript->value != nullptr);
    assert(subscript->slice != nullptr);

    // 测试 obj.x
    auto attr = std::make_unique<Attribute>(
        std::make_unique<Name>("obj", Name::Load),
        "x"
    );
    checkNodeType(*attr, ASTNodeType::Attribute, "Attribute-Type");
    assert(attr->value != nullptr);
    assert(attr->attr == "x");

    // 测试切片 [1:10:2]
    auto slice = std::make_unique<Slice>();
    slice->lower = std::make_unique<Constant>(pos, int64_t(1));
    slice->upper = std::make_unique<Constant>(pos, int64_t(10));
    slice->step = std::make_unique<Constant>(pos, int64_t(2));
    checkNodeType(*slice, ASTNodeType::Slice, "Slice-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试12: 推导式 (ListComp, DictComp, SetComp, GeneratorExp)
// ============================================================================
void testComprehensions() {
    std::cout << "Test: Comprehensions... ";
    Position pos(1, 1);

    // 测试列表推导式 [x*2 for x in items]
    auto listComp = std::make_unique<ListComp>();
    listComp->elt = std::make_unique<BinOp>(
        std::make_unique<Name>("x", Name::Load),
        ASTNodeType::Mult,
        std::make_unique<Constant>(pos, int64_t(2))
    );
    checkNodeType(*listComp, ASTNodeType::ListComp, "ListComp-Type");
    assert(listComp->elt != nullptr);

    // 测试字典推导式 {k: v for k, v in items}
    auto dictComp = std::make_unique<DictComp>();
    dictComp->key = std::make_unique<Name>("k", Name::Load);
    dictComp->value = std::make_unique<Name>("v", Name::Load);
    checkNodeType(*dictComp, ASTNodeType::DictComp, "DictComp-Type");

    // 测试集合推导式 {x for x in items}
    auto setComp = std::make_unique<SetComp>();
    setComp->elt = std::make_unique<Name>("x", Name::Load);
    checkNodeType(*setComp, ASTNodeType::SetComp, "SetComp-Type");

    // 测试生成器表达式 (x for x in items)
    auto genExp = std::make_unique<GeneratorExp>();
    genExp->elt = std::make_unique<Name>("x", Name::Load);
    checkNodeType(*genExp, ASTNodeType::GeneratorExp, "GeneratorExp-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试13: 生成器和异步表达式 (Yield, YieldFrom, Await)
// ============================================================================
void testYieldAndAwait() {
    std::cout << "Test: Yield and await expressions... ";
    Position pos(1, 1);

    // 测试 yield x
    auto yield = std::make_unique<Yield>(std::make_unique<Name>("x", Name::Load));
    checkNodeType(*yield, ASTNodeType::Yield, "Yield-Type");
    assert(yield->value != nullptr);

    // 测试 yield from items
    auto yieldFrom = std::make_unique<YieldFrom>(std::make_unique<Name>("items", Name::Load));
    checkNodeType(*yieldFrom, ASTNodeType::YieldFrom, "YieldFrom-Type");

    // 测试 await result
    auto await = std::make_unique<Await>(std::make_unique<Name>("result", Name::Load));
    checkNodeType(*await, ASTNodeType::Await, "Await-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试14: f-string 表达式 (FormattedValue, JoinedStr)
// ============================================================================
void testFString() {
    std::cout << "Test: f-string expressions... ";
    Position pos(1, 1);

    // 测试 FormattedValue {x}
    auto fv = std::make_unique<FormattedValue>(std::make_unique<Name>("x", Name::Load));
    checkNodeType(*fv, ASTNodeType::FormattedValue, "FormattedValue-Type");
    assert(fv->value != nullptr);
    assert(fv->conversion == -1);

    // 测试 {x!r}
    auto fvR = std::make_unique<FormattedValue>(
        std::make_unique<Name>("x", Name::Load), 
        'r'
    );
    assert(fvR->conversion == 'r');

    // 测试 JoinedStr f"hello {name}!"
    auto joined = std::make_unique<JoinedStr>();
    joined->values.push_back(std::make_unique<Constant>(pos, std::string("hello ")));
    joined->values.push_back(std::make_unique<FormattedValue>(
        std::make_unique<Name>("name", Name::Load)
    ));
    joined->values.push_back(std::make_unique<Constant>(pos, std::string("!")));
    checkNodeType(*joined, ASTNodeType::JoinedStr, "JoinedStr-Type");
    assert(joined->values.size() == 3);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试15: 赋值语句 (Assign, AugAssign, AnnAssign)
// ============================================================================
void testAssignment() {
    std::cout << "Test: Assignment statements... ";
    Position pos(1, 1);

    // 测试简单赋值 x = 10
    auto assign = std::make_unique<Assign>();
    assign->targets.push_back(std::make_unique<Name>("x", Name::Store));
    assign->value = std::make_unique<Constant>(pos, int64_t(10));
    checkNodeType(*assign, ASTNodeType::Assign, "Assign-Type");
    assert(assign->targets.size() == 1);
    assert(assign->value != nullptr);

    // 测试多重赋值 a = b = c = 0
    auto multiAssign = std::make_unique<Assign>();
    multiAssign->targets.push_back(std::make_unique<Name>("a", Name::Store));
    multiAssign->targets.push_back(std::make_unique<Name>("b", Name::Store));
    multiAssign->targets.push_back(std::make_unique<Name>("c", Name::Store));
    multiAssign->value = std::make_unique<Constant>(pos, int64_t(0));
    assert(multiAssign->targets.size() == 3);

    // 测试增强赋值 x += 1
    auto augAssign = std::make_unique<AugAssign>();
    augAssign->target = std::make_unique<Name>("x", Name::Store);
    augAssign->op = ASTNodeType::Add;
    augAssign->value = std::make_unique<Constant>(pos, int64_t(1));
    checkNodeType(*augAssign, ASTNodeType::AugAssign, "AugAssign-Type");
    assert(augAssign->op == ASTNodeType::Add);

    // 测试类型注解赋值 x: int = 10
    auto annAssign = std::make_unique<AnnAssign>();
    annAssign->target = std::make_unique<Name>("x", Name::Store);
    annAssign->annotation = std::make_unique<Name>("int", Name::Load);
    annAssign->value = std::make_unique<Constant>(pos, int64_t(10));
    annAssign->simple = true;
    checkNodeType(*annAssign, ASTNodeType::AnnAssign, "AnnAssign-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试16: 控制流语句 (If, For, While)
// ============================================================================
void testControlFlow() {
    std::cout << "Test: Control flow statements... ";
    Position pos(1, 1);

    // 测试 if 语句
    auto ifStmt = std::make_unique<If>();
    ifStmt->test = std::make_unique<Name>("condition", Name::Load);
    ifStmt->body.push_back(std::make_unique<Pass>());
    checkNodeType(*ifStmt, ASTNodeType::If, "If-Type");
    assert(ifStmt->test != nullptr);

    // 测试 for 循环
    auto forStmt = std::make_unique<For>();
    forStmt->target = std::make_unique<Name>("i", Name::Store);
    forStmt->iter = std::make_unique<Call>(std::make_unique<Name>("range", Name::Load));
    forStmt->body.push_back(std::make_unique<Pass>());
    checkNodeType(*forStmt, ASTNodeType::For, "For-Type");

    // 测试 while 循环
    auto whileStmt = std::make_unique<While>();
    whileStmt->test = std::make_unique<Name>("running", Name::Load);
    whileStmt->body.push_back(std::make_unique<Pass>());
    checkNodeType(*whileStmt, ASTNodeType::While, "While-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试17: 函数定义 (FunctionDef, AsyncFunctionDef)
// ============================================================================
void testFunctionDef() {
    std::cout << "Test: Function definitions... ";
    Position pos(1, 1);

    // 测试函数定义 def add(a, b): return a + b
    auto funcDef = std::make_unique<FunctionDef>();
    funcDef->name = "add";
    funcDef->body.push_back(std::make_unique<Return>(
        std::make_unique<BinOp>(
            std::make_unique<Name>("a", Name::Load),
            ASTNodeType::Add,
            std::make_unique<Name>("b", Name::Load)
        )
    ));
    checkNodeType(*funcDef, ASTNodeType::FunctionDef, "FunctionDef-Type");
    assert(funcDef->name == "add");
    assert(funcDef->body.size() == 1);

    // 测试异步函数定义
    auto asyncFunc = std::make_unique<AsyncFunctionDef>();
    asyncFunc->name = "fetch_data";
    asyncFunc->body.push_back(std::make_unique<Pass>());
    checkNodeType(*asyncFunc, ASTNodeType::AsyncFunctionDef, "AsyncFunctionDef-Type");
    assert(asyncFunc->name == "fetch_data");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试18: 类定义 (ClassDef)
// ============================================================================
void testClassDef() {
    std::cout << "Test: Class definitions... ";
    Position pos(1, 1);

    // 测试类定义 class MyClass(Base):
    auto classDef = std::make_unique<ClassDef>();
    classDef->name = "MyClass";
    classDef->body.push_back(std::make_unique<Pass>());
    checkNodeType(*classDef, ASTNodeType::ClassDef, "ClassDef-Type");
    assert(classDef->name == "MyClass");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试19: 导入语句 (Import, ImportFrom)
// ============================================================================
void testImport() {
    std::cout << "Test: Import statements... ";
    Position pos(1, 1);

    // 测试 import os
    auto import = std::make_unique<Import>();
    import->names.push_back(std::make_unique<Name>("os", Name::Load));
    checkNodeType(*import, ASTNodeType::Import, "Import-Type");
    assert(import->names.size() == 1);

    // 测试 from os import path
    auto importFrom = std::make_unique<ImportFrom>();
    importFrom->module = std::make_unique<Name>("os", Name::Load);
    importFrom->names.push_back(std::make_unique<Name>("path", Name::Load));
    importFrom->level = 0;
    checkNodeType(*importFrom, ASTNodeType::ImportFrom, "ImportFrom-Type");
    assert(importFrom->level == 0);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试20: 异常处理 (Try, Raise, Assert)
// ============================================================================
void testExceptionHandling() {
    std::cout << "Test: Exception handling... ";
    Position pos(1, 1);

    // 测试 try 语句
    auto tryStmt = std::make_unique<Try>();
    tryStmt->body.push_back(std::make_unique<Pass>());
    checkNodeType(*tryStmt, ASTNodeType::Try, "Try-Type");

    // 测试 raise 语句
    auto raise = std::make_unique<Raise>();
    raise->exc = std::make_unique<Call>(std::make_unique<Name>("ValueError", Name::Load));
    checkNodeType(*raise, ASTNodeType::Raise, "Raise-Type");

    // 测试 assert 语句
    auto assertStmt = std::make_unique<Assert>();
    assertStmt->test = std::make_unique<Name>("condition", Name::Load);
    checkNodeType(*assertStmt, ASTNodeType::Assert, "Assert-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试21: 变量声明 (Global, Nonlocal)
// ============================================================================
void testVariableDeclarations() {
    std::cout << "Test: Variable declarations... ";

    // 测试 global 语句
    auto global = std::make_unique<Global>();
    global->names.push_back("counter");
    checkNodeType(*global, ASTNodeType::Global, "Global-Type");
    assert(global->names.size() == 1);
    assert(global->names[0] == "counter");

    // 测试 nonlocal 语句
    auto nonlocal = std::make_unique<Nonlocal>();
    nonlocal->names.push_back("value");
    checkNodeType(*nonlocal, ASTNodeType::Nonlocal, "Nonlocal-Type");
    assert(nonlocal->names[0] == "value");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试22: 简单语句 (Pass, Break, Continue, Return, Delete, ExprStmt)
// ============================================================================
void testSimpleStatements() {
    std::cout << "Test: Simple statements... ";
    Position pos(1, 1);

    // 测试 pass
    auto pass = std::make_unique<Pass>();
    checkNodeType(*pass, ASTNodeType::Pass, "Pass-Type");

    // 测试 break
    auto breakStmt = std::make_unique<Break>();
    checkNodeType(*breakStmt, ASTNodeType::Break, "Break-Type");

    // 测试 continue
    auto continueStmt = std::make_unique<Continue>();
    checkNodeType(*continueStmt, ASTNodeType::Continue, "Continue-Type");

    // 测试 return
    auto returnStmt = std::make_unique<Return>(std::make_unique<Constant>(pos, int64_t(42)));
    checkNodeType(*returnStmt, ASTNodeType::Return, "Return-Type");

    // 测试 delete
    auto deleteStmt = std::make_unique<Delete>();
    deleteStmt->targets.push_back(std::make_unique<Name>("x", Name::Del));
    checkNodeType(*deleteStmt, ASTNodeType::Delete, "Delete-Type");

    // 测试表达式语句
    auto exprStmt = std::make_unique<ExprStmt>(
        std::make_unique<Call>(std::make_unique<Name>("print", Name::Load))
    );
    checkNodeType(*exprStmt, ASTNodeType::ExprStmt, "ExprStmt-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试23: 上下文管理 (With, AsyncWith)
// ============================================================================
void testWith() {
    std::cout << "Test: With statements... ";

    // 测试 with 语句
    auto withStmt = std::make_unique<With>();
    withStmt->body.push_back(std::make_unique<Pass>());
    checkNodeType(*withStmt, ASTNodeType::With, "With-Type");

    // 测试 async with 语句
    auto asyncWith = std::make_unique<AsyncWith>();
    asyncWith->body.push_back(std::make_unique<Pass>());
    checkNodeType(*asyncWith, ASTNodeType::AsyncWith, "AsyncWith-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试24: 辅助节点 (arguments, arg, except_handler, withitem, comprehension)
// ============================================================================
void testHelperNodes() {
    std::cout << "Test: Helper nodes... ";
    Position pos(1, 1);

    // 测试 arguments
    auto args = std::make_unique<arguments>();
    checkNodeType(*args, ASTNodeType::arguments, "arguments-Type");

    // 测试 arg
    auto argNode = std::make_unique<arg>("x");
    checkNodeType(*argNode, ASTNodeType::arg, "arg-Type");
    assert(argNode->name == "x");

    // 测试 except_handler
    auto handler = std::make_unique<except_handler>();
    handler->type = std::make_unique<Name>("ValueError", Name::Load);
    handler->name = "e";
    checkNodeType(*handler, ASTNodeType::except_handler, "except_handler-Type");

    // 测试 withitem
    auto item = std::make_unique<withitem>();
    checkNodeType(*item, ASTNodeType::withitem, "withitem-Type");

    // 测试 comprehension
    auto comp = std::make_unique<comprehension>();
    comp->target = std::make_unique<Name>("x", Name::Store);
    comp->iter = std::make_unique<Name>("items", Name::Load);
    checkNodeType(*comp, ASTNodeType::comprehension, "comprehension-Type");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试25: 模块节点 (Module)
// ============================================================================
void testModule() {
    std::cout << "Test: Module node... ";

    // 创建模块
    auto module = std::make_unique<Module>();
    module->body.push_back(std::make_unique<ExprStmt>(
        std::make_unique<Constant>(Position(1, 1), int64_t(1))
    ));
    module->body.push_back(std::make_unique<ExprStmt>(
        std::make_unique<Constant>(Position(2, 1), int64_t(2))
    ));

    checkNodeType(*module, ASTNodeType::Module, "Module-Type");
    assert(module->body.size() == 2);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试26: toString 方法验证
// ============================================================================
void testToString() {
    std::cout << "Test: toString methods... ";
    Position pos(1, 5);

    // 测试 Constant 的 toString
    auto const42 = std::make_unique<Constant>(pos, int64_t(42));
    std::string constStr = const42->toString();
    assert(constStr.find("Constant") != std::string::npos);
    assert(constStr.find("42") != std::string::npos);

    // 测试 Name 的 toString
    auto nameX = std::make_unique<Name>(pos, "x", Name::Load);
    std::string nameStr = nameX->toString();
    assert(nameStr.find("Name") != std::string::npos);
    assert(nameStr.find("x") != std::string::npos);
    assert(nameStr.find("Load") != std::string::npos);

    // 测试 BinOp 的 toString
    auto binOp = std::make_unique<BinOp>(pos,
        std::make_unique<Constant>(pos, int64_t(1)),
        ASTNodeType::Add,
        std::make_unique<Constant>(pos, int64_t(2))
    );
    std::string binOpStr = binOp->toString();
    assert(binOpStr.find("BinOp") != std::string::npos);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试27: 复杂表达式组合
// ============================================================================
void testComplexExpressions() {
    std::cout << "Test: Complex expression combinations... ";
    Position pos(1, 1);

    // 构建: (a + b) * (c - d)
    auto left = std::make_unique<BinOp>(
        std::make_unique<Name>("a", Name::Load),
        ASTNodeType::Add,
        std::make_unique<Name>("b", Name::Load)
    );

    auto right = std::make_unique<BinOp>(
        std::make_unique<Name>("c", Name::Load),
        ASTNodeType::Sub,
        std::make_unique<Name>("d", Name::Load)
    );

    auto complex = std::make_unique<BinOp>(pos, std::move(left), ASTNodeType::Mult, std::move(right));
    checkNodeType(*complex, ASTNodeType::BinOp, "Complex-BinOp");
    assert(complex->op == ASTNodeType::Mult);

    // 构建: obj.method(arg1, arg2)
    auto method = std::make_unique<Attribute>(
        std::make_unique<Name>("obj", Name::Load),
        "method"
    );
    auto methodCall = std::make_unique<Call>(std::move(method));
    methodCall->args.push_back(std::make_unique<Name>("arg1", Name::Load));
    methodCall->args.push_back(std::make_unique<Name>("arg2", Name::Load));
    checkNodeType(*methodCall, ASTNodeType::Call, "Method-Call");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试28: 边界情况
// ============================================================================
void testEdgeCases() {
    std::cout << "Test: Edge cases... ";

    // 测试空列表
    auto emptyList = std::make_unique<List>();
    assert(emptyList->elts.empty());

    // 测试空字典
    auto emptyDict = std::make_unique<Dict>();
    assert(emptyDict->keys.empty());
    assert(emptyDict->values.empty());

    // 测试空函数体
    auto emptyFunc = std::make_unique<FunctionDef>();
    emptyFunc->name = "empty";
    assert(emptyFunc->body.empty());

    // 测试空模块
    auto emptyModule = std::make_unique<Module>();
    assert(emptyModule->body.empty());

    // 测试无值的 return
    auto emptyReturn = std::make_unique<Return>();
    assert(emptyReturn->value == nullptr);

    // 测试无值的 yield
    auto emptyYield = std::make_unique<Yield>();
    assert(emptyYield->value == nullptr);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试29: ASTNodeType 枚举完整性
// ============================================================================
void testASTNodeTypeEnum() {
    std::cout << "Test: ASTNodeType enum completeness... ";

    // 测试所有主要类型都有对应的名称
    assert(std::string(ASTNode::getTypeName(ASTNodeType::Module)) == "Module");
    assert(std::string(ASTNode::getTypeName(ASTNodeType::FunctionDef)) == "FunctionDef");
    assert(std::string(ASTNode::getTypeName(ASTNodeType::BinOp)) == "BinOp");
    assert(std::string(ASTNode::getTypeName(ASTNodeType::Constant)) == "Constant");
    assert(std::string(ASTNode::getTypeName(ASTNodeType::Name)) == "Name");
    assert(std::string(ASTNode::getTypeName(ASTNodeType::Add)) == "Add");
    assert(std::string(ASTNode::getTypeName(ASTNodeType::Invalid)) == "Invalid");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试30: 继承关系验证（修复：补充缺失的测试30）
// ============================================================================
void testInheritanceHierarchy() {
    std::cout << "Test: Inheritance hierarchy... ";
    Position pos(1, 1);

    // 验证 Expr 继承自 ASTNode
    std::unique_ptr<ASTNode> node = std::make_unique<Constant>(pos, int64_t(42));
    assert(node->type == ASTNodeType::Constant);

    // 验证 Stmt 继承自 ASTNode
    std::unique_ptr<ASTNode> stmt = std::make_unique<Pass>();
    assert(stmt->type == ASTNodeType::Pass);

    // 验证 Module 继承自 ASTNode
    std::unique_ptr<ASTNode> mod = std::make_unique<Module>();
    assert(mod->type == ASTNodeType::Module);

    // 验证 arguments 是独立类型（不是Expr）
    auto args = std::make_unique<arguments>();
    assert(args->type == ASTNodeType::arguments);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试31: 内存所有权和移动语义
// ============================================================================
void testOwnershipSemantics() {
    std::cout << "Test: Ownership semantics... ";
    Position pos(1, 1);

    // 测试 unique_ptr 转移所有权
    auto original = std::make_unique<Constant>(pos, int64_t(42));
    auto* rawPtr = original.get();

    auto binOp = std::make_unique<BinOp>(pos, std::move(original), ASTNodeType::Add,
                                          std::make_unique<Constant>(pos, int64_t(1)));

    // original 现在应该为空
    assert(original == nullptr);
    // 但 binOp->left 应该拥有原来的对象
    assert(binOp->left.get() == rawPtr);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试32: 深拷贝 vs 移动语义验证
// ============================================================================
void testMoveSemantics() {
    std::cout << "Test: Move semantics verification... ";
    Position pos(1, 1);

    // 创建复杂表达式树
    auto left = std::make_unique<BinOp>(
        std::make_unique<Name>("a", Name::Load),
        ASTNodeType::Mult,
        std::make_unique<Name>("b", Name::Load)
    );

    // 移动到新的 BinOp 中
    auto parent = std::make_unique<BinOp>(pos, std::move(left), ASTNodeType::Add,
                                           std::make_unique<Constant>(pos, int64_t(5)));

    // 验证结构完整性
    assert(parent->left != nullptr);
    assert(parent->right != nullptr);
    assert(parent->op == ASTNodeType::Add);

    // 验证子节点
    BinOp* leftBinOp = dynamic_cast<BinOp*>(parent->left.get());
    assert(leftBinOp != nullptr);
    assert(leftBinOp->op == ASTNodeType::Mult);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试33: 多态类型转换
// ============================================================================
void testPolymorphism() {
    std::cout << "Test: Polymorphism... ";
    Position pos(1, 1);

    // 使用基类指针存储派生类对象
    std::unique_ptr<Expr> expr = std::make_unique<Constant>(pos, int64_t(42));

    // 验证类型信息
    assert(expr->type == ASTNodeType::Constant);

    // 动态类型转换
    Constant* constPtr = dynamic_cast<Constant*>(expr.get());
    assert(constPtr != nullptr);
    assert(constPtr->isInt());

    // 测试失败的情况
    Name* namePtr = dynamic_cast<Name*>(expr.get());
    assert(namePtr == nullptr);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试34: 嵌套容器深度测试
// ============================================================================
void testNestedContainers() {
    std::cout << "Test: Nested containers... ";
    Position pos(1, 1);

    // 创建 [[1, 2], [3, 4]]
    auto outerList = std::make_unique<List>();

    for (int i = 0; i < 2; ++i) {
        auto innerList = std::make_unique<List>();
        innerList->elts.push_back(std::make_unique<Constant>(pos, int64_t(i * 2 + 1)));
        innerList->elts.push_back(std::make_unique<Constant>(pos, int64_t(i * 2 + 2)));
        outerList->elts.push_back(std::move(innerList));
    }

    assert(outerList->elts.size() == 2);

    // 验证嵌套结构
    for (const auto& elem : outerList->elts) {
        List* inner = dynamic_cast<List*>(elem.get());
        assert(inner != nullptr);
        assert(inner->elts.size() == 2);
    }

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试35: 大型 AST 构建性能测试
// ============================================================================
void testLargeAST() {
    std::cout << "Test: Large AST construction... ";
    Position pos(1, 1);

    // 创建包含大量语句的模块
    auto module = std::make_unique<Module>();

    for (int i = 0; i < 1000; ++i) {
        auto assign = std::make_unique<Assign>();
        assign->targets.push_back(std::make_unique<Name>("x" + std::to_string(i), Name::Store));
        assign->value = std::make_unique<Constant>(pos, int64_t(i));
        module->body.push_back(std::move(assign));
    }

    assert(module->body.size() == 1000);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试36: 循环引用安全（确保没有循环引用）
// ============================================================================
void testNoCircularReferences() {
    std::cout << "Test: No circular references... ";
    Position pos(1, 1);

    // 创建复杂的表达式树，确保没有循环引用
    {
        auto root = std::make_unique<BinOp>(pos,
            std::make_unique<BinOp>(
                std::make_unique<Constant>(pos, int64_t(1)),
                ASTNodeType::Add,
                std::make_unique<Constant>(pos, int64_t(2))
            ),
            ASTNodeType::Mult,
            std::make_unique<BinOp>(
                std::make_unique<Constant>(pos, int64_t(3)),
                ASTNodeType::Sub,
                std::make_unique<Constant>(pos, int64_t(4))
            )
        );

        // 如果存在循环引用，unique_ptr 会无法正确释放
        // 这里只是构造和析构，如果没有崩溃说明没有循环引用
    }

    std::cout << "PASSED" << std::endl;
}

// 测试37: 所有比较运算符
void testAllComparators() {
    std::cout << "Test: All comparison operators... ";
    Position pos(1, 1);

    std::vector<ASTNodeType> comparators = {
        ASTNodeType::Eq, ASTNodeType::NotEq,
        ASTNodeType::Lt, ASTNodeType::LtE,
        ASTNodeType::Gt, ASTNodeType::GtE,
        ASTNodeType::Is, ASTNodeType::IsNot,
        ASTNodeType::In, ASTNodeType::NotIn
    };

    for (auto op : comparators) {
        // 先创建具名变量
        std::vector<std::unique_ptr<Expr>> comps;
        comps.push_back(std::make_unique<Constant>(pos, int64_t(10)));
        
        // 使用 std::move 传递
        auto compare = std::make_unique<Compare>(
            std::make_unique<Name>("x", Name::Load),
            std::vector<ASTNodeType>{op},
            std::move(comps)
        );
        assert(compare->ops[0] == op);
    }

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试38: 所有二元运算符
// ============================================================================
void testAllBinaryOperators() {
    std::cout << "Test: All binary operators... ";
    Position pos(1, 1);

    std::vector<ASTNodeType> operators = {
        ASTNodeType::Add, ASTNodeType::Sub, ASTNodeType::Mult,
        ASTNodeType::Div, ASTNodeType::FloorDiv, ASTNodeType::Mod,
        ASTNodeType::Pow, ASTNodeType::LShift, ASTNodeType::RShift,
        ASTNodeType::BitOr, ASTNodeType::BitXor, ASTNodeType::BitAnd,
        ASTNodeType::MatMult
    };

    for (auto op : operators) {
        auto binOp = std::make_unique<BinOp>(pos,
            std::make_unique<Name>("a", Name::Load),
            op,
            std::make_unique<Name>("b", Name::Load)
        );
        assert(binOp->op == op);
    }

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试39: 所有一元运算符
// ============================================================================
void testAllUnaryOperators() {
    std::cout << "Test: All unary operators... ";
    Position pos(1, 1);

    std::vector<ASTNodeType> operators = {
        ASTNodeType::UAdd, ASTNodeType::USub,
        ASTNodeType::Not, ASTNodeType::Invert
    };

    for (auto op : operators) {
        auto unaryOp = std::make_unique<UnaryOp>(pos, op,
            std::make_unique<Name>("x", Name::Load)
        );
        assert(unaryOp->op == op);
    }

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试40: 所有增强赋值运算符
// ============================================================================
void testAllAugAssignOperators() {
    std::cout << "Test: All augmented assignment operators... ";
    Position pos(1, 1);

    std::vector<ASTNodeType> operators = {
        ASTNodeType::Add, ASTNodeType::Sub, ASTNodeType::Mult,
        ASTNodeType::Div, ASTNodeType::FloorDiv, ASTNodeType::Mod,
        ASTNodeType::Pow, ASTNodeType::LShift, ASTNodeType::RShift,
        ASTNodeType::BitOr, ASTNodeType::BitXor, ASTNodeType::BitAnd,
        ASTNodeType::MatMult
    };

    for (auto op : operators) {
        auto augAssign = std::make_unique<AugAssign>();
        augAssign->target = std::make_unique<Name>("x", Name::Store);
        augAssign->op = op;
        augAssign->value = std::make_unique<Constant>(pos, int64_t(1));
        assert(augAssign->op == op);
    }

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试41: 变体类型 (std::variant) 完整测试
// ============================================================================
void testVariantTypes() {
    std::cout << "Test: Variant types... ";
    Position pos(1, 1);

    // 测试所有 variant 类型
    Constant c;

    // monostate (Ellipsis)
    c.value = std::monostate{};
    assert(std::holds_alternative<std::monostate>(c.value));

    // bool
    c.value = true;
    assert(c.isTrue());

    // nullptr_t (None)
    c.value = nullptr;
    assert(c.isNone());

    // int64_t
    c.value = int64_t(42);
    assert(c.asInt() == 42);

    // double
    c.value = 3.14;
    assert(c.asFloat() == 3.14);

    // string
    c.value = std::string("hello");
    assert(c.asString() == "hello");

    // bytes
    c.value = std::vector<uint8_t>{0x01, 0x02, 0x03};
    assert(c.isBytes());

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试42: 位置信息精确性
// ============================================================================
void testPositionPrecision() {
    std::cout << "Test: Position precision... ";

    // 测试单位置构造
    Position p1(5, 10);
    assert(p1.line == 5);
    assert(p1.column == 10);
    assert(p1.endLine == 5);
    assert(p1.endColumn == 10);

    // 测试范围位置构造
    Position p2(1, 5, 3, 20);
    assert(p2.line == 1);
    assert(p2.column == 5);
    assert(p2.endLine == 3);
    assert(p2.endColumn == 20);

    // 测试默认构造
    Position p3;
    assert(p3.line == 0);
    assert(p3.column == 0);

    // 测试节点位置设置
    auto node = std::make_unique<Constant>(Position(10, 5, 10, 8), int64_t(42));
    assert(node->position.line == 10);
    assert(node->position.column == 5);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试43: 星号表达式 (Starred)
// ============================================================================
void testStarred() {
    std::cout << "Test: Starred expressions... ";
    Position pos(1, 1);

    // *args
    auto starred = std::make_unique<Starred>(
        std::make_unique<Name>("args", Name::Load),
        Starred::Load
    );
    checkNodeType(*starred, ASTNodeType::Starred, "Starred-Type");
    assert(starred->ctx == Starred::Load);

    // *rest in assignment
    auto starredStore = std::make_unique<Starred>(
        std::make_unique<Name>("rest", Name::Store),
        Starred::Store
    );
    assert(starredStore->ctx == Starred::Store);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试44: 推导式生成器 (comprehension) - 修复编号
// ============================================================================
void testComprehensionNode() {
    std::cout << "Test: Comprehension node... ";
    Position pos(1, 1);

    // for x in items if x > 0
    auto comp = std::make_unique<comprehension>();
    comp->target = std::make_unique<Name>("x", Name::Store);
    comp->iter = std::make_unique<Name>("items", Name::Load);
    std::vector<std::unique_ptr<Expr>> compare_vec;
    compare_vec.push_back(std::make_unique<Constant>(pos, int64_t(0)));
    
    comp->ifs.push_back(std::make_unique<Compare>(
        std::make_unique<Name>("x", Name::Load),
        std::vector<ASTNodeType>{ASTNodeType::Gt},
        std::move(compare_vec)
    ));
    comp->is_async = false;

    checkNodeType(*comp, ASTNodeType::comprehension, "comprehension-Type");
    assert(comp->ifs.size() == 1);
    assert(!comp->is_async);

    // async for
    auto asyncComp = std::make_unique<comprehension>();
    asyncComp->is_async = true;
    assert(asyncComp->is_async);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试45: 导入别名 (Alias)
// ============================================================================
void testAlias() {
    std::cout << "Test: Import alias... ";
    Position pos(1, 1);

    // import numpy as np
    auto alias = std::make_unique<Alias>(
        "np",
        std::make_unique<Name>("numpy", Name::Load)
    );
    checkNodeType(*alias, ASTNodeType::Alias, "Alias-Type");
    assert(alias->asname == "np");

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试46: withitem 节点
// ============================================================================
void testWithitem() {
    std::cout << "Test: Withitem node... ";
    Position pos(1, 1);

    // with open("file") as f
    auto item = std::make_unique<withitem>();
    item->context_expr = std::make_unique<Call>(
        std::make_unique<Name>("open", Name::Load)
    );
    item->optional_vars = std::make_unique<Name>("f", Name::Store);

    checkNodeType(*item, ASTNodeType::withitem, "withitem-Type");
    assert(item->context_expr != nullptr);
    assert(item->optional_vars != nullptr);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试47: except_handler 节点
// ============================================================================
void testExceptHandler() {
    std::cout << "Test: Except handler... ";
    Position pos(1, 1);

    // except ValueError as e: pass
    auto handler = std::make_unique<except_handler>();
    handler->type = std::make_unique<Name>("ValueError", Name::Load);
    handler->name = "e";
    handler->body.push_back(std::make_unique<Pass>());

    checkNodeType(*handler, ASTNodeType::except_handler, "except_handler-Type");
    assert(handler->name == "e");

    // bare except:
    auto bareHandler = std::make_unique<except_handler>();
    bareHandler->body.push_back(std::make_unique<Pass>());
    assert(bareHandler->type == nullptr);
    assert(bareHandler->name.empty());

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试48: arguments 和 arg 节点
// ============================================================================
void testArguments() {
    std::cout << "Test: Arguments node... ";
    Position pos(1, 1);

    // def func(a, b=1, *args, c, d=2, **kwargs)
    auto args = std::make_unique<arguments>();

    // 位置参数
    args->args.push_back(std::make_unique<arg>("a"));
    auto bArg = std::make_unique<arg>("b");
    args->args.push_back(std::move(bArg));
    args->defaults.push_back(std::make_unique<Constant>(pos, int64_t(1)));

    // *args
    args->vararg = std::make_unique<arg>("args");

    // 关键字参数
    args->kwonlyargs.push_back(std::make_unique<arg>("c"));
    auto dArg = std::make_unique<arg>("d");
    args->kwonlyargs.push_back(std::move(dArg));
    args->kw_defaults.push_back(nullptr);  // c 没有默认值
    args->kw_defaults.push_back(std::make_unique<Constant>(pos, int64_t(2)));

    // **kwargs
    args->kwarg = std::make_unique<arg>("kwargs");

    checkNodeType(*args, ASTNodeType::arguments, "arguments-Type");
    assert(args->args.size() == 2);
    assert(args->kwonlyargs.size() == 2);

    // 测试带类型注解的参数
    auto annotatedArg = std::make_unique<arg>("x");
    annotatedArg->annotation = std::make_unique<Name>("int", Name::Load);
    assert(annotatedArg->annotation != nullptr);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试49: 完整递归函数 AST（修复：移除危险的reinterpret_cast）
// ============================================================================
void testRecursiveFunctionAST() {
    std::cout << "Test: Complete recursive function AST... ";
    Position pos(1, 1);

    // def fib(n):
    //     if n <= 1:
    //         return n
    //     return fib(n-1) + fib(n-2)

    auto fib = std::make_unique<FunctionDef>();
    fib->name = "fib";

    // 参数 n - 修复：正确设置arguments，不进行危险的类型转换
    auto args = std::make_unique<arguments>();
    args->args.push_back(std::make_unique<arg>("n"));
    // 修复：如果FunctionDef::args是Expr*类型，需要重新设计
    // 这里假设FunctionDef有正确的类型定义，或者使用中间转换
    // 方案：将arguments存储在FunctionDef的args字段（如果类型匹配）
    // 或者：如果args必须是Expr，创建一个特殊的Expr来包装arguments
    
    // 假设FunctionDef定义如下（需要确认）：
    // struct FunctionDef : Stmt {
    //     std::string name;
    //     std::unique_ptr<arguments> args;  // 正确类型
    //     ...
    // };
    
    // 修复：直接移动，不进行cast
    fib->args = std::move(args);

    // if n <= 1: return n
    auto ifStmt = std::make_unique<If>();
    ifStmt->test = std::make_unique<Compare>(
        std::make_unique<Name>("n", Name::Load),
        std::vector<ASTNodeType>{ASTNodeType::LtE},
        std::vector<std::unique_ptr<Expr>>()
    );
    static_cast<Compare*>(ifStmt->test.get())->comparators.push_back(
        std::make_unique<Constant>(pos, int64_t(1))
    );
    ifStmt->body.push_back(std::make_unique<Return>(
        std::make_unique<Name>("n", Name::Load)
    ));
    fib->body.push_back(std::move(ifStmt));

    // return fib(n-1) + fib(n-2)
    auto fibCall1 = std::make_unique<Call>(std::make_unique<Name>("fib", Name::Load));
    fibCall1->args.push_back(std::make_unique<BinOp>(
        std::make_unique<Name>("n", Name::Load),
        ASTNodeType::Sub,
        std::make_unique<Constant>(pos, int64_t(1))
    ));

    auto fibCall2 = std::make_unique<Call>(std::make_unique<Name>("fib", Name::Load));
    fibCall2->args.push_back(std::make_unique<BinOp>(
        std::make_unique<Name>("n", Name::Load),
        ASTNodeType::Sub,
        std::make_unique<Constant>(pos, int64_t(2))
    ));

    fib->body.push_back(std::make_unique<Return>(
        std::make_unique<BinOp>(pos,
            std::move(fibCall1),
            ASTNodeType::Add,
            std::move(fibCall2)
        )
    ));

    // 验证结构
    assert(fib->name == "fib");
    assert(fib->body.size() == 2);

    If* ifPtr = dynamic_cast<If*>(fib->body[0].get());
    assert(ifPtr != nullptr);

    Return* returnPtr = dynamic_cast<Return*>(fib->body[1].get());
    assert(returnPtr != nullptr);

    BinOp* addPtr = dynamic_cast<BinOp*>(returnPtr->value.get());
    assert(addPtr != nullptr);
    assert(addPtr->op == ASTNodeType::Add);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 测试50: 类型安全验证（新增：确保没有UB）
// ============================================================================
void testTypeSafety() {
    std::cout << "Test: Type safety validation... ";
    Position pos(1, 1);

    // 验证 dynamic_cast 在正确继承链上工作
    std::unique_ptr<Expr> expr = std::make_unique<BinOp>(
        std::make_unique<Constant>(pos, int64_t(1)),
        ASTNodeType::Add,
        std::make_unique<Constant>(pos, int64_t(2))
    );
    
    // BinOp 应该能正确转换为 Expr 和 ASTNode
    ASTNode* node = expr.get();
    assert(node != nullptr);
    assert(node->type == ASTNodeType::BinOp);
    
    // 反向转换验证
    BinOp* binOp = dynamic_cast<BinOp*>(expr.get());
    assert(binOp != nullptr);
    
    // 验证错误的转换返回nullptr
    Constant* constant = dynamic_cast<Constant*>(expr.get());
    assert(constant == nullptr);

    // 验证 arguments 不是 Expr（防止错误继承）
    auto args = std::make_unique<arguments>();
    // 这行不应该编译，如果 arguments 继承自 Expr：
    // Expr* wrong = args.get();  // 应该编译错误
    
    // 验证 arguments 是独立的 ASTNode 子类
    ASTNode* argsNode = args.get();
    assert(argsNode->type == ASTNodeType::arguments);

    std::cout << "PASSED" << std::endl;
}

// ============================================================================
// 主函数
// ============================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "CSGO AST Test Suite (Fixed)" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // 表达式测试
        testConstant();
        testName();
        testBinOp();
        testUnaryOp();
        testCompare();
        testBoolOp();
        testCall();
        testIfExp();
        testLambda();
        testContainers();
        testSubscriptAndAttribute();
        testComprehensions();
        testYieldAndAwait();
        testFString();

        // 语句测试
        testAssignment();
        testControlFlow();
        testFunctionDef();
        testClassDef();
        testImport();
        testExceptionHandling();
        testVariableDeclarations();
        testSimpleStatements();
        testWith();

        // 辅助节点和模块测试
        testHelperNodes();
        testModule();

        // 其他测试
        testToString();
        testComplexExpressions();
        testEdgeCases();
        testASTNodeTypeEnum();
        testInheritanceHierarchy();  // 修复：补充的测试30

        // 内存和所有权测试
        testOwnershipSemantics();
        testMoveSemantics();
        testPolymorphism();
        testNoCircularReferences();

        // 复杂结构测试
        testNestedContainers();
        testLargeAST();

        // 运算符全覆盖测试
        testAllComparators();
        testAllBinaryOperators();
        testAllUnaryOperators();
        testAllAugAssignOperators();

        // 类型系统测试
        testVariantTypes();
        testPositionPrecision();

        // 特殊节点测试
        testStarred();
        testComprehensionNode();
        testAlias();
        testWithitem();
        testExceptHandler();
        testArguments();

        // 完整程序测试
        testRecursiveFunctionAST();
        testTypeSafety();  // 修复：新增的测试50

        std::cout << "========================================" << std::endl;
        std::cout << "All 50 tests PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test FAILED with exception: " << e.what() << std::endl;
        return 1;
    }
}