/**
 * @file symbol_table.cpp
 * @brief CSGO 编程语言符号表实现文件
 *
 * @author Aurorp1g
 * @version 1.0
 * @date 2026
 *
 * @section description 描述
 * 本文件实现了 CSGO 语言的符号表（Symbol Table）功能。
 * 基于 CPython 的符号表设计，实现了完整的作用域管理功能。
 *
 * @section implementation 实现细节
 * - SymbolTable 类：作用域链管理
 * - SymbolTableEntry 类：符号表条目管理
 * - 作用域嵌套和符号查找
 * - 自由变量追踪
 * - 符号标志位管理
 *
 * @section reference 参考
 * - CPython Python/symtable.c: 符号表实现
 *
 * @see symbol_table.h 符号表头文件
 * @see TypeChecker 类型检查器
 */

#include "semantic/symbol_table.h"
#include <algorithm>
#include <stdexcept>

namespace csgo {

// ============================================================================
// 符号表条目实现
// ============================================================================

std::optional<ScopeType> SymbolTableEntry::getScope(const std::string& symbolName) const {
    auto it = symbols.find(symbolName);
    if (it == symbols.end()) {
        return std::nullopt;
    }
    
    int flags = it->second;
    int scope = (flags >> 11) & 0xF; // SCOPE_OFFSET = 11
    
    switch (scope) {
        case 1: return ScopeType::LOCAL;
        case 2: return ScopeType::GLOBAL_EXPLICIT;
        case 3: return ScopeType::GLOBAL_IMPLICIT;
        case 4: return ScopeType::FREE;
        case 5: return ScopeType::CELL;
        default: return std::nullopt;
    }
}

int SymbolTableEntry::getFlags(const std::string& symbolName) const {
    auto it = symbols.find(symbolName);
    if (it == symbols.end()) {
        return 0;
    }
    return it->second;
}

void SymbolTableEntry::addSymbol(const std::string& symbolName, int flags) {
    auto it = symbols.find(symbolName);
    if (it != symbols.end()) {
        // 合并标志位
        it->second |= flags;
    } else {
        symbols[symbolName] = flags;
    }
}

void SymbolTableEntry::addVarname(const std::string& varName) {
    if (std::find(varnames.begin(), varnames.end(), varName) == varnames.end()) {
        varnames.push_back(varName);
    }
}

void SymbolTableEntry::addDirective(const std::string& directiveName, size_t line, size_t col) {
    directives.emplace_back(directiveName, line, col);
}

std::string SymbolTableEntry::toString() const {
    std::string result = "SymbolTableEntry(" + name + ", type=";
    switch (type) {
        case BlockType::ModuleBlock: result += "Module"; break;
        case BlockType::FunctionBlock: result += "Function"; break;
        case BlockType::ClassBlock: result += "Class"; break;
    }
    result += ", symbols=";
    result += std::to_string(symbols.size());
    result += ", varnames=";
    result += std::to_string(varnames.size());
    result += ", children=";
    result += std::to_string(children.size());
    result += ")";
    return result;
}

// ============================================================================
// 符号表实现
// ============================================================================

SymbolTable::SymbolTable(const std::string& fileName)
    : filename(fileName),
      globalSymbols(nullptr),
      privateName(""),
      recursionDepth(0),
      recursionLimit(1000),
      hasError(false),
      errorMessage("") {}

bool SymbolTable::enterBlock(const std::string& name, BlockType type, 
                           const void* astNode, size_t lineno, size_t colOffset) {
    // 检查递归深度
    if (recursionDepth > recursionLimit) {
        reportError("maximum recursion depth exceeded during compilation");
        return false;
    }
    
    auto entry = std::make_shared<SymbolTableEntry>(name, type, lineno, colOffset);
    
    // 设置嵌套标志
    if (currentEntry) {
        entry->isNested = currentEntry->isNested || 
                          currentEntry->type == BlockType::FunctionBlock;
        currentEntry->children.push_back(entry);
    }
    
    // 初始化符号表
    entry->symbols = {};
    entry->varnames = {};
    entry->children = {};
    entry->directives = {};
    
    // 添加到块映射
    blocks[astNode] = entry;
    
    // 更新当前条目
    currentEntry = entry;
    scopeStack.push_back(entry);
    
    // 如果是顶层模块，设置全局符号
    if (type == BlockType::ModuleBlock) {
        globalSymbols = entry;
        topEntry = entry;
    }
    
    return true;
}

bool SymbolTable::exitBlock() {
    if (scopeStack.empty()) {
        return false;
    }
    
    scopeStack.pop_back();
    
    if (!scopeStack.empty()) {
        currentEntry = scopeStack.back();
    } else {
        currentEntry = nullptr;
    }
    
    return true;
}

std::shared_ptr<SymbolTableEntry> SymbolTable::current() const {
    return currentEntry;
}

std::shared_ptr<SymbolTableEntry> SymbolTable::top() const {
    return topEntry;
}

void SymbolTable::addSymbol(const std::string& name, int flags) {
    if (currentEntry) {
        currentEntry->addSymbol(name, flags);
    }
}

void SymbolTable::markUsed(const std::string& name) {
    // 从当前作用域向上查找
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        auto& entry = *it;
        auto symbolIt = entry->symbols.find(name);
        if (symbolIt != entry->symbols.end()) {
            symbolIt->second |= static_cast<int>(SymbolFlags::USE);
            return;
        }
    }
    
