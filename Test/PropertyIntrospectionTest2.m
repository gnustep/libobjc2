#include "Test.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#pragma GCC diagnostic ignored "-Wobjc-property-no-attribute"

enum FooManChu { FOO, MAN, CHU };
struct YorkshireTeaStruct { int pot; char lady; };
typedef struct YorkshireTeaStruct YorkshireTeaStructType;
union MoneyUnion { float alone; double down; };

#ifndef __has_attribute
#define __has_attribute(x)  0
#endif

#if __has_attribute(objc_root_class)
__attribute__((objc_root_class))
#endif
@interface PropertyTest
{
@public
	Class isa;
	char charDefault;
	double doubleDefault;
	enum FooManChu enumDefault;
	float floatDefault;
	int intDefault;
	long longDefault;
	short shortDefault;
	signed signedDefault;
	struct YorkshireTeaStruct structDefault;
	YorkshireTeaStructType typedefDefault;
	union MoneyUnion unionDefault;
	unsigned unsignedDefault;
	int (*functionPointerDefault)(char *);
	int *intPointer;
	void *voidPointerDefault;
	int intSynthEquals;
	int intSetterGetter;
	int intReadonly;
	int intReadonlyGetter;
	int intReadwrite;
	int intAssign;
	id idDefault;
	id idRetain;
	id idCopy;
	id idWeak;
	id idStrong;
	int intNonatomic;
	id idReadonlyCopyNonatomic;
	id idReadonlyRetainNonatomic;
	id idReadonlyWeakNonatomic;
	id _idOther;
}
@property char charDefault;
@property double doubleDefault;
@property enum FooManChu enumDefault;
@property float floatDefault;
@property int intDefault;
@property long longDefault;
@property short shortDefault;
@property signed signedDefault;
@property struct YorkshireTeaStruct structDefault;
@property YorkshireTeaStructType typedefDefault;
@property union MoneyUnion unionDefault;
@property unsigned unsignedDefault;
@property int (*functionPointerDefault)(char *);
@property int *intPointer;
@property void *voidPointerDefault;
@property(getter=intGetFoo, setter=intSetFoo:) int intSetterGetter;
@property(readonly) int intReadonly;
@property(getter=isIntReadOnlyGetter, readonly) int intReadonlyGetter;
@property(readwrite) int intReadwrite;
@property(assign) int intAssign;
@property id idDefault;
@property(retain) id idRetain;
@property(copy) id idCopy;
@property(weak) id idWeak;
@property(strong) id idStrong;
@property(nonatomic) int intNonatomic;
@property(nonatomic, readonly, copy) id idReadonlyCopyNonatomic;
@property(nonatomic, readonly, retain) id idReadonlyRetainNonatomic;
@property(nonatomic, readonly, weak) id idReadonlyWeakNonatomic;
@property(retain) id idOther;
@property(retain) id idDynamic;
@property(retain, nonatomic, getter=dynamicGetterSetter, setter=setDynamicGetterSetter:) id idDynamicGetterSetter;
@end

@interface PropertyTest (Informal)
- (void)setStructDefault2: (struct YorkshireTeaStruct)tp;
- (void)setIntDefault2: (int)i;
- (struct YorkshireTeaStruct)structDefault2;
- (int)intDefault2;
@end


@implementation PropertyTest
@synthesize charDefault;
@synthesize doubleDefault;
@synthesize enumDefault;
@synthesize floatDefault;
@synthesize intDefault;
@synthesize longDefault;
@synthesize shortDefault;
@synthesize signedDefault;
@synthesize structDefault;
@synthesize typedefDefault;
@synthesize unionDefault;
@synthesize unsignedDefault;
@synthesize functionPointerDefault;
@synthesize intPointer;
@synthesize voidPointerDefault;
@synthesize intSetterGetter;
@synthesize intReadonly;
@synthesize intReadonlyGetter;
@synthesize intReadwrite;
@synthesize intAssign;
@synthesize idDefault;
@synthesize idRetain;
@synthesize idCopy;
@synthesize idWeak;
@synthesize idStrong;
@synthesize intNonatomic;
@synthesize idReadonlyCopyNonatomic;
@synthesize idReadonlyRetainNonatomic;
@synthesize idReadonlyWeakNonatomic;
@synthesize idOther = _idOther;
@dynamic idDynamic;
@dynamic idDynamicGetterSetter;
@end

