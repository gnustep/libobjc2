llvm::ModulePass *createClassIMPCachePass(void);
llvm::ModulePass *createClassLookupCachePass(void);
llvm::ModulePass *createClassMethodInliner(void);
llvm::FunctionPass *createGNUNonfragileIvarPass(void);
llvm::FunctionPass *createGNULoopIMPCachePass(void);
llvm::ModulePass *createTypeFeedbackPass(void);
llvm::ModulePass *createTypeFeedbackDrivenInlinerPass(void);

namespace GNUstep {
  extern unsigned char ClassIMPCacheID;
  extern unsigned char ClassMethodInlinerID;
  extern unsigned char ClassLookupCacheID;
  extern unsigned char LoopIMPCacheID;
  extern unsigned char NonfragileIvarID;
}