    // 如果在模块级别未找到，标记为隐式全局使用
    if (globalSymbols) {
        globalSymbols->addSymbol(name, static_cast<int>(SymbolFlags::USE));
    }
}

std::pair<std::shared_ptr<SymbolTableEntry>, std::optional<ScopeType>> 
SymbolTable::lookup(const std::string& name) {
    // 从内向外查找
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        auto& entry = *it;
        auto symbolIt = entry->symbols.find(name);
        if (symbolIt != entry->symbols.end()) {
            return {entry, entry->getScope(name)};
        }
    }
    
    // 未找到
    return {nullptr, std::nullopt};
}

std::optional<ScopeType> SymbolTable::getScope(const std::string& name) {
    auto result = lookup(name);
    return result.second;
}

std::shared_ptr<SymbolTableEntry> SymbolTable::lookupEntry(const void* astNode) {
    auto it = blocks.find(astNode);
    if (it != blocks.end()) {
        return it->second;
    }
    return nullptr;
}

void SymbolTable::reportError(const std::string& message) {
    hasError = true;
    errorMessage = message;
}

void SymbolTable::clearError() {
    hasError = false;
    errorMessage = "";
}

// ============================================================================
// 符号分析核心函数（基于 CPython analyze_name）
// ============================================================================

namespace {

ScopeType analyzeName(
    SymbolTableEntry* ste,
    std::unordered_map<std::string, int>& scopes,
    const std::string& name,
    int flags,
    const std::set<std::string>* bound,
    std::set<std::string>& local,
    std::set<std::string>& freeVars,
    const std::set<std::string>* global
) {
    // global 语句声明
    if (flags & static_cast<int>(SymbolFlags::DEF_GLOBAL)) {
        if (flags & static_cast<int>(SymbolFlags::DEF_NONLOCAL)) {
            throw std::runtime_error("name '" + name + "' is nonlocal and global");
        }
        scopes[name] = static_cast<int>(ScopeType::GLOBAL_EXPLICIT);
        if (global) {
            const_cast<std::set<std::string>*>(global)->insert(name);
        }
        if (bound) {
            const_cast<std::set<std::string>*>(bound)->erase(name);
        }
        return ScopeType::GLOBAL_EXPLICIT;
    }
    
    // nonlocal 语句声明
    if (flags & static_cast<int>(SymbolFlags::DEF_NONLOCAL)) {
        if (!bound) {
            throw std::runtime_error("nonlocal declaration not allowed at module level");
        }
        if (!bound->count(name)) {
            throw std::runtime_error("no binding for nonlocal '" + name + "' found");
        }
        scopes[name] = static_cast<int>(ScopeType::FREE);
        ste->hasFreeVariables = true;
        freeVars.insert(name);
        return ScopeType::FREE;
    }
    
    // 有定义的符号（局部、参数、import）
    if (flags & (static_cast<int>(SymbolFlags::DEF_LOCAL) | 
                 static_cast<int>(SymbolFlags::DEF_PARAM) |
                 static_cast<int>(SymbolFlags::DEF_IMPORT))) {
            scopes[name] = static_cast<int>(ScopeType::LOCAL);
            local.insert(name);
            if (global) {
                const_cast<std::set<std::string>*>(global)->erase(name);
            }
            return ScopeType::LOCAL;
    }
    
    // 检查是否是自由变量（被外层作用域绑定）
    if (bound && bound->count(name)) {
        scopes[name] = static_cast<int>(ScopeType::FREE);
        ste->hasFreeVariables = true;
        freeVars.insert(name);
        return ScopeType::FREE;
    }
    
    // 检查是否是全局变量（被外层global声明）
    if (global && global->count(name)) {
        scopes[name] = static_cast<int>(ScopeType::GLOBAL_IMPLICIT);
        return ScopeType::GLOBAL_IMPLICIT;
    }
    
    // 嵌套作用域中的隐式全局变量
    if (ste->isNested) {
        ste->hasFreeVariables = true;
    }
    scopes[name] = static_cast<int>(ScopeType::GLOBAL_IMPLICIT);
    return ScopeType::GLOBAL_IMPLICIT;
}

// 分析单元变量
void analyzeCells(
    std::unordered_map<std::string, int>& scopes,
    const std::set<std::string>& freeVars
) {
    for (auto& [name, scope] : scopes) {
        if (scope == static_cast<int>(ScopeType::LOCAL) && 
            freeVars.count(name)) {
            scope = static_cast<int>(ScopeType::CELL);
        }
    }
}

// 更新符号表条目的作用域信息
bool updateSymbols(
    std::unordered_map<std::string, int>& symbols,
    const std::unordered_map<std::string, int>& scopes,
    const std::set<std::string>* bound,
    const std::set<std::string>& freeVars,
    bool isClass
) {
    // 更新当前作用域中符号的作用域信息
    for (const auto& [name, scope] : scopes) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            int flags = it->second;
            flags |= (scope << 11); // SCOPE_OFFSET = 11
            symbols[name] = flags;
        }
    }
    
    // 处理从子作用域传来的自由变量
    for (const auto& name : freeVars) {
        auto it = symbols.find(name);
        
        if (it != symbols.end()) {
            // 处理类作用域中的同名变量
            if (isClass && (it->second & (static_cast<int>(SymbolFlags::DEF_LOCAL) | 
                                           static_cast<int>(SymbolFlags::DEF_PARAM) |
                                           static_cast<int>(SymbolFlags::DEF_GLOBAL)))) {
                it->second |= static_cast<int>(SymbolFlags::DEF_FREE_CLASS);
            }
        } else if (!bound || !bound->count(name)) {
            // 全局符号不处理
        } else {
            // 添加向上传递的自由变量
            symbols[name] = (static_cast<int>(ScopeType::FREE) << 11);
        }
    }
    
    return true;
}

