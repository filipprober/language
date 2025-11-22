#ifndef LANG_TYPECHECKER_H
#define LANG_TYPECHECKER_H

using namespace std;

class ReturnPathAnalyzer
{
public:
    static bool hasReturnOnAllPaths(const vector<unique_ptr<Statement>>& statements) {
        for (size_t i = 0; i < statements.size(); i++) {
            const auto& stmt = statements[i];

            if (dynamic_cast<ReturnStatement*>(stmt.get())) {
                return true;
            }

            if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt.get())) {
                if (!ifStmt->elseBranch.empty()) {
                    bool thenReturns = hasReturnOnAllPaths(ifStmt->thenBranch);
                    bool elseReturns = hasReturnOnAllPaths(ifStmt->elseBranch);

                    if (thenReturns && elseReturns) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    static void validateFunction(const FunctionDeclaration* funcDecl) {
        string funcName = funcDecl->name;

        if (funcName == "main") {
            return;
        }

        if (funcDecl->resolvedReturnType->kind == TypeKind::Void) {
            return;
        }

        if (!hasReturnOnAllPaths(funcDecl->body)) {
            throw runtime_error(
                "Function '" + funcName + "' with return type '" + funcDecl->returnTypeStr +
                "' does not return a value on all code paths"
            );
        }
    }
};

// ======================
// Type Checker
// ======================

class TypeChecker {
public:
    TypeChecker(ExceptionReporter& reporter) : reporter(reporter) {}

    void check(Program* program) {
        // Pass 1: Sammle alle Funktionen ZUERST
        for (const auto& stmt : program->statements) {
            if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
                collectFunction(funcDecl);
            }
        }

        // Pass 2: Type-check alle Statements
        for (const auto& stmt : program->statements) {
            checkStatement(stmt.get());
        }
    }

    unordered_map<string, shared_ptr<MyType>>& getSymbolTable() {
        return symbolTable;
    }

private:
    ExceptionReporter& reporter;
    unordered_map<string, shared_ptr<MyType>> symbolTable;

    struct FunctionSignature {
        vector<shared_ptr<MyType>> paramTypes;
        shared_ptr<MyType> returnType;
    };
    unordered_map<string, FunctionSignature> functionTable;

    void collectFunction(FunctionDeclaration* funcDecl) {
        FunctionSignature sig;
        for (const auto& param : funcDecl->parameters) {
            sig.paramTypes.push_back(param.resolvedType);
        }
        sig.returnType = funcDecl->resolvedReturnType;

        functionTable[funcDecl->name] = sig;
    }

    void checkStatement(Statement* stmt) {
        if (auto* funcDecl = dynamic_cast<FunctionDeclaration*>(stmt)) {
            checkFunction(funcDecl);
        }
        else if (auto* varDecl = dynamic_cast<VariableDeclaration*>(stmt)) {
            checkVarDeclaration(varDecl);
        }
        else if (auto* ret = dynamic_cast<ReturnStatement*>(stmt)) {
            if (ret->value) {
                inferType(ret->value.get());
            }
        }
        else if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt)) {
            inferType(ifStmt->condition.get());
            for (const auto& s : ifStmt->thenBranch) checkStatement(s.get());
            for (const auto& s : ifStmt->elseBranch) checkStatement(s.get());
        }
        else if (auto* whileStmt = dynamic_cast<WhileStatement*>(stmt)) {
            inferType(whileStmt->condition.get());
            for (const auto& s : whileStmt->body) checkStatement(s.get());
        }
        else if (auto* exprStmt = dynamic_cast<ExpressionStatement*>(stmt)) {
            inferType(exprStmt->expression.get());
        }
    }

    void checkFunction(FunctionDeclaration* funcDecl) {
        // Parameters in symbol table
        for (const auto& param : funcDecl->parameters) {
            symbolTable[param.name] = param.resolvedType;
        }

        // Check body
        for (const auto& stmt : funcDecl->body) {
            checkStatement(stmt.get());
        }

        // Clear parameters
        for (const auto& param : funcDecl->parameters) {
            symbolTable.erase(param.name);
        }
    }

    void checkVarDeclaration(VariableDeclaration* varDecl) {
        if (varDecl->initializer) {
            inferType(varDecl->initializer.get());

            if (!varDecl->typeStr.empty()) {
                varDecl->resolvedType = MyType::parse(varDecl->typeStr);
            } else {
                varDecl->resolvedType = varDecl->initializer->exprType;
            }
        } else {
            varDecl->resolvedType = MyType::parse(varDecl->typeStr);
        }

        symbolTable[varDecl->name] = varDecl->resolvedType;
    }

    shared_ptr<MyType> inferType(Expression* expr) {
        if (expr->exprType) {
            return expr->exprType;  // Already inferred
        }

        if (auto* var = dynamic_cast<Variable*>(expr)) {
            auto it = symbolTable.find(var->name);
            if (it != symbolTable.end()) {
                expr->exprType = it->second;
            } else {
                expr->exprType = MyType::Int();  // Default fallback
            }
        }
        else if (auto* binOp = dynamic_cast<BinaryOperation*>(expr)) {
            auto leftType = inferType(binOp->left.get());
            auto rightType = inferType(binOp->right.get());

            if (binOp->op == TokenType::DOT) {
                expr->exprType = MyType::String();
            }
            else if (binOp->op == TokenType::EQUAL_EQUAL || binOp->op == TokenType::BANG_EQUAL ||
                     binOp->op == TokenType::LESS || binOp->op == TokenType::LESS_EQUAL ||
                     binOp->op == TokenType::GREATER || binOp->op == TokenType::GREATER_EQUAL ||
                     binOp->op == TokenType::AND || binOp->op == TokenType::OR) {
                expr->exprType = MyType::Bool();
            }
            else {
                expr->exprType = leftType;
            }
        }
        else if (auto* unaryOp = dynamic_cast<UnaryOperation*>(expr)) {
            auto operandType = inferType(unaryOp->operand.get());

            if (unaryOp->op == TokenType::NOT) {
                expr->exprType = MyType::Bool();
            } else {
                expr->exprType = operandType;
            }
        }
        else if (auto* call = dynamic_cast<FunctionCall*>(expr)) {
            if (call->name == "print") {
                expr->exprType = MyType::Void();
            } else {
                auto it = functionTable.find(call->name);
                if (it != functionTable.end()) {
                    expr->exprType = it->second.returnType;

                    const auto& paramTypes = it->second.paramTypes;
                    for (size_t i = 0; i < call->arguments.size() && i < paramTypes.size(); i++) {
                        inferType(call->arguments[i].get());

                        // Coerce null -> Optional Type
                        if (call->arguments[i]->exprType->kind == TypeKind::Null &&
                            paramTypes[i]->kind == TypeKind::Optional) {
                            // Setze den Typ des Arguments auf den erwarteten Optional-Typ
                            call->arguments[i]->exprType = paramTypes[i];
                            }
                    }
                } else {
                    expr->exprType = MyType::Int();
                }
            }

            // Infer argument types
            for (const auto& arg : call->arguments) {
                inferType(arg.get());
            }
        }
        else if (auto* assign = dynamic_cast<Assignment*>(expr)) {
            inferType(assign->value.get());
            expr->exprType = assign->value->exprType;
        }

        if (!expr->exprType) {
            expr->exprType = MyType::Int();
        }

        return expr->exprType;
    }
};

#endif //LANG_TYPECHECKER_H