#include "cminus_builder.hpp"
#include <llvm/IR/GlobalVariable.h>
#include <iostream>
using namespace llvm;
#define CONST(num) \
  ConstantInt::get(context, APInt(32, num))  //得到常数值的表示,方便后面多次用到

Value * ret;

//store function for creating basic block
Function * currentFunc;
BasicBlock* expHandler;

void CminusBuilder::visit(syntax_program &node) {
    // program → declaration-list 
    // just visit all declarations in declaration-list
    for(auto d : node.declarations){
        d->accept(*this);
    }
    builder.ClearInsertionPoint();
    
}

void CminusBuilder::visit(syntax_num &node) { 
    // It can be intepreted as a simple expression
    // Hence just directly return its valeu via ret
    ret = CONST(node.value);
 }

void CminusBuilder::visit(syntax_var_declaration &node) {
    // var-declaration →type-specifier ID ; ∣ type-specifier ID [ NUM ] ;
    // assert var can NOT be void type
    Type* TYPE32 = Type::getInt32Ty(context);
    Type* TYPEV = Type::getVoidTy(context);

    // Note that in param declaration, it just declare a pointer
    // But in var declarasiton, it should declare a array with its len
    if(!scope.in_global()){
        if(node.num){
            //  local array
            // 创建ArrayType用于分配数组的空间
            ArrayType* arrayType = ArrayType::get(TYPE32, node.num->value);
            auto lArrayAlloc = builder.CreateAlloca(arrayType); 
            scope.push(node.id,lArrayAlloc);
        } 
        else{
            auto ldAlloc = builder.CreateAlloca(TYPE32);
            scope.push(node.id, ldAlloc);
        }
    } else {
        if(node.num){
            ArrayType* arrayType = ArrayType::get(TYPE32, node.num->value);
            ConstantAggregateZero* zeroArray = ConstantAggregateZero::get(arrayType);
            auto gArrayAlloc = new GlobalVariable(*module, arrayType, false, GlobalVariable::LinkageTypes::CommonLinkage, zeroArray);
            
            // gArrayAlloc->setInitializer(zeroNum);
            scope.push(node.id, gArrayAlloc);
        }
        else {
            // declarations without initialization value and name
            // TODO: Global variable declaration failed
            ConstantAggregateZero* zeroNum = ConstantAggregateZero::get(TYPE32);
            auto gdAlloc = new GlobalVariable(*module,TYPE32, false, GlobalVariable::LinkageTypes::ExternalLinkage, zeroNum);            
            
            // gdAlloc->setAlignment(0);
            //gdAlloc->setInitializer(zeroArray);
            scope.push(node.id, gdAlloc);
        }
    }
    
}

void CminusBuilder::visit(syntax_fun_declaration &node) {
    //In the declaration, parameter identifiers should be pushed into the callee function scope. And function identifier should be caller function scope
    scope.enter();
    Type* TYPE32 = Type::getInt32Ty(context);
    Type* TYPEV = Type::getVoidTy(context);
    Type* TYPEARRAY_32 = PointerType::getInt32PtrTy(context);
    Type * funType = node.type == TYPE_VOID ? TYPEV : TYPE32;

    // function parameters' type
    std::vector<Type *> args_type;
    // get params type
    // If params type is VOID, then assert there should not be other params and return args_type with null
    if(node.params.size() > 0){
        for(auto arg : node.params){
            if(arg->isarray){
                //TODO: Should it push pointer type or array type
                args_type.push_back(TYPEARRAY_32);
            } else if(arg->type == TYPE_INT) {
                // it must be int
                args_type.push_back(TYPE32);
            }
        }
    }
    auto funcFF = Function::Create(FunctionType::get(funType, args_type, false),GlobalValue::LinkageTypes::ExternalLinkage,node.id, module.get());
    // FIXME: check if there can be labels with same id in different scope
    currentFunc = funcFF;
    expHandler = nullptr;
    scope.exit();
    // for later unction-call
    scope.push(node.id, funcFF);
    scope.enter();
    auto funBB = BasicBlock::Create(context, "entry", funcFF);
    builder.SetInsertPoint(funBB);
    

    // Note that node.params are Syntrx Tree Node ptr, it can NOT be directly passed into function declaration list
    for(auto arg : node.params ) {
        arg.get()->accept(*this);
    }
    // Note that it should be module.get() instead of module
    
    // store parameters' value
    std::vector<Value *> args_value;
    
    for (auto arg = funcFF->arg_begin(); arg != funcFF->arg_end(); arg++){
        args_value.push_back(arg);
    }
    
    // assert node params size = args_value size
    if(node.params.size() > 0 && args_value.size() > 0){
        int i = 0;
        for (auto arg : node.params){
            auto pAlloc = scope.find(arg->id);
            if(pAlloc == nullptr){
                exit(0);
            } else {
                builder.CreateStore(args_value[i++], pAlloc);
            }
        }
    }
    if(node.compound_stmt != nullptr)
        node.compound_stmt->accept(*this);
    if(builder.GetInsertBlock()->getTerminator() == nullptr){
        if(funType == TYPEV) builder.CreateRetVoid();
        else if(funType == TYPE32) builder.CreateRet(CONST(0));
    }
    if(expHandler){
        builder.SetInsertPoint(expHandler);
        auto neg_idx_except_fun = scope.find("neg_idx_except");
        builder.CreateCall(neg_idx_except_fun);
        if(funType == TYPEV) builder.CreateRetVoid();
        else if(funType == TYPE32) builder.CreateRet(CONST(0));
    }
    scope.exit();
}