// 分析块的作用域
bool analyzeBlock(
    SymbolTableEntry* ste,
    const std::set<std::string>* bound,
    std::set<std::string>& freeVars,
    const std::set<std::string>* global
) {
    std::set<std::string> local;
    std::unordered_map<std::string, int> scopes;
    std::set<std::string> newBound;
    std::set<std::string> newGlobal;
    std::set<std::string> newFree;
    
    // 类作用域的特殊处理
    if (ste->type == BlockType::ClassBlock) {
        if (global) {
            newGlobal.insert(global->begin(), global->end());
        }
        if (bound) {
            newBound.insert(bound->begin(), bound->end());
        }
    }
    
    // 分析所有符号
    for (const auto& [name, flags] : ste->symbols) {
        analyzeName(ste, scopes, name, flags, bound, local, freeVars, global);
    }
    
    // 非类作用域：准备传递给子作用域的绑定
    if (ste->type != BlockType::ClassBlock) {
        if (ste->type == BlockType::FunctionBlock) {
            newBound.insert(local.begin(), local.end());
        }
        if (bound) {
            newBound.insert(bound->begin(), bound->end());
        }
        if (global) {
            newGlobal.insert(global->begin(), global->end());
        }
    } else {
        // 类作用域中的 __class__ 特殊处理
        newBound.insert("__class__");
    }
    
    // 递归分析子块
    std::set<std::string> allFree;
    for (auto& child : ste->children) {
        if (!analyzeBlock(child.get(), &newBound, newFree, &newGlobal)) {
            return false;
        }
        
        if (child->hasFreeVariables || child->hasChildFree) {
            ste->hasChildFree = true;
        }
        
        allFree.insert(newFree.begin(), newFree.end());
    }
    
    // 合并自由变量
    freeVars.insert(newFree.begin(), newFree.end());
    
    // 函数块需要分析单元变量
    if (ste->type == BlockType::FunctionBlock) {
        analyzeCells(scopes, newFree);
    }
    
    // 更新符号表条目
    if (!updateSymbols(ste->symbols, scopes, bound, freeVars, 
                      ste->type == BlockType::ClassBlock)) {
        return false;
    }
    
    return true;
}

} // namespace

// ============================================================================
// 符号表分析主函数
// ============================================================================

bool SymbolTable::analyze() {
    if (!topEntry) {
        return false;
    }
    
    std::set<std::string> freeVars;
    std::set<std::string> global;
    
    return analyzeBlock(topEntry.get(), nullptr, freeVars, &global);
}

// ============================================================================
// 符号表构建器实现
// ============================================================================

SymbolTableBuilder::SymbolTableBuilder(const std::string& filename)
    : symtable(std::make_shared<SymbolTable>(filename)) {}

std::shared_ptr<SymbolTable> SymbolTableBuilder::build(Module& module) {
    // 进入模块作用域
    symtable->enterBlock("__main__", BlockType::ModuleBlock, &module, 0, 0);
    
    // 遍历模块体
    if (!visitModule(module.body)) {
        return nullptr;
    }
    
    // 退出模块作用域
    symtable->exitBlock();
    
    // 分析符号作用域
    if (!symtable->analyze()) {
        return nullptr;
    }
    
    return symtable;
}

bool SymbolTableBuilder::visitModule(const std::vector<std::unique_ptr<Stmt>>& body) {
    for (const auto& stmt : body) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    return true;
}

