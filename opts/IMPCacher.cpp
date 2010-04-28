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

void GNUstep::IMPCacher::CacheLookup(CallInst *lookup, Value *slot, Value
    *version) {

  BasicBlock *beforeLookupBB = lookup->getParent();
  BasicBlock *lookupBB = SplitBlock(beforeLookupBB, lookup, Owner);
  BasicBlock::iterator iter = lookup;
  iter++;
  BasicBlock *afterLookupBB = SplitBlock(iter->getParent(), iter, Owner);

  beforeLookupBB->getTerminator()->removeFromParent();

  IRBuilder<> B = IRBuilder<>(beforeLookupBB);
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
      
  BasicBlock *cacheLookupBB = BasicBlock::Create(Context, "cache_check",
      lookupBB->getParent());

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

