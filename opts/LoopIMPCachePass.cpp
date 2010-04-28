#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/GlobalAlias.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Constants.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "IMPCacher.h"
#include <string>

using namespace llvm;
using std::string;

namespace 
{
  class GNULoopIMPCachePass : public FunctionPass 
  {
    GNUstep::IMPCacher *cacher;
    const IntegerType *IntTy;

    public:
    static char ID;
    GNULoopIMPCachePass() : FunctionPass((intptr_t)&ID) {}
    ~GNULoopIMPCachePass() { delete cacher; }

    virtual bool doInitialization(Module &Mod) {
      cacher = new GNUstep::IMPCacher(Mod.getContext(), this);
      // FIXME: ILP64
      IntTy = Type::getInt32Ty(Mod.getContext());
      return false;  
    }

    virtual void getAnalysisUsage(AnalysisUsage &Info) const {
      Info.addRequired<LoopInfo>();
    }


    virtual bool runOnFunction(Function &F) {
      LoopInfo &LI = getAnalysis<LoopInfo>();
      bool modified = false;
      SmallVector<CallInst*, 16> Lookups;
      BasicBlock *entry = &F.getEntryBlock();

      for (Function::iterator i=F.begin(), end=F.end() ;
          i != end ; ++i) {
        // Ignore basic blocks that are not parts of loops.
        if (LI.getLoopDepth(i) == 0) { continue; }
        for (BasicBlock::iterator b=i->begin(), last=i->end() ;
            b != last ; ++b) {
          if (CallInst *call = dyn_cast<CallInst>(b)) {
            Value *callee = call->getCalledValue()->stripPointerCasts();
            if (Function *func = dyn_cast<Function>(callee)) {
              if (func->getName() == "objc_msg_lookup_sender") {
                modified = true;
                Lookups.push_back(call);
              }
            }
          }
        }
      }
      IRBuilder<> B = IRBuilder<>(entry);
      for (SmallVectorImpl<CallInst*>::iterator i=Lookups.begin(), 
          e=Lookups.end() ; e!=i ; i++) {
        const Type *SlotPtrTy = (*i)->getType();
        B.SetInsertPoint(entry, entry->begin());
        Value *slot = B.CreateAlloca(SlotPtrTy, 0, "slot");
        Value *version = B.CreateAlloca(IntTy, 0, "slot_version");

        B.SetInsertPoint(entry, entry->getTerminator());
        B.CreateStore(Constant::getNullValue(SlotPtrTy), slot);
        B.CreateStore(Constant::getNullValue(IntTy), version);
        cacher->CacheLookup(*i, slot, version);
      }
      return modified;
    }
  };

  char GNULoopIMPCachePass::ID = 0;
  RegisterPass<GNULoopIMPCachePass> X("gnu-loop-imp-cache", 
          "Cache IMPs in loops pass");
}

FunctionPass *createGNULoopIMPCachePass(void)
{
  return new GNULoopIMPCachePass();
}
