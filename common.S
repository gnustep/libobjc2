#if defined(__WIN32__) || defined(__APPLE__)
#define CDECL(symbol) _##symbol
#else
#define CDECL(symbol) symbol
#endif

#if HAS_TYPE_DIRECTIVE
#define TYPE_DIRECTIVE(symbol, symboltype) .type symbol, symboltype
#else
#define TYPE_DIRECTIVE(symbol, symboltype)
#endif

