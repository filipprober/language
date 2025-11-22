#pragma once

#include <iostream>
#include <utility>

#include "../helpers.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"

using namespace std;
using namespace llvm;

enum class TypeKind {
    Int,
    Bool,
    Float,
    String,
    Void,
    Null,
    Array,
    Optional,
    Unknown
};


class MyType {
public:
    TypeKind kind;
    shared_ptr<MyType> elementType;  // Für Arrays und Optional

    // Public constructor für make_shared
    explicit MyType(TypeKind k) : kind(k), elementType(nullptr) {}

    static shared_ptr<MyType> Int();
    static shared_ptr<MyType> Bool();
    static shared_ptr<MyType> Float();
    static shared_ptr<MyType> String();
    static shared_ptr<MyType> Void();
    static shared_ptr<MyType> Null();
    static shared_ptr<MyType> Array(shared_ptr<MyType> elem);
    static shared_ptr<MyType> Optional(shared_ptr<MyType> elem);

    static shared_ptr<MyType> parse(const string& typeStr);
    string toString() const;
    bool equals(const shared_ptr<MyType>& other) const;
    bool isNumeric() const;
    bool canAssignFrom(const shared_ptr<MyType>& other) const;
    llvm::Type* toLLVMType(LLVMContext& context) const;
};

inline shared_ptr<MyType> MyType::Int() {
    static auto t = make_shared<MyType>(TypeKind::Int);
    return t;
}

inline shared_ptr<MyType> MyType::Bool() {
    static auto t = make_shared<MyType>(TypeKind::Bool);
    return t;
}

inline shared_ptr<MyType> MyType::Float() {
    static auto t = make_shared<MyType>(TypeKind::Float);
    return t;
}

inline shared_ptr<MyType> MyType::String() {
    static auto t = make_shared<MyType>(TypeKind::String);
    return t;
}

inline shared_ptr<MyType> MyType::Void() {
    static auto t = make_shared<MyType>(TypeKind::Void);
    return t;
}

inline shared_ptr<MyType> MyType::Null() {
    static auto t = make_shared<MyType>(TypeKind::Null);
    return t;
}

inline shared_ptr<MyType> MyType::Array(shared_ptr<MyType> elem) {
    auto t = make_shared<MyType>(TypeKind::Array);
    t->elementType = elem;
    return t;
}

inline shared_ptr<MyType> MyType::Optional(shared_ptr<MyType> elem) {
    auto t = make_shared<MyType>(TypeKind::Optional);
    t->elementType = elem;
    return t;
}

inline shared_ptr<MyType> MyType::parse(const string& typeStr) {
    if (typeStr.empty() || typeStr == "void") {
        return Void();
    }

    // Optional: int?
    if (typeStr.back() == '?') {
        string base = typeStr.substr(0, typeStr.length() - 1);
        return Optional(parse(base));
    }

    // Array: int[]
    if (typeStr.length() > 2 && typeStr.substr(typeStr.length() - 2) == "[]") {
        string base = typeStr.substr(0, typeStr.length() - 2);
        return Array(parse(base));
    }

    if (typeStr == "int") return Int();
    if (typeStr == "bool") return Bool();
    if (typeStr == "float") return Float();
    if (typeStr == "string") return String();

    return make_shared<MyType>(TypeKind::Unknown);
}

inline string MyType::toString() const {
    switch (kind) {
        case TypeKind::Int: return "int";
        case TypeKind::Bool: return "bool";
        case TypeKind::Float: return "float";
        case TypeKind::String: return "string";
        case TypeKind::Void: return "void";
        case TypeKind::Null: return "null";
        case TypeKind::Array:
            return elementType->toString() + "[]";
        case TypeKind::Optional:
            return elementType->toString() + "?";
        default: return "unknown";
    }
}

inline bool MyType::equals(const shared_ptr<MyType>& other) const {
    if (!other) return false;
    if (kind != other->kind) return false;

    if (kind == TypeKind::Array || kind == TypeKind::Optional) {
        return elementType && other->elementType &&
               elementType->equals(other->elementType);
    }
    return true;
}

inline bool MyType::isNumeric() const {
    return kind == TypeKind::Int || kind == TypeKind::Float;
}

inline bool MyType::canAssignFrom(const shared_ptr<MyType>& other) const {
    if (equals(other)) return true;

    if (kind == TypeKind::Optional && other->kind == TypeKind::Null) {
        return true;
    }

    if (kind == TypeKind::Optional && elementType) {
        return elementType->equals(other);
    }

    return false;
}

