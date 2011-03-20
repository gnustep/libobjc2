#ifdef __cplusplus
extern "C" {
#endif
void *__cxa_allocate_exception(size_t thrown_size);
struct _Unwind_Exception *objc_init_cxx_exception(void *thrown_exception);
_Unwind_Reason_Code  __gxx_personality_v0(int version,
                                          _Unwind_Action actions,
                                          uint64_t exceptionClass,
                                          struct _Unwind_Exception *exceptionObject,
                                          struct _Unwind_Context *context);
void __cxa_free_exception(void *thrown_exception);
void *objc_object_for_cxx_exception(void *thrown_exception);

void print_type_info(void *thrown_exception);


#ifdef __cplusplus
}
#endif
