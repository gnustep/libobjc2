#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Support/CallSite.h"
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
    ClassIMPCachePass() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
        return false;
      GNUstep::IMPCacher cacher = GNUstep::IMPCacher(M.getContext(), this);
      IntTy = (sizeof(int) == 4 ) ? Type::getInt32Ty(M.getContext()) :
          Type::getInt64Ty(M.getContext()) ;
      bool modified = false;

      unsigned MessageSendMDKind = M.getContext().getMDKindID("GNUObjCMessageSend");

      for (Module::iterator F=M.begin(), fend=M.end() ;
          F != fend ; ++F) {

        if (F->isDeclaration()) { continue; }

        SmallVector<std::pair<CallSite, bool>, 16> Lookups;

        for (Function::iterator i=F->begin(), end=F->end() ;
            i != end ; ++i) {
          for (BasicBlock::iterator b=i->begin(), last=i->end() ;
              b != last ; ++b) {
            CallSite call(b);
            if (call.getInstruction()) {
              Value *callee = call.getCalledValue()->stripPointerCasts();
              if (Function *func = dyn_cast<Function>(callee)) {
                if (func->getName() == "objc_msg_lookup_sender") {
                  MDNode *messageType = 
                    call.getInstruction()->getMetadata(MessageSendMDKind);
                  if (0 == messageType) { continue; }
                  if (cast<ConstantInt>(messageType->getOperand(2))->isOne()) {
                    Lookups.push_back(std::pair<CallSite, bool>(call, false));
                  }
                } else if (func->getName() == "objc_slot_lookup_super") {
                  Lookups.push_back(std::pair<CallSite, bool>(call, true));
                }
              }
            }
          }
        }
        for (SmallVectorImpl<std::pair<CallSite, bool> >::iterator
            i=Lookups.begin(), e=Lookups.end() ; e!=i ; i++) {
          Instruction *call = i->first.getInstruction();
          const Type *SlotPtrTy = call->getType();

          Value *slot = new GlobalVariable(M, SlotPtrTy, false,
              GlobalValue::PrivateLinkage, Constant::getNullValue(SlotPtrTy),
              "slot");
          Value *version = new GlobalVariable(M, IntTy, false,
              GlobalValue::PrivateLinkage, Constant::getNullValue(IntTy),
              "version");
          cacher.CacheLookup(call, slot, version, i->second);
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