inline llvm::Type* MyType::toLLVMType(LLVMContext& context) const {
    switch (kind) {
        case TypeKind::Int:
            return llvm::Type::getInt32Ty(context);
        case TypeKind::Bool:
            return llvm::Type::getInt1Ty(context);
        case TypeKind::Float:
            return llvm::Type::getDoubleTy(context);
        case TypeKind::String:
            return PointerType::get(llvm::Type::getInt8Ty(context), 0);
        case TypeKind::Void:
            return llvm::Type::getVoidTy(context);
        case TypeKind::Optional:
            if (elementType) {
                return PointerType::get(elementType->toLLVMType(context), 0);
            }
            return PointerType::getUnqual(context);
        case TypeKind::Array: {
            if (elementType) {
                vector<llvm::Type*> fields = {
                    llvm::Type::getInt32Ty(context),
                    PointerType::get(elementType->toLLVMType(context), 0)
                };
                return StructType::create(context, fields, "array_" + elementType->toString());
            }
            return llvm::Type::getInt32Ty(context);
        }
        default:
            return llvm::Type::getInt32Ty(context);
    }
}

class ASTNode
{
public:
    virtual ~ASTNode() {}
    virtual void print(int indent = 0) const = 0;
};

class Expression : public ASTNode {
public:
    shared_ptr<MyType> exprType;  // Der Typ dieser Expression
    virtual ~Expression() override = default;
};

class Statement : public ASTNode {
public:
    virtual ~Statement() override = default;
};

// ======================
// Expressions
// ======================

class IntLiteral : public Expression {
public:
    int value;

    explicit IntLiteral(int value) : value(value) {
        exprType = MyType::Int();
    }

    void print(int indent = 0) const override;
};

class MyStringLiteral : public Expression {
public:
    string value;

    explicit MyStringLiteral(string value) : value(value) {
        exprType = MyType::String();
    }

    void print(int indent = 0) const override;
};


class InterpolatedString : public Expression {
public:
    string template_str;
    vector<string> variables;

    InterpolatedString(const string& template_str, vector<string> variables) :
        template_str(template_str),
        variables(std::move(variables)) {
        exprType = MyType::String();
    }

    void print(int indent = 0) const override;
};

class BoolLiteral : public Expression {
public:
    bool value;

    explicit BoolLiteral(bool value) : value(value) {
        exprType = MyType::Bool();
    }

    void print(int indent = 0) const override;
};

class NullLiteral : public Expression {
public:
    NullLiteral() {
        exprType = MyType::Null();
    }

    void print(int indent = 0) const override;
};



class Variable : public Expression {
public:
    string name;

    explicit Variable(const string& name) : name(name) {}

    void print(int indent = 0) const override;
};

class BinaryOperation : public Expression
{
public:
    unique_ptr<Expression> left;
    TokenType op;
    unique_ptr<Expression> right;

    BinaryOperation(unique_ptr<Expression> left, TokenType op, unique_ptr<Expression> right) :
        left(std::move(left)),
        op(op),
        right(std::move(right)) {}

    void print(int indent = 0) const override;
};


class Assignment : public Expression
{
public:
    string name;
    unique_ptr<Expression> value;

    Assignment(const string& name, unique_ptr<Expression> value) :
        name(name),
        value(std::move(value)) {
    }

    void print(int indent = 0) const override;
};

class UnaryOperation : public Expression {
public:
    TokenType op;
    unique_ptr<Expression> operand;

    UnaryOperation(TokenType op, unique_ptr<Expression> operand) :
        op(op),
        operand(std::move(operand)) {
    }

    void print(int indent = 0) const override;
};

// ======================
// Statements
// ======================

class VariableDeclaration : public Statement {
public:
    string name;
    string typeStr;
    shared_ptr<MyType> resolvedType;
    unique_ptr<Expression> initializer;

    VariableDeclaration(const string& name, const string& type, unique_ptr<Expression> initializer) :
        name(name),
        typeStr(type),
        initializer(std::move(initializer)) {
    }

    void print(int indent = 0) const override;
};

class ReturnStatement : public Statement {
public:
    unique_ptr<Expression> value;

    explicit ReturnStatement(unique_ptr<Expression> value) :
        value(std::move(value)) {}

    void print(int indent = 0) const override;
};

class ExpressionStatement : public Statement {
public:
    unique_ptr<Expression> expression;

    explicit ExpressionStatement(unique_ptr<Expression> expression) :
        expression(std::move(expression)) {}

    void print(int indent = 0) const override;
};

class Parameter {
public:
    string name;
    string typeStr;
    shared_ptr<MyType> resolvedType;  // UPDATED

    Parameter(const string& name, const string& type) :
        name(name),
        typeStr(type),
        resolvedType(MyType::parse(type)) {
    }
};

class FunctionDeclaration : public Statement {
public:
    string name;
    vector<Parameter> parameters;
    string returnTypeStr;
    shared_ptr<MyType> resolvedReturnType;  // UPDATED
    vector<unique_ptr<Statement>> body;

