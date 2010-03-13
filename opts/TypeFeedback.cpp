#include "llvm/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include <vector>

using namespace llvm;

namespace {
  struct GNUObjCTypeFeedback : public ModulePass {
    
  typedef std::pair<CallInst*,CallInst*> callPair;
  typedef std::vector<callPair > replacementVector;
    static char ID;
  uint32_t callsiteCount;
  const IntegerType *Int32Ty;
    GNUObjCTypeFeedback() : ModulePass(&ID), callsiteCount(0) {}

    void profileFunction(Function &F, Constant *ModuleID)
  {
    for (Function::iterator i=F.begin(), e=F.end() ;
                    i != e ; ++i)
    {
      replacementVector replacements;
      for (BasicBlock::iterator b=i->begin(), last=i->end() ;
          b != last ; ++b)
      {
        Module *M = F.getParent();
        if (CallInst *call = dyn_cast<CallInst>(b))
        { 
          if (Function *callee = call->getCalledFunction())
          {
            if (callee->getName() == "objc_msg_lookup_sender")
            {
              llvm::Value *args[] = { call->getOperand(1),
                call->getOperand(2), call->getOperand(3),
                ModuleID, ConstantInt::get(Int32Ty,
                    callsiteCount++) };
              Function *profile = cast<Function>(
                  M->getOrInsertFunction("objc_msg_lookup_profile",
                    callee->getFunctionType()->getReturnType(),
                    args[0]->getType(), args[1]->getType(),
                    args[2]->getType(),
                    ModuleID->getType(), Int32Ty, NULL));
              llvm::CallInst *profileCall = 
                CallInst::Create(profile, args, args+5, "", call);
              replacements.push_back(callPair(call, profileCall));
            }
          }
        }
      }
      for (replacementVector::iterator r=replacements.begin(), 
          e=replacements.end() ; e!=r ; r++)
      {
        r->first->replaceAllUsesWith(r->second);
        r->second->getParent()->getInstList().erase(r->first);
      }
    }
    }

  public:
  virtual bool runOnModule(Module &M)
  {
    LLVMContext &VMContext = M.getContext();
    Int32Ty = IntegerType::get(VMContext, 32);
    Constant *moduleName = 
      ConstantArray::get(VMContext, M.getModuleIdentifier(), true);
    moduleName = new GlobalVariable(M, moduleName->getType(), true,
        GlobalValue::InternalLinkage, moduleName,
        ".objc_profile_module_name");

    for (Module::iterator F=M.begin(), e=M.end() ;
      F != e ; ++F)
    {
      profileFunction(*F, moduleName);
    }
    return true;
  }

  };
  
  char GNUObjCTypeFeedback::ID = 0;
  RegisterPass<GNUObjCTypeFeedback> X("gnu-objc-type-feedback", 
      "Objective-C type feedback for the GNU runtime.", false, true);
}