@protocol ProtocolTest
@property char charDefault;
@property double doubleDefault;
@property enum FooManChu enumDefault;
@property float floatDefault;
@property int intDefault;
@property long longDefault;
@property short shortDefault;
@property signed signedDefault;
@property struct YorkshireTeaStruct structDefault;
@property YorkshireTeaStructType typedefDefault;
@property union MoneyUnion unionDefault;
@property unsigned unsignedDefault;
@property int (*functionPointerDefault)(char *);
@property int *intPointer;
@property void *voidPointerDefault;
@property(getter=intGetFoo, setter=intSetFoo:) int intSetterGetter;
@property(readonly) int intReadonly;
@property(getter=isIntReadOnlyGetter, readonly) int intReadonlyGetter;
@property(readwrite) int intReadwrite;
@property(assign) int intAssign;
@property id idDefault;
@property(retain) id idRetain;
@property(copy) id idCopy;
@property(weak) id idWeak;
@property(strong) id idStrong;
@property(nonatomic) int intNonatomic;
@property(nonatomic, readonly, copy) id idReadonlyCopyNonatomic;
@property(nonatomic, readonly, retain) id idReadonlyRetainNonatomic;
@property(nonatomic, readonly, weak) id idReadonlyWeakNonatomic;
@property(retain) id idOther;
@property(retain) id idDynamic;
@property(retain, nonatomic, getter=dynamicGetterSetter, setter=setDynamicGetterSetter:) id idDynamicGetterSetter;
@end

#if __has_attribute(objc_root_class)
__attribute__((objc_root_class))
#endif
@interface PropertyProtocolTest <ProtocolTest>
{
	Class isa;
	char charDefault;
	double doubleDefault;
	enum FooManChu enumDefault;
	float floatDefault;
	int intDefault;
	long longDefault;
	short shortDefault;
	signed signedDefault;
	struct YorkshireTeaStruct structDefault;
	YorkshireTeaStructType typedefDefault;
	union MoneyUnion unionDefault;
	unsigned unsignedDefault;
	int (*functionPointerDefault)(char *);
	int *intPointer;
	void *voidPointerDefault;
	int intSynthEquals;
	int intSetterGetter;
	int intReadonly;
	int intReadonlyGetter;
	int intReadwrite;
	int intAssign;
	id idDefault;
	id idRetain;
	id idCopy;
	id idWeak;
	id idStrong;
	int intNonatomic;
	id idReadonlyCopyNonatomic;
	id idReadonlyRetainNonatomic;
	id idReadonlyWeakNonatomic;
	id _idOther;
}
@end

@implementation PropertyProtocolTest
@synthesize charDefault;
@synthesize doubleDefault;
@synthesize enumDefault;
@synthesize floatDefault;
@synthesize intDefault;
@synthesize longDefault;
@synthesize shortDefault;
@synthesize signedDefault;
@synthesize structDefault;
@synthesize typedefDefault;
@synthesize unionDefault;
@synthesize unsignedDefault;
@synthesize functionPointerDefault;
@synthesize intPointer;
@synthesize voidPointerDefault;
@synthesize intSetterGetter;
@synthesize intReadonly;
@synthesize intReadonlyGetter;
@synthesize intReadwrite;
@synthesize intAssign;
@synthesize idDefault;
@synthesize idRetain;
@synthesize idCopy;
@synthesize idWeak;
@synthesize idStrong;
@synthesize intNonatomic;
@synthesize idReadonlyCopyNonatomic;
@synthesize idReadonlyRetainNonatomic;
@synthesize idReadonlyWeakNonatomic;
@synthesize idOther = _idOther;
@dynamic idDynamic;
@dynamic idDynamicGetterSetter;
@end

#define ATTR(n, v)  (objc_property_attribute_t){(n), (v)}
#define ATTRS(...)  (objc_property_attribute_t[]){ __VA_ARGS__ }, \
						sizeof((objc_property_attribute_t[]){ __VA_ARGS__ }) / sizeof(objc_property_attribute_t)

