find_package(benchmark REQUIRED)

add_executable(benchmark_objc_msgSend objc_msgSend.cpp TestClass.m)

target_compile_options(benchmark_objc_msgSend PRIVATE "-fobjc-runtime=gnustep-2.0")
target_link_libraries(benchmark_objc_msgSend benchmark::benchmark objc)
