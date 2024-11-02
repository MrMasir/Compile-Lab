#include "Mem2Reg.hpp"
#include "IRBuilder.hpp"
#include "Value.hpp"

#include <memory>

void Mem2Reg::run() {
    // 创建支配树分析 Pass 的实例
    dominators_ = std::make_unique<Dominators>(m_);
    // 建立支配树
    dominators_->run();
 /*   if(!m_->get_global_variable().empty()) {
        for(auto &global1 : m_->get_global_variable()) {
            auto global = &global1;
        }
    }*/
    // 以函数为单元遍历实现 Mem2Reg 算法
    for (auto &f : m_->get_functions()) {
        if (f.is_declaration())
            continue;
        func_ = &f;
        if (func_->get_basic_blocks().size() >= 1) {
            // 对应伪代码中 phi 指令插入的阶段
            generate_phi();
            // 对应伪代码中重命名阶段
            rename(func_->get_entry_block());
        }
        // 后续 DeadCode 将移除冗余的局部变量的分配空间
    }
    //创建死代码删除Pass的实例,这部分应该在哪里实现？
    deadcode_ = std::make_unique<DeadCode>(m_);
    deadcode_->run();
}

void Mem2Reg::generate_phi() {  //这个函数不需要传递参数吗？？
    // TODO:
    // 步骤一：找到活跃在多个 block 的全局名字集合，以及它们所属的 bb 块
    // 步骤二：从支配树获取支配边界信息，并在对应位置插入 phi 指令

    std::set<Value *> global_name;  //存放每个活跃在多个块的变量
    std::map<Value *, BasicBlock*> var_name; //存放每个变量
//    std::map<Value *, Value *> array_param;  //存放每个为数组形参alloca出的内存变量
//    std::set<Value*> gep_addr;  //存放每个gep指令计算出的地址
//    std::set<Value*> useful_load;   //存放所有有用的load指令
    for (auto &bb1 : func_->get_basic_blocks()) {//遍历每个基本块的每条指令
        auto bb = &bb1;
        for(auto &inst1 : bb->get_instructions()) {
            auto inst = &inst1;
            if(inst->is_alloca()) {  //若为alloca
                auto allocaInst = static_cast<AllocaInst*> (inst);
                if(allocaInst->get_alloca_type()->is_array_type()) {
                    if(alloca_name.find(inst) == alloca_name.end()) {   //若此时没有这个数组
                        alloca_name.insert({inst, {}}); //初始化关于这个数组的map
                    }
                }
                else {
                    var_name.insert({inst, bb});  //将该内存变量放入变量表中
                    Stack.insert({inst, {}});  //为每个内存变量创建一个栈
                }
            }
            else if(inst->is_load()) {
                if(var_name.find(inst->get_operand(0)) != var_name.end()) {
                    if(bb != var_name.at(inst->get_operand(0))) {   //若当前块不是该内存变量的定义块，说明该变量是多个块中活跃的
                        if(global_name.find(inst->get_operand(0)) == global_name.end()) {
                            global_name.insert(inst->get_operand(0));
                        }
                    }
                }
        /*        if(array_param.find(inst->get_operand(0)) != array_param.end()) {   //如果load的是记录形参数组的内存变量
                    inst->replace_all_use_with(inst->get_operand(0));
                }*/
            }
/*            else if(inst->is_gep()) {   //若为gep指令
                Value *idx;
                if(inst->get_num_operand() == 3)  {  //计算数组元素的地址
                    idx = inst->get_operand(2);  //先获得数组元素偏移量
                    if(alloca_name[inst->get_operand(0)].find(idx) == alloca_name[inst->get_operand(0)].end()) {    //是对于该数组元素的第一次定义
                        alloca_name[inst->get_operand(0)].insert({idx, inst});
                        var_name.insert({inst, bb});
                        Stack.insert({inst, {}});
                    }
                    else {  //之前已经出现过关于该数组元素的定义
                        auto new_val = alloca_name[inst->get_operand(0)][idx];
                        inst->replace_all_use_with(new_val);
                    }
                }
                else {  //数组作为函数参数传递进来时
                    idx = inst->get_operand(1);
                    auto array_addr = array_param[inst->get_operand(0)];
                /*    if(alloca_name[array_addr].find(idx) == alloca_name.end()) {   //若此时数组元素并未找到
                        alloca_name[array_addr].insert({idx, inst});
                        var_name.insert({inst, bb});
                        Stack.insert({inst, {}});
                    }
                    else {  //之前已经出现过该数组元素的定义
                        auto new_val = alloca_name[array_addr][idx];
                        inst->replace_all_use_with(new_val);
                    }
                }

            }*/
    /*        else if(inst->is_store()) {
                auto store_val = inst->get_operand(0);
                if(alloca_name.find(store_val) != alloca_name.end()) {  //若该store指令存放的是形参数组的地址
                    auto array_param_alloca = inst->get_operand(1);
                    array_param.insert({array_param_alloca, store_val});//将该内存变量,以及所记录的数组地址存储
                }
            }*/
            //load，store指令取到的内存地址操作数和alloca指令的返回值一样吗？
        }
    }
    using  BBset = std::set<BasicBlock *>;
    std::map<Value *, BBset> var_defined;   //这部分是要找到每个活跃变量的所有定义块
    for (auto &bb1 : func_->get_basic_blocks()) {  
        auto bb = &bb1;
        for(auto &inst1 : bb->get_instructions()) {
            auto inst = &inst1;
            if(inst->is_store()) {
                auto var_addr = inst->get_operand(1);
                if(global_name.find(var_addr) != global_name.end()) {   //若该变量时活跃在多个块的内存变量
                    if(var_defined.find(var_addr) == var_defined.end()) {//第一次定义
                        var_defined.insert({var_addr, {bb}});
                    }
                    else {  //第n次定义
                        var_defined[var_addr].insert(bb);   
                    }
                }
            }
        }
    }
    for(auto &var : global_name) {  //遍历每个活跃变量，插入phi函数
        //auto var = &var1;
        std::set<BasicBlock *> phi_insert;  //表示关于某变量已插入过phi函数的节点集合
    //    std::set<BasicBlock *> var_defined;
        auto node_list = var_defined.at(var);
        while(!node_list.empty()) { 
            auto insert_node = *node_list.begin();//获得一个定义过变量的基本块
            node_list.erase(insert_node);   //获得一个定义块并移除
            auto node_frontier = dominators_->get_dominance_frontier(insert_node);
            for(auto frontier_bb : node_frontier) {
                //auto frontier_bb = &frontier_bb1;
                if(phi_insert.find(frontier_bb) == phi_insert.end()) {  //该边界块未插入过phi函数
                  //  IRBuilder::set_insert_point(frontier_bb);//这里需要吗？如何保证将phi函数插在块开头？
                    auto phi_inst = PhiInst::create_phi(var->get_type(), frontier_bb);  //插入phi函数，这里的参数该怎么写？后两参数具有默认值{}
                    frontier_bb->add_instr_begin(phi_inst); //使用这个指令将phi插在块开头
                    phi_insert.insert(frontier_bb);
                    phi_var.insert({phi_inst, var}); //记录该phi指令是关于哪个内存变量的
                    if(node_list.find(frontier_bb) == node_list.end()) {
                        node_list.insert(frontier_bb);  //若该块原来不在对变量的定义块集合中，加入
                    }
                }
            }
        }
    }
    
}

