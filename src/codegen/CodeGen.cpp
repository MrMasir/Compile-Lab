#include "CodeGen.hpp"

#include "CodeGenUtil.hpp"

void CodeGen::allocate() {
    // 备份 $ra $fp
    unsigned offset = PROLOGUE_OFFSET_BASE;

    // 为每个参数分配栈空间
    for (auto &arg : context.func->get_args()) {
        auto size = arg.get_type()->get_size();
        offset = ALIGN(offset + size, size);
        context.offset_map[&arg] = -static_cast<int>(offset);
    }

    // 为指令结果分配栈空间
    for (auto &bb : context.func->get_basic_blocks()) {
        for (auto &instr : bb.get_instructions()) {
            // 每个非 void 的定值都分配栈空间
            if (not instr.is_void()) {//ret、br、store、空返回值类型的call不分配栈空间
                auto size = instr.get_type()->get_size();
                offset = ALIGN(offset + size, size);
                context.offset_map[&instr] = -static_cast<int>(offset);
            }
            // alloca 的副作用：分配额外空间
            if (instr.is_alloca()) {
                auto *alloca_inst = static_cast<AllocaInst *>(&instr);
                auto alloc_size = alloca_inst->get_alloca_type()->get_size();
                offset += alloc_size;
            }

        /*   //phi指令副作用：分配额外空间
            if (instr.is_phi()) {
                auto size_phi = instr.get_type()->get_pointer_element_type()->get_size();
                offset += size_phi;
            }*/
        }
    }

    // 分配栈空间，需要是 16 的整数倍
    context.frame_size = ALIGN(offset, PROLOGUE_ALIGN);
}

void CodeGen::load_to_greg(Value *val, const Reg &reg) {
    assert(val->get_type()->is_integer_type() ||
           val->get_type()->is_pointer_type());

    if (auto *constant = dynamic_cast<ConstantInt *>(val)) {//若为常量
        int32_t val = constant->get_value();
        if (IS_IMM_12(val)) {
            append_inst(ADDI WORD, {reg.print(), "$zero", std::to_string(val)});
        } else {
            load_large_int32(val, reg);
        }
    } else if (auto *global = dynamic_cast<GlobalVariable *>(val)) {//若为全局变量
        append_inst(LOAD_ADDR, {reg.print(), global->get_name()});
    } else {
        load_from_stack_to_greg(val, reg);
    }
}

void CodeGen::load_large_int32(int32_t val, const Reg &reg) {//加载大整数32位，2条指令
    int32_t high_20 = val >> 12; // si20
    uint32_t low_12 = val & LOW_12_MASK;
    append_inst(LU12I_W, {reg.print(), std::to_string(high_20)});
    append_inst(ORI, {reg.print(), reg.print(), std::to_string(low_12)});
}

void CodeGen::load_large_int64(int64_t val, const Reg &reg) {//加载大整数64位，4条指令
    auto low_32 = static_cast<int32_t>(val & LOW_32_MASK);
    load_large_int32(low_32, reg);

    auto high_32 = static_cast<int32_t>(val >> 32);
    int32_t high_32_low_20 = (high_32 << 12) >> 12; // si20
    int32_t high_32_high_12 = high_32 >> 20;        // si12
    append_inst(LU32I_D, {reg.print(), std::to_string(high_32_low_20)});
    append_inst(LU52I_D,
                {reg.print(), reg.print(), std::to_string(high_32_high_12)});
}

void CodeGen::load_from_stack_to_greg(Value *val, const Reg &reg) {
    auto offset = context.offset_map.at(val);
    auto offset_str = std::to_string(offset);
    auto *type = val->get_type();
    auto trans_inst = dynamic_cast<Instruction*> (val);
    if (IS_IMM_12(offset)) {    //是12bit立即数
        if(trans_inst != nullptr && trans_inst->is_phi()) {
            append_inst(LOAD WORD, {reg.print(), "$fp", offset_str});
        }
        else {
            if (type->is_int1_type()) { //1bit数
                append_inst(LOAD BYTE, {reg.print(), "$fp", offset_str});
            } else if (type->is_int32_type()) { //32bit数
                append_inst(LOAD WORD, {reg.print(), "$fp", offset_str});
            } else { // Pointer
                append_inst(LOAD DOUBLE, {reg.print(), "$fp", offset_str});
            }
        }
    } else {
        load_large_int64(offset, reg);
        append_inst(ADD DOUBLE, {reg.print(), "$fp", reg.print()});
        if (type->is_int1_type()) {
            append_inst(LOAD BYTE, {reg.print(), reg.print(), "0"});
        } else if (type->is_int32_type()) {
            append_inst(LOAD WORD, {reg.print(), reg.print(), "0"});
        } else { // Pointer
            append_inst(LOAD DOUBLE, {reg.print(), reg.print(), "0"});
        }
    }
}

