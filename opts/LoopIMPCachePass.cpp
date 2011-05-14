#include "llvm/LLVMContext.h"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/DefaultPasses.h"
#include "ObjectiveCOpts.h"
#include "IMPCacher.h"
#include <string>

using namespace GNUstep;
using namespace llvm;
using std::string;

namespace 
{
  class GNULoopIMPCachePass : public FunctionPass 
  {
    GNUstep::IMPCacher *cacher;
    const IntegerType *IntTy;
    Module *M;

    public:
    static char ID;
    GNULoopIMPCachePass() : FunctionPass(ID) {}
    ~GNULoopIMPCachePass() { delete cacher; }

    virtual bool doInitialization(Module &Mod) {
      cacher = new GNUstep::IMPCacher(Mod.getContext(), this);
      IntTy = (sizeof(int) == 4 ) ? Type::getInt32Ty(Mod.getContext()) :
          Type::getInt64Ty(Mod.getContext()) ;
      M = &Mod;
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

        B.CreateStore(Constant::getNullValue(SlotPtrTy), slot);
        B.CreateStore(Constant::getNullValue(IntTy), version);
        cacher->CacheLookup(*i, slot, version);
      }
      if (modified){
          verifyFunction(F);
      }
      return modified;
    }
  };

  char GNULoopIMPCachePass::ID = 0;
  RegisterPass<GNULoopIMPCachePass> X("gnu-loop-imp-cache", 
          "Cache IMPs in loops pass");
#if LLVM_MAJOR > 2
  StandardPass::RegisterStandardPass<GNULoopIMPCachePass> D(
        StandardPass::Module, &NonfragileIvarID,
        StandardPass::OptimzationFlags(1), &LoopIMPCacheID);
  StandardPass::RegisterStandardPass<GNULoopIMPCachePass> L(StandardPass::LTO,
      &NonfragileIvarID, StandardPass::OptimzationFlags(0),
      &LoopIMPCacheID);
#endif
}

FunctionPass *createGNULoopIMPCachePass(void)
{
  return new GNULoopIMPCachePass();
}
