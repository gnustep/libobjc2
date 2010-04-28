#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Analysis/LoopInfo.h"
#include <string>

using namespace llvm;
using std::string;
using std::pair;

namespace 
{
  class ClassLookupCachePass : public FunctionPass 
  {
    Module *M;
    typedef std::pair<CallInst*,std::string> ClassLookup;

    public:
    static char ID;
    ClassLookupCachePass() : FunctionPass((intptr_t)&ID) {}

    virtual bool doInitialization(Module &Mod) {
      M = &Mod;
      return false;  
    }

    virtual bool runOnFunction(Function &F) {
      bool modified = false;
      SmallVector<ClassLookup, 16> Lookups;
      BasicBlock *entry = &F.getEntryBlock();

      for (Function::iterator i=F.begin(), end=F.end() ;
          i != end ; ++i) {
        for (BasicBlock::iterator b=i->begin(), last=i->end() ;
            b != last ; ++b) {
          if (CallInst *call = dyn_cast<CallInst>(b)) {
            Value *callee = call->getCalledValue()->stripPointerCasts();
            if (Function *func = dyn_cast<Function>(callee)) {
              if (func->getName() == "objc_lookup_class") {
                ClassLookup lookup;
                GlobalVariable *classNameVar = dyn_cast<GlobalVariable>(
                    call->getOperand(1)->stripPointerCasts());
                if (0 == classNameVar) { continue; }
                ConstantArray *init = dyn_cast<ConstantArray>(
                    classNameVar->getInitializer());
                if (0 == init || !init->isCString()) { continue; }
                lookup.first = call;
                lookup.second = init->getAsString();
                modified = true;
                Lookups.push_back(lookup);
              }
            }
          }
        }
      }
      IRBuilder<> B = IRBuilder<>(entry);
      for (SmallVectorImpl<ClassLookup>::iterator i=Lookups.begin(), 
          e=Lookups.end() ; e!=i ; i++) {
        Value *global = M->getGlobalVariable(("_OBJC_CLASS_" + i->second).c_str(), true);
        if (global) {
          Value *cls = new BitCastInst(global, i->first->getType(), "class", i->first);
          i->first->replaceAllUsesWith(cls);
          i->first->removeFromParent();
        }
      }
      return modified;
    }
  };

  char ClassLookupCachePass::ID = 0;
  RegisterPass<ClassLookupCachePass> X("gnu-class-lookup-cache", 
          "Cache class lookups");
}

FunctionPass *createClassLookupCachePass(void)
{
  return new ClassLookupCachePass();
}