void CminusBuilder::visit(syntax_param &node) {
    // It is a declaration in the function
    // Hence, parameters cannot be in global scope
    Type* TYPE32 = Type::getInt32Ty(context);
    Type* TYPEV = Type::getVoidTy(context);
    //TODO: Should malloc or not ??? Should be array or ptr ???
    Type* TYPEARRAY_32 = PointerType::getInt32PtrTy(context);
    Value * pAlloc;
    if(node.type == TYPE_INT && !node.isarray){
        // assert that functions' parameters can NOT be global variables
        pAlloc = builder.CreateAlloca(TYPE32);
        scope.push(node.id, pAlloc);

    } else if (node.type == TYPE_INT && node.isarray){
        // array
        pAlloc = builder.CreateAlloca(TYPEARRAY_32);
        scope.push(node.id, pAlloc);
    } else{
            // void
    }
}

void CminusBuilder::visit(syntax_compound_stmt &node) {
    // accept local declarations
    // accept statementlist
    // {} 需要进入scope
    scope.enter();
    if(node.local_declarations.size() > 0){
        for(auto ld : node.local_declarations){
        // assert ld.type is INT
            ld->accept(*this);
        }
    }
    
    for(auto s : node.statement_list){
        s->accept(*this);
    }  
    // 结束时记得退出scope
    scope.exit();
}

void CminusBuilder::visit(syntax_expresion_stmt &node) {
    // expression-stmt→expression ; ∣ ;
    if(node.expression != nullptr){
        node.expression->accept(*this);
    }
}

void CminusBuilder::visit(syntax_selection_stmt &node) {
    // selection-stmt→ ​if ( expression ) statement∣ if ( expression ) statement else statement​
    node.expression->accept(*this);
    // ret = builder.CreateCast()

    Type* TYPE32 = Type::getInt32Ty(context);
    Type* TYPEARRAY_32 = PointerType::getInt32PtrTy(context);
    
    // !
    // 如果ret的类型是int32*，需要load让其变为int32
    if(ret->getType() == TYPEARRAY_32){
        ret = builder.CreateLoad(ret);
    }
    // !

    if(ret->getType() == TYPE32){
        ret = builder.CreateICmpNE(ret, ConstantInt::get(TYPE32, 0, true));
    }
    
    if(node.else_statement != nullptr){
        auto trueBranch = BasicBlock::Create(context, "trueBranch", currentFunc);
        auto falseBranch = BasicBlock::Create(context, "falseBranch", currentFunc);
        auto out = BasicBlock::Create(context, "outif");
        builder.CreateCondBr(ret,trueBranch,falseBranch);

        // tureBB
        builder.SetInsertPoint(trueBranch);
        node.if_statement->accept(*this);
        int insertedFlag = 0;
        if(builder.GetInsertBlock()->getTerminator() == nullptr){ // not returned inside the block
            insertedFlag = 1;
            out->insertInto(currentFunc);
            builder.CreateBr(out);
        }
        
        
        // falseBB
        builder.SetInsertPoint(falseBranch);
        node.else_statement->accept(*this);

        if(builder.GetInsertBlock()->getTerminator() == nullptr){ // not returned inside the block
            if (!insertedFlag){
                out->insertInto(currentFunc);
                insertedFlag = 1;
            }
            builder.CreateBr(out);
        }
        
        if(insertedFlag) builder.SetInsertPoint(out);
    }
    else{
        auto trueBranch = BasicBlock::Create(context, "trueBranch", currentFunc);
        auto out = BasicBlock::Create(context, "outif", currentFunc);
        builder.CreateCondBr(ret,trueBranch,out);
        // tureBB
        builder.SetInsertPoint(trueBranch);
        node.if_statement->accept(*this);

        if(builder.GetInsertBlock()->getTerminator() == nullptr) builder.CreateBr(out); // not returned inside the block
        
        builder.SetInsertPoint(out);
    }
}