bool SymbolTableBuilder::visitStatement(const Stmt& stmt) {
    switch (stmt.type) {
        case ASTNodeType::FunctionDef:
            return visitFunctionDef(static_cast<const FunctionDef&>(stmt));
        case ASTNodeType::AsyncFunctionDef:
            return visitAsyncFunctionDef(static_cast<const FunctionDef&>(stmt));
        case ASTNodeType::ClassDef:
            return visitClassDef(static_cast<const ClassDef&>(stmt));
        case ASTNodeType::Return:
            return visitReturn(static_cast<const Return&>(stmt));
        case ASTNodeType::Assign:
            return visitAssign(static_cast<const Assign&>(stmt));
        case ASTNodeType::AugAssign:
            return visitAugAssign(static_cast<const AugAssign&>(stmt));
        case ASTNodeType::AnnAssign:
            return visitAnnAssign(static_cast<const AnnAssign&>(stmt));
        case ASTNodeType::If:
            return visitIf(static_cast<const If&>(stmt));
        case ASTNodeType::For:
            return visitFor(static_cast<const For&>(stmt));
        case ASTNodeType::AsyncFor:
            return visitAsyncFor(static_cast<const For&>(stmt));
        case ASTNodeType::While:
            return visitWhile(static_cast<const While&>(stmt));
        case ASTNodeType::With:
            return visitWith(static_cast<const With&>(stmt));
        case ASTNodeType::AsyncWith:
            return visitAsyncWith(static_cast<const With&>(stmt));
        case ASTNodeType::Try:
            return visitTry(static_cast<const Try&>(stmt));
        case ASTNodeType::Raise:
            return visitRaise(static_cast<const Raise&>(stmt));
        case ASTNodeType::Assert:
            return visitAssert(static_cast<const Assert&>(stmt));
        case ASTNodeType::Delete:
            return visitDelete(static_cast<const Delete&>(stmt));
        case ASTNodeType::Global:
            return visitGlobal(static_cast<const Global&>(stmt));
        case ASTNodeType::Nonlocal:
            return visitNonlocal(static_cast<const Nonlocal&>(stmt));
        case ASTNodeType::Import:
            return visitImport(static_cast<const Import&>(stmt));
        case ASTNodeType::ImportFrom:
            return visitImportFrom(static_cast<const ImportFrom&>(stmt));
        case ASTNodeType::ExprStmt:
            return visitExpressionStmt(static_cast<const ExprStmt&>(stmt));
        case ASTNodeType::Pass:
            return true; // pass 不做任何操作
        case ASTNodeType::Break:
            return true;
        case ASTNodeType::Continue:
            return true;
        default:
            return true;
    }
}

void SymbolTableBuilder::addDef(const std::string& name, int flags) {
    symtable->addSymbol(name, flags);
}

void SymbolTableBuilder::addUse(const std::string& name) {
    symtable->markUsed(name);
}

// ============================================================================
// 函数定义处理
// ============================================================================

bool SymbolTableBuilder::visitFunctionDef(const FunctionDef& func) {
    // 添加函数名到当前作用域
    addDef(func.name, static_cast<int>(SymbolFlags::DEF_LOCAL));
    
    // 进入函数作用域
    symtable->enterBlock(func.name, BlockType::FunctionBlock, &func, 
                         func.position.line, func.position.column);
    
    // 处理函数参数
    if (func.args) {
        visitArguments(*func.args);
    }
    
    // 处理函数体
    for (const auto& stmt : func.body) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    
    // 退出函数作用域
    symtable->exitBlock();
    
    return true;
}

bool SymbolTableBuilder::visitAsyncFunctionDef(const FunctionDef& func) {
    // 与普通函数定义类似，但在符号表中标记为协程
    auto result = visitFunctionDef(func);
    if (result && symtable->current()) {
        symtable->current()->isCoroutine = true;
    }
    return result;
}

// ============================================================================
// 类定义处理
// ============================================================================

bool SymbolTableBuilder::visitClassDef(const ClassDef& cls) {
    // 添加类名到当前作用域
    addDef(cls.name, static_cast<int>(SymbolFlags::DEF_LOCAL));
    
    // 保存外层私有名称
    std::string outerPrivate = symtable->privateName;
    
    // 进入类作用域
    symtable->enterBlock(cls.name, BlockType::ClassBlock, &cls,
                         cls.position.line, cls.position.column);
    
    // 设置私有名称（用于名称修饰）
    symtable->privateName = cls.name;
    
    // 处理基类 - 由于bases现在是unique_ptr<Expr>而不是vector，我们需要特殊处理
    if (cls.bases) {
        if (!visitExpression(*cls.bases)) {
            return false;
        }
    }
    
    // 处理关键字参数 - keywords是unique_ptr<Expr>类型，不是容器
    if (cls.keywords) {
        if (!visitExpression(*cls.keywords)) {
            return false;
        }
    }
    
    // 处理类体
    for (const auto& stmt : cls.body) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    
    // 退出类作用域
    symtable->exitBlock();
    
    // 恢复外层私有名称
    symtable->privateName = outerPrivate;
    
    return true;
}

