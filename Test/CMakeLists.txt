
# Clear the LD_LIBRARY_PATH if GNUstep set it so that we don't accidentally use
# the installed version


# List of single-file tests.
set(TESTS
	alignTest.m
	AllocatePair.m
	AssociatedObject.m
	AssociatedObject2.m
	BlockImpTest.m
	BlockTest_arc.m
	BoxedForeignException.m
	ExceptionTest.m
	ForeignException.m
	Forward.m
	ManyManySelectors.m
	NestedExceptions.m
	PropertyAttributeTest.m
	PropertyIntrospectionTest.m
	PropertyIntrospectionTest2_arc.m
	ProtocolCreation.m
	ResurrectInDealloc_arc.m
	RuntimeTest.m
	WeakBlock_arc.m
	WeakReferences_arc.m
	ivar_arc.m
	IVarOverlap.m
	objc_msgSend.m
	msgInterpose.m
	NilException.m
	MethodArguments.m
	zeroSizedIVar.m
)

# Function for adding a test.  This takes the name of the test and the list of
# source files as arguments.
function(addtest_flags TEST_NAME FLAGS TEST_SOURCE)
	if (${TEST_NAME} MATCHES ".*_arc")
		# Only compile the main file with ARC
		set_source_files_properties(${TEST_SOURCE}
			COMPILE_FLAGS "-fobjc-arc")
		# Add the ARC-incompatible definitions of the test class.
		list(APPEND TEST_SOURCE "Test.m")
	endif()
	add_executable(${TEST_NAME} ${TEST_SOURCE})
	add_test(${TEST_NAME} ${TEST_NAME})
	set(ARC "")
	set_target_properties(${TEST_NAME} PROPERTIES
		INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}"
		COMPILE_FLAGS "-fobjc-runtime=gnustep-1.7 -fblocks ${FLAGS}"
		LINKER_LANGUAGE C
	)
	set_property(TEST ${TEST_NAME} PROPERTY
		ENVIRONMENT "LD_LIBRARY_PATH="
	)
	target_link_libraries(${TEST_NAME} objc)
endfunction(addtest_flags)

foreach(TEST_SOURCE ${TESTS})
	get_filename_component(TEST ${TEST_SOURCE} NAME_WE)
	addtest_flags(${TEST} "-O0 -UNDEBUG" ${TEST_SOURCE})
	addtest_flags("${TEST}_optimised" "-O3 -UNDEBUG" ${TEST_SOURCE})
endforeach()

# Tests that are more than a single file.
addtest_flags(CXXExceptions "-O0" "CXXException.m;CXXException.cc")
addtest_flags(CXXExceptions_optimised "-O3" "CXXException.m;CXXException.cc")