static void testPropertyForProperty(objc_property_t p,
									const char *name,
									const char *types,
									objc_property_attribute_t* list,
									unsigned int size)
{
	assert(0 != p);
	assert(strcmp(name, property_getName(p)) == 0);
	const char *attrs = property_getAttributes(p);
	assert(0 != attrs);
	assert(strcmp(types, attrs) == 0);
	unsigned int attrsCount = 0;
	objc_property_attribute_t *attrsList = property_copyAttributeList(p, &attrsCount);
	assert(0 != attrsList);
    assert(attrsCount == size);
    for (unsigned int index=0; index<size; index++) {
        int found = 0;
        for (unsigned int attrsIndex=0; attrsIndex<attrsCount; attrsIndex++) {
            if (strcmp(attrsList[attrsIndex].name, list[index].name) == 0) {
                assert(strcmp(attrsList[attrsIndex].value, list[index].value) == 0);
                found = 1;
            }
        }
        assert(found);
    }
	free(attrsList);
	attrsList = property_copyAttributeList(p, NULL);
	assert(0 != attrsList);
	objc_property_attribute_t *ra;
	for (attrsCount = 0, ra = attrsList; ra->name != NULL; attrsCount++, ra++) {}
    assert(attrsCount == size);
	free(attrsList);
    for (unsigned int index=0; index<size; index++) {
        const char* value = property_copyAttributeValue(p, list[index].name);
        assert(0 != value);
        assert(strcmp(value, list[index].value) == 0);
    }
}

static void testPropertyForClass(Class testClass,
								 const char *name,
								 const char *types,
								 objc_property_attribute_t* list,
								 unsigned int size)
{
    testPropertyForProperty(class_getProperty(testClass, name), name, types, list, size);

	static int addPropertyForClassIndex = 0;
	char addPropertyName[32];
	sprintf(addPropertyName, "addPropertyForClass%d", ++addPropertyForClassIndex);
    assert(class_addProperty(testClass, addPropertyName, list, size));
    testPropertyForProperty(class_getProperty(testClass, addPropertyName), addPropertyName, types, list, size);
}

static void testPropertyForProtocol(Protocol *testProto,
									const char *name,
									const char *types,
									objc_property_attribute_t* list,
									unsigned int size)
{
    testPropertyForProperty(protocol_getProperty(testProto, name, YES, YES), name, types, list, size);

   	static int addPropertyForProtocolIndex = 0;
	char addPropertyName[32];
	sprintf(addPropertyName, "addPropertyForProtocol%d", ++addPropertyForProtocolIndex);
    protocol_addProperty(testProto, addPropertyName, list, size, YES, YES);
	assert(0 == protocol_getProperty(testProto, addPropertyName, YES, YES));
}

static void testProperty(const char *name, const char *types, objc_property_attribute_t* list, unsigned int size)
{
    testPropertyForProperty(class_getProperty(objc_getClass("PropertyTest"), name), name, types, list, size);
    testPropertyForProperty(class_getProperty(objc_getClass("PropertyProtocolTest"), name), name, types, list, size);
}

