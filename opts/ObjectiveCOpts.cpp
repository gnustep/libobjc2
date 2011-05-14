#include "llvm/Pass.h"
#include "llvm/Module.h"

#include "ObjectiveCOpts.h"

using namespace llvm;
namespace 
{
  class ObjectiveCOpts : public ModulePass {
    ModulePass *ClassIMPCachePass;
    ModulePass *ClassLookupCachePass;
    ModulePass *ClassMethodInliner;
    FunctionPass *GNUNonfragileIvarPass;
    FunctionPass *GNULoopIMPCachePass;

    public:
    static char ID;
    ObjectiveCOpts() : ModulePass(ID) {
      ClassIMPCachePass = createClassIMPCachePass();
      ClassLookupCachePass = createClassLookupCachePass();
      ClassMethodInliner = createClassMethodInliner();
      GNUNonfragileIvarPass = createGNUNonfragileIvarPass();
      GNULoopIMPCachePass = createGNULoopIMPCachePass();
    }
    virtual ~ObjectiveCOpts() {
      delete ClassIMPCachePass;
      delete ClassMethodInliner;
      delete ClassLookupCachePass;
      delete GNULoopIMPCachePass;
      delete GNUNonfragileIvarPass;
    }

    virtual bool runOnModule(Module &Mod) {
      bool modified;
      modified = ClassIMPCachePass->runOnModule(Mod);
      modified |= ClassLookupCachePass->runOnModule(Mod);
      modified |= ClassMethodInliner->runOnModule(Mod);

      for (Module::iterator F=Mod.begin(), fend=Mod.end() ;
          F != fend ; ++F) {

        if (F->isDeclaration()) { continue; }
        modified |= GNUNonfragileIvarPass->runOnFunction(*F);
        modified |= GNULoopIMPCachePass->runOnFunction(*F);
      }

      return modified;
    };
  };

  char ObjectiveCOpts::ID = 0;
  RegisterPass<ObjectiveCOpts> X("gnu-objc", 
          "Run all of the GNUstep Objective-C runtimm optimisations");

}

unsigned char GNUstep::ClassIMPCacheID;
unsigned char GNUstep::ClassMethodInlinerID;
unsigned char GNUstep::ClassLookupCacheID;
unsigned char GNUstep::LoopIMPCacheID;
unsigned char GNUstep::NonfragileIvarID;