// ============================================================================
// 变量处理
// ============================================================================

bool SymbolTableBuilder::visitName(const Name& name) {
    if (name.ctx == Name::Load) {
        addUse(name.id);
    } else if (name.ctx == Name::Store) {
        addDef(name.id, static_cast<int>(SymbolFlags::DEF_LOCAL));
    } else if (name.ctx == Name::Del) {
        addDef(name.id, static_cast<int>(SymbolFlags::DEF_LOCAL));
    }
    return true;
}

bool SymbolTableBuilder::visitAssign(const Assign& assign) {
    // 处理右侧表达式
    if (!visitExpression(*assign.value)) {
        return false;
    }
    
    // 处理左侧目标 - 对Name类型的目标添加定义
    for (const auto& target : assign.targets) {
        visitExpression(*target);
        // 检查目标是否为Name类型，只有Name类型的才能添加符号定义
        if (target->type == ASTNodeType::Name) {
            const Name& name = static_cast<const Name&>(*target);
            addDef(name.id, static_cast<int>(SymbolFlags::DEF_LOCAL));
        }
    }
    
    return true;
}

bool SymbolTableBuilder::visitAugAssign(const AugAssign& assign) {
    // 先访问变量（加载）
    if (assign.target->type == ASTNodeType::Name) {
        const Name& name = static_cast<const Name&>(*assign.target);
        addUse(name.id);
    }
    
    // 再访问右侧表达式
    if (!visitExpression(*assign.value)) {
        return false;
    }
    
    // 最后标记为目标（存储）
    if (assign.target->type == ASTNodeType::Name) {
        const Name& name = static_cast<const Name&>(*assign.target);
        addDef(name.id, static_cast<int>(SymbolFlags::DEF_LOCAL));
    }
    
    return true;
}

bool SymbolTableBuilder::visitAnnAssign(const AnnAssign& assign) {
    // 先处理注解
    if (assign.annotation) {
        if (!visitExpression(*assign.annotation)) {
            return false;
        }
    }
    
    // 再处理值
    if (assign.value) {
        if (!visitExpression(*assign.value)) {
            return false;
        }
    }
    
    // 最后处理目标
    if (assign.target) {
        visitExpression(*assign.target);
        
        // 检查目标是否为Name类型，只有Name类型的才能添加符号定义
        if (assign.target->type == ASTNodeType::Name) {
            const Name& name = static_cast<const Name&>(*assign.target);
            addDef(name.id, 
                   static_cast<int>(SymbolFlags::DEF_LOCAL) | 
                   static_cast<int>(SymbolFlags::DEF_ANNOT));
        }
    }
    
    return true;
}

// ============================================================================
// 控制流处理
// ============================================================================

bool SymbolTableBuilder::visitIf(const If& ifStmt) {
    // 条件
    if (!visitExpression(*ifStmt.test)) {
        return false;
    }
    
    // if 分支
    for (const auto& stmt : ifStmt.body) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    
    // else 分支
    for (const auto& stmt : ifStmt.orelse) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    
    return true;
}

bool SymbolTableBuilder::visitFor(const For& forStmt) {
    // 迭代器表达式
    if (!visitExpression(*forStmt.iter)) {
        return false;
    }
    
    // 循环变量
    visitExpression(*forStmt.target);
    
    // 循环体
    for (const auto& stmt : forStmt.body) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    
    // else 分支
    for (const auto& stmt : forStmt.orelse) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    
    return true;
}

bool SymbolTableBuilder::visitAsyncFor(const For& forStmt) {
    // 与普通for循环相同
    return visitFor(forStmt);
}

bool SymbolTableBuilder::visitWhile(const While& whileStmt) {
    // 条件
    if (!visitExpression(*whileStmt.test)) {
        return false;
    }
    
    // 循环体
    for (const auto& stmt : whileStmt.body) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    
    // else 分支
    for (const auto& stmt : whileStmt.orelse) {
        if (!visitStatement(*stmt)) {
            return false;
        }
    }
    
    return true;
}

bool SymbolTableBuilder::visitReturn(const Return& ret) {
    if (ret.value) {
        return visitExpression(*ret.value);
    }
    return true;
}

// ============================================================================
// 表达式处理
// ============================================================================

