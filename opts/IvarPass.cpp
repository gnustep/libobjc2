#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/GlobalAlias.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Constants.h"
#include <string>

using namespace llvm;
using std::string;

namespace 
{
  class GNUNonfragileIvarPass : public FunctionPass 
  {

    public:
    static char ID;
    GNUNonfragileIvarPass() : FunctionPass((intptr_t)&ID) {}

    Module *M;
    size_t PointerSize;
    virtual bool doInitialization(Module &Mod) {
      M = &Mod;
      PointerSize = 8;
      if (M->getPointerSize() == Module::Pointer32) 
        PointerSize = 4;
      return false;  
    }

    std::string getSuperName(Constant *ClsStruct) {
      GlobalVariable *name =
        cast<GlobalVariable>(ClsStruct->getOperand(1)->getOperand(0));
      return cast<ConstantArray>(name->getInitializer())->getAsString();
    }

    size_t sizeOfClass(const std::string &className) {
      if (className.compare(0, 8, "NSObject") == 0 || 
          className.compare(0, 6, "Object") == 0) {
        return PointerSize;
      }
      GlobalVariable *Cls = M->getGlobalVariable("_OBJC_CLASS_" + className);
      if (!Cls) return 0;
      Constant *ClsStruct = Cls->getInitializer();
      // Size is initialized to be negative.
      ConstantInt *Size = cast<ConstantInt>(ClsStruct->getOperand(5));
      return sizeOfClass(getSuperName(ClsStruct)) - Size->getSExtValue();
    }

    size_t hardCodedOffset(const StringRef &className, 
                           const StringRef &ivarName) {
      GlobalVariable *Cls = M->getGlobalVariable(("_OBJC_CLASS_" + className).str(), true);
      if (!Cls) return 0;
      Constant *ClsStruct = Cls->getInitializer();
      size_t superSize = sizeOfClass(getSuperName(ClsStruct));
      if (!superSize) return 0;
      ConstantStruct *IvarStruct = cast<ConstantStruct>(
          cast<GlobalVariable>(ClsStruct->getOperand(6))->getInitializer());
      int ivarCount = cast<ConstantInt>(IvarStruct->getOperand(0))->getSExtValue();
      Constant *ivars = IvarStruct->getOperand(1);
      for (int i=0 ; i<ivarCount ; i++) {
        Constant *ivar = ivars->getOperand(i);
        GlobalVariable *name =
          cast<GlobalVariable>(ivar->getOperand(0)->getOperand(0));
        std::string ivarNameStr = 
          cast<ConstantArray>(name->getInitializer())->getAsString();
        if (ivarNameStr.compare(0, ivarName.size(), ivarName.str()) == 0)
          return superSize +
            cast<ConstantInt>(ivar->getOperand(2))->getSExtValue();
      }
      return 0;
    }

    virtual bool runOnFunction(Function &F) {
      bool modified = false;
      //llvm::cerr << "IvarPass: " << F.getName() << "\n";
      for (Function::iterator i=F.begin(), end=F.end() ;
          i != end ; ++i) {
        for (BasicBlock::iterator b=i->begin(), last=i->end() ;
            b != last ; ++b) {
          if (LoadInst *indirectload = dyn_cast<LoadInst>(b)) {
            if (LoadInst *load = dyn_cast<LoadInst>(indirectload->getOperand(0))) {
              if (GlobalVariable *ivar =
                  dyn_cast<GlobalVariable>(load->getOperand(0))) {
                StringRef variableName = ivar->getName();
                if (!variableName.startswith("__objc_ivar_offset_"))
                  break;
                size_t prefixLength = strlen("__objc_ivar_offset_");

                StringRef suffix = variableName.substr(prefixLength,
                    variableName.size()-prefixLength);

                std::pair<StringRef,StringRef> parts = suffix.split('.');
                StringRef className = parts.first;
                StringRef ivarName = parts.second;

                // If the class, and all superclasses, are visible in this module
                // then we can hard-code the ivar offset
                if (size_t offset = hardCodedOffset(className, ivarName)) {
                  indirectload->replaceAllUsesWith(ConstantInt::get(indirectload->getType(), offset));
                  modified = true;
                } else {
                  // If the class was compiled with the new 
                  if (Value *offset =
                     M->getGlobalVariable(("__objc_ivar_offset_value_" + suffix).str())) {
                    load->replaceAllUsesWith(offset);
                    modified = true;
                  }
                }
              }
            }
          }
        }
      }
      return modified;
    }
  };

  char GNUNonfragileIvarPass::ID = 0;
  RegisterPass<GNUNonfragileIvarPass> X("gnu-nonfragile-ivar", "Ivar fragility pass");
}

FunctionPass *createGNUNonfragileIvarPass(void)
{
  return new GNUNonfragileIvarPass();
}