void CodeGen::store_from_greg(Value *val, const Reg &reg) {
    auto offset = context.offset_map.at(val);
    auto offset_str = std::to_string(offset);
    auto *type = val->get_type();
    if (IS_IMM_12(offset)) {
        if (type->is_int1_type()) {
            append_inst(STORE BYTE, {reg.print(), "$fp", offset_str});
        } else if (type->is_int32_type()) {
            append_inst(STORE WORD, {reg.print(), "$fp", offset_str});
        } else { // Pointer
            append_inst(STORE DOUBLE, {reg.print(), "$fp", offset_str});
        }
    } else {
        auto addr = Reg::t(8);
        load_large_int64(offset, addr);
        append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
        if (type->is_int1_type()) {
            append_inst(STORE BYTE, {reg.print(), addr.print(), "0"});
        } else if (type->is_int32_type()) {
            append_inst(STORE WORD, {reg.print(), addr.print(), "0"});
        } else { // Pointer
            append_inst(STORE DOUBLE, {reg.print(), addr.print(), "0"});
        }
    }
}

void CodeGen::load_to_freg(Value *val, const FReg &freg) {
    assert(val->get_type()->is_float_type());
    if (auto *constant = dynamic_cast<ConstantFP *>(val)) {
        float val = constant->get_value();
        load_float_imm(val, freg);
    } else {
        auto offset = context.offset_map.at(val);
        auto offset_str = std::to_string(offset);
        if (IS_IMM_12(offset)) {
            append_inst(FLOAD SINGLE, {freg.print(), "$fp", offset_str});
        } else {
            auto addr = Reg::t(8);
            load_large_int64(offset, addr);
            append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
            append_inst(FLOAD SINGLE, {freg.print(), addr.print(), "0"});
        }
    }
}

void CodeGen::load_float_imm(float val, const FReg &r) {
    int32_t bytes = *reinterpret_cast<int32_t *>(&val);
    load_large_int32(bytes, Reg::t(8));
    append_inst(GR2FR WORD, {r.print(), Reg::t(8).print()});
}

void CodeGen::store_from_freg(Value *val, const FReg &r) {
    auto offset = context.offset_map.at(val);
    if (IS_IMM_12(offset)) {
        auto offset_str = std::to_string(offset);
        append_inst(FSTORE SINGLE, {r.print(), "$fp", offset_str});
    } else {
        auto addr = Reg::t(8);
        load_large_int64(offset, addr);
        append_inst(ADD DOUBLE, {addr.print(), "$fp", addr.print()});
        append_inst(FSTORE SINGLE, {r.print(), addr.print(), "0"});
    }
}

void CodeGen::gen_prologue() {
    // 寄存器备份及栈帧设置
    if (IS_IMM_12(-static_cast<int>(context.frame_size))) {
        append_inst("st.d $ra, $sp, -8");
        append_inst("st.d $fp, $sp, -16");
        append_inst("addi.d $fp, $sp, 0");
        append_inst("addi.d $sp, $sp, " +
                    std::to_string(-static_cast<int>(context.frame_size)));
    } else {
        load_large_int64(context.frame_size, Reg::t(0));
        append_inst("st.d $ra, $sp, -8");
        append_inst("st.d $fp, $sp, -16");
        append_inst("sub.d $sp, $sp, $t0");
        append_inst("add.d $fp, $sp, $t0");
    }

    // 将函数参数转移到栈帧上
    int garg_cnt = 0;
    int farg_cnt = 0;
    for (auto &arg : context.func->get_args()) {
        if (arg.get_type()->is_float_type()) {
            store_from_freg(&arg, FReg::fa(farg_cnt++));    //这里函数参数是在哪里加载到参数寄存器中的?
        } else { // int or pointer
            store_from_greg(&arg, Reg::a(garg_cnt++));
        }
    }
}