static void testAddPropertyForClass(Class testClass)
{
    objc_property_attribute_t emptyType = { "T", "i" };
    assert(!class_addProperty(testClass, NULL, &emptyType, 1));
    class_replaceProperty(testClass, NULL, &emptyType, 1);

    assert(class_addProperty(testClass, "addProperty1", ATTRS(ATTR("T", "@"))));
	testPropertyForProperty(class_getProperty(testClass, "addProperty1"),
							"addProperty1", "T@", ATTRS(ATTR("T", "@")));

    assert(class_addProperty(testClass, "addProperty2", ATTRS(ATTR("T", "@"),
															  ATTR("D", ""))));
	testPropertyForProperty(class_getProperty(testClass, "addProperty2"),
							"addProperty2", "T@,D", ATTRS(ATTR("T", "@"),
														  ATTR("D", "")));

    assert(class_addProperty(testClass, "addProperty3", ATTRS(ATTR("T", "@"),
															  ATTR("D", ""),
															  ATTR("V", "backingIvar"))));
	testPropertyForProperty(class_getProperty(testClass, "addProperty3"),
							"addProperty3", "T@,D,VbackingIvar", ATTRS(ATTR("T", "@"),
																	   ATTR("D", ""),
																	   ATTR("V", "backingIvar")));

    assert(class_addProperty(testClass, "addProperty4", ATTRS(ATTR("T", "@"),
															  ATTR("R", ""),
															  ATTR("&", ""),
															  ATTR("C", ""),
															  ATTR("W", ""),
															  ATTR("D", ""),
															  ATTR("V", "backingIvar"))));
	testPropertyForProperty(class_getProperty(testClass, "addProperty4"),
							"addProperty4", "T@,R,&,C,W,D,VbackingIvar", ATTRS(ATTR("T", "@"),
																			   ATTR("R", ""),
																			   ATTR("&", ""),
																			   ATTR("C", ""),
																			   ATTR("W", ""),
																			   ATTR("D", ""),
																			   ATTR("V", "backingIvar")));

    assert(class_addProperty(testClass, "addProperty5", ATTRS(ATTR("T", "@"),
															  ATTR("D", ""),
															  ATTR("W", ""),
															  ATTR("C", ""),
															  ATTR("&", ""),
															  ATTR("R", ""),
															  ATTR("V", "backingIvar"))));
	// The only concession to MacOS X is that we reorder the attributes string
#if __APPLE__
	testPropertyForProperty(class_getProperty(testClass, "addProperty5"),
							"addProperty5", "T@,D,W,C,&,R,VbackingIvar", ATTRS(ATTR("T", "@"),
																			   ATTR("R", ""),
																			   ATTR("&", ""),
																			   ATTR("C", ""),
																			   ATTR("W", ""),
																			   ATTR("D", ""),
																			   ATTR("V", "backingIvar")));
#else
	testPropertyForProperty(class_getProperty(testClass, "addProperty5"),
							"addProperty5", "T@,R,&,C,W,D,VbackingIvar", ATTRS(ATTR("T", "@"),
																			   ATTR("R", ""),
																			   ATTR("&", ""),
																			   ATTR("C", ""),
																			   ATTR("W", ""),
																			   ATTR("D", ""),
																			   ATTR("V", "backingIvar")));
#endif

    assert(class_addProperty(testClass, "replaceProperty", ATTRS(ATTR("T", "@"))));
	testPropertyForProperty(class_getProperty(testClass, "replaceProperty"),
							"replaceProperty", "T@", ATTRS(ATTR("T", "@")));

    assert(!class_addProperty(testClass, "replaceProperty", ATTRS(ATTR("T", "i"))));
	testPropertyForProperty(class_getProperty(testClass, "replaceProperty"),
							"replaceProperty", "T@", ATTRS(ATTR("T", "@")));

    class_replaceProperty(testClass, "replaceProperty", ATTRS(ATTR("T", "i")));
	testPropertyForProperty(class_getProperty(testClass, "replaceProperty"),
							"replaceProperty", "Ti", ATTRS(ATTR("T", "i")));
}

static void testAddProperty()
{
    testAddPropertyForClass(objc_getClass("PropertyTest"));
    testAddPropertyForClass(objc_getClass("PropertyProtocolTest"));
}

static void testAddPropertyForProtocol(Protocol *testProto)
{
    objc_property_attribute_t emptyType = { "T", "i" };
    protocol_addProperty(testProto, NULL, &emptyType, 1, YES, YES);

    protocol_addProperty(testProto, "addProperty1", ATTRS(ATTR("T", "@")), YES, YES);
    protocol_addProperty(testProto, "addProperty2", ATTRS(ATTR("T", "@"),
														  ATTR("D", "")), YES, YES);
    protocol_addProperty(testProto, "addProperty3", ATTRS(ATTR("T", "@"),
														  ATTR("D", ""),
														  ATTR("V", "backingIvar")), YES, YES);

	objc_registerProtocol(testProto);

	testPropertyForProperty(protocol_getProperty(testProto, "addProperty1", YES, YES),
							"addProperty1", "T@", ATTRS(ATTR("T", "@")));
	testPropertyForProperty(protocol_getProperty(testProto, "addProperty2", YES, YES),
							"addProperty2", "T@,D", ATTRS(ATTR("T", "@"),
														  ATTR("D", "")));
	testPropertyForProperty(protocol_getProperty(testProto, "addProperty3", YES, YES),
							"addProperty3", "T@,D,VbackingIvar", ATTRS(ATTR("T", "@"),
																	   ATTR("D", ""),
																	   ATTR("V", "backingIvar")));
}

