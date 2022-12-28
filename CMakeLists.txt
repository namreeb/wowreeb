project(wowreeb)

cmake_minimum_required(VERSION 3.2.3)

# wowreeb depends on a compiler which supports C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# threading library is required
find_package(Threads REQUIRED)

# currently there is a bug in the cmake included with visual studio where the wrong
# version number is resolved for the compiler.  this should work around it.
# FIXME: this should be removable once visual studio begins using cmake 3.8
if (MSVC)
	if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 19.10)
        set(Boost_COMPILER -vc141)
    endif()
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(boost)

if (NOT EXISTS "${HADESMEM_ROOT}/include/memory/hadesmem")
    message(FATAL_ERROR "hadesmem not found at ${HADESMEM_ROOT}")
else()
    message(STATUS "hadesmem found at ${HADESMEM_ROOT}")
endif()

if ("x_${CMAKE_BUILD_TYPE}" STREQUAL "x_Debug")
    set(HADESMEM_BUILD "Debug")
else()
    set(HADESMEM_BUILD "Release")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(HADESMEM_ARCH "x64")
else()
    set(HADESMEM_ARCH "Win32")
endif()

if (EXISTS "${HADESMEM_ROOT}/build/vs/${HADESMEM_BUILD}/${HADESMEM_ARCH}")
    set(HADESMEM_LIB_DIR "${HADESMEM_ROOT}/build/vs/${HADESMEM_BUILD}/${HADESMEM_ARCH}")
elseif (EXISTS "${HADESMEM_ROOT}/lib")
    set(HADESMEM_LIB_DIR "${HADESMEM_ROOT}/lib")
else()
    message(FATAL_ERROR "hadesmem libraries not found")
endif()

message(STATUS "hadesmem library directory: ${HADESMEM_LIB_DIR}")

add_definitions(
    -DUNICODE
    -D_UNICODE
    -D_SCL_SECURE_NO_WARNINGS
    -D_CRT_SECURE_NO_WARNINGS
    -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    -DASMJIT_STATIC
    -DASMJIT_BUILD_X86
)

include_directories(Include
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${Boost_INCLUDE_DIR}"
    "${HADESMEM_ROOT}/include/memory/"
    "${HADESMEM_ROOT}/deps/udis86/udis86"
    "${HADESMEM_ROOT}/deps/asmjit/asmjit/src"
)

link_directories("${HADESMEM_LIB_DIR}")

add_subdirectory(dll)
add_subdirectory(wowreeb)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/example_config.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION "${CMAKE_INSTALL_PREFIX}")