bool SymbolTableBuilder::visitExpression(const Expr& expr) {
    switch (expr.type) {
        case ASTNodeType::Constant:
            return visitConstant(static_cast<const Constant&>(expr));
        case ASTNodeType::Name:
            return visitName(static_cast<const Name&>(expr));
        case ASTNodeType::BinOp:
            return visitBinaryOp(static_cast<const BinOp&>(expr));
        case ASTNodeType::UnaryOp:
            return visitUnaryOp(static_cast<const UnaryOp&>(expr));
        case ASTNodeType::Lambda:
            return visitLambda(static_cast<const Lambda&>(expr));
        case ASTNodeType::IfExp:
            return visitIfExp(static_cast<const IfExp&>(expr));
        case ASTNodeType::Dict:
            return visitDict(static_cast<const Dict&>(expr));
        case ASTNodeType::Set:
            return visitSet(static_cast<const Set&>(expr));
        case ASTNodeType::ListComp:
            return visitListComp(static_cast<const ListComp&>(expr));
        case ASTNodeType::SetComp:
            return visitSetComp(static_cast<const SetComp&>(expr));
        case ASTNodeType::DictComp:
            return visitDictComp(static_cast<const DictComp&>(expr));
        case ASTNodeType::GeneratorExp:
            return visitGeneratorExp(static_cast<const GeneratorExp&>(expr));
        case ASTNodeType::Yield:
            return visitYield(static_cast<const Yield&>(expr));
        case ASTNodeType::YieldFrom:
            return visitYieldFrom(static_cast<const YieldFrom&>(expr));
        case ASTNodeType::Await:
            return visitAwait(static_cast<const Await&>(expr));
        case ASTNodeType::Compare:
            return visitCompare(static_cast<const Compare&>(expr));
        case ASTNodeType::Call:
            return visitCall(static_cast<const Call&>(expr));
        case ASTNodeType::Attribute:
            return visitAttribute(static_cast<const Attribute&>(expr));
        case ASTNodeType::Subscript:
            return visitSubscript(static_cast<const Subscript&>(expr));
        case ASTNodeType::Starred:
            return visitStarred(static_cast<const Starred&>(expr));
        case ASTNodeType::Tuple:
            return visitTuple(static_cast<const Tuple&>(expr));
        case ASTNodeType::List:
            return visitList(static_cast<const List&>(expr));
        case ASTNodeType::BoolOp:
            return visitBoolOp(static_cast<const BoolOp&>(expr));
        default:
            return true;
    }
}

bool SymbolTableBuilder::visitConstant(const Constant&) {
    return true;
}

bool SymbolTableBuilder::visitBinaryOp(const BinOp& binop) {
    if (!visitExpression(*binop.left)) return false;
    if (!visitExpression(*binop.right)) return false;
    return true;
}

bool SymbolTableBuilder::visitUnaryOp(const UnaryOp& unop) {
    return visitExpression(*unop.operand);
}

bool SymbolTableBuilder::visitLambda(const Lambda& lambda) {
    // 进入lambda作用域
    symtable->enterBlock("<lambda>", BlockType::FunctionBlock, &lambda,
                         lambda.position.line, lambda.position.column);
    
    // 处理参数 - Lambda的参数列表本身是一个表达式，需要特别处理
    // 在CPython实现中，Lambda的参数是以表达式形式存储的，但实际应该是一个参数列表
    // 我们这里假定lambda.args如果是Name类型则代表单个参数
    if (lambda.args) {
        // 对于lambda表达式，参数通常是一个占位符，实际参数处理可能需要特殊处理
        // 目前我们只处理表达式，具体参数逻辑会在其他地方处理
        if (!visitExpression(*lambda.args)) {
            return false;
        }
    }
    
    // 处理函数体
    if (!visitExpression(*lambda.body)) {
        return false;
    }
    
    // 退出作用域
    symtable->exitBlock();
    
    return true;
}

bool SymbolTableBuilder::visitIfExp(const IfExp& ifexp) {
    if (!visitExpression(*ifexp.test)) return false;
    if (!visitExpression(*ifexp.body)) return false;
    if (!visitExpression(*ifexp.orelse)) return false;
    return true;
}

bool SymbolTableBuilder::visitBoolOp(const BoolOp& boolop) {
    for (const auto& value : boolop.values) {
        if (!visitExpression(*value)) return false;
    }
    return true;
}

bool SymbolTableBuilder::visitCompare(const Compare& compare) {
    if (!visitExpression(*compare.left)) return false;
    for (const auto& comparator : compare.comparators) {
        if (!visitExpression(*comparator)) return false;
    }
    return true;
}

bool SymbolTableBuilder::visitCall(const Call& call) {
    // 处理函数表达式
    if (!visitExpression(*call.func)) {
        return false;
    }
    
    // 处理参数
    for (const auto& arg : call.args) {
        if (!visitExpression(*arg)) return false;
    }
    
    // 处理关键字参数
    for (const auto& keyword : call.keywords) {
        if (!visitExpression(*keyword)) return false;
    }
    
    return true;
}

bool SymbolTableBuilder::visitAttribute(const Attribute& attr) {
    return visitExpression(*attr.value);
}

bool SymbolTableBuilder::visitSubscript(const Subscript& subscript) {
    if (!visitExpression(*subscript.value)) return false;
    if (!visitExpression(*subscript.slice)) return false;
    return true;
}

bool SymbolTableBuilder::visitStarred(const Starred& starred) {
    return visitExpression(*starred.value);
}

