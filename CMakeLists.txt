cmake_minimum_required(VERSION 2.8)


project(libobjc)

list(INSERT CMAKE_C_FLAGS_DEBUG 0 "-g -O0 -fno-inline")
list(INSERT CMAKE_C_FLAGS_RELEASE 0 "-O3")
list(APPEND CMAKE_C_FLAGS "-std=gnu99")

set(libobjc_VERSION 4.6)

add_definitions(
	-DGNUSTEP -D__OBJC_RUNTIME_INTERNAL__=1  -D_XOPEN_SOURCE=500 -D__BSD_VISIBLE=1 -D_BSD_SOURCE=1)

set(libobjc_ASM_SRCS 
	block_trampolines.S
	objc_msgSend.S)
set(libobjc_OBJC_SRCS 
	NSBlocks.m
	Protocol2.m
	arc.m
	associate.m
	blocks_runtime.m
	properties.m)
set(libobjc_C_SRCS 
	abi_version.c
	alias_table.c
	block_to_imp.c
	caps.c
	category_loader.c
	class_table.c
	dtable.c
	eh_personality.c
	encoding2.c
	gc_none.c
	hash_table.c
	hooks.c
	ivar.c
	legacy_malloc.c
	loader.c
	mutation.m
	protocol.c
	runtime.c
	sarray2.c
	selector_table.c
	sendmsg2.c
	statics_loader.c
	toydispatch.c)
set(libobjc_HDRS
	objc/Availability.h
	objc/Object.h
	objc/Protocol.h
	objc/blocks_private.h
	objc/blocks_runtime.h
	objc/capabilities.h
	objc/developer.h
	objc/encoding.h
	objc/hooks.h
	objc/objc-api.h
	objc/objc-arc.h
	objc/objc-auto.h
	objc/objc.h
	objc/runtime-deprecated.h
	objc/runtime.h
	objc/slot.h
)

set(libobjcxx_CXX_SRCS objcxx_eh.cc)

# For release builds, we disable spamming the terminal with warnings about
# selector type mismatches
if (CMAKE_BUILD_TYPE STREQUAL Release)
	ADD_DEFINITIONS(-DNO_SELECTOR_MISMATCH_WARNINGS)
else ()
	ADD_DEFINITIONS(-DGC_DEBUG)
endif ()

set(TYPE_DEPENDENT_DISPATCH TRUE CACHE BOOL
	"Enable type-dependent dispatch")
if (TYPE_DEPENDENT_DISPATCH)
	add_definitions(-DTYPE_DEPENDENT_DISPATCH)
endif ()
set(LOW_MEMORY FALSE CACHE BOOL
	"Enable low-memory profile *HIGHLY EXPERIMENTAL*")
if (LOW_MEMORY)
	add_definitions(-D__OBJC_LOW_MEMORY__)
endif ()
set(BOEHM_GC FALSE CACHE BOOL
	"Enable garbage collection support (not recommended)")
if (BOEHM_GC)
	include(FindPkgConfig)
	pkg_check_modules(GC REQUIRED bdw-gc)
	link_directories(${GC_LIBRARY_DIRS})
	# If there's a threaded version, use it
	find_library(LIBGC gc-threaded PATHS "${GC_LIBRARY_DIRS}")
	if (LIBGC)
	else ()
		find_library(LIBGC gc PATHS GC_LIBRARY_DIRS)
	endif ()
	message("-- Using Boehm GC library: ${LIBGC}")
	include_directories(GC_INCLUDE_DIRS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GC_CFLAGS}")
	set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-gc")
	set(CMAKE_LINK_FLAGS "${CMAKE_LINK_FLAGS} ${GC_CFLAGS}")
	add_definitions(-DENABLE_GC)
endif ()
set(LEGACY_COMPAT FALSE CACHE BOOL
	"Enable legacy compatibility features")
if (LEGACY_COMPAT)
	list(APPEND libobjc_C_SRCS legacy_malloc.c)
else ()
	add_definitions(-DNO_LEGACY)
endif ()
set(LLVM_OPTS TRUE CACHE BOOL
	"Build LLVM Objective-C optimisations")
if (LLVM_OPTS)
	add_subdirectory(opts)
endif ()


if (${CMAKE_C_COMPILER_ID} MATCHES Clang*)
	set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -Wno-deprecated-objc-isa-usage -Wno-objc-root-class")
else (${CMAKE_C_COMPILER_ID} MATCHES Clang*)
	MESSAGE("WARNING: It is strongly recommended that you compile with clang")
