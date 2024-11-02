#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

Value* CminusfBuilder::visit(ASTProgram &node) {
    VOID_T = module->get_void_type();
    INT1_T = module->get_int1_type();
    INT32_T = module->get_int32_type();
    INT32PTR_T = module->get_int32_ptr_type();
    FLOAT_T = module->get_float_type();
    FLOATPTR_T = module->get_float_ptr_type();

    Value *ret_val = nullptr;
    for (auto &decl : node.declarations) {
        ret_val = decl->accept(*this);
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTNum &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if(node.type == TYPE_INT) 
        return CONST_INT(node.i_val);       //这里返回值应该是什么？？
    else
        return CONST_FP(node.f_val);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTVarDeclaration &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if (node.num == nullptr)//非数组类型
    {
        if(node.type == TYPE_INT) {
          //这里push进去的到底是啥？？
            if(context.func == nullptr) {
                auto initializer = ConstantZero::get(INT32_T, module.get());
                auto ialloca = GlobalVariable::create(node.id, module.get(), INT32_T, false, initializer);
                scope.push(node.id, ialloca);
            }
            else {
                auto ialloca = builder->create_alloca(INT32_T);
                scope.push(node.id, ialloca); //这里push进去的到底是啥？？
            }
        }
        else {
            if(context.func == nullptr) {
                auto initializer = ConstantZero::get(FLOAT_T, module.get());
                auto falloca = GlobalVariable::create(node.id, module.get(), FLOAT_T, false, initializer);
                scope.push(node.id, falloca);
            }
            else {
                auto falloca = builder->create_alloca(FLOAT_T);
                scope.push(node.id, falloca);
            }
        }
    }
    else {  //数组类型
        if(node.type == TYPE_INT) {
            if(context.func == nullptr) {   //全局变量
                auto *arrayType = ArrayType::get(INT32_T, node.num->i_val);
                auto initializer = ConstantZero::get(INT32_T, module.get());
                auto inalloca = GlobalVariable::create(node.id, module.get(), arrayType, false, initializer);
                scope.push(node.id, inalloca);
            }
            else {  
                auto *arrayType = ArrayType::get(INT32_T, node.num->i_val);
                auto inalloca = builder->create_alloca(arrayType);
                scope.push(node.id, inalloca);
            }
        }
        else {
            if(context.func == nullptr) {
                auto *arrayType = ArrayType::get(FLOAT_T, node.num->i_val);
                auto initializer = ConstantZero::get(FLOAT_T, module.get());
                auto fnalloca = GlobalVariable::create(node.id, module.get(), arrayType, false, initializer);
                scope.push(node.id, fnalloca);
            }
            else {
                auto *arrayType = ArrayType::get(FLOAT_T, node.num->i_val);
                auto fnalloca = builder->create_alloca(arrayType);
                scope.push(node.id, fnalloca);
            }
        }
    }
    
    return nullptr;
}

Value* CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params) {
        // TODO: Please accomplish param_types.
        if(param->isarray) {//如果参数是数组
            if(param->type == TYPE_INT)
                param_types.push_back(INT32PTR_T);
            else
                param_types.push_back(FLOATPTR_T);
        }  
        else {
            if(param->type == TYPE_INT)
                param_types.push_back(INT32_T);
            else
                param_types.push_back(FLOAT_T);
        }
    }
