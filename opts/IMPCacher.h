namespace llvm
{
  class BasicBlock;
  class CallInst;
  class Function;
  class Instruction;
  class IntegerType;
  class LLVMContext;
  class MDNode;
  class Pass;
  class PointerType;
  class Value;
}

using namespace llvm;

namespace GNUstep
{
  class IMPCacher
  {
    private:
      LLVMContext &Context;
      MDNode *AlreadyCachedFlag;
      unsigned IMPCacheFlagKind;
      Pass *Owner;
      const PointerType *PtrTy;
      const PointerType *IdTy;
      const IntegerType *IntTy;
    public:
      IMPCacher(LLVMContext &C, Pass *owner);
      void CacheLookup(CallInst *lookup, Value *slot, Value *version);
      void SpeculativelyInline(Instruction *call, Function *function);
  };

  void removeTerminator(BasicBlock *BB);
}