void CminusBuilder::visit(syntax_iteration_stmt &node) {
    // iteration-stmt→while ( expression ) statement
    auto loopJudge = BasicBlock::Create(context, "loopJudge", currentFunc);
    auto loopBody = BasicBlock::Create(context, "loopBody", currentFunc);
    auto out = BasicBlock::Create(context, "outloop", currentFunc);
    Type* TYPE32 = Type::getInt32Ty(context);
    Type* TYPE1 = Type::getInt1Ty(context);
    if(builder.GetInsertBlock()->getTerminator() == nullptr) builder.CreateBr(loopJudge);
    
    builder.SetInsertPoint(loopJudge);
    node.expression->accept(*this);
    if(ret->getType() == TYPE32){
        // ret = builder.CreateIntCast(ret, Type::getInt1Ty(context), false);
        ret = builder.CreateICmpNE(ret, ConstantInt::get(TYPE32, 0, true));
    }
    // 如果ret的类型是int32*，需要load让其变为int32
    if(ret->getType() == Type::getInt32PtrTy(context)){
        ret = builder.CreateLoad(TYPE32,ret);
        ret = builder.CreateICmpNE(ret, ConstantInt::get(TYPE32, 0, true));
    }
    builder.CreateCondBr(ret, loopBody, out);

    builder.SetInsertPoint(loopBody);
    node.statement->accept(*this);
    if(builder.GetInsertBlock()->getTerminator() == nullptr) builder.CreateBr(loopJudge);

    builder.SetInsertPoint(out);
}

void CminusBuilder::visit(syntax_return_stmt &node) {
    if(node.expression == nullptr){
        builder.CreateRetVoid();
    } else {
        node.expression.get() -> accept(*this);
        Type* TYPE32 = Type::getInt32Ty(context);
        if(ret->getType() == TYPE32) builder.CreateRet(ret);
        else{
            auto retLoad = builder.CreateLoad(TYPE32, ret, "tmp");
            builder.CreateRet(retLoad);
        }
    }
}

void CminusBuilder::visit(syntax_var &node) {

    Type* TY32Ptr= PointerType::getInt32PtrTy(context);
    Type* TYPE1 = Type::getInt1Ty(context);
    Type* TYPE32 = Type::getInt32Ty(context);

    auto var = scope.find(node.id);
   
    if(var){
        // 没有expression说明： 1.普通变量 或者 2. call() 语句中的参数（数组指针或者i32）
        if(!node.expression){
            // 无论怎样，原样类型传回
            ret = var;
        }
        // 有expression，说明是数组变量
        else{
            node.expression.get()->accept(*this);
            auto num = ret;
            if (num->getType() == TY32Ptr) num = builder.CreateLoad(num); // num can be a variable, not only integer


            // 判断数组指数为负
            if(!expHandler) expHandler = BasicBlock::Create(context,"expHandler", currentFunc);
            auto normalCond = BasicBlock::Create(context, "normalCond", currentFunc);
            auto exp = builder.CreateICmpSLT(num, CONST(0));
            builder.CreateCondBr(exp, expHandler, normalCond);
            builder.SetInsertPoint(normalCond);
            /***/
            num = builder.CreateIntCast(num, Type::getInt64Ty(context),true);

            //无论怎样，都转成 ｉ32*
            Value* varLoad;

            // 如果var类型是[? x ?]* :从[? x ？]*load之后, varLoad的类型是[? x ?] 即数组
            // 如果var类型是i32** : 从var load之后， varLoad的类型是 i32*
            varLoad = builder.CreateLoad(var);

            if(varLoad -> getType()->isArrayTy()){
                auto i32Zero = CONST(0);
                Value* indices[2] = {i32Zero,i32Zero};
                varLoad = builder.CreateInBoundsGEP(var, ArrayRef<Value *>(indices, 2));
                // 现在 varLoad 类型是 i32*
            }
            // if(varLoad->getType() == TY32Ptr) {  std::cout<<"bb"<<std::endl;}
            
            varLoad = builder.CreateInBoundsGEP(varLoad,num); 
            
            // 现在ret 统一是 i32*
            ret = varLoad;
            // if(ret->getType() == TY32Ptr) {  std::cout<<"a"<<std::endl;}
        }            

    }
    else{
        exit(0);
    }
}

