find_package(benchmark REQUIRED)

add_executable(objc_msgSend objc_msgSend.cpp TestClass.m)

target_compile_options(objc_msgSend PRIVATE "-fobjc-runtime=gnustep-2.0")
target_link_libraries(objc_msgSend benchmark::benchmark objc)
