/* use __USER_LABEL_PREFIX__ if it is available */
#if defined(__USER_LABEL_PREFIX__)
#define CDECL_PREFIX(x) x
#define CDECL(symbol) CDECL_PREFIX(__USER_LABEL_PREFIX__) ## symbol
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

