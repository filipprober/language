#ifndef LANG_CODEGENERATOR_H
#define LANG_CODEGENERATOR_H

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

// ======================
// LLVM Code Generator
// ======================

class CodeGenerator {
public:
    CodeGenerator()
        : context(make_unique<LLVMContext>()),
          builder(make_unique<IRBuilder<>>(*context)),
          module(make_unique<Module>("mylang", *context)) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
    }

    void declarePrintf() {
        FunctionType* printfType = FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            {PointerType::get(llvm::Type::getInt8Ty(*context), 0)},
            true
        );

        Function::Create(
            printfType,
            Function::ExternalLinkage,
            "printf",
            module.get()
        );
    }

    void declareFunction(FunctionDeclaration* funcDecl) {
        vector<llvm::Type*> paramTypes;
        for (const auto& param : funcDecl->parameters) {
            paramTypes.push_back(param.resolvedType->toLLVMType(*context));
        }

        llvm::Type* returnType;
        if (funcDecl->name == "main") {
            returnType = llvm::Type::getInt32Ty(*context);
        } else {
            returnType = funcDecl->resolvedReturnType->toLLVMType(*context);
        }
        FunctionType* funcType = FunctionType::get(returnType, paramTypes, false);

        string mangledName = mangleFunctionName(funcDecl->name, funcDecl->parameters);

        if (funcDecl->name != "main") {
            mangledName = "c_" + mangledName;
        }

        Function* function = Function::Create(
            funcType,
            Function::ExternalLinkage,
            mangledName,
            module.get()
        );

        string tableName = mangleFunctionName(funcDecl->name, funcDecl->parameters);
        functions[tableName] = function;
    }

    void declareStringFunctions() {
        FunctionType* strlenType = FunctionType::get(
            llvm::Type::getInt64Ty(*context),
            {PointerType::get(llvm::Type::getInt8Ty(*context), 0)},
            false
        );
        Function::Create(strlenType, Function::ExternalLinkage, "strlen", module.get());

        FunctionType* mallocType = FunctionType::get(
            PointerType::get(llvm::Type::getInt8Ty(*context), 0),
            {llvm::Type::getInt64Ty(*context)},
            false
        );
        Function::Create(mallocType, Function::ExternalLinkage, "malloc", module.get());

        FunctionType* strcpyType = FunctionType::get(
            PointerType::get(llvm::Type::getInt8Ty(*context), 0),
            {
                PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                PointerType::get(llvm::Type::getInt8Ty(*context), 0)
            },
            false
        );
        Function::Create(strcpyType, Function::ExternalLinkage, "strcpy", module.get());

        FunctionType* strcatType = FunctionType::get(
            PointerType::get(llvm::Type::getInt8Ty(*context), 0),
            {
                PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                PointerType::get(llvm::Type::getInt8Ty(*context), 0)
            },
            false
        );
        Function::Create(strcatType, Function::ExternalLinkage, "strcat", module.get());

        FunctionType* sprintfType = FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            {
                PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                PointerType::get(llvm::Type::getInt8Ty(*context), 0)
            },
            true
        );
        Function::Create(sprintfType, Function::ExternalLinkage, "sprintf", module.get());
    }

    void generate(const Program& program) {
        declarePrintf();
        declareStringFunctions();

        for (const auto& stmt : program.statements) {
            if (auto* classDecl = dynamic_cast<ClassDeclaration*>(stmt.get())) {
                declareClass(classDecl);
            }
        }

        for (const auto &stmt: program.statements) {
            if (auto *funcDecl = dynamic_cast<FunctionDeclaration *>(stmt.get())) {
                declareFunction(funcDecl);
            }
        }

        for (const auto &stmt: program.statements) {
            generateStatement(stmt.get());
        }
    }

    void declareClass(ClassDeclaration *classDecl) {
        // Erstelle leeren Struct (wir haben keine Properties)
        vector<llvm::Type *> memberTypes;

        StructType *classStruct = StructType::create(*context, memberTypes, classDecl->name);
        classTypes[classDecl->name] = classStruct;
        classDecls[classDecl->name] = classDecl;

        // Declare Constructor
        if (!classDecl->constructors.empty()) {
            for (const auto &ctor: classDecl->constructors) {
                declareConstructor(classDecl, ctor.get());
            }
        }

        for (const auto& method : classDecl->methods) {
            declareMethod(classDecl, method.get());
        }
    }

    void declareMethod(ClassDeclaration* classDecl, MethodDeclaration* method) {
        // Methode hat als ersten Parameter immer 'this' (Pointer auf Klassen-Instanz)
        vector<llvm::Type*> paramTypes;

        if (!method->isStatic) {
            paramTypes.push_back(PointerType::get(classTypes[classDecl->name], 0));
        }

        for (const auto& param : method->parameters) {
            paramTypes.push_back(param.resolvedType->toLLVMType(*context));
        }

        llvm::Type* returnType = method->resolvedReturnType->toLLVMType(*context);
        FunctionType* methodType = FunctionType::get(returnType, paramTypes, false);

        string methodName = classDecl->name + "_" + method->name;
        Function* methodFunc = Function::Create(
            methodType,
            Function::ExternalLinkage,
            methodName,
            module.get()
        );

        functions[methodName] = methodFunc;
    }

    void declareConstructor(ClassDeclaration *classDecl, ConstructorDeclaration *ctor) {
        // Constructor Parameter
        vector<llvm::Type *> paramTypes;
        for (const auto &param: ctor->parameters) {
            paramTypes.push_back(param.resolvedType->toLLVMType(*context));
        }

        // Constructor gibt Pointer auf Klassen-Instanz zurück
        llvm::Type *returnType = PointerType::get(classTypes[classDecl->name], 0);
        FunctionType *ctorType = FunctionType::get(returnType, paramTypes, false);

        string ctorName = classDecl->name + "_init";
        Function *ctorFunc = Function::Create(
            ctorType,
            Function::ExternalLinkage,
            ctorName,
            module.get()
        );

        functions[ctorName] = ctorFunc;
    }

    void generateClass(ClassDeclaration *classDecl) {
        currentClass = classDecl;

        // Generate Constructors
        for (const auto &ctor: classDecl->constructors) {
            generateConstructor(classDecl, ctor.get());
        }

        for (const auto& method : classDecl->methods) {
            generateMethod(classDecl, method.get());
        }

        currentClass = nullptr;
    }

    void generateMethod(ClassDeclaration* classDecl, MethodDeclaration* method) {
        string methodName = classDecl->name + "_" + method->name;
        Function* methodFunc = functions[methodName];

        BasicBlock* entryBlock = BasicBlock::Create(*context, "entry", methodFunc);
        builder->SetInsertPoint(entryBlock);

        // Setup Parameters
        int argIndex = 0;

        if (!method->isStatic) {
            // Erster Parameter ist 'this'
            auto& thisArg = *methodFunc->arg_begin();
            thisArg.setName("this");

            AllocaInst* thisAlloca = builder->CreateAlloca(
                thisArg.getType(),
                nullptr,
                "this"
            );
            builder->CreateStore(&thisArg, thisAlloca);
            namedValues["this"] = thisAlloca;
            argIndex = 1;
        }

        // Rest der Parameter
        auto argIt = methodFunc->arg_begin();
        if (!method->isStatic) {
            ++argIt;  // Skip 'this'
        }

        for (size_t i = 0; i < method->parameters.size(); i++, ++argIt) {
            argIt->setName(method->parameters[i].name);

            AllocaInst* alloca = builder->CreateAlloca(
                argIt->getType(),
                nullptr,
                argIt->getName()
            );
            builder->CreateStore(&*argIt, alloca);
            namedValues[std::string(argIt->getName())] = alloca;
        }

        // Generate Method Body
        for (const auto& stmt : method->body) {
            generateStatement(stmt.get());
        }

        // Auto-return für void methods
        BasicBlock* currentBlock = builder->GetInsertBlock();
        if (currentBlock && !currentBlock->getTerminator()) {
            if (methodFunc->getReturnType()->isVoidTy()) {
                builder->CreateRetVoid();
            } else {
                builder->CreateUnreachable();
            }
        }

        verifyFunction(*methodFunc, &errs());
        namedValues.clear();
    }

    void generateConstructor(ClassDeclaration *classDecl, ConstructorDeclaration *ctor) {
        string ctorName = classDecl->name + "_init";
        Function *ctorFunc = functions[ctorName];

        BasicBlock *entryBlock = BasicBlock::Create(*context, "entry", ctorFunc);
        builder->SetInsertPoint(entryBlock);

        // Allocate memory für die Instanz
        Function *mallocFunc = module->getFunction("malloc");
        StructType *classStruct = classTypes[classDecl->name];
        Value *size = ConstantExpr::getSizeOf(classStruct);
        Value *instancePtr = builder->CreateCall(mallocFunc, {size}, "instance");
        Value *typedPtr = builder->CreateBitCast(
            instancePtr,
            PointerType::get(classStruct, 0)
        );

        // Parameter in namedValues speichern
        int argIndex = 0;
        for (auto &arg: ctorFunc->args()) {
            arg.setName(ctor->parameters[argIndex].name);

            AllocaInst *alloca = builder->CreateAlloca(
                arg.getType(),
                nullptr,
                arg.getName()
            );
            builder->CreateStore(&arg, alloca);
            namedValues[std::string(arg.getName())] = alloca;
            argIndex++;
        }

        // Generate Constructor Body
        for (const auto &stmt: ctor->body) {
            generateStatement(stmt.get());
        }

        // Return instance
        builder->CreateRet(typedPtr);

        verifyFunction(*ctorFunc, &errs());
        namedValues.clear();
    }

    void printIR() {
        module->print(outs(), nullptr);
    }

    void writeObjectFile(const string& filename) {
        auto targetTriple = sys::getDefaultTargetTriple();
        module->setTargetTriple(Triple(targetTriple));

        string error;
        auto target = TargetRegistry::lookupTarget(targetTriple, error);

        if (!target) {
            errs() << "Error: " << error << "\n";
            return;
        }

        auto CPU = "generic";
        auto features = "";

        TargetOptions opt;
        auto machine = target->createTargetMachine(
            Triple(targetTriple), CPU, features, opt, std::nullopt
        );

        module->setDataLayout(machine->createDataLayout());

        error_code EC;
        raw_fd_ostream dest(filename, EC, sys::fs::OF_None);

        if (EC) {
            errs() << "Could not open file: " << EC.message() << "\n";
            return;
        }

        legacy::PassManager pass;
        auto fileType = CodeGenFileType::ObjectFile;

        if (machine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
            errs() << "Target machine can't emit object file\n";
            return;
        }

        pass.run(*module);
        dest.flush();
    }

