cmake_minimum_required(VERSION 2.8)

add_executable(test_cxx_runtime typeinfo_test.cc)
set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
target_link_libraries(test_cxx_runtime ${CXX_RUNTIME})
set_target_properties(test_cxx_runtime PROPERTIES
	LINKER_LANGUAGE C)
