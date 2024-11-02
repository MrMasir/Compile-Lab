#include "Dominators.hpp"

int count;
std::map<BasicBlock *, int> node_order;

void Dominators::run() {
    for (auto &f1 : m_->get_functions()) {
        auto f = &f1;
        if (f->get_basic_blocks().size() == 0)
            continue;
        for (auto &bb1 : f->get_basic_blocks()) {//先将函数每个基本块创建在map中的条目
            auto bb = &bb1;
            idom_.insert({bb, {}});
            dom_frontier_.insert({bb, {}});
            dom_tree_succ_blocks_.insert({bb, {}});
        }

        create_idom(f);
        create_dominance_frontier(f);
        create_dom_tree_succ(f);
    }
}


//深度优先遍历CFG,返回一个map，map表示每个基本块的序号
void Dominators::reverse_postorder(BasicBlock *entry) {
/*/    if(entry->get_pred_basic_block().size() == 0) { //若为入口块，则直接插入。
        node_order.insert({entry, count});
    }
    else {*/
    if(entry->get_succ_basic_blocks().size() == 0) {    //若没有后继，则该块为出口块之一
        if(node_order.find(entry) == node_order.end()) {  //当map中没有该基本块序号信息时，插入序号
            node_order.insert({entry, count});
        }
        return ;
    }
    else {  //若有后继
        if(node_order.find(entry) == node_order.end()) {
            node_order.insert({entry, count});
        }
        for (auto &succ : entry->get_succ_basic_blocks()) { //遍历该块的每个后继
            if(node_order.find(succ) == node_order.end()) {   //若该后继在map中没有序号信息，则以其为起点调用函数
                count += 1;
                reverse_postorder(succ);
            }
        }
        return ;
    }

}

void Dominators::create_idom(Function *f) {
    // TODO: 分析得到 f 中各个基本块的 idom
/*    for (auto &bb1 : f->get_basic_blocks()) {//建立各节点的前驱和后继
        auto bb = &bb1;
        auto ter_inst = bb->get_terminator();
        if(ter_inst->is_br()) {
            auto *branchInst = static_cast<BranchInst *>(ter_inst);
            if(branchInst->is_cond_br()) {  //如果是条件跳转
                auto succ_bb1 = static_cast <BasicBlock*> (branchInst->get_operand(1));
                auto succ_bb2 = static_cast <BasicBlock*> (branchInst->get_operand(2));
                bb->add_succ_basic_block(succ_bb1);
                bb->add_succ_basic_block(succ_bb2);
                succ_bb1->add_pre_basic_block(bb);
                succ_bb2->add_pre_basic_block(bb);
            }
            else {  //  如果是无条件跳转
            auto succ_bb = static_cast <BasicBlock*> (branchInst->get_operand(0));
                bb->add_succ_basic_block(succ_bb);
                succ_bb->add_pre_basic_block(bb);
            }
        }
    }*/
    BasicBlock  *entry_bb;
    for (auto &bb1 : f->get_basic_blocks()) {   
        auto bb = &bb1;
        if(bb->get_pre_basic_blocks().size() == 0) {    //找到entry block
            auto entry = idom_.find(bb);
            entry->second = bb;
            entry_bb = bb;
            break;
        }
    }

    //std::map<BasicBlock *, int> basic_order;
    count = 1;
    reverse_postorder(entry_bb);

    auto changed = 1;
    while(changed) {
        changed = 0;
        for (auto &pair : node_order) {
            auto bb = pair.first;
            if(bb != entry_bb) {    //遍历所有非入口块的基本块
                BasicBlock * new_idom;
                auto mark = 0;
                for(auto &pre_bb1 : bb->get_pre_basic_blocks()) { //遍历该基本块的所有前驱块
                    if(mark == 0) { //若为第一个前驱，则将其赋为idom初值
                        new_idom = pre_bb1;
                        mark = 1;
                    }
                    else {  //若不是第一个前驱
                        if(idom_.at(pre_bb1) != nullptr) {  //若该前驱有定义的idom
                            auto find1 = idom_.at(pre_bb1); //当前前驱的支配节点
                            auto find2 = new_idom;          //该节点的当前支配节点
                            while(find1 != find2) { //找到find1和find2的公共支配节点
                                while(node_order.at(find1) > node_order.at(find2)) {
                                    find1 = idom_.at(find1);
                                }
                                while(node_order.at(find1) < node_order.at(find2)) {
                                    find2 = idom_.at(find2);
                                }
                            }
                            new_idom = find1;
                        }
                    }
                }
                if(new_idom != idom_.at(bb)) {
                    idom_[bb] = new_idom;
                    changed = 1;
                }
            }
        }
    }
    
}

void Dominators::create_dominance_frontier(Function *f) {
    // TODO: 分析得到 f 中各个基本块的支配边界集合
    for (auto &bb1 : f->get_basic_blocks()) {
        auto bb = &bb1;
        if(bb->get_pre_basic_blocks().size() >= 2) {
            for(auto &pre_bb1 : bb->get_pre_basic_blocks()) {   //这里返回的是基本块指针列表 
                BasicBlock * runner = pre_bb1;
                while(runner != idom_.at(bb)) {
                    dom_frontier_[runner].insert(bb);
                    runner = idom_.at(runner);
                }
            }
        }
    }

}

void Dominators::create_dom_tree_succ(Function *f) {
    // TODO: 分析得到 f 中各个基本块的支配树后继
    //支配树后继和CFG节点后继有什么区别？
    for (auto &bb1 : f->get_basic_blocks()) {
        auto bb = &bb1;
        if(bb->get_succ_basic_blocks().size() != 0) {
            for(auto &succ_bb1 : bb->get_succ_basic_blocks()) {
                dom_tree_succ_blocks_[bb].insert(succ_bb1);
            }
        }
    }
}