private:
    unique_ptr<LLVMContext> context;
    unique_ptr<IRBuilder<> > builder;
    unique_ptr<Module> module;

    unordered_map<string, Value *> namedValues;
    unordered_map<string, Function *> functions;
    Function *currentFunction = nullptr;

    unordered_map<string, StructType*> classTypes;
    unordered_map<string, ClassDeclaration*> classDecls;
    ClassDeclaration* currentClass = nullptr;

    string mangleFunctionName(const string& name, const vector<Parameter>& parameters) {
        if (name == "main") {
            return "main";
        }

        string mangledName = name;

        for (const auto& parameter : parameters) {
            string typeName = parameter.typeStr;
            // Normalize: Entferne Spaces, ersetze ? und []
            typeName.erase(remove(typeName.begin(), typeName.end(), ' '), typeName.end());
            for (char& c : typeName) {
                if (c == '?' || c == '[' || c == ']') {
                    c = '_';
                }
            }
            mangledName += "_" + typeName;
        }

        if (parameters.empty()) {
            mangledName += "_void";
        }

        return mangledName;
    }

    string mangleFunctionName(const string& name, const vector<shared_ptr<MyType>>& paramTypes) {
        if (name == "main") {
            return "main";
        }

        string mangledName = name;

        for (const auto& type : paramTypes) {
            string typeName = type->toString();
            // WICHTIG: GLEICHE Normalisierung wie oben!
            typeName.erase(remove(typeName.begin(), typeName.end(), ' '), typeName.end());
            for (char& c : typeName) {
                if (c == '?' || c == '[' || c == ']') {
                    c = '_';
                }
            }
            mangledName += "_" + typeName;
        }

        if (paramTypes.empty()) {
            mangledName += "_void";
        }

        return mangledName;
    }

    void generateStatement(Statement *stmt) {
        if (auto *funcDecl = dynamic_cast<FunctionDeclaration *>(stmt)) {
            generateFunction(funcDecl);
            return;
        }

        if (auto* classDecl = dynamic_cast<ClassDeclaration*>(stmt)) {
            generateClass(classDecl);
            return;
        }

        BasicBlock* currentBlock = builder->GetInsertBlock();
        if (currentBlock && currentBlock->getTerminator()) {
            return;
        }

        if (auto *varDecl = dynamic_cast<VariableDeclaration *>(stmt)) {
            generateVariableDeclaration(varDecl);
        } else if (auto *returnStmt = dynamic_cast<ReturnStatement *>(stmt)) {
            generateReturn(returnStmt);
        } else if (auto *ifStmt = dynamic_cast<IfStatement *>(stmt)) {
            generateIf(ifStmt);
        } else if (auto *whileStmt = dynamic_cast<WhileStatement *>(stmt)) {
            generateWhile(whileStmt);
        } else if (auto *exprStmt = dynamic_cast<ExpressionStatement *>(stmt)) {
            generateExpression(exprStmt->expression.get());
        }
    }

    void generateFunction(FunctionDeclaration* funcDecl) {
        string mangledName = mangleFunctionName(funcDecl->name, funcDecl->parameters);
        Function* function = functions[mangledName];
        currentFunction = function;

        BasicBlock* entryBlock = BasicBlock::Create(*context, "entry", function);
        builder->SetInsertPoint(entryBlock);

        for (auto& arg : function->args()) {
            arg.setName(funcDecl->parameters[arg.getArgNo()].name);

            AllocaInst* alloca = builder->CreateAlloca(
                arg.getType(),
                nullptr,
                arg.getName()
            );

            builder->CreateStore(&arg, alloca);

            namedValues[std::string(arg.getName())] = alloca;
        }

        for (const auto& stmt : funcDecl->body) {
            generateStatement(stmt.get());
        }

        BasicBlock* currentBlock = builder->GetInsertBlock();
        if (currentBlock && !currentBlock->getTerminator()) {
            if (funcDecl->name == "main") {
                builder->CreateRet(ConstantInt::get(*context, APInt(32, 0)));
            } else if (function->getReturnType()->isVoidTy()) {
                builder->CreateRetVoid();
            } else {
                builder->CreateUnreachable();
            }
        }

        verifyFunction(*function, &errs());

        namedValues.clear();
        currentFunction = nullptr;
    }

    void generateVariableDeclaration(VariableDeclaration* varDecl) {
        Function* function = builder->GetInsertBlock()->getParent();
        IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());

        // VEREINFACHT: Type ist bereits resolved!
        llvm::Type* type = varDecl->resolvedType->toLLVMType(*context);
        Value* initValue = nullptr;

        if (varDecl->initializer) {
            initValue = generateExpression(varDecl->initializer.get());

            // Optional boxing
            if (varDecl->resolvedType->kind == TypeKind::Optional &&
                varDecl->initializer->exprType->kind != TypeKind::Optional &&
                varDecl->initializer->exprType->kind != TypeKind::Null) {

                Function* mallocFunc = module->getFunction("malloc");
                Value* size = ConstantExpr::getSizeOf(initValue->getType());
                Value* ptr = builder->CreateCall(mallocFunc, {size}, "boxed");
                Value* typedPtr = builder->CreateBitCast(ptr, type);
                builder->CreateStore(initValue, typedPtr);
                initValue = typedPtr;
            }
        } else {
            // Default values
            if (varDecl->resolvedType->kind == TypeKind::Int) {
                initValue = ConstantInt::get(*context, APInt(32, 0));
            } else if (varDecl->resolvedType->kind == TypeKind::Bool) {
                initValue = ConstantInt::get(*context, APInt(1, 0));
            } else if (varDecl->resolvedType->kind == TypeKind::Optional ||
                       varDecl->resolvedType->kind == TypeKind::String) {
                initValue = ConstantPointerNull::get(cast<PointerType>(type));
            }
        }

        AllocaInst* alloca = tmpBuilder.CreateAlloca(type, nullptr, varDecl->name);

        if (initValue) {
            builder->CreateStore(initValue, alloca);
        }

        namedValues[varDecl->name] = alloca;
    }

    void generateReturn(ReturnStatement* returnStmt) {
        if (returnStmt->value) {
            Value* retValue = generateExpression(returnStmt->value.get());
            builder->CreateRet(retValue);
        } else {
            builder->CreateRetVoid();
        }
    }

    void generateIf(IfStatement *ifStmt) {
        Value *condition = generateExpression(ifStmt->condition.get());

        if (!condition->getType()->isIntegerTy(1)) {
            condition = builder->CreateICmpNE(
                condition,
                ConstantInt::get(*context, APInt(32, 0)),
                "ifcond"
            );
        }

        Function *function = builder->GetInsertBlock()->getParent();

        BasicBlock *thenBB = BasicBlock::Create(*context, "then", function);
        BasicBlock *elseBB = nullptr;
        BasicBlock *mergeBB = nullptr;

        if (!ifStmt->elseBranch.empty()) {
            elseBB = BasicBlock::Create(*context, "else");
            builder->CreateCondBr(condition, thenBB, elseBB);
        } else {
            mergeBB = BasicBlock::Create(*context, "ifcont");
            builder->CreateCondBr(condition, thenBB, mergeBB);
        }

        builder->SetInsertPoint(thenBB);
        for (const auto &stmt: ifStmt->thenBranch) {
            generateStatement(stmt.get());
        }
        bool thenHasTerminator = builder->GetInsertBlock()->getTerminator() != nullptr;

        bool elseHasTerminator = false;
        if (!ifStmt->elseBranch.empty()) {
            function->insert(function->end(), elseBB);
            builder->SetInsertPoint(elseBB);
            for (const auto &stmt: ifStmt->elseBranch) {
                generateStatement(stmt.get());
            }
            elseHasTerminator = builder->GetInsertBlock()->getTerminator() != nullptr;
        }

        if (!ifStmt->elseBranch.empty()) {
            if (!thenHasTerminator || !elseHasTerminator) {
                mergeBB = BasicBlock::Create(*context, "ifcont");

                if (!thenHasTerminator) {
                    builder->SetInsertPoint(thenBB);
                    builder->CreateBr(mergeBB);
                }

                if (!elseHasTerminator) {
                    builder->SetInsertPoint(elseBB);
                    builder->CreateBr(mergeBB);
                }

                function->insert(function->end(), mergeBB);
                builder->SetInsertPoint(mergeBB);
            }
        } else {
            if (!thenHasTerminator) {
                builder->SetInsertPoint(thenBB);
                builder->CreateBr(mergeBB);
            }
            function->insert(function->end(), mergeBB);
            builder->SetInsertPoint(mergeBB);
        }
    }

    void generateWhile(WhileStatement* whileStmt) {
        Function* function = builder->GetInsertBlock()->getParent();

        BasicBlock* condBB = BasicBlock::Create(*context, "whilecond", function);
        BasicBlock* loopBB = BasicBlock::Create(*context, "whileloop");
        BasicBlock* afterBB = BasicBlock::Create(*context, "afterloop");

        builder->CreateBr(condBB);

        builder->SetInsertPoint(condBB);
        Value* condition = generateExpression(whileStmt->condition.get());

        if (!condition->getType()->isIntegerTy(1)) {
            condition = builder->CreateICmpNE(
                condition,
                ConstantInt::get(*context, APInt(32, 0)),
                "loopcond"
            );
        }

        builder->CreateCondBr(condition, loopBB, afterBB);

        function->insert(function->end(), loopBB);
        builder->SetInsertPoint(loopBB);
        for (const auto& stmt : whileStmt->body) {
            generateStatement(stmt.get());
        }
        builder->CreateBr(condBB);

        function->insert(function->end(), afterBB);
        builder->SetInsertPoint(afterBB);
    }

    Value* generateExpression(Expression* expr) {
        if (!expr->exprType) {
            errs() << "Expression has no type!\n";
            return nullptr;
        }

        if (auto* intLit = dynamic_cast<IntLiteral*>(expr)) {
            return ConstantInt::get(*context, APInt(32, intLit->value));
        }
        else if (auto* boolLit = dynamic_cast<BoolLiteral*>(expr)) {
            return ConstantInt::get(*context, APInt(1, boolLit->value ? 1 : 0));
        }
        else if (auto* strLit = dynamic_cast<MyStringLiteral*>(expr)) {
            return builder->CreateGlobalStringPtr(strLit->value);
        }
        else if (dynamic_cast<NullLiteral*>(expr)) {
            return ConstantPointerNull::get(PointerType::getUnqual(*context));
        }
        else if (auto* newExpr = dynamic_cast<NewExpression*>(expr)) {
            return generateNewExpression(newExpr);
        }
        else if (auto* interpStr = dynamic_cast<InterpolatedString*>(expr)) {
            return generateInterpolatedString(interpStr);
        }
        else if (auto* arrayLiteral = dynamic_cast<ArrayLiteral*>(expr)) {
            return generateArrayLiteral(arrayLiteral);
        }
        else if (auto* var = dynamic_cast<Variable*>(expr)) {
            Value* varPtr = namedValues[var->name];
            if (!varPtr) {
                errs() << "Unknown variable: " << var->name << "\n";
                return nullptr;
            }

            if (auto* allocaInst = dyn_cast<AllocaInst>(varPtr)) {
                return builder->CreateLoad(
                    allocaInst->getAllocatedType(),
                    varPtr,
                    var->name.c_str()
                );
            } else {
                return varPtr;
            }
        }
        else if (auto* unaryOp = dynamic_cast<UnaryOperation*>(expr)) {
            return generateUnaryOp(unaryOp);
        }
        else if (auto* binOp = dynamic_cast<BinaryOperation*>(expr)) {
            return generateBinaryOp(binOp);
        }
        else if (auto* call = dynamic_cast<FunctionCall*>(expr)) {
            return generateFunctionCall(call);
        }
        else if (auto* assign = dynamic_cast<Assignment*>(expr)) {
            Value* val = generateExpression(assign->value.get());
            Value* varPtr = namedValues[assign->name];
            if (!varPtr) {
                errs() << "Unknown variable: " << assign->name << "\n";
                return nullptr;
            }
            builder->CreateStore(val, varPtr);
            return val;
        }
        else if (auto* superCall = dynamic_cast<SuperCall*>(expr)) {
            return generateSuperCall(superCall);
        }
        else if (auto* methodCall = dynamic_cast<MethodCall*>(expr)) {
            return generateMethodCall(methodCall);
        }

        return nullptr;
    }

    Value *generateMethodCall(MethodCall *methodCall) {
        // Get object
        Value *object = generateExpression(methodCall->object.get());
        if (!object) return nullptr;

        // Hole den Klassenname aus dem Type der Expression
        string className;

        if (methodCall->object->exprType) {
            className = methodCall->object->exprType->toString();
        }

        if (className.empty()) {
            errs() << "Could not determine class name for method call\n";
            return nullptr;
        }

        string methodName = className + "_" + methodCall->methodName;

        Function *methodFunc = functions[methodName];
        if (!methodFunc) {
            errs() << "Unknown method: " << methodName << "\n";
            return nullptr;
        }

        // Build arguments (first arg is 'this')
        vector<Value *> args;
        args.push_back(object); // 'this' pointer

        for (const auto &arg: methodCall->arguments) {
            args.push_back(generateExpression(arg.get()));
            if (!args.back()) return nullptr;
        }

        // Call method
        if (methodFunc->getReturnType()->isVoidTy()) {
            builder->CreateCall(methodFunc, args);
            return nullptr;
        }

        return builder->CreateCall(methodFunc, args, "method_result");
    }

    Value* generateSuperCall(SuperCall* superCall) {
        if (!currentClass) {
            errs() << "super() called outside of class context\n";
            return nullptr;
        }

        // Get parent class name
        string parentClassName = currentClass->baseClass;
        if (parentClassName.empty()) {
            errs() << "super() called but no parent class\n";
            return nullptr;
        }

        // Get parent constructor
        string parentCtorName = parentClassName + "_init";
        Function* parentCtor = functions[parentCtorName];

        if (!parentCtor) {
            errs() << "Parent constructor not found: " << parentCtorName << "\n";
            return nullptr;
        }

        // Generate arguments
        vector<Value*> args;
        for (const auto& arg : superCall->arguments) {
            args.push_back(generateExpression(arg.get()));
            if (!args.back()) return nullptr;
        }

        Value* parentInstance = builder->CreateCall(parentCtor, args, "parent_instance");

        return parentInstance;
    }

    Value* generateStringConcatenation(BinaryOperation *binOp) {
        Value* left = generateExpression(binOp->left.get());
        Value* right = generateExpression(binOp->right.get());

        if (!left || !right) return nullptr;

        left = convertToString(left, binOp->left->exprType);
        right = convertToString(right, binOp->right->exprType);

        if (!left || !right) return nullptr;

        Function *strlenFunc = module->getFunction("strlen");
        Function *mallocFunc = module->getFunction("malloc");
        Function *strcpyFunc = module->getFunction("strcpy");
        Function *strcatFunc = module->getFunction("strcat");

        Value *len1 = builder->CreateCall(strlenFunc, {left}, "len1");
        Value *len2 = builder->CreateCall(strlenFunc, {right}, "len2");
        Value *totalLen = builder->CreateAdd(len1, len2, "totallen");
        totalLen = builder->CreateAdd(
            totalLen,
            ConstantInt::get(*context, APInt(64, 1)),
            "totallen_plus1"
        );

        Value *result = builder->CreateCall(mallocFunc, {totalLen}, "concat_result");

        builder->CreateCall(strcpyFunc, {result, left});
        builder->CreateCall(strcatFunc, {result, right});

        return result;
    }

    Value* generateBinaryOp(BinaryOperation* binOp) {
        if (binOp->op == TokenType::AND || binOp->op == TokenType::OR) {
            return generateLogicalOp(binOp);
        }

        if (binOp->op == TokenType::DOT) {
            return generateStringConcatenation(binOp);
        }

        Value* left = generateExpression(binOp->left.get());
        Value* right = generateExpression(binOp->right.get());

        if (!left || !right) return nullptr;

        if (binOp->op == TokenType::EQUAL_EQUAL) {
            if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
                return builder->CreateICmpEQ(left, right, "ptreqtmp");
            }
            return builder->CreateICmpEQ(left, right, "eqtmp");
        }

        if (binOp->op == TokenType::BANG_EQUAL) {
            if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
                return builder->CreateICmpNE(left, right, "ptrnetmp");
            }
            return builder->CreateICmpNE(left, right, "netmp");
        }

        switch (binOp->op) {
            case TokenType::PLUS:
                return builder->CreateAdd(left, right, "addtmp");
            case TokenType::MINUS:
                return builder->CreateSub(left, right, "subtmp");
            case TokenType::STAR:
                return builder->CreateMul(left, right, "multmp");
            case TokenType::SLASH:
                return builder->CreateSDiv(left, right, "divtmp");
            case TokenType::PERCENT:
                return builder->CreateSRem(left, right, "modtmp");
            case TokenType::LESS:
                return builder->CreateICmpSLT(left, right, "lttmp");
            case TokenType::LESS_EQUAL:
                return builder->CreateICmpSLE(left, right, "letmp");
            case TokenType::GREATER:
                return builder->CreateICmpSGT(left, right, "gttmp");
            case TokenType::GREATER_EQUAL:
                return builder->CreateICmpSGE(left, right, "getmp");
            default:
                errs() << "Unknown binary operator\n";
                return nullptr;
        }
    }

    Value* generateNewExpression(NewExpression* newExpr) {
        string ctorName = newExpr->className + "_init";
        Function* ctorFunc = functions[ctorName];

        if (!ctorFunc) {
            errs() << "Unknown class: " << newExpr->className << "\n";
            return nullptr;
        }

        // Generate arguments
        vector<Value*> args;
        for (const auto& arg : newExpr->arguments) {
            args.push_back(generateExpression(arg.get()));
            if (!args.back()) return nullptr;
        }

        // Call constructor
        return builder->CreateCall(ctorFunc, args, "new_instance");
    }

    Value* generateInterpolatedString(InterpolatedString* interpStr) {
        string template_str = interpStr->template_str;
        vector<Value*> parts;

        size_t lastPos = 0;
        size_t pos = 0;

        while ((pos = template_str.find('{', lastPos)) != string::npos) {
            if (pos > lastPos) {
                string literal = template_str.substr(lastPos, pos - lastPos);
                parts.push_back(builder->CreateGlobalStringPtr(literal));
            }

            size_t end = template_str.find('}', pos);
            string varName = template_str.substr(pos + 1, end - pos - 1);

            Value* varPtr = namedValues[varName];
            if (!varPtr) {
                errs() << "Unknown variable in interpolation: " << varName << "\n";
                return nullptr;
            }

            Value* varValue = nullptr;
            if (auto* allocaInst = dyn_cast<AllocaInst>(varPtr)) {
                varValue = builder->CreateLoad(
                    allocaInst->getAllocatedType(),
                    varPtr,
                    varName.c_str()
                );
            }

            // Finde Type aus Symbol Table (müsste eigentlich auch in Expression gespeichert sein)
            shared_ptr<MyType> varType = MyType::Int(); // Default
            for (const auto& [name, type] : namedValues) {
                if (name == varName) {
                    if (auto* ai = dyn_cast<AllocaInst>(varPtr)) {
                        llvm::Type* llvmType = ai->getAllocatedType();
                        if (llvmType->isIntegerTy(32)) varType = MyType::Int();
                        else if (llvmType->isIntegerTy(1)) varType = MyType::Bool();
                        else if (llvmType->isPointerTy()) varType = MyType::String();
                    }
                    break;
                }
            }

            Value* strValue = convertToString(varValue, varType);
            parts.push_back(strValue);

            lastPos = end + 1;
        }

        if (lastPos < template_str.length()) {
            string literal = template_str.substr(lastPos);
            parts.push_back(builder->CreateGlobalStringPtr(literal));
        }

        if (parts.empty()) {
            return builder->CreateGlobalStringPtr("");
        }

        Value* result = parts[0];
        for (size_t i = 1; i < parts.size(); i++) {
            Function *strlenFunc = module->getFunction("strlen");
            Function *mallocFunc = module->getFunction("malloc");
            Function *strcpyFunc = module->getFunction("strcpy");
            Function *strcatFunc = module->getFunction("strcat");

            Value *len1 = builder->CreateCall(strlenFunc, {result});
            Value *len2 = builder->CreateCall(strlenFunc, {parts[i]});
            Value *totalLen = builder->CreateAdd(len1, len2);
            totalLen = builder->CreateAdd(totalLen, ConstantInt::get(*context, APInt(64, 1)));

            Value *newResult = builder->CreateCall(mallocFunc, {totalLen});
            builder->CreateCall(strcpyFunc, {newResult, result});
            builder->CreateCall(strcatFunc, {newResult, parts[i]});

            result = newResult;
        }

        return result;
    }

    Value *generateArrayLiteral(ArrayLiteral *arrayLit) {
        if (arrayLit->elements.empty()) {
            // Leeres Array
            Function *mallocFunc = module->getFunction("malloc");

            // Struct: {i32 length, element_type* data}
            llvm::Type *elementType = arrayLit->exprType->elementType->toLLVMType(*context);
            vector<llvm::Type *> fields = {
                llvm::Type::getInt32Ty(*context),
                PointerType::get(elementType, 0)
            };
            StructType *arrayStructType = StructType::create(*context, fields,
                                                             "array_" + arrayLit->exprType->elementType->toString());

            Value *arrayPtr = builder->CreateCall(
                mallocFunc,
                {ConstantExpr::getSizeOf(arrayStructType)},
                "empty_array"
            );
            Value *typedArrayPtr = builder->CreateBitCast(arrayPtr, PointerType::get(arrayStructType, 0));

            // Setze length = 0
            Value *lengthPtr = builder->CreateStructGEP(arrayStructType, typedArrayPtr, 0, "length_ptr");
            builder->CreateStore(ConstantInt::get(*context, APInt(32, 0)), lengthPtr);

            // Setze data = null
            Value *dataPtr = builder->CreateStructGEP(arrayStructType, typedArrayPtr, 1, "data_ptr");
            builder->CreateStore(ConstantPointerNull::get(PointerType::get(elementType, 0)), dataPtr);

            return typedArrayPtr;
        }

        // Array mit Elementen
        size_t numElements = arrayLit->elements.size();
        llvm::Type *elementType = arrayLit->elements[0]->exprType->toLLVMType(*context);

        Function *mallocFunc = module->getFunction("malloc");

        // Allokiere Speicher für die Elemente
        Value *elementSize = ConstantExpr::getSizeOf(elementType);
        Value *totalSize = builder->CreateMul(
            elementSize,
            ConstantInt::get(*context, APInt(64, numElements)),
            "total_size"
        );
        Value *dataPtr = builder->CreateCall(mallocFunc, {totalSize}, "array_data");
        Value *typedDataPtr = builder->CreateBitCast(dataPtr, PointerType::get(elementType, 0));

        // Fülle die Elemente
        for (size_t i = 0; i < numElements; i++) {
            Value *element = generateExpression(arrayLit->elements[i].get());
            Value *elementPtr = builder->CreateGEP(
                elementType,
                typedDataPtr,
                ConstantInt::get(*context, APInt(64, i)),
                "element_ptr"
            );
            builder->CreateStore(element, elementPtr);
        }

        // Erstelle Array-Struct: {i32 length, element_type* data}
        vector<llvm::Type *> fields = {
            llvm::Type::getInt32Ty(*context),
            PointerType::get(elementType, 0)
        };
        StructType *arrayStructType = StructType::create(*context, fields,
                                                         "array_" + arrayLit->exprType->elementType->toString());

        Value *arrayStructPtr = builder->CreateCall(
            mallocFunc,
            {ConstantExpr::getSizeOf(arrayStructType)},
            "array_struct"
        );
        Value *typedArrayPtr = builder->CreateBitCast(arrayStructPtr, PointerType::get(arrayStructType, 0));

        // Setze length
        Value *lengthPtr = builder->CreateStructGEP(arrayStructType, typedArrayPtr, 0, "length_ptr");
        builder->CreateStore(ConstantInt::get(*context, APInt(32, numElements)), lengthPtr);

        // Setze data pointer
        Value *dataPtrField = builder->CreateStructGEP(arrayStructType, typedArrayPtr, 1, "data_ptr");
        builder->CreateStore(typedDataPtr, dataPtrField);

        return typedArrayPtr;
    }

    Value* generateUnaryOp(UnaryOperation* unaryOp) {
        Value* operand = generateExpression(unaryOp->operand.get());
        if (!operand) return nullptr;

        switch (unaryOp->op) {
            case TokenType::MINUS:
                if (operand->getType()->isIntegerTy()) {
                    return builder->CreateNeg(operand, "negtmp");
                } else if (operand->getType()->isDoubleTy()) {
                    return builder->CreateFNeg(operand, "negtmp");
                }
                break;
            case TokenType::NOT:
                return builder->CreateNot(operand, "nottmp");
            default:
                errs() << "Unknown unary operator\n";
                return nullptr;
        }
        return nullptr;
    }

    Value *generateLogicalOp(BinaryOperation *binOp) {
        Function *function = builder->GetInsertBlock()->getParent();

        Value *left = generateExpression(binOp->left.get());
        if (!left) return nullptr;

        if (!left->getType()->isIntegerTy(1)) {
            left = builder->CreateICmpNE(
                left,
                ConstantInt::get(*context, APInt(32, 0)),
                "tobool"
            );
        }

        BasicBlock *startBB = builder->GetInsertBlock();
        BasicBlock *rightBB = BasicBlock::Create(*context, "logical_right");
        BasicBlock *mergeBB = BasicBlock::Create(*context, "logical_merge");

        if (binOp->op == TokenType::AND) {
            builder->CreateCondBr(left, rightBB, mergeBB);
        } else {
            builder->CreateCondBr(left, mergeBB, rightBB);
        }

        function->insert(function->end(), rightBB);
        builder->SetInsertPoint(rightBB);
        Value *right = generateExpression(binOp->right.get());
        if (!right) return nullptr;

        if (!right->getType()->isIntegerTy(1)) {
            right = builder->CreateICmpNE(
                right,
                ConstantInt::get(*context, APInt(32, 0)),
                "tobool"
            );
        }

        BasicBlock *rightEndBB = builder->GetInsertBlock();
        builder->CreateBr(mergeBB);

        function->insert(function->end(), mergeBB);
        builder->SetInsertPoint(mergeBB);

        PHINode *phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "logical_result");

        if (binOp->op == TokenType::AND) {
            phi->addIncoming(ConstantInt::getFalse(*context), startBB);
            phi->addIncoming(right, rightEndBB);
        } else {
            phi->addIncoming(ConstantInt::getTrue(*context), startBB);
            phi->addIncoming(right, rightEndBB);
        }

        return phi;
    }

    Value* generatePrint(FunctionCall* call) {
        Function* printfFunc = module->getFunction("printf");

        if (call->arguments.empty()) {
            errs() << "print() requires at least one argument\n";
            return nullptr;
        }

        Value* arg = generateExpression(call->arguments[0].get());
        if (!arg) return nullptr;

        shared_ptr<MyType> argType = call->arguments[0]->exprType;

        Value* formatStr = nullptr;
        vector<Value*> printfArgs;

        // Handle optional types: unwrap or print "null"
        if (argType->kind == TypeKind::Optional) {
            // Check if null
            Value* isNull = builder->CreateICmpEQ(arg, ConstantPointerNull::get(cast<PointerType>(arg->getType())));

            Function* function = builder->GetInsertBlock()->getParent();
            BasicBlock* nullBB = BasicBlock::Create(*context, "print_null");
            BasicBlock* valueBB = BasicBlock::Create(*context, "print_value");
            BasicBlock* contBB = BasicBlock::Create(*context, "print_cont");

            builder->CreateCondBr(isNull, nullBB, valueBB);

            // Null branch: print "null"
            function->insert(function->end(), nullBB);
            builder->SetInsertPoint(nullBB);
            Value* nullStr = builder->CreateGlobalStringPtr("null\n");
            builder->CreateCall(printfFunc, {nullStr});
            builder->CreateBr(contBB);

            // Value branch: unwrap and print
            function->insert(function->end(), valueBB);
            builder->SetInsertPoint(valueBB);

            // Unwrap: load from pointer
            Value* unwrapped = builder->CreateLoad(
                argType->elementType->toLLVMType(*context),
                arg,
                "unwrapped"
            );

            // Print based on inner type
            if (argType->elementType->kind == TypeKind::Int) {
                formatStr = builder->CreateGlobalStringPtr("%d\n");
                builder->CreateCall(printfFunc, {formatStr, unwrapped});
            } else if (argType->elementType->kind == TypeKind::Bool) {
                Value* boolAsInt = builder->CreateZExt(unwrapped, llvm::Type::getInt32Ty(*context));
                Value* trueStr = builder->CreateGlobalStringPtr("true");
                Value* falseStr = builder->CreateGlobalStringPtr("false");
                Value* isTrue = builder->CreateICmpNE(boolAsInt, ConstantInt::get(*context, APInt(32, 0)));
                Value* selectedStr = builder->CreateSelect(isTrue, trueStr, falseStr);
                formatStr = builder->CreateGlobalStringPtr("%s\n");
                builder->CreateCall(printfFunc, {formatStr, selectedStr});
            } else if (argType->elementType->kind == TypeKind::String) {
                // String is already a pointer, load gives us the string pointer
                formatStr = builder->CreateGlobalStringPtr("%s\n");
                builder->CreateCall(printfFunc, {formatStr, unwrapped});
            } else if (argType->kind == TypeKind::Array) {
                // Print array: [element1, element2, ...]
                Function *function = builder->GetInsertBlock()->getParent();

                // Load array struct
                llvm::Type *arrayStructType = argType->toLLVMType(*context);

                // Get length
                Value *lengthPtr = builder->CreateStructGEP(arrayStructType, arg, 0, "length_ptr");
                Value *length = builder->CreateLoad(llvm::Type::getInt32Ty(*context), lengthPtr, "length");

                // Get data pointer
                Value *dataPtrField = builder->CreateStructGEP(arrayStructType, arg, 1, "data_ptr");
                llvm::Type *elementType = argType->elementType->toLLVMType(*context);
                Value *dataPtr = builder->CreateLoad(PointerType::get(elementType, 0), dataPtrField, "data");

                // Print opening bracket
                Value *openBracket = builder->CreateGlobalStringPtr("[");
                builder->CreateCall(printfFunc, {builder->CreateGlobalStringPtr("%s"), openBracket});

                // Loop through elements
                BasicBlock *loopCondBB = BasicBlock::Create(*context, "array_loop_cond");
                BasicBlock *loopBodyBB = BasicBlock::Create(*context, "array_loop_body");
                BasicBlock *loopEndBB = BasicBlock::Create(*context, "array_loop_end");

                // Initialize counter
                AllocaInst *counterAlloca = builder->CreateAlloca(llvm::Type::getInt32Ty(*context), nullptr, "counter");
                builder->CreateStore(ConstantInt::get(*context, APInt(32, 0)), counterAlloca);
                builder->CreateBr(loopCondBB);

                // Loop condition
                function->insert(function->end(), loopCondBB);
                builder->SetInsertPoint(loopCondBB);
                Value *counter = builder->CreateLoad(llvm::Type::getInt32Ty(*context), counterAlloca, "i");
                Value *cond = builder->CreateICmpSLT(counter, length, "loop_cond");
                builder->CreateCondBr(cond, loopBodyBB, loopEndBB);

                // Loop body
                function->insert(function->end(), loopBodyBB);
                builder->SetInsertPoint(loopBodyBB);

                // Print separator if not first element
                BasicBlock *printSepBB = BasicBlock::Create(*context, "print_sep");
                BasicBlock *skipSepBB = BasicBlock::Create(*context, "skip_sep");
                Value *isFirst = builder->CreateICmpEQ(counter, ConstantInt::get(*context, APInt(32, 0)));
                builder->CreateCondBr(isFirst, skipSepBB, printSepBB);

                function->insert(function->end(), printSepBB);
                builder->SetInsertPoint(printSepBB);
                Value *separator = builder->CreateGlobalStringPtr(", ");
                builder->CreateCall(printfFunc, {builder->CreateGlobalStringPtr("%s"), separator});
                builder->CreateBr(skipSepBB);

                function->insert(function->end(), skipSepBB);
                builder->SetInsertPoint(skipSepBB);

                // Load and print element
                Value *elementPtr = builder->CreateGEP(elementType, dataPtr, counter, "element_ptr");
                Value *element = builder->CreateLoad(elementType, elementPtr, "element");

                // Print based on element type
                if (argType->elementType->kind == TypeKind::Int) {
                    Value *intFormat = builder->CreateGlobalStringPtr("%d");
                    builder->CreateCall(printfFunc, {intFormat, element});
                } else if (argType->elementType->kind == TypeKind::String) {
                    Value *strFormat = builder->CreateGlobalStringPtr("%s");
                    builder->CreateCall(printfFunc, {strFormat, element});
                } else if (argType->elementType->kind == TypeKind::Bool) {
                    Value *boolAsInt = builder->CreateZExt(element, llvm::Type::getInt32Ty(*context));
                    Value *trueStr = builder->CreateGlobalStringPtr("true");
                    Value *falseStr = builder->CreateGlobalStringPtr("false");
                    Value *isTrue = builder->CreateICmpNE(boolAsInt, ConstantInt::get(*context, APInt(32, 0)));
                    Value *selectedStr = builder->CreateSelect(isTrue, trueStr, falseStr);
                    builder->CreateCall(printfFunc, {builder->CreateGlobalStringPtr("%s"), selectedStr});
                }

                // Increment counter
                Value *nextCounter = builder->CreateAdd(counter, ConstantInt::get(*context, APInt(32, 1)));
                builder->CreateStore(nextCounter, counterAlloca);
                builder->CreateBr(loopCondBB);

                // Loop end
                function->insert(function->end(), loopEndBB);
                builder->SetInsertPoint(loopEndBB);

                // Print closing bracket and newline
                Value *closeBracket = builder->CreateGlobalStringPtr("]\n");
                builder->CreateCall(printfFunc, {builder->CreateGlobalStringPtr("%s"), closeBracket});

                return nullptr;
            }

            builder->CreateBr(contBB);

            // Continue
            function->insert(function->end(), contBB);
            builder->SetInsertPoint(contBB);

            return nullptr;
        }

        // Non-optional types (original code)
        if (argType->kind == TypeKind::Int) {
            formatStr = builder->CreateGlobalStringPtr("%d\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else if (argType->kind == TypeKind::Bool) {
            Value* boolAsInt = builder->CreateZExt(arg, llvm::Type::getInt32Ty(*context), "booltoint");
            Value* trueStr = builder->CreateGlobalStringPtr("true");
            Value* falseStr = builder->CreateGlobalStringPtr("false");

            Value* isTrue = builder->CreateICmpNE(
                boolAsInt,
                ConstantInt::get(*context, APInt(32, 0))
            );
            Value* selectedStr = builder->CreateSelect(isTrue, trueStr, falseStr);

            formatStr = builder->CreateGlobalStringPtr("%s\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(selectedStr);
        } else if (argType->kind == TypeKind::Float) {
            formatStr = builder->CreateGlobalStringPtr("%f\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else if (argType->kind == TypeKind::String) {
            formatStr = builder->CreateGlobalStringPtr("%s\n");
            printfArgs.push_back(formatStr);
            printfArgs.push_back(arg);
        } else if (argType->kind == TypeKind::Array) {
            Function* function = builder->GetInsertBlock()->getParent();

            // Wir brauchen den POINTER zur Variable, nicht den geladenen Wert
            // arg ist bereits geladen, also holen wir uns den Pointer zurück
            Value *arrayVarPtr = nullptr;

            // Wenn das Argument eine Variable ist, holen wir den Pointer direkt
            if (auto *varExpr = dynamic_cast<Variable *>(call->arguments[0].get())) {
                arrayVarPtr = namedValues[varExpr->name];
            } else {
                // Fallback: erstelle temporäre Variable
                AllocaInst *tempAlloca = builder->CreateAlloca(arg->getType(), nullptr, "temp_array");
                builder->CreateStore(arg, tempAlloca);
                arrayVarPtr = tempAlloca;
            }

            // Load the array struct pointer from the variable
            llvm::Type *elementType = argType->elementType->toLLVMType(*context);
            vector<llvm::Type *> fields = {
                llvm::Type::getInt32Ty(*context),
                PointerType::get(elementType, 0)
            };
            StructType *arrayStructType = StructType::create(*context, fields,
                                                             "array_print_" + argType->elementType->toString());

            Value *arrayStructPtr = builder->CreateLoad(
                PointerType::get(arrayStructType, 0),
                arrayVarPtr,
                "array_struct_ptr"
            );

            // Get length
            Value *lengthPtr = builder->CreateStructGEP(arrayStructType, arrayStructPtr, 0, "length_ptr");
            Value *length = builder->CreateLoad(llvm::Type::getInt32Ty(*context), lengthPtr, "length");

            // Get data pointer
            Value *dataPtrField = builder->CreateStructGEP(arrayStructType, arrayStructPtr, 1, "data_ptr");
            Value *dataPtr = builder->CreateLoad(PointerType::get(elementType, 0), dataPtrField, "data");

            // Print opening bracket
            Value *openBracket = builder->CreateGlobalStringPtr("[");
            builder->CreateCall(printfFunc, {builder->CreateGlobalStringPtr("%s"), openBracket});

            // Loop through elements
            BasicBlock *loopCondBB = BasicBlock::Create(*context, "array_loop_cond");
            BasicBlock *loopBodyBB = BasicBlock::Create(*context, "array_loop_body");
            BasicBlock *loopEndBB = BasicBlock::Create(*context, "array_loop_end");

            // Initialize counter
            AllocaInst *counterAlloca = builder->CreateAlloca(llvm::Type::getInt32Ty(*context), nullptr, "counter");
            builder->CreateStore(ConstantInt::get(*context, APInt(32, 0)), counterAlloca);
            builder->CreateBr(loopCondBB);

            // Loop condition
            function->insert(function->end(), loopCondBB);
            builder->SetInsertPoint(loopCondBB);
            Value *counter = builder->CreateLoad(llvm::Type::getInt32Ty(*context), counterAlloca, "i");
            Value *cond = builder->CreateICmpSLT(counter, length, "loop_cond");
            builder->CreateCondBr(cond, loopBodyBB, loopEndBB);

            // Loop body
            function->insert(function->end(), loopBodyBB);
            builder->SetInsertPoint(loopBodyBB);

            // Print separator if not first element
            BasicBlock *printSepBB = BasicBlock::Create(*context, "print_sep");
            BasicBlock *skipSepBB = BasicBlock::Create(*context, "skip_sep");
            Value *isFirst = builder->CreateICmpEQ(counter, ConstantInt::get(*context, APInt(32, 0)));
            builder->CreateCondBr(isFirst, skipSepBB, printSepBB);

            function->insert(function->end(), printSepBB);
            builder->SetInsertPoint(printSepBB);
            Value *separator = builder->CreateGlobalStringPtr(", ");
            builder->CreateCall(printfFunc, {builder->CreateGlobalStringPtr("%s"), separator});
            builder->CreateBr(skipSepBB);

            function->insert(function->end(), skipSepBB);
            builder->SetInsertPoint(skipSepBB);

            // Load and print element
            Value *elementPtr = builder->CreateGEP(elementType, dataPtr, counter, "element_ptr");
            Value *element = builder->CreateLoad(elementType, elementPtr, "element");

            // Print based on element type
            if (argType->elementType->kind == TypeKind::Int) {
                Value *intFormat = builder->CreateGlobalStringPtr("%d");
                builder->CreateCall(printfFunc, {intFormat, element});
            } else if (argType->elementType->kind == TypeKind::String) {
                Value *strFormat = builder->CreateGlobalStringPtr("\"%s\"");
                builder->CreateCall(printfFunc, {strFormat, element});
            } else if (argType->elementType->kind == TypeKind::Bool) {
                Value *boolAsInt = builder->CreateZExt(element, llvm::Type::getInt32Ty(*context));
                Value *trueStr = builder->CreateGlobalStringPtr("true");
                Value *falseStr = builder->CreateGlobalStringPtr("false");
                Value *isTrue = builder->CreateICmpNE(boolAsInt, ConstantInt::get(*context, APInt(32, 0)));
                Value *selectedStr = builder->CreateSelect(isTrue, trueStr, falseStr);
                builder->CreateCall(printfFunc, {builder->CreateGlobalStringPtr("%s"), selectedStr});
            }

            // Increment counter
            Value *nextCounter = builder->CreateAdd(counter, ConstantInt::get(*context, APInt(32, 1)));
            builder->CreateStore(nextCounter, counterAlloca);
            builder->CreateBr(loopCondBB);

            // Loop end
            function->insert(function->end(), loopEndBB);
            builder->SetInsertPoint(loopEndBB);

            // Print closing bracket and newline
            Value *closeBracket = builder->CreateGlobalStringPtr("]\n");
            builder->CreateCall(printfFunc, {builder->CreateGlobalStringPtr("%s"), closeBracket});

            return nullptr;
        } else {
            errs() << "Unsupported type for print(): " << argType->toString() << "\n";
            return nullptr;
        }

        return builder->CreateCall(printfFunc, printfArgs, "printcall");
    }

    Value* generateFunctionCall(FunctionCall* call) {
        if (call->name == "print") {
            return generatePrint(call);
        }

        // Generiere argument types
        vector<shared_ptr<MyType>> argTypes;
        for (const auto& arg : call->arguments) {
            argTypes.push_back(arg->exprType);
        }

        string mangledName = mangleFunctionName(call->name, argTypes);

        Function* calleeF = functions[mangledName];
        if (!calleeF) {
            errs() << "Unknown function: " << call->name
                   << " (mangled: " << mangledName << ")\n";
            return nullptr;
        }

        if (calleeF->arg_size() != call->arguments.size()) {
            errs() << "Incorrect # of arguments passed\n";
            return nullptr;
        }

        vector<Value*> args;
        for (const auto& arg : call->arguments) {
            args.push_back(generateExpression(arg.get()));
            if (!args.back()) return nullptr;
        }

        if (calleeF->getReturnType()->isVoidTy()) {
            builder->CreateCall(calleeF, args);
            return nullptr;
        }

        return builder->CreateCall(calleeF, args, "calltmp");
    }

    // VEREINFACHT: mit Type parameter!
    Value* convertToString(Value* val, shared_ptr<MyType> type) {
        if (!val || !type) return nullptr;

        if (type->kind == TypeKind::String) {
            return val;
        }

        Function* mallocFunc = module->getFunction("malloc");
        Function* sprintfFunc = module->getFunction("sprintf");

        Value* buffer = builder->CreateCall(
            mallocFunc,
            {ConstantInt::get(*context, APInt(64, 32))},
            "str_buffer"
        );

        if (type->kind == TypeKind::Int) {
            Value* format = builder->CreateGlobalStringPtr("%d");
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            return buffer;
        }
        else if (type->kind == TypeKind::Bool) {
            Value* boolAsInt = builder->CreateZExt(val, llvm::Type::getInt32Ty(*context));
            Value* trueStr = builder->CreateGlobalStringPtr("true");
            Value* falseStr = builder->CreateGlobalStringPtr("false");
            Value* isTrue = builder->CreateICmpNE(
                boolAsInt,
                ConstantInt::get(*context, APInt(32, 0))
            );
            return builder->CreateSelect(isTrue, trueStr, falseStr);
        }
        else if (type->kind == TypeKind::Float) {
            Value* format = builder->CreateGlobalStringPtr("%.2f");
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            return buffer;
        }

        errs() << "Cannot convert type to string: " << type->toString() << "\n";
        return nullptr;
    }
};

#endif //LANG_CODEGENERATOR_H