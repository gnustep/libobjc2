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
#include <string>

using namespace llvm;
using std::string;

namespace 
{
  class GNULoopIMPCachePass : public FunctionPass 
  {

    public:
    static char ID;
    GNULoopIMPCachePass() : FunctionPass((intptr_t)&ID) {}

    Module *M;
    LLVMContext *Context;
    const PointerType *PtrTy;
    const PointerType *IdTy;
    const IntegerType *IntTy;

    virtual bool doInitialization(Module &Mod) {
      M = &Mod;
      Context = &M->getContext();
      PtrTy = Type::getInt8PtrTy(*Context);
      // FIXME: 64-bit.
      IntTy = Type::getInt32Ty(*Context);
      IdTy = PointerType::getUnqual(PtrTy);
      return false;  
    }
    virtual void getAnalysisUsage(AnalysisUsage &Info) const {
      Info.addRequired<LoopInfo>();
    }


    // TODO: Move this to a helper class.
    void CacheLookup(CallInst *lookup, BasicBlock *allocaBlock) {
      const Type *SlotPtrTy = lookup->getType();

      IRBuilder<> B = IRBuilder<>(allocaBlock);
      B.SetInsertPoint(allocaBlock, allocaBlock->begin());
      Value *slot = B.CreateAlloca(SlotPtrTy, 0, "slot");
      Value *version = B.CreateAlloca(IntTy, 0, "slot_version");

      B.SetInsertPoint(allocaBlock, allocaBlock->getTerminator());
      B.CreateStore(Constant::getNullValue(SlotPtrTy), slot);
      B.CreateStore(Constant::getNullValue(IntTy), version);

      BasicBlock *beforeLookupBB = lookup->getParent();
      BasicBlock *lookupBB = SplitBlock(beforeLookupBB, lookup, this);
      BasicBlock::iterator iter = lookup;
      iter++;
      BasicBlock *afterLookupBB = SplitBlock(iter->getParent(), iter, this);

      beforeLookupBB->getTerminator()->removeFromParent();
      B.SetInsertPoint(beforeLookupBB);
      // Load the slot and check that neither it nor the version is 0.
      Value *slotValue = B.CreateLoad(slot);
      Value *versionValue = B.CreateLoad(version);
      Value *receiver = lookup->getOperand(1);

      Value *isCacheEmpty = 
            B.CreateOr(versionValue, B.CreatePtrToInt(slotValue, IntTy));
      isCacheEmpty = 
        B.CreateICmpEQ(isCacheEmpty, Constant::getNullValue(IntTy));
      Value *receiverNotNil =
         B.CreateICmpNE(receiver, Constant::getNullValue(receiver->getType()));
      isCacheEmpty = B.CreateAnd(isCacheEmpty, receiverNotNil);
          
      BasicBlock *cacheLookupBB = BasicBlock::Create(*Context, "cache_check",
          allocaBlock->getParent());

      B.CreateCondBr(isCacheEmpty, lookupBB, cacheLookupBB);

      // Check the cache node is current
      B.SetInsertPoint(cacheLookupBB);
      Value *slotVersion = B.CreateStructGEP(slotValue, 3);
      // Note: Volatile load because the slot version might have changed in
      // another thread.
      slotVersion = B.CreateLoad(slotVersion, true, "slot_version");
      Value *slotCachedFor = B.CreateStructGEP(slotValue, 1);
      slotCachedFor = B.CreateLoad(slotCachedFor, true, "slot_owner");
      Value *cls = B.CreateLoad(B.CreateBitCast(receiver, IdTy));
      Value *isVersionCorrect = B.CreateICmpEQ(slotVersion, versionValue);
      Value *isOwnerCorrect = B.CreateICmpEQ(slotCachedFor, cls);
      Value *isSlotValid = B.CreateAnd(isVersionCorrect, isOwnerCorrect);
      // If this slot is still valid, skip the lookup.
      B.CreateCondBr(isSlotValid, afterLookupBB, lookupBB);

      // Replace the looked up slot with the loaded one
      B.SetInsertPoint(afterLookupBB, afterLookupBB->begin());
      // Not volatile, so a redundant load elimination pass can do some phi
      // magic with this later.
      lookup->replaceAllUsesWith(B.CreateLoad(slot));

      // Perform the real lookup and cache the result
      lookupBB->getTerminator()->removeFromParent();
      B.SetInsertPoint(lookupBB);
      // Store it even if the version is 0, because we always check that the
      // version is not 0 at the start and an occasional redundant store is
      // probably better than a branch every time.
      B.CreateStore(lookup, slot);
      B.CreateStore(B.CreateLoad(B.CreateStructGEP(lookup, 3)), version);
      cls = B.CreateLoad(B.CreateBitCast(receiver, IdTy));
      B.CreateStore(cls, B.CreateStructGEP(lookup, 1));
      B.CreateBr(afterLookupBB);
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
      for (SmallVectorImpl<CallInst*>::iterator i=Lookups.begin(), 
          e=Lookups.end() ; e!=i ; i++) {
        CacheLookup(*i, entry);
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