void CodeGen::gen_epilogue() {
    // TODO: 根据你的理解设定函数的 epilogue
    append_inst(context.func->get_name() + "_function_end", ASMInstruction::Label);
    if (IS_IMM_12(-static_cast<int>(context.frame_size))) {
        append_inst("ld.d $ra, $fp, -8");
        append_inst("ld.d $fp, $fp, -16");
    //    append_inst("addi.d $fp, $sp, 0");
        append_inst("addi.d $sp, $sp, " +
                    std::to_string(static_cast<int>(context.frame_size)));
    } else {
        load_large_int64(context.frame_size, Reg::t(0));
        append_inst("ld.d $ra, $fp, -8");
        append_inst("ld.d $fp, $fp, -16");
        append_inst("add.d $sp, $sp, $t0");
    //    append_inst("add.d $fp, $sp, $t0");
    }
    append_inst("jr $ra");
   // throw not_implemented_error{__FUNCTION__};
}

//get_operand()返回值为Value类型

void CodeGen::gen_ret() {
    // TODO: 函数返回，思考如何处理返回值、寄存器备份，如何返回调用者地址
    auto *retInst = static_cast<ReturnInst *>(context.inst);
    if(retInst->is_void_ret()) {
        load_large_int32(0, Reg::a(0));
    }
    else if(context.inst->get_operand(0)->get_type()->is_integer_type()){
        load_to_greg(context.inst->get_operand(0), Reg::a(0));
    }
    else if(context.inst->get_operand(0)->get_type()->is_float_type()){
        load_to_freg(context.inst->get_operand(0), FReg::fa(0));
    }
    append_inst("b " + context.func->get_name() + "_function_end");
  //  throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_br() {
    auto *branchInst = static_cast<BranchInst *>(context.inst); //这里为什么要做这个类型转化？
    if (branchInst->is_cond_br()) {
        // TODO: 补全条件跳转的情况
        load_to_greg(branchInst->get_operand(0), Reg::t(0));
        auto *truebb = static_cast<BasicBlock *>(branchInst->get_operand(1));
        append_inst("bnez $t0, " + label_name(truebb));
        auto *falsebb = static_cast<BasicBlock *>(branchInst->get_operand(2));
        append_inst("b " + label_name(falsebb));    //这里的true，false标签需要用append()取成不同的吗？
     //   throw not_implemented_error{__FUNCTION__};
    } else {
        auto *branchbb = static_cast<BasicBlock *>(branchInst->get_operand(0));
        append_inst("b " + label_name(branchbb));   //+是字符串拼接操作符
    }
}

void CodeGen::gen_binary() {
    // 分别将左右操作数加载到 $t0 $t1
    load_to_greg(context.inst->get_operand(0), Reg::t(0));
    load_to_greg(context.inst->get_operand(1), Reg::t(1));
    // 根据指令类型生成汇编
    switch (context.inst->get_instr_type()) {
    case Instruction::add:
        output.emplace_back("add.w $t2, $t0, $t1");
        break;
    case Instruction::sub:
        output.emplace_back("sub.w $t2, $t0, $t1");
        break;
    case Instruction::mul:
        output.emplace_back("mul.w $t2, $t0, $t1");
        break;
    case Instruction::sdiv:
        output.emplace_back("div.w $t2, $t0, $t1");
        break;
    default:
        assert(false);
    }
    // 将结果填入栈帧中
    store_from_greg(context.inst, Reg::t(2));
}

