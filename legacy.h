#pragma once
#include "visibility.h"
#include "ivar.h"
#include "class.h"
#include "category.h"
#include "protocol.h"

PRIVATE Class objc_upgrade_class(struct legacy_gnustep_objc_class *oldClass);
PRIVATE struct objc_category *objc_upgrade_category(struct objc_category_legacy *);

PRIVATE struct legacy_gnustep_objc_class* objc_legacy_class_for_class(Class);

PRIVATE struct objc_protocol *objc_upgrade_protocol_gcc(struct objc_protocol_gcc*);
PRIVATE struct objc_protocol *objc_upgrade_protocol_gsv1(struct objc_protocol_gsv1*);

