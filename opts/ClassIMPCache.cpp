#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Analysis/LoopInfo.h"
#include "IMPCacher.h"
#include <string>

using namespace llvm;
using std::string;

namespace 
{
  class ClassIMPCachePass : public ModulePass 
  {
    const IntegerType *IntTy;

    public:
    static char ID;
    ClassIMPCachePass() : ModulePass((intptr_t)&ID) {}

    virtual bool runOnModule(Module &M) {
      GNUstep::IMPCacher cacher = GNUstep::IMPCacher(M.getContext(), this);
      // FIXME: ILP64
      IntTy = Type::getInt32Ty(M.getContext());
      bool modified = false;

      for (Module::iterator F=M.begin(), fend=M.end() ;
          F != fend ; ++F) {

        if (F->isDeclaration()) { continue; }

        SmallVector<CallInst*, 16> Lookups;
        BasicBlock *entry = &F->getEntryBlock();

        for (Function::iterator i=F->begin(), end=F->end() ;
            i != end ; ++i) {
          for (BasicBlock::iterator b=i->begin(), last=i->end() ;
              b != last ; ++b) {
            if (CallInst *call = dyn_cast<CallInst>(b)) {
              Value *callee = call->getCalledValue()->stripPointerCasts();
              if (Function *func = dyn_cast<Function>(callee)) {
                if (func->getName() == "objc_msg_lookup_sender") {
                  // TODO: Move this to a helper
                  Value *receiverPtr = call->getOperand(1);
                  Value *receiver = 0;
                  // Find where the receiver comes from
                  for (BasicBlock::iterator start=i->begin(),s=b ; s!=start ; s--) {
                    if (StoreInst *store = dyn_cast<StoreInst>(s)) {
                      if (store->getOperand(1) == receiverPtr) {
                        receiver = store->getOperand(0);
                        break;
                      }
                    }
                  }
                  if (0 == receiver) { continue; }
                  if (CallInst *classLookup = dyn_cast<CallInst>(receiver)) {
                    Value *lookupVal = classLookup->getCalledValue()->stripPointerCasts();
                    if (Function *lookupFunc = dyn_cast<Function>(lookupVal)) {
                      if (lookupFunc->getName() == "objc_lookup_class") {
                        modified = true;
                        Lookups.push_back(call);
                      }
                    }
                  }
                }
              }
            }
          }
        }
        IRBuilder<> B = IRBuilder<>(entry);
        for (SmallVectorImpl<CallInst*>::iterator i=Lookups.begin(), 
            e=Lookups.end() ; e!=i ; i++) {
          const Type *SlotPtrTy = (*i)->getType();

          Value *slot = new GlobalVariable(M, SlotPtrTy, false,
              GlobalValue::PrivateLinkage, Constant::getNullValue(SlotPtrTy),
              "slot");
          Value *version = new GlobalVariable(M, IntTy, false,
              GlobalValue::PrivateLinkage, Constant::getNullValue(IntTy),
              "version");
          cacher.CacheLookup(*i, slot, version);
        }
      }
      return modified;
    }
  };

  char ClassIMPCachePass::ID = 0;
  RegisterPass<ClassIMPCachePass> X("gnu-class-imp-cache", 
          "Cache IMPs for class messages");
}

ModulePass *createClassIMPCachePass(void)
{
  return new ClassIMPCachePass();
}
