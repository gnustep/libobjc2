namespace llvm
{
  class LLVMContext;
  class Pass;
  class PointerType;
  class IntegerType;
  class CallInst;
}

using namespace llvm;

namespace GNUstep
{
  class IMPCacher
  {
    private:
      LLVMContext &Context;
      Pass *Owner;
      const PointerType *PtrTy;
      const PointerType *IdTy;
      const IntegerType *IntTy;
    public:
      IMPCacher(LLVMContext &C, Pass *owner) : Context(C), Owner(owner) {
        PtrTy = Type::getInt8PtrTy(Context);
        // FIXME: 64-bit.
        IntTy = Type::getInt32Ty(Context);
        IdTy = PointerType::getUnqual(PtrTy);
      }
      void CacheLookup(CallInst *lookup, Value *slot, Value *version);
  };
}
