#include "ast/ast_node.h"
#include <sstream>
#include <iomanip>

namespace csgo {

const char* ASTNode::getTypeName(ASTNodeType type) {
    switch (type) {
        case ASTNodeType::Module: return "Module";
        case ASTNodeType::Import: return "Import";
        case ASTNodeType::ImportFrom: return "ImportFrom";
        case ASTNodeType::Alias: return "Alias";
        case ASTNodeType::FunctionDef: return "FunctionDef";
        case ASTNodeType::AsyncFunctionDef: return "AsyncFunctionDef";
        case ASTNodeType::ClassDef: return "ClassDef";
        case ASTNodeType::Return: return "Return";
        case ASTNodeType::Delete: return "Delete";
        case ASTNodeType::Assign: return "Assign";
        case ASTNodeType::AugAssign: return "AugAssign";
        case ASTNodeType::AnnAssign: return "AnnAssign";
        case ASTNodeType::For: return "For";
        case ASTNodeType::AsyncFor: return "AsyncFor";
        case ASTNodeType::While: return "While";
        case ASTNodeType::If: return "If";
        case ASTNodeType::With: return "With";
        case ASTNodeType::AsyncWith: return "AsyncWith";
        case ASTNodeType::Raise: return "Raise";
        case ASTNodeType::Try: return "Try";
        case ASTNodeType::Assert: return "Assert";
        case ASTNodeType::Global: return "Global";
        case ASTNodeType::Nonlocal: return "Nonlocal";
        case ASTNodeType::ExprStmt: return "ExprStmt";
        case ASTNodeType::Pass: return "Pass";
        case ASTNodeType::Break: return "Break";
        case ASTNodeType::Continue: return "Continue";
        case ASTNodeType::BoolOp: return "BoolOp";
        case ASTNodeType::BinOp: return "BinOp";
        case ASTNodeType::UnaryOp: return "UnaryOp";
        case ASTNodeType::Lambda: return "Lambda";
        case ASTNodeType::IfExp: return "IfExp";
        case ASTNodeType::Dict: return "Dict";
        case ASTNodeType::Set: return "Set";
        case ASTNodeType::ListComp: return "ListComp";
        case ASTNodeType::SetComp: return "SetComp";
        case ASTNodeType::DictComp: return "DictComp";
        case ASTNodeType::GeneratorExp: return "GeneratorExp";
        case ASTNodeType::Yield: return "Yield";
        case ASTNodeType::YieldFrom: return "YieldFrom";
        case ASTNodeType::Await: return "Await";
        case ASTNodeType::Compare: return "Compare";
        case ASTNodeType::Call: return "Call";
        case ASTNodeType::FormattedValue: return "FormattedValue";
        case ASTNodeType::JoinedStr: return "JoinedStr";
        case ASTNodeType::Constant: return "Constant";
        case ASTNodeType::List: return "List";
        case ASTNodeType::Attribute: return "Attribute";
        case ASTNodeType::Subscript: return "Subscript";
        case ASTNodeType::Slice: return "Slice";
        case ASTNodeType::Starred: return "Starred";
        case ASTNodeType::Tuple: return "Tuple";
        case ASTNodeType::Name: return "Name";
        case ASTNodeType::Load: return "Load";
        case ASTNodeType::Store: return "Store";
        case ASTNodeType::Del: return "Del";
        case ASTNodeType::arguments: return "arguments";
        case ASTNodeType::arg: return "arg";
        case ASTNodeType::except_handler: return "except_handler";
        case ASTNodeType::withitem: return "withitem";
        case ASTNodeType::comprehension: return "comprehension";
        case ASTNodeType::Num: return "Num";
        case ASTNodeType::Str: return "Str";
        case ASTNodeType::Bytes: return "Bytes";
        case ASTNodeType::NameConstant: return "NameConstant";
        case ASTNodeType::Ellipsis: return "Ellipsis";
        case ASTNodeType::Add: return "Add";
        case ASTNodeType::Sub: return "Sub";
        case ASTNodeType::Mult: return "Mult";
        case ASTNodeType::MatMult: return "MatMult";
        case ASTNodeType::Div: return "Div";
        case ASTNodeType::FloorDiv: return "FloorDiv";
        case ASTNodeType::Mod: return "Mod";
        case ASTNodeType::Pow: return "Pow";
        case ASTNodeType::LShift: return "LShift";
        case ASTNodeType::RShift: return "RShift";
        case ASTNodeType::BitOr: return "BitOr";
        case ASTNodeType::BitXor: return "BitXor";
        case ASTNodeType::BitAnd: return "BitAnd";
        case ASTNodeType::Eq: return "Eq";
        case ASTNodeType::NotEq: return "NotEq";
        case ASTNodeType::Lt: return "Lt";
        case ASTNodeType::LtE: return "LtE";
        case ASTNodeType::Gt: return "Gt";
        case ASTNodeType::GtE: return "GtE";
        case ASTNodeType::Is: return "Is";
        case ASTNodeType::IsNot: return "IsNot";
        case ASTNodeType::In: return "In";
        case ASTNodeType::NotIn: return "NotIn";
        case ASTNodeType::Invert: return "Invert";
        case ASTNodeType::Not: return "Not";
        case ASTNodeType::UAdd: return "UAdd";
        case ASTNodeType::USub: return "USub";
        case ASTNodeType::And: return "And";
        case ASTNodeType::Or: return "Or";
        case ASTNodeType::Expr: return "Expr";
        case ASTNodeType::Stmt: return "Stmt";
        case ASTNodeType::Invalid: return "Invalid";
        default: return "Unknown";
    }
}

std::string Constant::toString() const {
    std::ostringstream oss;
    oss << "Constant(";
    
    if (isNone()) {
        oss << "None";
    } else if (isTrue()) {
        oss << "True";
    } else if (isFalse()) {
        oss << "False";
    } else if (std::holds_alternative<std::monostate>(value)) {
        oss << "...";
    } else if (isInt()) {
        oss << asInt();
    } else if (isFloat()) {
        oss << std::setprecision(15) << asFloat();
    } else if (isString()) {
        oss << "\"" << asString() << "\"";
    } else if (isBytes()) {
        auto bytes = std::get<std::vector<uint8_t>>(value);
        oss << "b\"";
        for (auto byte : bytes) {
            oss << std::hex << std::setfill('0') << std::setw(2) << (int)byte;
        }
        oss << "\"";
    } else {
        oss << "<unknown>";
    }
    
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Name::toString() const {
    std::ostringstream oss;
    oss << "Name(" << "id='" << id << "', ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string BinOp::toString() const {
    std::ostringstream oss;
    oss << "BinOp(";
    if (left) oss << left->toString();
    oss << " " << getTypeName(op) << ", ";
    if (right) oss << right->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string UnaryOp::toString() const {
    std::ostringstream oss;
    oss << "UnaryOp(" << getTypeName(op);
    if (operand) oss << " " << operand->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Compare::toString() const {
    std::ostringstream oss;
    oss << "Compare(";
    if (left) oss << left->toString();
    oss << ", [";
    for (size_t i = 0; i < ops.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << getTypeName(ops[i]);
    }
    oss << "], [";
    for (size_t i = 0; i < comparators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (comparators[i]) oss << comparators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string BoolOp::toString() const {
    std::ostringstream oss;
    oss << "BoolOp(" << getTypeName(op) << ", [";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        if (values[i]) oss << values[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Call::toString() const {
    std::ostringstream oss;
    oss << "Call(";
    if (func) oss << func->toString();
    oss << ", args=[";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) oss << ", ";
        if (args[i]) oss << args[i]->toString();
    }
    oss << "], keywords=[";
    for (size_t i = 0; i < keywords.size(); ++i) {
        if (i > 0) oss << ", ";
        if (keywords[i]) oss << keywords[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string IfExp::toString() const {
    std::ostringstream oss;
    oss << "IfExp(test=";
    if (test) oss << test->toString();
    oss << ", body=";
    if (body) oss << body->toString();
    oss << ", orelse=";
    if (orelse) oss << orelse->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Lambda::toString() const {
    std::ostringstream oss;
    oss << "Lambda(args=";
    if (args) oss << args->toString();
    oss << ", body=";
    if (body) oss << body->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Dict::toString() const {
    std::ostringstream oss;
    oss << "Dict(keys=[";
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) oss << ", ";
        if (keys[i]) oss << keys[i]->toString(); else oss << "None";
    }
    oss << "], values=[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        if (values[i]) oss << values[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Set::toString() const {
    std::ostringstream oss;
    oss << "Set(elts=[";
    for (size_t i = 0; i < elts.size(); ++i) {
        if (i > 0) oss << ", ";
        if (elts[i]) oss << elts[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string List::toString() const {
    std::ostringstream oss;
    oss << "List(elts=[";
    for (size_t i = 0; i < elts.size(); ++i) {
        if (i > 0) oss << ", ";
        if (elts[i]) oss << elts[i]->toString();
    }
    oss << "], ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Tuple::toString() const {
    std::ostringstream oss;
    oss << "Tuple(elts=[";
    for (size_t i = 0; i < elts.size(); ++i) {
        if (i > 0) oss << ", ";
        if (elts[i]) oss << elts[i]->toString();
    }
    oss << "], ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Subscript::toString() const {
    std::ostringstream oss;
    oss << "Subscript(value=";
    if (value) oss << value->toString();
    oss << ", slice=";
    if (slice) oss << slice->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Attribute::toString() const {
    std::ostringstream oss;
    oss << "Attribute(value=";
    if (value) oss << value->toString();
    oss << ", attr='" << attr << "', line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Starred::toString() const {
    std::ostringstream oss;
    oss << "Starred(value=";
    if (value) oss << value->toString();
    oss << ", ctx=";
    switch (ctx) {
        case Load: oss << "Load"; break;
        case Store: oss << "Store"; break;
        case Del: oss << "Del"; break;
    }
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Slice::toString() const {
    std::ostringstream oss;
    oss << "Slice(lower=";
    if (lower) oss << lower->toString(); else oss << "None";
    oss << ", upper=";
    if (upper) oss << upper->toString(); else oss << "None";
    oss << ", step=";
    if (step) oss << step->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string ListComp::toString() const {
    std::ostringstream oss;
    oss << "ListComp(elt=";
    if (elt) oss << elt->toString();
    oss << ", generators=[";
    for (size_t i = 0; i < generators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (generators[i]) oss << generators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string SetComp::toString() const {
    std::ostringstream oss;
    oss << "SetComp(elt=";
    if (elt) oss << elt->toString();
    oss << ", generators=[";
    for (size_t i = 0; i < generators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (generators[i]) oss << generators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string DictComp::toString() const {
    std::ostringstream oss;
    oss << "DictComp(key=";
    if (key) oss << key->toString();
    oss << ", value=";
    if (value) oss << value->toString();
    oss << ", generators=[";
    for (size_t i = 0; i < generators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (generators[i]) oss << generators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string GeneratorExp::toString() const {
    std::ostringstream oss;
    oss << "GeneratorExp(elt=";
    if (elt) oss << elt->toString();
    oss << ", generators=[";
    for (size_t i = 0; i < generators.size(); ++i) {
        if (i > 0) oss << ", ";
        if (generators[i]) oss << generators[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Yield::toString() const {
    std::ostringstream oss;
    oss << "Yield(value=";
    if (value) oss << value->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string YieldFrom::toString() const {
    std::ostringstream oss;
    oss << "YieldFrom(value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Await::toString() const {
    std::ostringstream oss;
    oss << "Await(value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string FormattedValue::toString() const {
    std::ostringstream oss;
    oss << "FormattedValue(value=";
    if (value) oss << value->toString();
    oss << ", conversion=" << conversion;
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string JoinedStr::toString() const {
    std::ostringstream oss;
    oss << "JoinedStr(values=[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ", ";
        if (values[i]) oss << values[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string FunctionDef::toString() const {
    std::ostringstream oss;
    oss << "FunctionDef(name='" << name << "', args=";
    if (args) oss << args->toString(); else oss << "None";
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], decorators=[";
    for (size_t i = 0; i < decorator_list.size(); ++i) {
        if (i > 0) oss << ", ";
        if (decorator_list[i]) oss << decorator_list[i]->toString();
    }
    oss << "], returns='" << returns << "', line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AsyncFunctionDef::toString() const {
    std::ostringstream oss;
    oss << "AsyncFunctionDef(";
    oss << "name='" << name << "', args=";
    if (args) oss << args->toString(); else oss << "None";
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string ClassDef::toString() const {
    std::ostringstream oss;
    oss << "ClassDef(name='" << name << "', bases=";
    if (bases) oss << bases->toString(); else oss << "None";
    oss << ", keywords=";
    if (keywords) oss << keywords->toString(); else oss << "None";
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], decorators=[";
    for (size_t i = 0; i < decorator_list.size(); ++i) {
        if (i > 0) oss << ", ";
        if (decorator_list[i]) oss << decorator_list[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Return::toString() const {
    std::ostringstream oss;
    oss << "Return(value=";
    if (value) oss << value->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Delete::toString() const {
    std::ostringstream oss;
    oss << "Delete(targets=[";
    for (size_t i = 0; i < targets.size(); ++i) {
        if (i > 0) oss << ", ";
        if (targets[i]) oss << targets[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Assign::toString() const {
    std::ostringstream oss;
    oss << "Assign(targets=[";
    for (size_t i = 0; i < targets.size(); ++i) {
        if (i > 0) oss << ", ";
        if (targets[i]) oss << targets[i]->toString();
    }
    oss << "], value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AugAssign::toString() const {
    std::ostringstream oss;
    oss << "AugAssign(target=";
    if (target) oss << target->toString();
    oss << ", op=" << getTypeName(op) << ", value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AnnAssign::toString() const {
    std::ostringstream oss;
    oss << "AnnAssign(target=";
    if (target) oss << target->toString();
    oss << ", annotation=";
    if (annotation) oss << annotation->toString();
    oss << ", value=";
    if (value) oss << value->toString(); else oss << "None";
    oss << ", simple=" << (simple ? "true" : "false");
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string For::toString() const {
    std::ostringstream oss;
    oss << "For(target=";
    if (target) oss << target->toString();
    oss << ", iter=";
    if (iter) oss << iter->toString();
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AsyncFor::toString() const {
    std::ostringstream oss;
    oss << "AsyncFor(";
    oss << "target=";
    if (target) oss << target->toString();
    oss << ", iter=";
    if (iter) oss << iter->toString();
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string While::toString() const {
    std::ostringstream oss;
    oss << "While(test=";
    if (test) oss << test->toString();
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string If::toString() const {
    std::ostringstream oss;
    oss << "If(test=";
    if (test) oss << test->toString();
    oss << ", body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string With::toString() const {
    std::ostringstream oss;
    oss << "With(items=[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << ", ";
        if (items[i]) oss << items[i]->toString();
    }
    oss << "], body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string AsyncWith::toString() const {
    std::ostringstream oss;
    oss << "AsyncWith(";
    oss << "items=[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << ", ";
        if (items[i]) oss << items[i]->toString();
    }
    oss << "], body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Raise::toString() const {
    std::ostringstream oss;
    oss << "Raise(exc=";
    if (exc) oss << exc->toString(); else oss << "None";
    oss << ", cause=";
    if (cause) oss << cause->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Try::toString() const {
    std::ostringstream oss;
    oss << "Try(body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], handlers=[";
    for (size_t i = 0; i < handlers.size(); ++i) {
        if (i > 0) oss << ", ";
        if (handlers[i]) oss << handlers[i]->toString();
    }
    oss << "], orelse=[";
    for (size_t i = 0; i < orelse.size(); ++i) {
        if (i > 0) oss << ", ";
        if (orelse[i]) oss << orelse[i]->toString();
    }
    oss << "], finalbody=[";
    for (size_t i = 0; i < finalbody.size(); ++i) {
        if (i > 0) oss << ", ";
        if (finalbody[i]) oss << finalbody[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Assert::toString() const {
    std::ostringstream oss;
    oss << "Assert(test=";
    if (test) oss << test->toString();
    oss << ", msg=";
    if (msg) oss << msg->toString(); else oss << "None";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Global::toString() const {
    std::ostringstream oss;
    oss << "Global(names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "'" << names[i] << "'";
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Nonlocal::toString() const {
    std::ostringstream oss;
    oss << "Nonlocal(names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "'" << names[i] << "'";
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Import::toString() const {
    std::ostringstream oss;
    oss << "Import(names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        if (names[i]) oss << names[i]->toString();
    }
    oss << "], line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string ImportFrom::toString() const {
    std::ostringstream oss;
    oss << "ImportFrom(module=";
    if (module) oss << module->toString(); else oss << "None";
    oss << ", names=[";
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << ", ";
        if (names[i]) oss << names[i]->toString();
    }
    oss << "], level=" << level;
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Alias::toString() const {
    std::ostringstream oss;
    oss << "Alias(name=";
    if (name) oss << name->toString(); else oss << "None";
    oss << ", asname='" << asname << "'";
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string ExprStmt::toString() const {
    std::ostringstream oss;
    oss << "ExprStmt(value=";
    if (value) oss << value->toString();
    oss << ", line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Pass::toString() const {
    std::ostringstream oss;
    oss << "Pass(line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Break::toString() const {
    std::ostringstream oss;
    oss << "Break(line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string Continue::toString() const {
    std::ostringstream oss;
    oss << "Continue(line=" << position.line << ", col=" << position.column << ")";
    return oss.str();
}

std::string arguments::toString() const {
    std::ostringstream oss;
    oss << "arguments(args=[";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) oss << ", ";
        if (args[i]) oss << args[i]->toString();
    }
    oss << "], vararg=";
    if (vararg) oss << vararg->toString(); else oss << "None";
    oss << ", kwonlyargs=[";
    for (size_t i = 0; i < kwonlyargs.size(); ++i) {
        if (i > 0) oss << ", ";
        if (kwonlyargs[i]) oss << kwonlyargs[i]->toString();
    }
    oss << "], kw_defaults=[";
    for (size_t i = 0; i < kw_defaults.size(); ++i) {
        if (i > 0) oss << ", ";
        if (kw_defaults[i]) oss << kw_defaults[i]->toString(); else oss << "None";
    }
    oss << "], kwarg=";
    if (kwarg) oss << kwarg->toString(); else oss << "None";
    oss << ", defaults=[";
    for (size_t i = 0; i < defaults.size(); ++i) {
        if (i > 0) oss << ", ";
        if (defaults[i]) oss << defaults[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string arg::toString() const {
    std::ostringstream oss;
    oss << "arg(arg='" << name << "', annotation=";
    if (annotation) oss << annotation->toString(); else oss << "None";
    oss << ")";
    return oss.str();
}

std::string except_handler::toString() const {
    std::ostringstream oss;
    oss << "except_handler(type=";
    if (type) oss << type->toString(); else oss << "None";
    oss << ", name='" << name << "', body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string withitem::toString() const {
    std::ostringstream oss;
    oss << "withitem(context_expr=";
    if (context_expr) oss << context_expr->toString(); else oss << "None";
    oss << ", optional_vars=";
    if (optional_vars) oss << optional_vars->toString(); else oss << "None";
    oss << ")";
    return oss.str();
}

std::string comprehension::toString() const {
    std::ostringstream oss;
    oss << "comprehension(target=";
    if (target) oss << target->toString(); else oss << "None";
    oss << ", iter=";
    if (iter) oss << iter->toString(); else oss << "None";
    oss << ", ifs=[";
    for (size_t i = 0; i < ifs.size(); ++i) {
        if (i > 0) oss << ", ";
        if (ifs[i]) oss << ifs[i]->toString();
    }
    oss << "], is_async=" << (is_async ? "true" : "false");
    oss << ")";
    return oss.str();
}

std::string Module::toString() const {
    std::ostringstream oss;
    oss << "Module(body=[";
    for (size_t i = 0; i < body.size(); ++i) {
        if (i > 0) oss << ", ";
        if (body[i]) oss << body[i]->toString();
    }
    oss << "], type_ignores=[";
    for (size_t i = 0; i < type_ignores.size(); ++i) {
        if (i > 0) oss << ", ";
        if (type_ignores[i]) oss << type_ignores[i]->toString();
    }
    oss << "])";
    return oss.str();
}

} // namespace csgo