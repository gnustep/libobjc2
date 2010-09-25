#import <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <assert.h>

@interface Foo : NSObject
{
  id a;
}
- (void) aMethod;
+ (void) aMethod;
@end

@interface Bar : Foo
{
  id b;
}
- (void) anotherMethod;
+ (void) anotherMethod;
@end


@implementation Foo
- (void) aMethod
{
}
+ (void) aMethod
{
}
@end

@implementation Bar
- (void) anotherMethod
{
}
+ (void) anotherMethod
{
}
@end


void testInvalidArguments()
{
  assert(NO == class_conformsToProtocol([NSObject class], NULL));
  assert(NO == class_conformsToProtocol(Nil, NULL));
  assert(NO == class_conformsToProtocol(Nil, @protocol(NSCoding)));
  assert(NULL == class_copyIvarList(Nil, NULL));
  assert(NULL == class_copyMethodList(Nil, NULL));
  assert(NULL == class_copyPropertyList(Nil, NULL));
  assert(NULL == class_copyProtocolList(Nil, NULL));
  assert(nil == class_createInstance(Nil, 0));
  assert(0 == class_getVersion(Nil));
  assert(NO == class_isMetaClass(Nil));
  assert(Nil == class_getSuperclass(Nil));
        
  assert(NULL == method_getName(NULL));
  assert(NULL == method_copyArgumentType(NULL, 0));
  assert(NULL == method_copyReturnType(NULL));
  method_exchangeImplementations(NULL, NULL);
  assert((IMP)NULL == method_setImplementation(NULL, (IMP)NULL));
  assert((IMP)NULL == method_getImplementation(NULL));
  method_getArgumentType(NULL, 0, NULL, 0);
  assert(0 == method_getNumberOfArguments(NULL));
  assert(NULL == method_getTypeEncoding(NULL));
  method_getReturnType(NULL, NULL, 0);
  
  assert(NULL == ivar_getName(NULL));
  assert(0 == ivar_getOffset(NULL));
  assert(NULL == ivar_getTypeEncoding(NULL));
  
  assert(nil == objc_getProtocol(NULL));
  
  assert(0 == strcmp("<null selector>", sel_getName((SEL)0)));
  assert((SEL)0 == sel_getUid(NULL));
  assert(0 != sel_getUid("")); // the empty string is permitted as a selector
  assert(0 == strcmp("", sel_getName(sel_getUid(""))));
  assert(YES == sel_isEqual((SEL)0, (SEL)0));
  
  printf("testInvalidArguments() passed\n");
}

void testAMethod(Method m)
{
  assert(NULL != m);
  assert(0 == strcmp("aMethod", sel_getName(method_getName(m))));
  
  printf("testAMethod() passed\n");
}

void testGetMethod()
{
  testAMethod(class_getClassMethod([Bar class], @selector(aMethod)));
  testAMethod(class_getClassMethod([Bar class], sel_getUid("aMethod")));
}

int main (int argc, const char * argv[])
{
  testInvalidArguments();
  testGetMethod();
  printf("Instance of NSObject: %p\n", class_createInstance([NSObject class], 0));
  return 0;
}