endif (${CMAKE_C_COMPILER_ID} MATCHES Clang*)



set_source_files_properties(
	${libobjc_OBJC_SRCS}
	PROPERTIES LANGUAGE C
	COMPILE_FLAGS "${CMAKE_OBJC_FLAGS}"
)

#
# C++ Runtime interaction
#

# Try to find libcxxrt.so.  We can link to this to provide the C++ ABI layer,
# if it exists.
find_library(CXX_RUNTIME cxxrt)
# If it doesn't, then look for GNU libsupc++.so instead (either works, they're
# ABI compatible).
if (NOT CXX_RUNTIME)
	find_library(CXX_RUNTIME supc++)
endif (NOT CXX_RUNTIME)

# If we have a C++ ABI library, then we can produce a single libobjc that works
# for Objective-C and Objective-C++.  If not, then we need to provide a
# separate libobjcxx.
if (CXX_RUNTIME)
	message("-- Using ${CXX_RUNTIME} as the C++ runtime library")
	set(libobjc_CXX_SRCS ${libobjcxx_CXX_SRCS})
	# We don't want to link the STL implementation (e.g. libstdc++) if we have
	# a separate C++ runtime.
	set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
else (CXX_RUNTIME)
	message("-- No C++ runtime library found")
	add_library(objcxx SHARED ${libobjcxx_CXX_SRCS})
	set_target_properties(objcxx PROPERTIES
		LINKER_LANGUAGE C
		SOVERSION ${libobjc_VERSION}
		)
endif (CXX_RUNTIME)

add_library(objc SHARED ${libobjc_C_SRCS} ${libobjc_ASM_SRCS} ${libobjc_OBJC_SRCS} ${libobjc_CXX_SRCS} ${libobjc_HDRS})

set_target_properties(objc PROPERTIES
	LINKER_LANGUAGE C
	PUBLIC_HEADER "${libobjc_HDRS}"
	SOVERSION ${libobjc_VERSION}
	)

# Currently, we actually need pthreads, but we should use the platform's native
# threading implementation (we do for everything except thread-local storage)
set(CMAKE_THREAD_PREFER_PTHREAD)
include(FindThreads)
list(APPEND CMAKE_LINK_FLAGS CMAKE_THREAD_LIBS_INIT)

# Explicitly link the C++ runtime and libgc if we are compiling with gc support.
target_link_libraries(objc ${CXX_RUNTIME})
if (LIBGC)
	target_link_libraries(objc ${LIBGC})
endif ()

#
# Installation
#


set(GNUSTEP_LOCAL_ROOT  "$ENV{GNUSTEP_LOCAL_ROOT}")
set(GNUSTEP_SYSTEM_ROOT "$ENV{GNUSTEP_SYSTEM_ROOT}")
# If we have GNUstep environment variables, then default to installing in the
# GNUstep local environment.
if (GNUSTEP_LOCAL_ROOT)
	set(DEFAULT_INSTALL_TYPE "Local")
else ()
	set(DEFAULT_INSTALL_TYPE "None")
endif ()
set(GNUSTEP_INSTALL_TYPE ${DEFAULT_INSTALL_TYPE} CACHE STRING
	"GNUstep installation type.  Options are None, System or Local.")

if (${GNUSTEP_INSTALL_TYPE} STREQUAL "System")
	SET(LIB_INSTALL_PATH "${GNUSTEP_SYSTEM_ROOT}/Library/Libraries")
	SET(HEADER_INSTALL_PATH "${GNUSTEP_SYSTEM_ROOT}/Library/Headers/objc")
elseif (${GNUSTEP_INSTALL_TYPE} STREQUAL "Local")
	SET(LIB_INSTALL_PATH "${GNUSTEP_LOCAL_ROOT}/Library/Libraries")
	SET(HEADER_INSTALL_PATH "${GNUSTEP_LOCAL_ROOT}/Library/Headers/objc")
else ()
	SET(LIB_INSTALL_PATH lib)
	SET(HEADER_INSTALL_PATH "include/objc")
elseif (${GNUSTEP_INSTALL_TYPE} EQUAL "Local")
endif ()

install(TARGETS objc
	LIBRARY DESTINATION ${LIB_INSTALL_PATH}
	ARCHIVE DESTINATION ${LIB_INSTALL_PATH}
	PUBLIC_HEADER DESTINATION ${HEADER_INSTALL_PATH})
