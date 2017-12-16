/* use __USER_LABEL_PREFIX__ if it is available */
#if defined(__USER_LABEL_PREFIX__)
    /* define symbol here for each possible non-empty prefix */
#   define ULP_  1
    /* define macro depending on result */
#   define ULP_PASTE(x,y) X##y
#   if ULP_PASTE(ULP, __USER_LABEL_PREFIX__) == 1
#       define CDECL(symbol) _##symbol
#   else
#       define CDECL(symbol) symbol
#   endif
#elif defined(__WIN32__) || defined(__APPLE__)
#define CDECL(symbol) _##symbol
#else
#define CDECL(symbol) symbol
#endif

#if __ELF__
#define TYPE_DIRECTIVE(symbol, symboltype) .type symbol, symboltype
#else
#define TYPE_DIRECTIVE(symbol, symboltype)
#endif