void CminusBuilder::visit(syntax_assign_expression &node) {
    // var = expression
    // std::cout<<"enter assign-expression"<<std::endl;
        
    Type* TY32Ptr= PointerType::getInt32PtrTy(context);
    node.var.get()->accept(*this);
    Value* var = ret; 
    // if (var->getType() == TY32Ptr) std::cout<<"aa"<<std::endl;

    node.expression.get()->accept(*this);

    if(ret->getType() == TY32Ptr) ret = builder.CreateLoad(ret);

    Type* TYPE32 = Type::getInt32Ty(context);
    Type* TYPE1 = Type::getInt1Ty(context);
    // 如果ret是通过关系运算得到的，那么其类型为int1，需要转换为int32
    if (ret->getType() == TYPE1){
        ret = builder.CreateIntCast(ret, Type::getInt32Ty(context), false);
    }

    // the value of expression is stored in ret
    builder.CreateStore(ret,var);
    // 此时 ret 的类型是 TYPE32
    
    // if(ret->getType() == TYPE32)  std::cout<<"ccc"<<std::endl;
}

void CminusBuilder::visit(syntax_simple_expression &node) {
    // simple-expression → additive-expression relop additive- expression | additive-expression

    node.additive_expression_l.get()->accept(*this);

    // 按照 simple-expression → additive-expression relop additive- expression
    if(node.additive_expression_r != nullptr){
        Type* TYPE32 = Type::getInt32Ty(context);
        Type* TY32Ptr= PointerType::getInt32PtrTy(context);
        Value* lValue;
        Value* rValue;
        
        if(ret->getType() == TY32Ptr) lValue = builder.CreateLoad(TYPE32, ret);
        else lValue = ret;

        // lValue = builder.CreateLoad(TYPE32, ret);
        // lValue = ret;
        node.additive_expression_r.get()->accept(*this);
       
        if(ret->getType() == TY32Ptr) rValue = builder.CreateLoad(TYPE32, ret);
        else rValue = ret;

        //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // 将int1类型转换为int32类型
        Type* TYPE1 = Type::getInt1Ty(context);
        if (lValue->getType() == TYPE1){
            lValue = builder.CreateIntCast(lValue, Type::getInt32Ty(context), false);
        }
        if (rValue->getType() == TYPE1){
            rValue = builder.CreateIntCast(rValue, Type::getInt32Ty(context), false);
        }
        //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        // auto rValue = ret;

        Value* icmp ;   
        switch (node.op)
        {
            case OP_LE:
                icmp = builder.CreateICmpSLE(lValue, rValue);      
                break;
            case OP_LT:
                icmp = builder.CreateICmpSLT(lValue, rValue);        
                break;
            case OP_GT:
                icmp = builder.CreateICmpSGT(lValue, rValue);        
                break;
            case OP_GE:
                icmp = builder.CreateICmpSGE(lValue, rValue);  
                break;
            case OP_EQ:
                icmp = builder.CreateICmpEQ(lValue, rValue);  
                break;
            case OP_NEQ:
                icmp = builder.CreateICmpNE(lValue, rValue); 
                break;
            default:
                break;
        }
        // 然后修改全局变量ret的值
        ret = icmp;
    }
    
}

