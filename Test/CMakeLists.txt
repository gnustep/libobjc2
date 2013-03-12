
# Clear the LD_LIBRARY_PATH if GNUstep set it so that we don't accidentally use
# the installed version


# List of single-file tests.
set(TESTS
	AllocatePair.m
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
	PropertyIntrospectionTest2.m
	ProtocolCreation.m
	RuntimeTest.m
	objc_msgSend.m
)

# Function for adding a test.  This takes the name of the test and the list of
# source files as arguments.
function(addtest_flags TEST FLAGS TEST_SOURCE)
	add_executable(${TEST} ${TEST_SOURCE})
	add_test(${TEST} ${TEST})
	set(ARC "")
	if (TEST MATCHES ".*_arc")
		set(ARC "-fobjc-arc")
	endif()
	set_target_properties(${TEST} PROPERTIES
		INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}"
		COMPILE_FLAGS "-fobjc-runtime=gnustep-1.7 -fblocks ${FLAGS} ${ARC}"
		LINKER_LANGUAGE C
	)
	set_property(TEST ${TEST} PROPERTY
		ENVIRONMENT "LD_LIBRARY_PATH="
	)
	target_link_libraries(${TEST} objc)
endfunction(addtest_flags)

foreach(TEST_SOURCE ${TESTS})
	get_filename_component(TEST ${TEST_SOURCE} NAME_WE)
	addtest_flags(${TEST} "-O0" ${TEST_SOURCE})
	addtest_flags("${TEST}_optimised" "-O3" ${TEST_SOURCE})
endforeach()

# Tests that are more than a single file.
addtest_flags(CXXExceptions "-O0" "CXXException.m;CXXException.cc")
addtest_flags(CXXExceptions_optimised "-O3" "CXXException.m;CXXException.cc")