//    for(int i=0; i< param_types.size(); i++)
//        context.param_type[i] == param_types[i];

    fun_type = FunctionType::get(ret_type, param_types);
    auto func = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, func);
    context.func = func;
    auto funBB = BasicBlock::create(module.get(), "entry", func);
    builder->set_insert_point(funBB);
  //  context.now_bb = funBB; //需要吗？？
    scope.enter();
    std::vector<Value *> args;
    for (auto &arg : func->get_args()) {
        args.push_back(&arg);
    }
    for (int i = 0; i < node.params.size(); ++i) {
        // TODO: You need to deal with params and store them in the scope.
        node.params[i]->accept(*this);
        auto addr = scope.find(node.params[i]->id);
        builder->create_store(args[i], addr);
    }
    node.compound_stmt->accept(*this);
    if (not builder->get_insert_block()->is_terminated())
    {
        if (context.func->get_return_type()->is_void_type())
            builder->create_void_ret(); //这里或许能看出来参数context里加啥？？ 
        else if (context.func->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTParam &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if(node.isarray) {      //是数组
        if(node.type == TYPE_INT) {
            
            auto inAlloca = builder->create_alloca(INT32PTR_T);
            scope.push(node.id, inAlloca);
        }
        else {
            auto fnAlloca = builder->create_alloca(FLOATPTR_T);
            scope.push(node.id, fnAlloca);
        }
        
    }
    else {
        if(node.type == TYPE_INT) {
            auto iAlloca = builder->create_alloca(INT32_T);
            scope.push(node.id, iAlloca);
        }
        else {
            auto fAlloca = builder->create_alloca(FLOAT_T);
            scope.push(node.id, fAlloca);
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCompoundStmt &node) {
    // TODO: This function is not complete.
    // You may need to add some code here
    // to deal with complex statements.
    scope.enter();
    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->is_terminated())//这个是啥作用
            break;
    }
//    builder->create_void_ret();
    scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTExpressionStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    node.expression->accept(*this);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSelectionStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    //auto originBB = context.now_bb;
    auto cond = node.expression->accept(*this);
    Value* trans_cond;
    if(cond->get_type() == INT32_T) {
        trans_cond = builder->create_icmp_gt(cond, CONST_INT(0));
    }
    else if(cond->get_type() == FLOAT_T) {
        trans_cond = builder->create_fcmp_gt(cond, CONST_FP(0));
    }

//    auto cond_int = static_cast<int>(cond);
    auto newBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
    auto trueBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
    if(node.else_statement == nullptr) {       //只有if语句
        builder->create_cond_br(trans_cond, trueBB, newBB);
        builder->set_insert_point(trueBB);
        //context.now_bb = trueBB;
        node.if_statement->accept(*this);
        if(builder->get_insert_block()->is_terminated()) {//已在if语句中有返回值
            builder->set_insert_point(newBB);
        }
        else {
            builder->create_br(newBB);
            builder->set_insert_point(newBB);
        }
        //这里应该跳转到那个block？
    }
    else {//有else语句
        auto falseBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
        builder->create_cond_br(trans_cond, trueBB, falseBB);
        builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);
        if(builder->get_insert_block()->is_terminated()) {//已在if语句中有返回值
            builder->set_insert_point(newBB);
        }
        else {
            builder->create_br(newBB);
            builder->set_insert_point(newBB);
        }
        builder->set_insert_point(falseBB);
        //context.now_bb = falseBB;
        node.else_statement->accept(*this);
        //context.now_bb = originBB;
        if(builder->get_insert_block()->is_terminated()) {//已在if语句中有返回值
            builder->set_insert_point(newBB);
        }
        else {
            builder->create_br(newBB);
            builder->set_insert_point(newBB);
        }
    }


    return nullptr;
}