void CminusBuilder::visit(syntax_additive_expression &node) {
    // additive-expression → additive-expression addop term | term 
    
    // 按照 additive-expression → term 
    if(node.additive_expression == nullptr){
        // It seems that it doesn't matter whether use shared-ptr.get() or not to refer to its member function
        node.term.get()->accept(*this);
    } 

    // 按照 additive-expression → additive-expression addop term 
    else {
        node.additive_expression.get()->accept(*this);
        Type* TYPE32 = Type::getInt32Ty(context);
        Type* TY32Ptr= PointerType::getInt32PtrTy(context);
        Value* lValue;

        if(ret->getType() == TY32Ptr){
            lValue = builder.CreateLoad(TYPE32, ret);
        }
        else lValue = ret;

        node.term.get()->accept(*this);
        auto rValue = ret;
        if(rValue->getType() == TY32Ptr) rValue = builder.CreateLoad(TYPE32, rValue);
        
        Type* TYPE1 = Type::getInt1Ty(context);
        if (lValue->getType() == TYPE1){
            lValue = builder.CreateIntCast(lValue, Type::getInt32Ty(context), false);
        }
        if (rValue->getType() == TYPE1){
            rValue = builder.CreateIntCast(rValue, Type::getInt32Ty(context), false);
        }
        
        Value* iAdd;
        if (node.op == OP_PLUS){
            //  builder.CreateNSWAdd 返回的type是TYPE32, 不是ptr to TYPE32
            iAdd = builder.CreateNSWAdd(lValue,rValue);
        }
        else if(node.op == OP_MINUS){
            iAdd = builder.CreateNSWSub(lValue,rValue);
        }
        
        ret = iAdd;
    }
}

void CminusBuilder::visit(syntax_term &node) {
    // term → term mulop factor | factor
    // 按照 term -> factor
    if(node.term == nullptr){
        if(node.factor != nullptr) 
            node.factor->accept(*this);
    }

    // 按照 term -> term mulop factor
    else {
        Type* TYPE32 = Type::getInt32Ty(context);
        Type* TY32Ptr= PointerType::getInt32PtrTy(context);

        node.term.get()-> accept(*this);
        Value* lValue;
        if(ret->getType() == TY32Ptr) lValue = builder.CreateLoad(TYPE32, ret, "tmp");
        else lValue = ret;
        node.factor.get()->accept(*this);
        Value* rValue;
        if(ret->getType() == TY32Ptr) rValue = builder.CreateLoad(TYPE32, ret, "tmp");
        else rValue = ret;
        Value* result;
        Type* TYPE1 = Type::getInt1Ty(context);
        if (lValue->getType() == TYPE1){
            lValue = builder.CreateIntCast(lValue, Type::getInt32Ty(context), false);
        }
        if (rValue->getType() == TYPE1){
            rValue = builder.CreateIntCast(rValue, Type::getInt32Ty(context), false);
        }


        // mulop is declared in syntax_tree.hpp
        if(node.op == OP_MUL){
            result = builder.CreateNSWMul(lValue, rValue);
        }
        else if(node.op == OP_DIV){
            // use UDIV rather than SDIV for cminus：cminus只有int
            result = builder.CreateUDiv(lValue, rValue);
        }
        ret = result;
    }
}

void CminusBuilder::visit(syntax_call &node) {
    auto fAlloc = scope.find(node.id);
    Type* TYPE32 = Type::getInt32Ty(context);
    auto TYPEARRAY = ArrayType::getInt32Ty(context);

    if(fAlloc == nullptr){
        exit(0);
    } 
    else {
        Type* TYPE1 = Type::getInt1Ty(context);
        Type* TY32Ptr= PointerType::getInt32PtrTy(context);

        std::vector<Value *> funargs;
        for(auto expr : node.args){
            expr->accept(*this);

            // std::cout<<"!"<<std::endl;
            if (ret->getType() == TYPE32){
                ret = ret;
            }
            
            else if  (ret->getType() == TYPE1){
                ret = builder.CreateIntCast(ret, Type::getInt32Ty(context), false);
            }

            // 如果 ret的类型是 i32* ,那就是传参数 int
            else if(ret->getType() == TY32Ptr){
                ret = builder.CreateLoad(ret);
            }

            // 如果ret类型是 [？ x ？]* 即指向数组的指针: call(int a[])--> 调用：call(a)，按照syntax_var原样返回的逻辑，a是[?x?]*
            else if(ret -> getType()->getPointerElementType()->isArrayTy()){
            
                auto i32Zero = CONST(0);
                Value* indices[2] = {i32Zero,i32Zero};
                ret = builder.CreateInBoundsGEP(ret, ArrayRef<Value *>(indices, 2));
                // 此时ret 类型是 i32*
                
                // if(ret->getType() == TY32Ptr) std::cout<<"bbb"<<std::endl;
            }

            // 如果ret 类型是 i32**
            else if(ret -> getType()->getPointerElementType() == TY32Ptr){
                //  std::cout<<"bbbb"<<std::endl;
                ret = builder.CreateLoad(ret);
                // 此时ret类型是 i32*
            }

            funargs.push_back(ret); 
       }
        // std::cout<<"create call and return "<<std::endl;
        ret = builder.CreateCall(fAlloc, funargs);
    }
}