bool SymbolTableBuilder::visitTuple(const Tuple& tuple) {
    for (const auto& elt : tuple.elts) {
        if (!visitExpression(*elt)) return false;
    }
    return true;
}

bool SymbolTableBuilder::visitList(const List& list) {
    for (const auto& elt : list.elts) {
        if (!visitExpression(*elt)) return false;
    }
    return true;
}

// ============================================================================
// 容器表达式处理
// ============================================================================

bool SymbolTableBuilder::visitDict(const Dict& dict) {
    for (const auto& key : dict.keys) {
        if (key) {
            if (!visitExpression(*key)) return false;
        }
    }
    for (const auto& value : dict.values) {
        if (!visitExpression(*value)) return false;
    }
    return true;
}

bool SymbolTableBuilder::visitSet(const Set& set) {
    for (const auto& elt : set.elts) {
        if (!visitExpression(*elt)) return false;
    }
    return true;
}

bool SymbolTableBuilder::visitListComp(const ListComp& listcomp) {
    return handleComprehension(listcomp, listcomp.generators);
}

bool SymbolTableBuilder::visitSetComp(const SetComp& setcomp) {
    return handleComprehension(setcomp, setcomp.generators);
}

bool SymbolTableBuilder::visitDictComp(const DictComp& dictcomp) {
    // 处理键
    if (!visitExpression(*dictcomp.key)) return false;
    // 处理值
    if (!visitExpression(*dictcomp.value)) return false;
    // 处理生成器
    return handleComprehension(dictcomp, dictcomp.generators);
}

bool SymbolTableBuilder::visitGeneratorExp(const GeneratorExp& genexp) {
    return handleComprehension(genexp, genexp.generators);
}

bool SymbolTableBuilder::handleComprehension(
    const Expr& expr,
    const std::vector<std::unique_ptr<comprehension>>& generators
) {
    // 进入推导式作用域
    symtable->enterBlock("<comprehension>", BlockType::FunctionBlock, &expr,
                         expr.position.line, expr.position.column);
    
    // 标记当前作用域为推导式
    if (symtable->current()) {
        symtable->current()->isComprehension = true;
    }
    
    // 处理每个生成器
    for (const auto& gen : generators) {
        if (!visitComprehension(*gen)) {
            return false;
        }
    }
    
    // 退出推导式作用域
    symtable->exitBlock();
    
    return true;
}

// ============================================================================
// 异常处理和上下文管理
// ============================================================================

bool SymbolTableBuilder::visitWith(const With& withStmt) {
    for (const auto& item : withStmt.items) {
        if (!visitWithitem(*item)) return false;
    }
    
    for (const auto& stmt : withStmt.body) {
        if (!visitStatement(*stmt)) return false;
    }
    
    return true;
}

bool SymbolTableBuilder::visitAsyncWith(const With& withStmt) {
    return visitWith(withStmt);
}

bool SymbolTableBuilder::visitWithitem(const withitem& item) {
    if (item.context_expr) {
        if (!visitExpression(*item.context_expr)) return false;
    }
    
    if (item.optional_vars) {
        visitExpression(*item.optional_vars);
    }
    
    return true;
}

bool SymbolTableBuilder::visitTry(const Try& tryStmt) {
    // try 块
    for (const auto& stmt : tryStmt.body) {
        if (!visitStatement(*stmt)) return false;
    }
    
    // except 处理器
    for (const auto& handler : tryStmt.handlers) {
        if (!visitExceptHandler(*handler)) return false;
    }
    
    // else 块
    for (const auto& stmt : tryStmt.orelse) {
        if (!visitStatement(*stmt)) return false;
    }
    
    // finally 块
    for (const auto& stmt : tryStmt.finalbody) {
        if (!visitStatement(*stmt)) return false;
    }
    
    return true;
}

bool SymbolTableBuilder::visitExceptHandler(const except_handler& handler) {
    if (handler.type) {
        if (!visitExpression(*handler.type)) return false;
    }
    
    if (!handler.name.empty()) {
        // 异常处理器的命名异常是局部变量
        addDef(handler.name, static_cast<int>(SymbolFlags::DEF_LOCAL));
    }
    
    for (const auto& stmt : handler.body) {
        if (!visitStatement(*stmt)) return false;
    }
    
    return true;
}

bool SymbolTableBuilder::visitRaise(const Raise& raiseStmt) {
    if (raiseStmt.exc) {
        if (!visitExpression(*raiseStmt.exc)) return false;
    }
    
    if (raiseStmt.cause) {
        if (!visitExpression(*raiseStmt.cause)) return false;
    }
    
    return true;
}

bool SymbolTableBuilder::visitAssert(const Assert& assertStmt) {
    if (!visitExpression(*assertStmt.test)) return false;
    
    if (assertStmt.msg) {
        if (!visitExpression(*assertStmt.msg)) return false;
    }
    
    return true;
}

// ============================================================================
// 删除和导入处理
// ============================================================================