void CodeGen::gen_float_binary() {
    // TODO: 浮点类型的二元指令
    load_to_freg(context.inst->get_operand(0), FReg::ft(0));
    load_to_freg(context.inst->get_operand(1), FReg::ft(1));
    // 根据指令类型生成汇编
    switch (context.inst->get_instr_type()) {
    case Instruction::fadd:
        output.emplace_back("fadd.s $ft2, $ft0, $ft1");
        break;
    case Instruction::fsub:
        output.emplace_back("fsub.s $ft2, $ft0, $ft1");
        break;
    case Instruction::fmul:
        output.emplace_back("fmul.s $ft2, $ft0, $ft1");
        break;
    case Instruction::fdiv:
        output.emplace_back("fdiv.s $ft2, $ft0, $ft1");
        break;
    default:
        assert(false);
    }
    // 将结果填入栈帧中
    store_from_freg(context.inst, FReg::ft(2));
 //   throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_alloca() {
    /* 我们已经为 alloca 的内容分配空间，在此我们还需保存 alloca
     * 指令自身产生的定值，即指向 alloca 空间起始地址的指针
     */
    // TODO: 将 alloca 出空间的起始地址保存在栈帧上
    auto offset = context.offset_map.at(context.inst);
    //alloca的空间大小如何获取？
    auto *allocaInst = static_cast<AllocaInst *>(context.inst);
    auto alloca_size = allocaInst->get_alloca_type()->get_size();
    offset -= alloca_size;  //这里存的地址指的是偏移量吗？
    auto offset_str = std::to_string(offset);
    append_inst(ADDI DOUBLE, {Reg::t(1).print(), "$fp", offset_str});
//    load_large_int64(offset, Reg::t(1));
    store_from_greg(context.inst, Reg::t(1));
   // throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_load() {
    auto load_addr = dynamic_cast<Instruction *> (context.inst->get_operand(0));
    if(load_addr != nullptr && load_addr->is_phi()) {
        auto ptr = context.inst->get_operand(0);
        auto offset = context.offset_map.at(ptr);
        auto offset_str = std::to_string(offset);
        append_inst(ADDI DOUBLE, {Reg::t(0).print(), "$fp", offset_str});
    }
    else {
        auto *ptr = context.inst->get_operand(0);
        auto *type = context.inst->get_type();
        load_to_greg(ptr, Reg::t(0));
    }
 // TODO: load 整数类型的数据
    if (context.inst->get_type()->is_float_type()) {
        append_inst("fld.s $ft0, $t0, 0");
        store_from_freg(context.inst, FReg::ft(0));
    }
    else {
        if(context.inst->get_operand(0)->get_type()->get_pointer_element_type()->is_pointer_type()) {
            append_inst("ld.d $t1, $t0, 0");
        }
        else {
            append_inst("ld.w $t1, $t0, 0");
        }
        store_from_greg(context.inst, Reg::t(1));
    }
    //    throw not_implemented_error{__FUNCTION__};
}


void CodeGen::gen_store() {
    // TODO: 翻译 store 指令
    auto ptr_inst = dynamic_cast<Instruction*> (context.inst->get_operand(1));
    if(ptr_inst!= nullptr && ptr_inst->is_phi()) {
        auto ptr = context.inst->get_operand(1);
        auto offset = context.offset_map.at(ptr);
        auto offset_str = std::to_string(offset);
        append_inst(ADDI DOUBLE, {Reg::t(0).print(), "$fp", offset_str});
    }
    else {
        auto *ptr = context.inst->get_operand(1);
        auto *type = context.inst->get_type();
        load_to_greg(ptr, Reg::t(0));
    }
    
//    store_from_greg(&instr, Reg::t(1));

    if (context.inst->get_operand(0)->get_type()->is_float_type()/*type->is_float_type()*/) {
        load_to_freg(context.inst->get_operand(0), FReg::ft(0));
        append_inst("fst.s $ft0, $t0, 0");
    }
    else {
        load_to_greg(context.inst->get_operand(0), Reg::t(1));
        if(context.inst->get_operand(1)->get_type()->get_pointer_element_type()->is_pointer_type()) {
            append_inst("st.d $t1, $t0, 0");
        }
        else {
            append_inst("st.w $t1, $t0, 0");
        }
        
    }
        //store_from_freg(context.inst, FReg::ft(0));
}
   // throw not_implemented_error{__FUNCTION__};


