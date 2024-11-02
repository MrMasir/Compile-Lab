#pragma once

#include "Dominators.hpp"
#include "Instruction.hpp"
#include "Value.hpp"
#include "DeadCode.hpp"   //需要吗？

#include <map>
#include <memory>
#include <stack>

class Mem2Reg : public Pass {
  private:
    Function *func_;
    std::unique_ptr<Dominators> dominators_;
    std::map<Value *, Value *> phi_var; //存放每个phi指令对应的内存变量,这个变量是不是应该放在hpp里？
    using value_set = std::stack <Value *>;
    std::map<Value*, value_set> Stack;
    std::set<BasicBlock*> Visited;
    std::map<Value *, std::map<Value*, Value*>> alloca_name;  //存放每个数组变量的每个元素的值
    std::unique_ptr<DeadCode> deadcode_;
    // TODO: 添加需要的变量

  public:
    Mem2Reg(Module *m) : Pass(m) {}
    ~Mem2Reg() = default;

    void run() override;

    void generate_phi();
    void rename(BasicBlock *bb);

    static inline bool is_global_variable(Value *l_val) {
        return dynamic_cast<GlobalVariable *>(l_val) != nullptr;
    }
    static inline bool is_gep_instr(Value *l_val) {
        return dynamic_cast<GetElementPtrInst *>(l_val) != nullptr;
    }

    static inline bool is_valid_ptr(Value *l_val) {
        return not is_global_variable(l_val) and not is_gep_instr(l_val);
    }
};
