// detect_arch.c
#if defined(__aarch64__)
#error aarch64
#elif defined(__arm__)
#error arm
#elif defined(__i386__)
#error i386
#elif defined(__x86_64__)
#error x86_64
#else
#error unknown
#endif