void Mem2Reg::rename(BasicBlock *bb) {
    // TODO:
    // 步骤三：将 phi 指令作为 lval 的最新定值，lval 即是为局部变量 alloca 出的地址空间
    // 步骤四：用 lval 最新的定值替代对应的load指令
    // 步骤五：将 store 指令的 rval，也即被存入内存的值，作为 lval 的最新定值
    // 步骤六：为 lval 对应的 phi 指令参数补充完整
    // 步骤七：对 bb 在支配树上的所有后继节点，递归执行 re_name 操作
    // 步骤八：pop出 lval 的最新定值
    // 步骤九：清除冗余的指令
    std::unordered_set<Instruction *> del_inst{};
    std::set<Value*> gep_addr;  //存放每个gep指令计算出的地址
    std::set<Value*> useful_load;   //存放所有有用的load指令
    std::set<Value*> useful_store;  //存放每个有用的store指令

    for(auto &inst1 : bb->get_instructions()) { //遍历所有指令，找出与gep指令相关的load，store指令并记录
        auto inst = &inst1;
        if(inst->is_gep()) {
            gep_addr.insert(inst);
        }
        else if(inst->is_load()) {
            auto load_addr = inst->get_operand(0);
            if(gep_addr.find(load_addr) != gep_addr.end()) {
                useful_load.insert(inst);
            }
            auto load_addr_global = dynamic_cast<GlobalVariable*> (inst->get_operand(0));
            if(load_addr_global != nullptr) {
                useful_load.insert(inst);
            }
        }
        else if(inst->is_store()) {
            auto store_addr = inst->get_operand(1);
            if(gep_addr.find(store_addr) != gep_addr.end()) {
                useful_store.insert(inst);
            }
            auto store_val_global = dynamic_cast<GlobalVariable*> (inst->get_operand(0));
            auto store_addr_global = dynamic_cast<GlobalVariable*> (inst->get_operand(1));
            if(store_val_global != nullptr || store_addr_global != nullptr) {
                useful_store.insert(inst);
            }
        }
    }

    for(auto &inst1 : bb->get_instructions()) {
        auto inst = &inst1;
        if(inst->is_phi()) {
            auto var = phi_var.at(inst);
            auto phi_x = static_cast<Value*> (inst);
            Stack[var].push(phi_x); 
        }
        else if(inst->is_store()) {
            if(useful_store.find(inst) == useful_store.end()) { //若不属于有用store
                auto var = inst->get_operand(1);
                Stack[var].push(inst->get_operand(0));
                del_inst.insert(inst);
            }
        }
        else if(inst->is_load()) {
            if(useful_load.find(inst) == useful_load.end()) {
                auto var = inst->get_operand(0);
                auto new_val = inst;
                new_val->replace_all_use_with(Stack[var].top());//这里的替换是使用这个函数吗？
            }
        }
    }

    auto final_inst = bb->get_terminator();
    Value* ret_value;
    Value* cond;
    BasicBlock* label1;
    BasicBlock* label2;
    int type;
    if(final_inst->is_ret()) {
        auto ret_inst = static_cast<ReturnInst*>(final_inst);
        if(ret_inst->is_void_ret()) {
            type = 3;
        }
        else {
            ret_value = final_inst->get_operand(0);
            auto x_inst = dynamic_cast<Instruction*>(ret_value);
            if(x_inst != nullptr && x_inst->is_phi()) {
                type = 4;
            }
        //    std::cout<<"ret_value="<<final_inst->get_operand(0)->get_type()->print()<<std::endl;
            else {
                type = 0;
            }
        }
    }
    else if(final_inst->is_br()) {
        auto branchInst = static_cast<BranchInst *>(final_inst);
        if(branchInst->is_cond_br()) {
            cond = final_inst->get_operand(0);
            label1 = static_cast <BasicBlock*> (final_inst->get_operand(1));
            label2 = static_cast <BasicBlock*> (final_inst->get_operand(2));
            type = 1;
        }
        else {
            label1 = static_cast <BasicBlock*> (final_inst->get_operand(0));
            type = 2;
        }
    }
    bb->erase_instr(final_inst);  //先将终结指令从bb块中暂时删除
    for(auto succ_bb : dominators_->get_dom_tree_succ_blocks(bb)) {
       // auto succ_bb = &succ_bb1;
        for(auto &inst1 : succ_bb->get_instructions()) {
            auto inst = &inst1;
            if(dynamic_cast<Instruction*>(inst) == nullptr) {
                std::cout<<"null!"<<std::endl;
            }
            if(inst->is_phi()) {    //如果是phi指令
                //为phi指令添加操作数
            /*    auto phi_inst = static_cast<PhiInst *> (inst);
                if(phi_var.find(inst) == phi_var.end()) {
                    std::cout<<"cannot find"<<std::endl;
                }
                if(Stack.find(phi_var.at(inst)) == Stack.end()) {
                    std::cout<<"cannot find"<<std::endl;
                }
                if(Stack[phi_var.at(inst)].empty()) {
                    std::cout<<"empty"<<std::endl;
                }
                auto xxx = dynamic_cast<Instruction*>(Stack[phi_var.at(inst)].top());
                if(xxx!=nullptr && xxx->is_phi()) {
                    std::cout<<"is_phi"<<std::endl;
                }
                else {
                    std::cout<<"is_not_phi"<<std::endl;
                }*/
                auto phi_inst = static_cast<PhiInst *> (inst);
                if(!Stack[phi_var.at(inst)].empty()) {
                    phi_inst -> add_phi_pair_operand(Stack[phi_var.at(inst)].top(), bb);    //这里是不是不应该插入bb？bb是CFG图的前驱，但是不一定是跳转到该块的前驱
                //这里是不是需要完成copystatement？将copy指令插在该块br或ret指令之前。
                    auto x = Stack[phi_var.at(inst)].top();
                
              //  std::cout<<inst->get_type()->get_pointer_element_type()->print()<<std::endl;
              // std::cout<<x->get_type()->print()<<std::endl;
                    auto x_inst = dynamic_cast<Instruction*>(x);
                    if(x_inst != nullptr && x_inst->is_phi()) {
                        auto load_inst = LoadInst::create_load(x, bb);
                        StoreInst::create_store(load_inst, inst, bb);
                    }
                    else {
                        auto store_inst = StoreInst::create_store(x, inst, bb);
                    }
                 //在倒数第二条指令处插入store函数*/
                }
                
            }
            else {
                break;
            }
        }
    }
    if(type == 0) {
        std::cout<<bb->get_parent()->get_return_type()->print()<<std::endl;
        std::cout<<ret_value->get_type()->print()<<std::endl;
        ReturnInst::create_ret(ret_value, bb);
    }
    else if(type == 1) {
        BranchInst::create_cond_br(cond, label1, label2, bb);
    }
    else if(type == 2){
        BranchInst::create_br(label1, bb);
    }
    else if(type == 3){
        ReturnInst::create_void_ret(bb);
    }
    else if(type == 4) {
        auto trans_ret = LoadInst::create_load(ret_value, bb);
        ReturnInst::create_ret(trans_ret, bb);
    }
    
    
 //   bb->add_instruction(final_inst);

    Visited.insert(bb);     //标记该块已执行过rename
    for(auto succ_bb : dominators_->get_dom_tree_succ_blocks(bb)) {   //递归遍历后继块
        //auto succ_bb = &succ_bb1;
        //这里或许需要增加一个判断：该后继块是否已执行过rename
        if(Visited.find(succ_bb) == Visited.end()) {
            rename(succ_bb);
        }
    }

    auto size = bb->get_instructions().size();
    for(auto &inst1 : bb->get_instructions()) { //在rename一个块后对内存变量定义值进行退栈操作
        auto inst = &inst1;
        if(inst->is_store()) {
            auto var = inst->get_operand(1);
            Stack[var].pop();
        }
        else if(inst->is_phi()) {
            auto var = phi_var.at(inst);
            Stack[var].pop();
        }
    }

    for(auto inst : del_inst)
        inst->remove_all_operands();    //将定义值push入栈后将该store指令删除
    for(auto inst : del_inst)
        inst->get_parent()->get_instructions().erase(inst);

        //store,load,alloca指令删除
 /*   for(auto &inst1 : bb->get_instructions())  {    //先删除所有store指令
        auto inst = &inst1;
        if(inst->is_store()) {
            inst->remove_all_operands();
            inst->get_parent()->get_instructions().erase(inst);
        }
    } */
}
