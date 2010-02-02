/*
 * Category on NSObject to support automatic cycle detection.
 */
@implementation NSObject (CycleDetection)
/**
 * Increments the 16-bit reference count.  Replaces version that sets a
 * one-word reference count.
 */
- (id) retain
{
	return GCRetain(self);
}
/**
 * Decrements the reference count for an object.  If the reference count
 * reaches zero, calls -dealloc.  If the reference count is not zero then the
 * objectt may be part of a cycle.  In this case, it is addded to a buffer and
 * cycle detection is later invoked.
 */
- (void) release
{
	GCRelease(self);
}
/**
 * Dealloc now does not free objects, they are freed after -dealloc is called.
 */
- (void) dealloc
{
}
@end
