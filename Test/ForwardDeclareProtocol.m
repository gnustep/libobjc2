#pragma clang diagnostic ignored "-Wat-protocol"
@protocol P;

Protocol *getProtocol(void)
{
// Don't try to compile this on known-broken compilers.
#if !defined(__clang__)
	return @protocol(P);
#elif __clang_major__ > 6
	return @protocol(P);
#else
	return 0;
#endif
}
