
# Clear the LD_LIBRARY_PATH if GNUstep set it so that we don't accidentally use
# the installed version


set(TESTS
	BlockImpTest.m
	PropertyAttributeTest.m
	PropertyIntrospectionTest.m
	ProtocolCreation.m
	RuntimeTest.m
	objc_msgSend.m
)

#ExceptionTest.m
#PropertyAttributeTest.m
#RuntimeTest.m
foreach(TEST_SOURCE ${TESTS})
	get_filename_component(TEST ${TEST_SOURCE} NAME_WE)
	add_executable(${TEST} ${TEST_SOURCE})
	add_test(${TEST} ${TEST})
	set_target_properties(${TEST} PROPERTIES
		INCLUDE_DIRECTORIES ".."
		COMPILE_FLAGS "-fobjc-runtime=gnustep-1.6 -fblocks"
		LANGUAGE C
		LINKER_LANGUAGE C
	)
	set_property(TEST ${TEST} PROPERTY
	ENVIRONMENT "LD_LIBRARY_PATH="
	)
	target_link_libraries(${TEST} objc)
endforeach()