    FunctionDeclaration(const string& name, vector<Parameter> parameters, const string& returnType, vector<unique_ptr<Statement>> body) :
        name(name),
        parameters(std::move(parameters)),
        returnTypeStr(returnType),
        resolvedReturnType(MyType::parse(returnType)),
        body(std::move(body)) {
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "FunctionDeclaration(name=" << name;

        cout << ", params=[";
        for (size_t i = 0; i < parameters.size(); i++) {
            if (i > 0) cout << ", ";
            cout << parameters[i].name << ": " << parameters[i].typeStr;
        }
        cout << "]";

        if (!returnTypeStr.empty()) {
            cout << ", returns=" << returnTypeStr;
        }
        cout << ")" << endl;

        for (const auto& statement : body) {
            statement->print(indent + 2);
        }
    }
};

class FunctionCall : public Expression {
public:
    string name;
    vector<unique_ptr<Expression>> arguments;

    FunctionCall(const string& name, vector<unique_ptr<Expression>> arguments) :
        name(name),
        arguments(std::move(arguments)) {
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "FunctionCall(" << name << ")" << endl;
        for (const auto& argument : arguments) {
            argument->print(indent + 2);
        }
    }
};

class IfStatement : public Statement {
public:
    unique_ptr<Expression> condition;
    vector<unique_ptr<Statement>> thenBranch;
    vector<unique_ptr<Statement>> elseBranch;

    IfStatement(
        unique_ptr<Expression> condition,
        vector<unique_ptr<Statement> > thenBranch,
        vector<unique_ptr<Statement> > elseBranch
    ) : condition(std::move(condition)),
        thenBranch(std::move(thenBranch)),
        elseBranch(std::move(elseBranch)) {
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "IfStatement" << endl;
        cout << string(indent + 2, ' ') << "Condition:" << endl;
        condition->print(indent + 4);
        cout << string(indent + 2, ' ') << "Then:" << endl;

        for (const auto& statement: thenBranch) {
            statement->print(indent + 4);
        }

        if (!elseBranch.empty()) {
            cout << string(indent + 2, ' ') << "Else:" << endl;
            for (const auto& statement: elseBranch) {
                statement->print(indent + 4);
            }
        }
    }
};

class WhileStatement : public Statement {
public:
    unique_ptr<Expression> condition;
    vector<unique_ptr<Statement>> body;

    WhileStatement(
        unique_ptr<Expression> condition,
        vector<unique_ptr<Statement>> body
    ) : condition(std::move(condition)),
        body(std::move(body)) {}

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "WhileStatement" << endl;
        cout << string(indent + 2, ' ') << "Condition:" << endl;
        condition->print(indent + 4);
        cout << string(indent + 2, ' ') << "Body:" << endl;

        for (const auto& statement: body) {
            statement->print(indent + 4);
        }
    }
};

class Program : public ASTNode {
public:
    vector<unique_ptr<Statement>> statements;

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "Program" << endl;

        for (const unique_ptr<Statement>& statement : statements) {
            statement->print(indent + 2);
        }
    }
};

inline void Variable::print(int indent) const {
    cout << string(indent, ' ') << "Variable(" << name << ")" << endl;
}

inline void BinaryOperation::print(int indent) const {
    cout << string(indent, ' ') << "BinaryOperation(" << tokenTypeToString(op) << ")" << endl;
    left->print(indent + 2);
    right->print(indent + 2);
}

inline void Assignment::print(int indent) const {
    cout << string(indent, ' ') << "Assignment(" << name << ")" << endl;
    value->print(indent + 2);
}

inline void UnaryOperation::print(int indent) const {
    cout << string(indent, ' ') << "UnaryOperation(" << tokenTypeToString(op) << ")" << endl;
    operand->print(indent + 2);
}

// ======================
// Statements
// ======================

inline void VariableDeclaration::print(int indent) const {
    cout << string(indent, ' ') << "VariableDeclaration(name=" << name;
    if (!typeStr.empty()) {
        cout << ", type=" << typeStr;
    }
    cout << ")" << endl;
    if (initializer) {
        initializer->print(indent + 2);
    }
}

inline void ReturnStatement::print(int indent) const {
    cout << string(indent, ' ') << "ReturnStatement" << endl;
    if (value) {
        value->print(indent + 2);
    }
}

inline void ExpressionStatement::print(int indent) const {
    cout << string(indent, ' ') << "ExpressionStatement" << endl;
    expression->print(indent + 2);
}

// ======================
// Literals
// ======================

inline void IntLiteral::print(int indent) const {
    cout << string(indent, ' ') << "IntLiteral(" << value << ")" << endl;
}

inline void MyStringLiteral::print(int indent) const {
    cout << string(indent, ' ') << "StringLiteral(" << value << ")" << endl;
}

inline void InterpolatedString::print(int indent) const {
    cout << string(indent, ' ') << "InterpolatedString(\"" << template_str << "\")" << endl;

    for (const auto& var : variables) {
        cout << string(indent + 2, ' ') << "Variable: " << var << endl;
    }
}

inline void BoolLiteral::print(int indent) const {
    cout << string(indent, ' ') << "BoolLiteral(" << (value ? "true" : "false") << ")" << endl;
}

inline void NullLiteral::print(int indent) const {
    cout << string(indent, ' ') << "NullLiteral(null)" << endl;
}