bool SymbolTableBuilder::visitDelete(const Delete& delStmt) {
    for (const auto& target : delStmt.targets) {
        visitExpression(*target);
        
        // 检查目标是否为Name类型
        if (target->type == ASTNodeType::Name) {
            const Name& name = static_cast<const Name&>(*target);
            addDef(name.id, static_cast<int>(SymbolFlags::DEF_LOCAL));
        }
    }
    return true;
}

bool SymbolTableBuilder::visitImport(const Import&) {
    return true;
}

bool SymbolTableBuilder::visitImportFrom(const ImportFrom&) {
    return true;
}

// ============================================================================
// 全局和非局部声明
// ============================================================================

bool SymbolTableBuilder::visitGlobal(const Global& globalStmt) {
    // 记录全局声明位置
    for (const auto& name : globalStmt.names) {
        symtable->addSymbol(name, static_cast<int>(SymbolFlags::DEF_GLOBAL));
        if (symtable->current()) {
            symtable->current()->addDirective(
                name, globalStmt.position.line, globalStmt.position.column);
        }
    }
    return true;
}

bool SymbolTableBuilder::visitNonlocal(const Nonlocal& nonlocalStmt) {
    for (const auto& name : nonlocalStmt.names) {
        symtable->addSymbol(name, static_cast<int>(SymbolFlags::DEF_NONLOCAL));
        if (symtable->current()) {
            symtable->current()->addDirective(
                name, nonlocalStmt.position.line, nonlocalStmt.position.column);
        }
    }
    return true;
}

// ============================================================================
// 表达式语句和参数处理
// ============================================================================

bool SymbolTableBuilder::visitExpressionStmt(const ExprStmt& exprStmt) {
    return visitExpression(*exprStmt.value);
}

bool SymbolTableBuilder::visitArguments(const arguments& args) {
    // 处理位置参数
    for (const auto& arg : args.args) {
        // 将 arg 转换为实际的 arg 对象并访问其 name 属性
        if (arg && arg->type == ASTNodeType::arg) {
            const auto& param = static_cast<const csgo::arg&>(*arg);
            addDef(param.name, static_cast<int>(SymbolFlags::DEF_PARAM));
            if (symtable->current()) {
                symtable->current()->addVarname(param.name);
            }
        }
    }
    
    // 处理关键字参数
    for (const auto& arg : args.kwonlyargs) {
        if (arg && arg->type == ASTNodeType::arg) {
            const auto& param = static_cast<const csgo::arg&>(*arg);
            addDef(param.name, static_cast<int>(SymbolFlags::DEF_PARAM));
            if (symtable->current()) {
                symtable->current()->addVarname(param.name);
            }
        }
    }
    
    // 处理默认值
    for (const auto& arg : args.defaults) {
        if (!visitExpression(*arg)) return false;
    }
    
    for (const auto& arg : args.kw_defaults) {
        if (arg) {
            if (!visitExpression(*arg)) return false;
        }
    }
    
    // 处理 *args
    if (args.vararg) {
        if (args.vararg->type == ASTNodeType::arg) {
            const auto& param = static_cast<const csgo::arg&>(*args.vararg);
            addDef(param.name, static_cast<int>(SymbolFlags::DEF_PARAM) | 
                                   static_cast<int>(SymbolFlags::DEF_LOCAL));
            if (symtable->current()) {
                symtable->current()->addVarname(param.name);
                symtable->current()->isVarargs = true;
            }
        }
    }
    
    // 处理 **kwargs
    if (args.kwarg) {
        if (args.kwarg->type == ASTNodeType::arg) {
            const auto& param = static_cast<const csgo::arg&>(*args.kwarg);
            addDef(param.name, static_cast<int>(SymbolFlags::DEF_PARAM) |
                                    static_cast<int>(SymbolFlags::DEF_LOCAL));
            if (symtable->current()) {
                symtable->current()->addVarname(param.name);
                symtable->current()->isVarkeywords = true;
            }
        }
    }
    
    return true;
}

bool SymbolTableBuilder::visitComprehension(const comprehension& comp) {
    // 先处理迭代表达式
    if (!visitExpression(*comp.iter)) return false;
    
    // 目标（循环变量）是局部定义
    visitExpression(*comp.target);
    
    // 处理条件
    for (const auto& ifCond : comp.ifs) {
        if (!visitExpression(*ifCond)) return false;
    }
    
    return true;
}

// ============================================================================
// Yield 相关表达式
// ============================================================================

bool SymbolTableBuilder::visitYield(const Yield& yield) {
    if (yield.value) {
        return visitExpression(*yield.value);
    }
    return true;
}

bool SymbolTableBuilder::visitYieldFrom(const YieldFrom& yieldfrom) {
    return visitExpression(*yieldfrom.value);
}

bool SymbolTableBuilder::visitAwait(const Await& await) {
    return visitExpression(*await.value);
}

} // namespace csgo