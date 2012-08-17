find_package(LLVM)
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})


add_llvm_loadable_module( libGNUObjCRuntime
  ClassIMPCache.cpp
  ClassMethodInliner.cpp
  IvarPass.cpp
  ObjectiveCOpts.cpp
  TypeFeedbackDrivenInliner.cpp
  ClassLookupCache.cpp
  IMPCacher.cpp
  LoopIMPCachePass.cpp
  TypeFeedback.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-variadic-macros -DLLVM_MAJOR=3 -DLLVM_MINOR=2")

set(CMAKE_CXX "clang++")

EXEC_PROGRAM(llvm-config
	ARGS --src-root
	OUTPUT_VARIABLE LLVM_SRC)
EXEC_PROGRAM(llvm-config
	ARGS --obj-root
	OUTPUT_VARIABLE LLVM_OBJ)
include_directories( ${LLVM_INCLUDE_DIRS} "${LLVM_SRC}/include/" "${LLVM_OBJ}/include/")