static int intDefault2Getter(id self, SEL _cmd) {
    Ivar ivar = class_getInstanceVariable(objc_getClass("PropertyTest"), "intDefault");
    return (int)object_getIvar(self, ivar);
}

static void intDefault2Setter(id self, SEL _cmd, int value) {
    Ivar ivar = class_getInstanceVariable(objc_getClass("PropertyTest"), "intDefault");
    object_setIvar(self, ivar, (id)(intptr_t)value);
}

static struct YorkshireTeaStruct* structDefault2Getter(id self, SEL _cmd) {
    Ivar ivar = class_getInstanceVariable(objc_getClass("PropertyTest"), "structDefault");
    return (struct YorkshireTeaStruct*)object_getIvar(self, ivar);
}

void structDefault2Setter(id self, SEL _cmd, struct YorkshireTeaStruct* value) {
    Ivar ivar = class_getInstanceVariable(objc_getClass("PropertyTest"), "structDefault");
    object_setIvar(self, ivar, (id)value);
}

int main(void)
{
	testProperty("charDefault", "Tc,VcharDefault", ATTRS(ATTR("T", "c"), ATTR("V", "charDefault")));
	testProperty("doubleDefault", "Td,VdoubleDefault", ATTRS(ATTR("T", "d"), ATTR("V", "doubleDefault")));
	testProperty("enumDefault", "Ti,VenumDefault", ATTRS(ATTR("T", "i"), ATTR("V", "enumDefault")));
	testProperty("floatDefault", "Tf,VfloatDefault", ATTRS(ATTR("T", "f"), ATTR("V", "floatDefault")));
	testProperty("intDefault", "Ti,VintDefault", ATTRS(ATTR("T", "i"), ATTR("V", "intDefault")));
	if (sizeof(long) == 4)
	{
		testProperty("longDefault", "Tl,VlongDefault", ATTRS(ATTR("T", "l"), ATTR("V", "longDefault")));
	}
	else
	{
		testProperty("longDefault", "Tq,VlongDefault", ATTRS(ATTR("T", "q"), ATTR("V", "longDefault")));
	}
	testProperty("shortDefault", "Ts,VshortDefault", ATTRS(ATTR("T", "s"), ATTR("V", "shortDefault")));
	testProperty("signedDefault", "Ti,VsignedDefault", ATTRS(ATTR("T", "i"), ATTR("V", "signedDefault")));
	testProperty("structDefault", "T{YorkshireTeaStruct=ic},VstructDefault", ATTRS(ATTR("T", "{YorkshireTeaStruct=ic}"),
                                                                                   ATTR("V", "structDefault")));
	testProperty("typedefDefault", "T{YorkshireTeaStruct=ic},VtypedefDefault", ATTRS(ATTR("T", "{YorkshireTeaStruct=ic}"),
                                                                                     ATTR("V", "typedefDefault")));
	testProperty("unionDefault", "T(MoneyUnion=fd),VunionDefault", ATTRS(ATTR("T", "(MoneyUnion=fd)"),
                                                                         ATTR("V", "unionDefault")));
	testProperty("unsignedDefault", "TI,VunsignedDefault", ATTRS(ATTR("T", "I"), ATTR("V", "unsignedDefault")));
	testProperty("functionPointerDefault", "T^?,VfunctionPointerDefault", ATTRS(ATTR("T", "^?"), ATTR("V", "functionPointerDefault")));
	testProperty("intPointer", "T^i,VintPointer", ATTRS(ATTR("T", "^i"), ATTR("V", "intPointer")));
	testProperty("voidPointerDefault", "T^v,VvoidPointerDefault", ATTRS(ATTR("T", "^v"), ATTR("V", "voidPointerDefault")));
	testProperty("intSetterGetter", "Ti,GintGetFoo,SintSetFoo:,VintSetterGetter", ATTRS(ATTR("T", "i"),
                                                                                        ATTR("G", "intGetFoo"),
                                                                                        ATTR("S", "intSetFoo:"),
                                                                                        ATTR("V", "intSetterGetter")));
	testProperty("intReadonly", "Ti,R,VintReadonly", ATTRS(ATTR("T", "i"),
                                                           ATTR("R", ""),
                                                           ATTR("V", "intReadonly")));
	testProperty("intReadonlyGetter", "Ti,R,GisIntReadOnlyGetter,VintReadonlyGetter", ATTRS(ATTR("T", "i"),
                                                                                            ATTR("R", ""),
                                                                                            ATTR("G", "isIntReadOnlyGetter"),
                                                                                            ATTR("V", "intReadonlyGetter")));
	testProperty("intReadwrite", "Ti,VintReadwrite", ATTRS(ATTR("T", "i"), ATTR("V", "intReadwrite")));
	testProperty("intAssign", "Ti,VintAssign", ATTRS(ATTR("T", "i"), ATTR("V", "intAssign")));
	testProperty("idDefault", "T@,VidDefault", ATTRS(ATTR("T", "@"),
                                                     ATTR("V", "idDefault")));
	testProperty("idRetain", "T@,&,VidRetain", ATTRS(ATTR("T", "@"),
                                                     ATTR("&", ""),
                                                     ATTR("V", "idRetain")));
	testProperty("idCopy", "T@,C,VidCopy", ATTRS(ATTR("T", "@"),
                                                 ATTR("C", ""),
                                                 ATTR("V", "idCopy")));
	testProperty("idWeak", "T@,W,VidWeak", ATTRS(ATTR("T", "@"),
                                                 ATTR("W", ""),
                                                 ATTR("V", "idWeak")));
	testProperty("idStrong", "T@,&,VidStrong", ATTRS(ATTR("T", "@"),
                                                     ATTR("&", ""),
                                                     ATTR("V", "idStrong")));
	testProperty("intNonatomic", "Ti,N,VintNonatomic", ATTRS(ATTR("T", "i"),
                                                             ATTR("N", ""),
                                                             ATTR("V", "intNonatomic")));
	testProperty("idReadonlyCopyNonatomic", "T@,R,C,N,VidReadonlyCopyNonatomic", ATTRS(ATTR("T", "@"),
                                                                                     ATTR("R", ""),
                                                                                     ATTR("N", ""),
                                                                                     ATTR("V", "idReadonlyCopyNonatomic")));
	testProperty("idReadonlyRetainNonatomic", "T@,R,&,N,VidReadonlyRetainNonatomic", ATTRS(ATTR("T", "@"),
                                                                                         ATTR("R", ""),
                                                                                         ATTR("N", ""),
                                                                                         ATTR("V", "idReadonlyRetainNonatomic")));
	testProperty("idReadonlyWeakNonatomic", "T@,R,N,VidReadonlyWeakNonatomic", ATTRS(ATTR("T", "@"),
                                                                                     ATTR("R", ""),
                                                                                     ATTR("N", ""),
                                                                                     ATTR("V", "idReadonlyWeakNonatomic")));
	testProperty("idOther", "T@,&,V_idOther", ATTRS(ATTR("T", "@"), ATTR("&", ""), ATTR("V", "_idOther")));
	testProperty("idDynamic", "T@,&,D", ATTRS(ATTR("T", "@"), ATTR("&", ""), ATTR("D", "")));
	testProperty("idDynamicGetterSetter", "T@,&,D,N,GdynamicGetterSetter,SsetDynamicGetterSetter:", ATTRS(ATTR("T", "@"),
                                                                                                          ATTR("&", ""),
                                                                                                          ATTR("D", ""),
                                                                                                          ATTR("N", ""),
                                                                                                          ATTR("G", "dynamicGetterSetter"),
                                                                                                          ATTR("S", "setDynamicGetterSetter:")));

    Protocol *testProto = objc_getProtocol("ProtocolTest");
    testPropertyForProtocol(testProto, "charDefault", "Tc", ATTRS(ATTR("T", "c")));
	testPropertyForProtocol(testProto, "doubleDefault", "Td", ATTRS(ATTR("T", "d")));
	testPropertyForProtocol(testProto, "enumDefault", "Ti", ATTRS(ATTR("T", "i")));
	testPropertyForProtocol(testProto, "floatDefault", "Tf", ATTRS(ATTR("T", "f")));
	testPropertyForProtocol(testProto, "intDefault", "Ti", ATTRS(ATTR("T", "i")));
	if (sizeof(long) == 4)
	{
		testPropertyForProtocol(testProto, "longDefault", "Tl", ATTRS(ATTR("T", "l")));
	}
	else
	{
		testPropertyForProtocol(testProto, "longDefault", "Tq", ATTRS(ATTR("T", "q")));
	}
	testPropertyForProtocol(testProto, "shortDefault", "Ts", ATTRS(ATTR("T", "s")));
	testPropertyForProtocol(testProto, "signedDefault", "Ti", ATTRS(ATTR("T", "i")));
	testPropertyForProtocol(testProto, "structDefault", "T{YorkshireTeaStruct=ic}", ATTRS(ATTR("T", "{YorkshireTeaStruct=ic}")));
	testPropertyForProtocol(testProto, "typedefDefault", "T{YorkshireTeaStruct=ic}", ATTRS(ATTR("T", "{YorkshireTeaStruct=ic}")));
	testPropertyForProtocol(testProto, "unionDefault", "T(MoneyUnion=fd)", ATTRS(ATTR("T", "(MoneyUnion=fd)")));
	testPropertyForProtocol(testProto, "unsignedDefault", "TI", ATTRS(ATTR("T", "I")));
	testPropertyForProtocol(testProto, "functionPointerDefault", "T^?", ATTRS(ATTR("T", "^?")));
	testPropertyForProtocol(testProto, "intPointer", "T^i", ATTRS(ATTR("T", "^i")));
	testPropertyForProtocol(testProto, "voidPointerDefault", "T^v", ATTRS(ATTR("T", "^v")));
	testPropertyForProtocol(testProto, "intSetterGetter", "Ti,GintGetFoo,SintSetFoo:", ATTRS(ATTR("T", "i"),
																							 ATTR("G", "intGetFoo"),
																							 ATTR("S", "intSetFoo:")));
	testPropertyForProtocol(testProto, "intReadonly", "Ti,R", ATTRS(ATTR("T", "i"),
																	ATTR("R", "")));
	testPropertyForProtocol(testProto, "intReadonlyGetter", "Ti,R,GisIntReadOnlyGetter", ATTRS(ATTR("T", "i"),
																							   ATTR("R", ""),
																							   ATTR("G", "isIntReadOnlyGetter")));
	testPropertyForProtocol(testProto, "intReadwrite", "Ti", ATTRS(ATTR("T", "i")));
	testPropertyForProtocol(testProto, "intAssign", "Ti", ATTRS(ATTR("T", "i")));
	testPropertyForProtocol(testProto, "idDefault", "T@", ATTRS(ATTR("T", "@")));
	testPropertyForProtocol(testProto, "idRetain", "T@,&", ATTRS(ATTR("T", "@"),
																 ATTR("&", "")));
	testPropertyForProtocol(testProto, "idCopy", "T@,C", ATTRS(ATTR("T", "@"),
															   ATTR("C", "")));
	testPropertyForProtocol(testProto, "idWeak", "T@,W", ATTRS(ATTR("T", "@"),
															   ATTR("W", "")));
	testPropertyForProtocol(testProto, "idStrong", "T@,&", ATTRS(ATTR("T", "@"),
																 ATTR("&", "")));
	testPropertyForProtocol(testProto, "intNonatomic", "Ti,N", ATTRS(ATTR("T", "i"),
																	 ATTR("N", "")));
	testPropertyForProtocol(testProto, "idReadonlyCopyNonatomic", "T@,R,C,N", ATTRS(ATTR("T", "@"),
																				  ATTR("R", ""),
																				  ATTR("N", "")));
	testPropertyForProtocol(testProto, "idReadonlyRetainNonatomic", "T@,R,&,N", ATTRS(ATTR("T", "@"),
																					ATTR("R", ""),
																					ATTR("N", "")));
	testPropertyForProtocol(testProto, "idReadonlyWeakNonatomic", "T@,R,N", ATTRS(ATTR("T", "@"),
																				  ATTR("R", ""),
																				  ATTR("N", "")));
	testPropertyForProtocol(testProto, "idOther", "T@,&", ATTRS(ATTR("T", "@"), ATTR("&", "")));
	testPropertyForProtocol(testProto, "idDynamic", "T@,&", ATTRS(ATTR("T", "@"), ATTR("&", "")));
	testPropertyForProtocol(testProto, "idDynamicGetterSetter", "T@,&,N,GdynamicGetterSetter,SsetDynamicGetterSetter:", ATTRS(ATTR("T", "@"),
																															  ATTR("&", ""),
																															  ATTR("N", ""),
																															  ATTR("G", "dynamicGetterSetter"),
																															  ATTR("S", "setDynamicGetterSetter:")));
    
    testAddProperty();

	Protocol *testAddProtocol = objc_allocateProtocol("TestAddProtocol");
	assert(0 != testAddProtocol);
    testAddPropertyForProtocol(testAddProtocol);

    Class testClass = objc_getClass("PropertyTest");
    objc_property_attribute_t intDefault2Attrs[] = { ATTR("T", "i"), ATTR("V", "intDefault") };
    assert(class_addProperty(testClass, "intDefault2", intDefault2Attrs, 2));
    assert(class_addMethod(testClass, @selector(intDefault2), (IMP)intDefault2Getter, "i@:"));
    assert(class_addMethod(testClass, @selector(setIntDefault2:), (IMP)intDefault2Setter, "v@:i"));
	testPropertyForClass(testClass, "intDefault2", "Ti,VintDefault", ATTRS(ATTR("T", "i"), ATTR("V", "intDefault")));
    
    objc_property_attribute_t structDefault2Attrs[] = { ATTR("T", "{YorkshireTeaStruct=ic}"),
														ATTR("V", "structDefault") };
    assert(class_addProperty(testClass, "structDefault2", structDefault2Attrs, 2));
    assert(class_addMethod(testClass, @selector(structDefault2), (IMP)structDefault2Getter, "{YorkshireTeaStruct=ic}@:"));
    assert(class_addMethod(testClass, @selector(setStructDefault2:), (IMP)structDefault2Setter, "v@:{YorkshireTeaStruct=ic}"));
	testPropertyForClass(testClass, "structDefault2", "T{YorkshireTeaStruct=ic},VstructDefault", ATTRS(ATTR("T", "{YorkshireTeaStruct=ic}"),
                                                                                                       ATTR("V", "structDefault")));
    
    PropertyTest* t = class_createInstance(testClass, 0);
    object_setClass(t, testClass);
    t.intDefault = 2;
    assert(t.intDefault == 2);
    [t setIntDefault2:3];
    assert((int)[t intDefault2] == 3);
    assert(t.intDefault == 3);
    
    struct YorkshireTeaStruct struct1 = { 2, 'A' };
    t.structDefault = struct1;
    struct YorkshireTeaStruct readStruct = t.structDefault;
    assert(memcmp(&struct1, &readStruct, sizeof(struct1)) == 0);
    struct YorkshireTeaStruct struct2 = { 3, 'B' };
    [t setStructDefault2:struct2];
    struct YorkshireTeaStruct readStruct2 = [t structDefault2];
    assert(memcmp(&struct2, &readStruct2, sizeof(struct2)) == 0);
    readStruct = t.structDefault;
    assert(memcmp(&struct2, &readStruct, sizeof(struct2)) == 0);
    
    objc_property_attribute_t idRetainAttrs[] = { ATTR("T", "@"),
												  ATTR("&", ""),
												  ATTR("V", "_idOther") };
    class_replaceProperty(testClass, "idRetain", idRetainAttrs, 3);
	testPropertyForClass(testClass, "idRetain", "T@,&,V_idOther", ATTRS(ATTR("T", "@"),
                                                                        ATTR("&", ""),
                                                                        ATTR("V", "_idOther")));
    id testValue = [Test new];
    t.idRetain = testValue;
    assert(t->idRetain == testValue);
    assert(t->_idOther == nil);
    
    Method idRetainSetter = class_getInstanceMethod(testClass, @selector(setIdRetain:));
    Method idOtherSetter = class_getInstanceMethod(testClass, @selector(setIdOther:));
    method_setImplementation(idRetainSetter, method_getImplementation(idOtherSetter));
    idRetainSetter = class_getInstanceMethod(testClass, @selector(setIdRetain:));
    
    id testValue2 = [Test new];
    t.idRetain = testValue2;
    assert(t->idRetain == testValue);
    assert(t->_idOther == testValue2);
	return 0;
}
