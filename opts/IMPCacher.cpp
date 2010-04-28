#include "IMPCacher.h"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/LLVMContext.h"
#include "llvm/Metadata.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

GNUstep::IMPCacher::IMPCacher(LLVMContext &C, Pass *owner) : Context(C),
  Owner(owner) {

  PtrTy = Type::getInt8PtrTy(Context);
  // FIXME: 64-bit.
  IntTy = Type::getInt32Ty(Context);
  IdTy = PointerType::getUnqual(PtrTy);
  Value *AlreadyCachedFlagValue = MDString::get(C, "IMPCached");
  AlreadyCachedFlag = MDNode::get(C, &AlreadyCachedFlagValue, 1);
  IMPCacheFlagKind = Context.getMDKindID("IMPCache");
}

void GNUstep::IMPCacher::CacheLookup(CallInst *lookup, Value *slot, Value
    *version) {

  // If this IMP is already cached, don't cache it again.
  if (lookup->getMetadata(IMPCacheFlagKind)) { return; }

  lookup->setMetadata(IMPCacheFlagKind, AlreadyCachedFlag);

  BasicBlock *beforeLookupBB = lookup->getParent();
  BasicBlock *lookupBB = SplitBlock(beforeLookupBB, lookup, Owner);
  BasicBlock::iterator iter = lookup;
  iter++;
  BasicBlock *afterLookupBB = SplitBlock(iter->getParent(), iter, Owner);

  removeTerminator(beforeLookupBB);

  IRBuilder<> B = IRBuilder<>(beforeLookupBB);
  // Load the slot and check that neither it nor the version is 0.
  Value *slotValue = B.CreateLoad(slot);
  Value *versionValue = B.CreateLoad(version);
  Value *receiverPtr = lookup->getOperand(1);
  Value *receiver = B.CreateLoad(receiverPtr);

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
  removeTerminator(lookupBB);
  B.SetInsertPoint(lookupBB);
  Value * newReceiver = B.CreateLoad(receiverPtr);
  BasicBlock *storeCacheBB = BasicBlock::Create(Context, "cache_store",
      lookupBB->getParent());

  // Don't store the cached lookup if we are doing forwarding tricks.
  B.CreateCondBr(B.CreateICmpEQ(receiver, newReceiver), storeCacheBB,
      afterLookupBB);
  B.SetInsertPoint(storeCacheBB);

  // Store it even if the version is 0, because we always check that the
  // version is not 0 at the start and an occasional redundant store is
  // probably better than a branch every time.
  B.CreateStore(lookup, slot);
  B.CreateStore(B.CreateLoad(B.CreateStructGEP(lookup, 3)), version);
  cls = B.CreateLoad(B.CreateBitCast(receiver, IdTy));
  B.CreateStore(cls, B.CreateStructGEP(lookup, 1));
  B.CreateBr(afterLookupBB);
}


void GNUstep::IMPCacher::SpeculativelyInline(Instruction *call, Function
    *function) {
  BasicBlock *beforeCallBB = call->getParent();
  BasicBlock *callBB = SplitBlock(beforeCallBB, call, Owner);
  BasicBlock *inlineBB = BasicBlock::Create(Context, "inline",
      callBB->getParent());


  BasicBlock::iterator iter = call;
  iter++;

  BasicBlock *afterCallBB = SplitBlock(iter->getParent(), iter, Owner);

  removeTerminator(beforeCallBB);

  // Put a branch before the call, testing whether the callee really is the
  // function
  IRBuilder<> B = IRBuilder<>(beforeCallBB);
  Value *callee = call->getOperand(0);
  if (callee->getType() != function->getType()) {
    callee = B.CreateBitCast(callee, function->getType());
  }
  Value *isInlineValid = B.CreateICmpEQ(callee, function);
  B.CreateCondBr(isInlineValid, inlineBB, callBB);

  // In the inline BB, add a copy of the call, but this time calling the real
  // version.
  Instruction *inlineCall = call->clone();
  inlineBB->getInstList().push_back(inlineCall);
  inlineCall->setOperand(0, function);

  B.SetInsertPoint(inlineBB);
  B.CreateBr(afterCallBB);

  // Unify the return values
  if (call->getType() != Type::getVoidTy(Context)) {
    B.SetInsertPoint(afterCallBB, afterCallBB->begin());
    PHINode *phi = B.CreatePHI(call->getType());
    call->replaceAllUsesWith(phi);
    phi->addIncoming(call, callBB);
    phi->addIncoming(inlineCall, inlineBB);
  }

  // Really do the real inlining
  InlineFunctionInfo IFI(0, 0);
  if (CallInst *c = dyn_cast<CallInst>(inlineCall)) {
    InlineFunction(c, IFI);
  } else if (InvokeInst *c = dyn_cast<InvokeInst>(inlineCall)) {
    InlineFunction(c, IFI);
  }
}

// Cleanly removes a terminator instruction.
void GNUstep::removeTerminator(BasicBlock *BB) {
  TerminatorInst *BBTerm = BB->getTerminator();

  // Remove the BB as a predecessor from all of  successors
  for (unsigned i = 0, e = BBTerm->getNumSuccessors(); i != e; ++i) {
    BBTerm->getSuccessor(i)->removePredecessor(BB);
  }

  BBTerm->replaceAllUsesWith(UndefValue::get(BBTerm->getType()));
  // Remove the terminator instruction itself.
  BBTerm->eraseFromParent();
}
