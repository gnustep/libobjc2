/**
 * Compatibility header that wraps LLVM API breakage and lets us compile with
 * old and new versions of LLVM.
 */

__attribute((unused)) static inline 
PHINode* CreatePHI(const Type *Ty,
                   unsigned NumReservedValues,
                   const Twine &NameStr="",
                   Instruction *InsertBefore=0) {
#if LLVM_MAJOR < 3
    PHINode *phi = PHINode::Create(Ty, NameStr, InsertBefore);
    phi->reserveOperandSpace(NumReservedValues);
    return phi;
#else
    return PHINode::Create(Ty, NumReservedValues, NameStr, InsertBefore);
#endif
}

__attribute((unused)) static inline 
MDNode* CreateMDNode(LLVMContext &C,
                     Value *V) {
#if LLVM_MAJOR < 3
  return MDNode::get(C, &V, 1);
#else
  ArrayRef<Value*> val(V);
  return MDNode::get(C, val);
#endif
}

#if LLVM_MAJOR < 3
#define GetStructType(context, ...) StructType::get(context, __VA_ARGS__)
#else
#define GetStructType(context, ...) StructType::get(__VA_ARGS__)
#endif

__attribute((unused)) static inline 
Constant* GetConstantStruct(LLVMContext &C, const std::vector<Constant*>
		&V, bool Packed) {
#if LLVM_MAJOR < 3
	return ConstantStruct::get(C, V, Packed);
#else
	return ConstantStruct::getAnon(C, V, Packed);
#endif
}
