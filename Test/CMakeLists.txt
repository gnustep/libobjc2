
# Clear the LD_LIBRARY_PATH if GNUstep set it so that we don't accidentally use
# the installed version


# List of single-file tests.
set(TESTS
	alias.m
	alignTest.m
	AllocatePair.m
	AssociatedObject.m
	AssociatedObject2.m
	BlockImpTest.m
	BlockTest_arc.m
	ConstantString.m
	BoxedForeignException.m
	Category.m
	ExceptionTest.m
	ForeignException.m
	Forward.m
	ManyManySelectors.m
	NestedExceptions.m
	PropertyAttributeTest.m
	ProtocolExtendedProperties.m
	PropertyIntrospectionTest.m
	PropertyIntrospectionTest2_arc.m
	ProtocolCreation.m
	ResurrectInDealloc_arc.m
	RuntimeTest.m
	WeakBlock_arc.m
	WeakReferences_arc.m
	WeakImportClass.m
	ivar_arc.m
	IVarOverlap.m
	objc_msgSend.m
	msgInterpose.m
	NilException.m
	MethodArguments.m
	zeroSizedIVar.m
	exchange.m
	hash_table_delete.c
	setSuperclass.m
)

# List of single-file tests that won't work with the legacy ABI and so
# shouldn't be run in legacy mode.
set(NEW_TESTS
	category_properties.m
)


add_library(test_runtime_legacy OBJECT Test.m)
set_target_properties(test_runtime_legacy PROPERTIES
	INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}"
	COMPILE_FLAGS "-fblocks -fobjc-runtime=gnustep-1.7"
	LINKER_LANGUAGE C
)

add_library(test_runtime OBJECT Test.m)
set_target_properties(test_runtime PROPERTIES
	INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}"
	COMPILE_FLAGS "-fblocks -fobjc-runtime=gnustep-2.0"
	LINKER_LANGUAGE C
)


# Function for adding a test.  This takes the name of the test and the list of
# source files as arguments.
function(addtest_flags TEST_NAME FLAGS TEST_SOURCE)
	if (${TEST_NAME} MATCHES ".*_arc")
		# Only compile the main file with ARC
		set_source_files_properties(${TEST_SOURCE}
			COMPILE_FLAGS "-fobjc-arc")
	endif()
	add_executable(${TEST_NAME} ${TEST_SOURCE})
	add_test(${TEST_NAME} ${TEST_NAME})
	set(ARC "")
	set_target_properties(${TEST_NAME} PROPERTIES
		INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}"
		COMPILE_FLAGS "-fblocks ${FLAGS}"
		LINKER_LANGUAGE C
	)
	set_property(TEST ${TEST_NAME} PROPERTY
		ENVIRONMENT "LD_LIBRARY_PATH="
	)
	target_link_libraries(${TEST_NAME} objc)
endfunction(addtest_flags)

function(addtest_variants TEST TEST_SOURCE LEGACY)
	addtest_flags(${TEST} "-O0 -fobjc-runtime=gnustep-2.0 -UNDEBUG -DGS_RUNTIME_V2" "${TEST_SOURCE}")
	target_sources(${TEST} PRIVATE $<TARGET_OBJECTS:test_runtime>)
	addtest_flags("${TEST}_optimised" "-O3 -fobjc-runtime=gnustep-2.0 -UNDEBUG -DGS_RUNTIME_V2" "${TEST_SOURCE}")
	target_sources("${TEST}_optimised" PRIVATE $<TARGET_OBJECTS:test_runtime>)
	if (LEGACY)
		addtest_flags("${TEST}_legacy" "-O0 -fobjc-runtime=gnustep-1.7 -UNDEBUG" "${TEST_SOURCE}")
		target_sources("${TEST}_legacy" PRIVATE $<TARGET_OBJECTS:test_runtime_legacy>)
		addtest_flags("${TEST}_legacy_optimised" "-O3 -fobjc-runtime=gnustep-1.7 -UNDEBUG" "${TEST_SOURCE}")
		target_sources("${TEST}_legacy_optimised" PRIVATE $<TARGET_OBJECTS:test_runtime_legacy>)
	endif()
endfunction(addtest_variants)

foreach(TEST_SOURCE ${TESTS})
	get_filename_component(TEST ${TEST_SOURCE} NAME_WE)
	addtest_variants(${TEST} ${TEST_SOURCE} true)
endforeach()

foreach(TEST_SOURCE ${NEW_TESTS})
	get_filename_component(TEST ${TEST_SOURCE} NAME_WE)
	addtest_variants(${TEST} ${TEST_SOURCE} false)
endforeach()

# Tests that are more than a single file.
addtest_variants("CXXExceptions" "CXXException.m;CXXException.cc" true)
addtest_variants("ForwardDeclareProtocolAccess" "ForwardDeclareProtocolAccess.m;ForwardDeclareProtocol.m" true)
if (ENABLE_OBJCXX)
addtest_variants(ObjCXXEHInterop "ObjCXXEHInterop.mm;ObjCXXEHInterop.m" true)
endif()
