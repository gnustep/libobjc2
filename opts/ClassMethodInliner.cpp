#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/LLVMContext.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Analysis/LoopInfo.h"
#include "IMPCacher.h"
#include <string>

using namespace llvm;
using std::string;

// Mangle a method name
//
// From clang:
static std::string SymbolNameForMethod(const std::string &ClassName, const
      std::string &CategoryName, const std::string &MethodName, bool isClassMethod)
{
    std::string MethodNameColonStripped = MethodName;
      std::replace(MethodNameColonStripped.begin(), MethodNameColonStripped.end(),
                ':', '_');
        return std::string(isClassMethod ? "_c_" : "_i_") + ClassName + "_" +
              CategoryName + "_" + MethodNameColonStripped;
}

namespace 
{
  class ClassMethodInliner : public ModulePass 
  {
    const IntegerType *IntTy;

    public:
    static char ID;
    ClassMethodInliner() : ModulePass((intptr_t)&ID) {}

    virtual bool runOnModule(Module &M) {
      unsigned MessageSendMDKind = M.getContext().getMDKindID("GNUObjCMessageSend");

      GNUstep::IMPCacher cacher = GNUstep::IMPCacher(M.getContext(), this);
      // FIXME: ILP64
      IntTy = Type::getInt32Ty(M.getContext());
      bool modified = false;

      for (Module::iterator F=M.begin(), fend=M.end() ;
          F != fend ; ++F) {

        SmallVector<CallInst*, 16> messages;

        if (F->isDeclaration()) { continue; }

        SmallVector<CallInst*, 16> Lookups;

        for (Function::iterator i=F->begin(), end=F->end() ;
            i != end ; ++i) {
          for (BasicBlock::iterator b=i->begin(), last=i->end() ;
              b != last ; ++b) {
            // FIXME: InvokeInst
            if (CallInst *call = dyn_cast<CallInst>(b)) {
              Instruction *callee = 
                dyn_cast<Instruction>(call->getCalledValue()->stripPointerCasts());
              if (0 == callee) { continue; }
              MDNode *messageType = callee->getMetadata(MessageSendMDKind);
              if (0 == messageType) { continue; }
              messages.push_back(call);
            }
          }
        }
        for (SmallVectorImpl<CallInst*>::iterator i=messages.begin(), 
            e=messages.end() ; e!=i ; i++) {

          Instruction *callee = 
            dyn_cast<Instruction>((*i)->getCalledValue()->stripPointerCasts());
          MDNode *messageType = callee->getMetadata(MessageSendMDKind);
          StringRef sel = cast<MDString>(messageType->getOperand(0))->getString();
          StringRef cls = cast<MDString>(messageType->getOperand(1))->getString();
          StringRef functionName = SymbolNameForMethod(cls, "", sel, true);
          Function *method = M.getFunction(functionName);

          if (0 == method || method->isDeclaration()) { continue; }

          cacher.SpeculativelyInline(*i, method);
        }
      }
      return modified;
    }
  };

  char ClassMethodInliner::ID = 0;
  RegisterPass<ClassMethodInliner> X("gnu-class-method-inline", 
          "Inline class methods");
}

ModulePass *createClassMethodInliner(void)
{
  return new ClassMethodInliner();
}
