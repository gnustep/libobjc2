#pragma clang diagnostic ignored "-Wat-protocol"
@protocol P;

Protocol *getProtocol(void)
{
	return @protocol(P);
}
