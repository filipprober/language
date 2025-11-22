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

    static shared_ptr<MyType> Float() {
        static auto t = make_shared<MyType>(TypeKind::Float);
        return t;
    }

    static shared_ptr<MyType> String() {
        static auto t = make_shared<MyType>(TypeKind::String);
        return t;
    }

    static shared_ptr<MyType> Void() {
        static auto t = make_shared<MyType>(TypeKind::Void);
        return t;
    }

    static shared_ptr<MyType> Null() {
        static auto t = make_shared<MyType>(TypeKind::Null);
        return t;
    }

    static shared_ptr<MyType> Array(shared_ptr<MyType> elem) {
        auto t = make_shared<MyType>(TypeKind::Array);
        t->elementType = elem;
        return t;
    }

    static shared_ptr<MyType> Optional(shared_ptr<MyType> elem) {
        auto t = make_shared<MyType>(TypeKind::Optional);
        t->elementType = elem;
        return t;
    }

    static shared_ptr<MyType> parse(const string& typeStr) {
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

    string toString() const {
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

    bool equals(const shared_ptr<MyType>& other) const {
        if (!other) return false;
        if (kind != other->kind) return false;

        if (kind == TypeKind::Array || kind == TypeKind::Optional) {
            return elementType && other->elementType &&
                   elementType->equals(other->elementType);
        }
        return true;
    }

    bool isNumeric() const {
        return kind == TypeKind::Int || kind == TypeKind::Float;
    }

    bool canAssignFrom(const shared_ptr<MyType>& other) const {
        if (equals(other)) return true;

        if (kind == TypeKind::Optional && other->kind == TypeKind::Null) {
            return true;
        }

        if (kind == TypeKind::Optional && elementType) {
            return elementType->equals(other);
        }

        return false;
    }

    llvm::Type* toLLVMType(LLVMContext& context) const {
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
};

inline shared_ptr<MyType> MyType::Int() {
    static auto t = make_shared<MyType>(TypeKind::Int);
    return t;
}

inline shared_ptr<MyType> MyType::Bool() {
    static auto t = make_shared<MyType>(TypeKind::Bool);
    return t;
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

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "IntLiteral(" << value << ")" << endl;
    }
};

class MyStringLiteral : public Expression {
public:
    string value;

    explicit MyStringLiteral(string value) : value(value) {
        exprType = MyType::String();
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "StringLiteral(" << value << ")" << endl;
    }
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

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "InterpolatedString(\"" << template_str << "\")" << endl;

        for (const auto& var : variables) {
            cout << string(indent + 2, ' ') << "Variable: " << var << endl;
        }
    }
};

class BoolLiteral : public Expression {
public:
    bool value;

    explicit BoolLiteral(bool value) : value(value) {
        exprType = MyType::Bool();
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "BoolLiteral(" << (value ? "true" : "false") << ")" << endl;
    }
};

class NullLiteral : public Expression {
public:
    NullLiteral() {
        exprType = MyType::Null();
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "NullLiteral(null)" << endl;
    }
};

class Variable : public Expression {
public:
    string name;

    explicit Variable(const string& name) : name(name) {
        // Type wird später vom TypeChecker gesetzt
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "Variable(" << name << ")" << endl;
    }
};

class BinaryOperation : public Expression {
public:
    unique_ptr<Expression> left;
    TokenType op;
    unique_ptr<Expression> right;

    BinaryOperation(unique_ptr<Expression> left, TokenType op, unique_ptr<Expression> right) :
        left(std::move(left)),
        op(op),
        right(std::move(right)) {
        // Type wird später gesetzt
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "BinaryOperation(" << tokenTypeToString(op) << ")" << endl;
        left->print(indent + 2);
        right->print(indent + 2);
    }
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

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "Assignment(" << name << ")" << endl;
        value->print(indent + 2);
    }
};

class UnaryOperation : public Expression {
public:
    TokenType op;
    unique_ptr<Expression> operand;

    UnaryOperation(TokenType op, unique_ptr<Expression> operand) :
        op(op),
        operand(std::move(operand)) {
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "UnaryOperation(" << tokenTypeToString(op) << ")" << endl;
        operand->print(indent + 2);
    }
};

// ======================
// Statements
// ======================

class VariableDeclaration : public Statement {
public:
    string name;
    string typeStr;
    shared_ptr<MyType> resolvedType;  // UPDATED: Resolved type
    unique_ptr<Expression> initializer;

    VariableDeclaration(const string& name, const string& type, unique_ptr<Expression> initializer) :
        name(name),
        typeStr(type),
        initializer(std::move(initializer)) {
    }

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "VariableDeclaration(name=" << name;
        if (!typeStr.empty()) {
            cout << ", type=" << typeStr;
        }
        cout << ")" << endl;
        if (initializer) {
            initializer->print(indent + 2);
        }
    }
};

class ReturnStatement : public Statement {
public:
    unique_ptr<Expression> value;

    explicit ReturnStatement(unique_ptr<Expression> value) :
        value(std::move(value)) {}

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "ReturnStatement" << endl;
        if (value) {
            value->print(indent + 2);
        }
    }
};

class ExpressionStatement : public Statement {
public:
    unique_ptr<Expression> expression;

    explicit ExpressionStatement(unique_ptr<Expression> expression) :
        expression(std::move(expression)) {}

    void print(int indent = 0) const override {
        cout << string(indent, ' ') << "ExpressionStatement" << endl;
        expression->print(indent + 2);
    }
};