void CodeGen::gen_icmp() {//比较指令的返回值为bool型，在栈中仅分配1个字节
    // TODO: 处理各种整数比较的情况
    // 分别将左右操作数加载到 $t0 $t1
    load_to_greg(context.inst->get_operand(0), Reg::t(0));
    load_to_greg(context.inst->get_operand(1), Reg::t(1));
    // 根据指令类型生成汇编
    auto offset = context.offset_map.at(context.inst); 
    switch (context.inst->get_instr_type()) {
    case Instruction::ge:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("bge $t0, $t1, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("bge $t0, $t1, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::gt:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("blt $t1, $t0, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("blt $t1, $t0, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::le:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("bge $t1, $t0, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("bge $t1, $t0, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::lt:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("blt $t0, $t1, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("blt $t0, $t1, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::eq:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("beq $t0, $t1, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("beq $t0, $t1, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::ne:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("bne $t0, $t1, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("bne $t0, $t1, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    default:
        assert(false);
    }
   // throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_fcmp() {
    // TODO: 处理各种浮点数比较的情况
    load_to_freg(context.inst->get_operand(0), FReg::ft(0));
    load_to_freg(context.inst->get_operand(1), FReg::ft(1));
    // 根据指令类型生成汇编
    auto offset = context.offset_map.at(context.inst); 
    switch (context.inst->get_instr_type()) {
    case Instruction::fge:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("fcmp.sle.s $t1, $t0, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("fcmp.sle.s $t1, $t0, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::fgt:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("fcmp.slt.s $t1, $t0, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("fcmp.slt.s $t1, $t0, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::fle:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("fcmp.sle.s $t0, $t1, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("fcmp.sle.s $t0, $t1, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::flt:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("fcmp.slt.s $t0, $t1, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("fcmp.slt.s $t0, $t1, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::feq:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("fcmp.seq.s $t0, $t1, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("fcmp.seq.s $t0, $t1, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    case Instruction::fne:
        if(IS_IMM_12(offset)) {//12位立即数，store_from_greg只创建一条指令；
            append_inst("fcmp.sne.s $t0, $t1, 0xc");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0xc");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
            //codegen->append_inst("a", ASMInstruction::Label);
        }
        else {//否则，store_from_greg创建6条指令；
            append_inst("fcmp.sne.s $t0, $t1, 0x20");
            store_from_greg(context.inst, Reg(0));
            append_inst("b 0x20");
            append_inst("addi.w $t2, $zero, 1 ");
            store_from_greg(context.inst, Reg::t(2));
        }
        break;
    default:
        assert(false);
    }
  //  throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_zext() {
    // TODO: 将窄位宽的整数数据进行零扩展
    load_to_greg(context.inst->get_operand(0), Reg::t(0));  //此时取出的一个字节进行符号扩展后放入寄存器t0中
    append_inst("andi $t0, $t0, 0xff");
    store_from_greg(context.inst, Reg::t(0));
   // throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_call() {
    // TODO: 函数调用，注意我们只需要通过寄存器传递参数，即不需考虑栈上传参的情况
    auto *callInst = static_cast<CallInst *>(context.inst);
    auto funtype = callInst->get_function_type();
    auto arg_num = funtype->get_num_of_args();
    auto fun = static_cast<Function*>(context.inst->get_operand(0));
    int garg_cnt = 0;
    int farg_cnt = 0;
    for (int i = 0; i < arg_num; i++) {
        if (funtype->get_param_type(i)->is_float_type()) {
            load_to_freg(context.inst->get_operand(i+1), FReg::fa(farg_cnt++));    //这里函数参数是在哪里加载到参数寄存器中的?
        } else { // int or pointer
            load_to_greg(context.inst->get_operand(i+1), Reg::a(garg_cnt++));
        }
    }
/*    for (auto &arg : fun->get_args()) {
        if (arg.get_type()->is_float_type()) {
            load_to_freg(&arg, FReg::fa(farg_cnt++));    //这里函数参数是在哪里加载到参数寄存器中的?
        } else { // int or pointer
            load_to_greg(&arg, Reg::a(garg_cnt++));
        }
    }*/
    append_inst("bl " + fun->get_name());
    if(fun->get_return_type()->is_integer_type()) {
        store_from_greg(context.inst, Reg::a(0));
    }
    else if(fun->get_return_type()->is_float_type()) {
        store_from_freg(context.inst, FReg::fa(0));
    }
   // throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_gep() {
    // TODO: 计算内存地址
    auto *getelemInst = static_cast<GetElementPtrInst*>(context.inst);
    if(context.inst->get_num_operand() == 3/*getelemInst->get_element_type()->is_array_type()*/) {//数组类型
        load_to_greg(context.inst->get_operand(0), Reg::t(0));
        load_to_greg(context.inst->get_operand(2), Reg::t(1));
        //auto index = context.inst->get_operand(2);//这里计算后存的结果是偏移量还是什么？
        //auto size = context.inst->get_operand(0)->get_array_element_type()->get_size();
        auto size = 4;
    //    auto offset = size * index;
        load_large_int64(size, Reg::t(2));
        append_inst(MUL DOUBLE, {Reg::t(1).print(), Reg::t(1).print(), Reg::t(2).print()});
        append_inst(ADD DOUBLE, {Reg::t(0).print(), Reg::t(0).print(), Reg::t(1).print()});
        store_from_greg(context.inst, Reg::t(0));
    }
    else {//指针类型
        load_to_greg(context.inst->get_operand(0), Reg::t(0));
        load_to_greg(context.inst->get_operand(1), Reg::t(1));
        //auto index = context.inst->get_operand(1);//这里计算后存的结果是偏移量还是什么？
        auto size = getelemInst->get_element_type()->get_size();
        load_large_int64(size, Reg::t(2));
        append_inst(MUL DOUBLE, {Reg::t(1).print(), Reg::t(1).print(), Reg::t(2).print()});
        append_inst(ADD DOUBLE, {Reg::t(0).print(), Reg::t(0).print(), Reg::t(1).print()});
        store_from_greg(context.inst, Reg::t(0));
    }
   // throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_sitofp() {
    // TODO: 整数转向浮点数
    load_to_greg(context.inst->get_operand(0), Reg::t(0));
    append_inst("movgr2fr.w $ft0, $t0");
    append_inst("ffint.s.w $ft1, $ft0");
    store_from_freg(context.inst, FReg::ft(1));
   // throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_fptosi() {
    // TODO: 浮点数转向整数，注意向下取整(round to zero)
    load_to_freg(context.inst->get_operand(0), FReg::ft(0));
    append_inst("ftintrz.w.s $ft1, $ft0");
    append_inst("movfr2gr.s $t0, $ft1");
    store_from_greg(context.inst, Reg::t(0));
   // throw not_implemented_error{__FUNCTION__};
}

void CodeGen::gen_phi() {
    auto offset = context.offset_map.at(context.inst);
    //将phi指令本身的地址保存在这个指令处，且需要在最开始保存
}

void CodeGen::run() {
    // 确保每个函数中基本块的名字都被设置好
    // 想一想：为什么？
    m->set_print_name();

    /* 使用 GNU 伪指令为全局变量分配空间
     * 你可以使用 `la.local` 指令将标签 (全局变量) 的地址载入寄存器中, 比如
     * 要将 `a` 的地址载入 $t0, 只需要 `la.local $t0, a`
     */
    if (!m->get_global_variable().empty()) {
        append_inst("Global variables", ASMInstruction::Comment);
        /* 虽然下面两条伪指令可以简化为一条 `.bss` 伪指令, 但是我们还是选择使用
         * `.section` 将全局变量放到可执行文件的 BSS 段, 原因如下:
         * - 尽可能对齐交叉编译器 loongarch64-unknown-linux-gnu-gcc 的行为
         * - 支持更旧版本的 GNU 汇编器, 因为 `.bss` 伪指令是应该相对较新的指令,
         *   GNU 汇编器在 2023 年 2 月的 2.37 版本才将其引入
         */
        append_inst(".text", ASMInstruction::Atrribute);
        append_inst(".section", {".bss", "\"aw\"", "@nobits"},
                    ASMInstruction::Atrribute);
        for (auto &global : m->get_global_variable()) {
            auto size =
                global.get_type()->get_pointer_element_type()->get_size();
            append_inst(".globl", {global.get_name()},
                        ASMInstruction::Atrribute);
            append_inst(".type", {global.get_name(), "@object"},
                        ASMInstruction::Atrribute);
            append_inst(".size", {global.get_name(), std::to_string(size)},
                        ASMInstruction::Atrribute);
            append_inst(global.get_name(), ASMInstruction::Label);
            append_inst(".space", {std::to_string(size)},
                        ASMInstruction::Atrribute);
        }
    }

    // 函数代码段
    output.emplace_back(".text", ASMInstruction::Atrribute);
    for (auto &func : m->get_functions()) {
        if (not func.is_declaration()) {
            // 更新 context
            context.clear();
            context.func = &func;

            // 函数信息
            append_inst(".globl", {func.get_name()}, ASMInstruction::Atrribute);
            append_inst(".type", {func.get_name(), "@function"},
                        ASMInstruction::Atrribute);
            append_inst(func.get_name(), ASMInstruction::Label);

            // 分配函数栈帧
            allocate();
            // 生成 prologue
            gen_prologue();

/*            for(auto &bb : func.get_basic_blocks()) {   //先处理每个phi指令
                for (auto &instr : bb.get_instructions()) {
                    if(instr.is_phi()) {
                        auto offset = context.offset_map.at(&instr);
                        auto phi_size = instr.get_type()->get_pointer_element_type()->get_size();
                        offset -= phi_size;
                        auto offset_str = std::to_string(offset);
                        append_inst(ADDI DOUBLE, {Reg::t(1).print(), "$fp", offset_str});
                        store_from_greg(&instr, Reg::t(1));
                        auto offset = context.offset_map.at(&instr);
                        auto offset_str = std::to_string(offset);
                        append_inst(ADDI DOUBLE, {Reg::t(1).print(), "$fp", offset_str});
                        store_from_greg(&instr, Reg::t(1));
                    }
                }
            }*/

            for (auto &bb : func.get_basic_blocks()) {
                append_inst(label_name(&bb), ASMInstruction::Label);
                for (auto &instr : bb.get_instructions()) {
                    // For debug
                    append_inst(instr.print(), ASMInstruction::Comment);
                    context.inst = &instr; // 更新 context
                    switch (instr.get_instr_type()) {
                    case Instruction::ret:
                        gen_ret();
                        break;
                    case Instruction::br:
                        gen_br();
                        break;
                    case Instruction::add:
                    case Instruction::sub:
                    case Instruction::mul:
                    case Instruction::sdiv:
                        gen_binary();
                        break;
                    case Instruction::fadd:
                    case Instruction::fsub:
                    case Instruction::fmul:
                    case Instruction::fdiv:
                        gen_float_binary();
                        break;
                    case Instruction::alloca:
                        gen_alloca();
                        break;
                    case Instruction::load:
                        gen_load();
                        break;
                    case Instruction::store:
                        gen_store();
                        break;
                    case Instruction::ge:
                    case Instruction::gt:
                    case Instruction::le:
                    case Instruction::lt:
                    case Instruction::eq:
                    case Instruction::ne:
                        gen_icmp();
                        break;
                    case Instruction::fge:
                    case Instruction::fgt:
                    case Instruction::fle:
                    case Instruction::flt:
                    case Instruction::feq:
                    case Instruction::fne:
                        gen_fcmp();
                        break;
                    case Instruction::phi:
                        //throw not_implemented_error{"need to handle phi!"};
                        gen_phi();
                        break;
                    case Instruction::call:
                        gen_call();
                        break;
                    case Instruction::getelementptr:
                        gen_gep();
                        break;
                    case Instruction::zext:
                        gen_zext();
                        break;
                    case Instruction::fptosi:
                        gen_fptosi();
                        break;
                    case Instruction::sitofp:
                        gen_sitofp();
                        break;
                    }
                }
            }
            // 生成 epilogue
            gen_epilogue();
        }
    }
}

std::string CodeGen::print() const {
    std::string result;
    for (const auto &inst : output) {
        result += inst.format();
    }
    return result;
}