Value* CminusfBuilder::visit(ASTIterationStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    auto relationBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
    auto relopBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
    auto retBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
    
    //auto originBB = context.now_bb;
    builder->create_br(relationBB);

    builder->set_insert_point(relationBB);
    //context.now_bb = relationBB;
    auto cond = node.expression->accept(*this);
    Value* trans_cond;
    if(cond->get_type() == INT32_T) {
        trans_cond = builder->create_icmp_gt(cond, CONST_INT(0));
    }
    else if(cond->get_type() == FLOAT_T) {
        trans_cond = builder->create_fcmp_gt(cond, CONST_FP(0));
    }
//    auto cond_int = static_cast<int>(cond);
    builder->create_cond_br(trans_cond, relopBB, retBB);

    builder->set_insert_point(relopBB);
    //context.now_bb = relopBB;
    node.statement->accept(*this);
    if(!builder->get_insert_block()->is_terminated()) {
        builder->create_br(relationBB);
    }
    builder->set_insert_point(retBB);
//    builder->create_ret(a);
    //context.now_bb = originBB;
//    builder->create_br(context.now_bb);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
        return nullptr;
    } else {
        // TODO: The given code is incomplete.
        // You need to solve other return cases (e.g. return an integer).
        auto ret = node.expression->accept(*this);
        
        auto expected_type = builder->get_insert_block()->get_parent()->get_return_type();
        if(ret->get_type() != expected_type) {    //返回类型转化
            if(ret->get_type() == INT32_T ) {
                if(expected_type == FLOAT_T) {
                    auto trans_ret = builder->create_sitofp(ret, FLOAT_T);
                    builder->create_ret(trans_ret);
                    return trans_ret;
                }
            }
            else if(ret->get_type() == FLOAT_T){
                if(expected_type == INT32_T) {
                    auto trans_ret = builder->create_fptosi(ret, INT32_T);
                    builder->create_ret(trans_ret);
                    return trans_ret;
                }
            } 
            else {  //参数类型为bool
                if(expected_type == INT32_T) {
                    auto trans_ret = builder->create_zext(ret, INT32_T);
                    builder->create_ret(trans_ret);
                    return trans_ret;
                }
                else if(expected_type == FLOAT_T){
                    auto trans_ret = builder->create_zext(ret, FLOAT_T);
                    builder->create_ret(trans_ret);
                    return trans_ret;
                }
            }   
        }
        else {
            builder->create_ret(ret);
            return ret;
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTVar &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if(node.expression == nullptr) {//非数组元素
        auto addr = scope.find(node.id);
        //return builder->create_load(addr);
       /* if(addr->get_type()->is_array_type()) //若某函数调用的实参是数组名称
            return &addr;*/
        return addr;
    }
    else {//数组元素
        auto idx = node.expression->accept(*this);
        ICmpInst *icmp;
        if(idx->get_type() == FLOAT_T) {
            auto idx_trans = builder->create_fptosi(idx, INT32_T);
            icmp = builder->create_icmp_ge(idx_trans, CONST_INT(0));
        }
        else
            icmp = builder->create_icmp_ge(idx, CONST_INT(0));

        auto newBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
        auto trueBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
        auto falseBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
        builder->create_cond_br(icmp, trueBB, falseBB);
        builder->set_insert_point(trueBB);
        builder->create_br(newBB);
        builder->set_insert_point(falseBB);
        builder->create_call(scope.find("neg_idx_except"),{} );
        builder->create_br(newBB);

        builder->set_insert_point(newBB);
        //context.now_bb = newBB;
        auto addr = scope.find(node.id);
        std::cout << addr->get_type()->print() << std::endl;
        if(idx->get_type() == FLOAT_T) {
            if(addr->get_type()->get_pointer_element_type() == INT32PTR_T ||
            addr->get_type()->get_pointer_element_type() == FLOATPTR_T) {//函数参数是数组时
                auto idx_trans = builder->create_fptosi(idx, INT32_T);
                auto addr_trans = builder->create_load(addr);   //将参数转化为数组类型
                auto ADDR = builder->create_gep(addr_trans, {/*CONST_FP(0), */idx_trans});
                return ADDR;
            }   
            auto idx_trans = builder->create_fptosi(idx, INT32_T);
            auto ADDR = builder->create_gep(addr, {CONST_INT(0),idx_trans});
            //std::cout << ADDR->print() <<std::endl;
            //return builder->create_load(ADDR);
            return ADDR;
        }
        
        if(addr->get_type()->get_pointer_element_type() == INT32PTR_T ||
            addr->get_type()->get_pointer_element_type() == FLOATPTR_T) {//addr不是数组类型，也就是当var是函数数组类型形参时
            auto addr_trans = builder->create_load(addr);   //将参数转化为数组类型
            //builder->create_alloca
            auto ADDR = builder->create_gep(addr_trans, {/*CONST_INT(0), */idx});
            return ADDR;
        }
        auto ADDR = builder->create_gep(addr, {CONST_INT(0), idx});
       // std::cout << ADDR->print() <<std::endl;
        //return builder->create_load(ADDR);
        return ADDR;
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAssignExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
/*    Value *addr;
    if(node.var->expression == nullptr) //非数组元素
        addr = scope.find(node.var->id);
    else {
        auto idx = node.expression->accept(*this);
        ICmpInst *icmp;
        if(idx->get_type() == FLOAT_T) {
            auto idx_trans = builder->create_fptosi(idx, INT32_T);
            icmp = builder->create_icmp_ge(idx_trans, CONST_INT(0));
        }
        else
            icmp = builder->create_icmp_ge(idx, CONST_INT(0));

        auto trueBB = BasicBlock::create(module.get(), "trueBB", context.func);
        auto falseBB = BasicBlock::create(module.get(), "falseBB", context.func);
        builder->create_cond_br(icmp, trueBB, falseBB);
        builder->set_insert_point(trueBB);
        builder->create_br(context.now_bb);
        builder->set_insert_point(falseBB);
        builder->create_call(scope.find("neg_idx_except"),{} );
        builder->create_br(context.now_bb);

        auto addr = scope.find(node.id);
        if(idx->get_type() == FLOAT_T) {
            auto idx_trans = builder->create_fptosi(idx, INT32_T);
            auto ADDR = builder->create_gep(addr, {idx_trans});
            return builder->create_load(ADDR);
        }
        auto ADDR = builder->create_gep(addr, {idx});
        return builder->create_load(ADDR);
    }*/
    auto addr = node.var->accept(*this);
    auto value = node.expression->accept(*this);
    if(value->get_type() == INT32_T && addr->get_type() == FLOATPTR_T) {
        auto value_trans = builder->create_sitofp(value, FLOAT_T);
        return builder->create_store(value_trans, addr);
    }
    else if (value->get_type() == FLOAT_T && addr->get_type() == INT32PTR_T) {
        auto value_trans = builder->create_fptosi(value, INT32_T);
        return builder->create_store(value_trans, addr);
    }
    builder->create_store(value, addr);
    return value;
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSimpleExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if(node.additive_expression_r == nullptr) {
        return node.additive_expression_l->accept(*this);
    }
    else {
        auto ret1 = node.additive_expression_l->accept(*this);
        auto ret2 = node.additive_expression_r->accept(*this);
        if(ret1->get_type() != FLOAT_T && ret2->get_type() != FLOAT_T) {
            ICmpInst *icmp;
            switch (node.op)
            {
            case OP_LE: 
                icmp = builder->create_icmp_le(ret1, ret2);
                break;
            case OP_LT:
                icmp = builder->create_icmp_lt(ret1, ret2);
                break;
            case OP_GT:
                icmp = builder->create_icmp_gt(ret1, ret2);
                break;
            case OP_GE:
                icmp = builder->create_icmp_ge(ret1, ret2);
                break;
            case OP_EQ:
                icmp = builder->create_icmp_eq(ret1, ret2);
                break;
            case OP_NEQ:
                icmp = builder->create_icmp_ne(ret1, ret2);
                break;
            default:
                break;
            }
            auto icmp_ret = builder->create_zext(icmp, INT32_T);
            return icmp_ret;
        }
        else if(ret1->get_type() == FLOAT_T && ret2->get_type() == FLOAT_T){
            FCmpInst* fcmp;
            switch (node.op)
            {
            case OP_LE: 
                fcmp = builder->create_fcmp_le(ret1, ret2);
                break;
            case OP_LT:
                fcmp = builder->create_fcmp_lt(ret1, ret2);
                break;
            case OP_GT:
                fcmp = builder->create_fcmp_gt(ret1, ret2);
                break;
            case OP_GE:
                fcmp = builder->create_fcmp_ge(ret1, ret2);
                break;
            case OP_EQ:
                fcmp = builder->create_fcmp_eq(ret1, ret2);
                break;
            case OP_NEQ:
                fcmp = builder->create_fcmp_ne(ret1, ret2);
                break;
            default:
                break;
            }
            auto fcmp_ret = builder->create_zext(fcmp, INT32_T);
            return fcmp_ret;
        }
        else {
            FCmpInst* fcmp;
            if(ret1->get_type() != FLOAT_T) {
                auto ret1_trans = builder->create_sitofp(ret1,FLOAT_T);
                switch (node.op)
            {
            case OP_LE: 
                fcmp = builder->create_fcmp_le(ret1_trans, ret2);
                break;
            case OP_LT:
                fcmp = builder->create_fcmp_lt(ret1_trans, ret2);
                break;
            case OP_GT:
                fcmp = builder->create_fcmp_gt(ret1_trans, ret2);
                break;
            case OP_GE:
                fcmp = builder->create_fcmp_ge(ret1_trans, ret2);
                break;
            case OP_EQ:
                fcmp = builder->create_fcmp_eq(ret1_trans, ret2);
                break;
            case OP_NEQ:
                fcmp = builder->create_fcmp_ne(ret1_trans, ret2);
                break;
            default:
                break;
            }
            auto fcmp_ret = builder->create_zext(fcmp, INT32_T);
            return fcmp_ret;
            }
            else {  //ret2为整形
                auto re2_trans = builder->create_sitofp(ret2,FLOAT_T);
                switch (node.op)
            {
            case OP_LE: 
                fcmp = builder->create_fcmp_le(ret1, re2_trans);
                break;
            case OP_LT:
                fcmp = builder->create_fcmp_lt(ret1, re2_trans);
                break;
            case OP_GT:
                fcmp = builder->create_fcmp_gt(ret1, re2_trans);
                break;
            case OP_GE:
                fcmp = builder->create_fcmp_ge(ret1, re2_trans);
                break;
            case OP_EQ:
                fcmp = builder->create_fcmp_eq(ret1, re2_trans);
                break;
            case OP_NEQ:
                fcmp = builder->create_fcmp_ne(ret1, re2_trans);
                break;
            default:
                break;
            }
            auto fcmp_ret = builder->create_zext(fcmp, INT32_T);
            return fcmp_ret;
            }
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAdditiveExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    auto op2 = node.term->accept(*this);
    if(node.additive_expression == nullptr) {
        return op2;
    }
    else {
        auto op1 = node.additive_expression->accept(*this);
        switch (node.op)
        {
        case OP_PLUS:
            if(op1->get_type() == INT32_T && op2->get_type() == INT32_T)
                return builder->create_iadd(op1, op2);
            else if(op1->get_type() == FLOAT_T && op2->get_type() == FLOAT_T)
                return builder->create_fadd(op1, op2);
            else {
                if(op1->get_type() == INT32_T) {
                    auto trans_op1 = builder->create_sitofp(op1, FLOAT_T);
                    return builder->create_fadd(trans_op1, op2);
                }
                if(op2->get_type() == INT32_T) {
                    auto trans_op2 = builder->create_sitofp(op2, FLOAT_T);
                    return builder->create_fadd(op1, trans_op2);
                }
                
            }
            break;
        case OP_MINUS:
            if(op1->get_type() == INT32_T && op2->get_type() == INT32_T)
                return builder->create_isub(op1, op2);
            else if(op1->get_type() == FLOAT_T && op2->get_type() == FLOAT_T)
                return builder->create_fsub(op1, op2);
            else {
                if(op1->get_type() == INT32_T) {
                    auto trans_op1 = builder->create_sitofp(op1, FLOAT_T);
                    return builder->create_fsub(trans_op1, op2);
                }
                if(op2->get_type() == INT32_T) {
                    auto trans_op2 = builder->create_sitofp(op2, FLOAT_T);
                    return builder->create_fsub(op1, trans_op2);
                }
                
            }
            break;
        default:
            break;
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTTerm &node) {
    // TODO: This function is empty now.
    // Add some code here.
    auto op2 = node.factor->accept(*this);
    if(node.term == nullptr) {
        if(op2->get_type()->is_pointer_type()) {
            if(op2->get_type()->get_pointer_element_type()->is_array_type())  {
            //若是数组类型 ,对应f(a),a是数组名，且a为局部变量
                return builder->create_gep(op2, {CONST_INT(0), CONST_INT(0)});
            }
            else if(op2->get_type()->get_pointer_element_type()->is_pointer_type()) {
            //若是指针类型 ,对应f(a),a是数组名，且a为形参
                return builder->create_load(op2);
            }
            //这里是不是要再讨论一个元素是int类型的？对应形参是整数
        }
        
        /*else*/ if(op2->get_type() == INT32PTR_T || op2->get_type() == FLOATPTR_T) { //var {}
            //if(!op2->get_type()->get_pointer_element_type()->is_array_type()) //若不是数组类型
                return builder->create_load(op2);
        }
        return op2;
    }
    else {      //term为有两个操作数的运算表达式时
        auto op1 = node.term->accept(*this);
        //Value* op2_trans;
        if(op2->get_type()->is_pointer_type()) {
            if(op2->get_type()->get_pointer_element_type()->is_array_type())  {
            //若是数组类型 ,对应f(a),a是数组名，且a为局部变量
                op2 = builder->create_gep(op2, {CONST_INT(0), CONST_INT(0)});
            }
            else if(op2->get_type()->get_pointer_element_type()->is_pointer_type()) {
            //若是指针类型 ,对应f(a),a是数组名，且a为形参
                op2= builder->create_load(op2);
            }
            //这里是不是要再讨论一个元素是int类型的？对应形参是整数
        }
        if(op2->get_type() == INT32PTR_T || op2->get_type() == FLOATPTR_T) { //var {}
            //if(!op2->get_type()->get_pointer_element_type()->is_array_type()) //若不是数组类型
            op2 = builder->create_load(op2);
        }

        switch (node.op)
        {
        case OP_MUL:
            if(op1->get_type() == INT32_T && op2->get_type() == INT32_T)
                return builder->create_imul(op1, op2);
            else if(op1->get_type() == FLOAT_T && op2->get_type() == FLOAT_T)
                return builder->create_fmul(op1, op2);
            else {
                if(op1->get_type() == INT32_T) {
                    auto trans_op1 = builder->create_sitofp(op1, FLOAT_T);
                    return builder->create_fmul(trans_op1, op2);
                }
                if(op2->get_type() == INT32_T) {
                    auto trans_op2 = builder->create_sitofp(op2, FLOAT_T);
                    return builder->create_fmul(op1, trans_op2);
                }
                
            }
            break;
        case OP_DIV:
            if(op1->get_type() == INT32_T && op2->get_type() == INT32_T)
                return builder->create_isdiv(op1, op2);
            else if(op1->get_type() == FLOAT_T && op2->get_type() == FLOAT_T)
                return builder->create_fdiv(op1, op2);
            else {
                if(op1->get_type() == INT32_T) {
                    auto trans_op1 = builder->create_sitofp(op1, FLOAT_T);
                    return builder->create_fdiv(trans_op1, op2);
                }
                if(op2->get_type() == INT32_T) {
                    auto trans_op2 = builder->create_sitofp(op2, FLOAT_T);
                    return builder->create_fdiv(op1, trans_op2);
                }
                
            }
            break;
        default:
            break;
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCall &node) {
    // TODO: This function is empty now.
    // Add some code here.
    std::vector<Value *> args;
    auto funaddr = scope.find(node.id);
    //auto arg_begin = funtype->param_begin();
    auto funADDR = static_cast<Function*>(funaddr);
    auto funtype = funADDR->get_function_type();
    for (int i = 0; i < node.args.size(); ++i) {
        auto arg = node.args[i]->accept(*this);
        if(arg->get_type() != funtype->get_param_type(i)) {  //强制类型转化？？
            //auto printty = arg->get_type();
            //std::cout<< printty->print() <<std::endl;
            if(arg->get_type() == INT32_T ) {
                if(funtype->get_param_type(i) == FLOAT_T) {
                    auto trans_arg = builder->create_sitofp(arg, FLOAT_T);
                    args.push_back(trans_arg);
                }
                else if(funtype->get_param_type(i) == INT1_T) {
                    auto trans_arg = builder->create_icmp_gt(arg, CONST_INT(0));
                    args.push_back(trans_arg);
                }
            }
            else if(arg->get_type() == FLOAT_T){
                if(funtype->get_param_type(i) == INT32_T) {
                    auto trans_arg = builder->create_fptosi(arg, INT32_T);
                    args.push_back(trans_arg);
                }
                else if(funtype->get_param_type(i) == INT1_T){
                    auto trans_arg = builder->create_fcmp_gt(arg, CONST_FP(0));
                    args.push_back(trans_arg);
                }
            } 
            else {  //参数类型为bool
                if(funtype->get_param_type(i) == INT32_T) {
                    auto trans_arg = builder->create_zext(arg, INT32_T);
                    args.push_back(trans_arg);
                }
                else if(funtype->get_param_type(i) == FLOAT_T){
                    auto trans_arg = builder->create_zext(arg, FLOAT_T);
                    args.push_back(trans_arg);
                }
            }   
        }
        else
            args.push_back(arg);
    }
    auto funid = scope.find(node.id);
    auto call = builder->create_call(funid, args);
/*    auto ret_type = call->get_type();
    if(ret_type != funtype->get_return_type()) {    //返回类型转化
            if(ret_type == INT32_T ) {
                if(funtype->get_return_type() == FLOAT_T) {
                    auto trans_ret = builder->create_sitofp(call, FLOAT_T);
                    return trans_ret;
                }
            }
            else if(ret_type == FLOAT_T){
                if(funtype->get_return_type() == INT32_T) {
                    auto trans_ret = builder->create_fptosi(call, INT32_T);
                    return trans_ret;
                }
            } 
            else {  //参数类型为bool
                if(funtype->get_return_type() == INT32_T) {
                    auto trans_ret = builder->create_zext(call, INT32_T);
                    return trans_ret;
                }
                else if(funtype->get_return_type() == FLOAT_T){
                    auto trans_ret = builder->create_zext(call, FLOAT_T);
                    return trans_ret;
                }
            }   
    }*/
    return call;
    return nullptr;
}
