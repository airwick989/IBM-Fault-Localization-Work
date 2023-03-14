cmake_minimum_required(VERSION 2.4)
project(IBM_PI)

# set the project common locations
set (PI_SRC_LOC ${CMAKE_CURRENT_SOURCE_DIR})
set (PI_INCLUDE_LOC ${PI_SRC_LOC}/../include)
set (PI_BIN_LOC ${PI_SRC_LOC}/../bin)
set (PI_LIB_LOC ${PI_SRC_LOC}/../lib)

# set the common include directories
include_directories ("${PI_SRC_LOC}")
include_directories ("${PI_INCLUDE_LOC}")

#set the output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PI_LIB_LOC}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PI_LIB_LOC}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PI_BIN_LOC}")

if (UNIX)
    set (OS_DIR "linux")
    set (LINUX 1)
    set (GCC_COVERAGE_C_FLAGS "-Wno-narrowing -Wno-write-strings")
    set (GCC_COVERAGE_CXX_FLAGS "-std=c++11 -Wno-narrowing -Wno-write-strings")
    set (GCC_COVERAGE_LINK_FLAGS "-Wno-narrowing -Wno-write-strings")
    #add_definitions(-DPU_DEBUG)
    #add_definitions(-DPU_TRACE)
    add_definitions(-DLINUX)
    add_definitions(-D_LINUX)
    add_definitions(-DIBM_JVM)
    set (EXTRA_EXE_LIBS pthread rt dl)
    SET( CMAKE_C_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC ${GCC_COVERAGE_C_FLAGS}" )
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC ${GCC_COVERAGE_CXX_FLAGS}" )
    SET( CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -fPIC ${GCC_COVERAGE_LINK_FLAGS}" )
    SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}" )

    EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
    message( STATUS "Architecture: ${ARCHITECTURE}" )
    if ((${ARCHITECTURE} STREQUAL "x86_64") OR (${ARCHITECTURE} STREQUAL "x86_32"))
        add_definitions(-DCONFIG_X86_64)
    elseif ((${ARCHITECTURE} STREQUAL "ppc64") OR (${ARCHITECTURE} STREQUAL "ppc64le") OR (${ARCHITECTURE} STREQUAL "ppc"))
        add_definitions(-DCONFIG_PPC64)
    elseif ((${ARCHITECTURE} STREQUAL "s390x") OR (${ARCHITECTURE} STREQUAL "s390"))
        add_definitions(-DCONFIG_S390X)
    endif()

    if (NOT DEFINED MAX_NUM_CPUS)
	# Let the user decide if he either specifies the number of CPUs on the
	# command-line via e.g. cmake -DMAX_NUM_CPUS=128
	# or use the number of CPUs of the current build system.
	EXECUTE_PROCESS( COMMAND nproc --all COMMAND tr -d '\n' OUTPUT_VARIABLE MAX_NUM_CPUS)
    endif()
    message( STATUS "Num CPUS: ${MAX_NUM_CPUS}" )
    add_definitions(-DMAX_CPUS=${MAX_NUM_CPUS})
endif(UNIX)

# build the the common utils
add_subdirectory (utils)

# build the perfutil library
add_subdirectory (perfutil)

# build the a2n library
add_subdirectory (a2n)

# build jprof
add_subdirectory (jprof)

# build swtrace
add_subdirectory (swtrace)

# build post
add_subdirectory (post)

# build rtdriver
add_subdirectory (rtdriver)

# build arcf
add_subdirectory (arcf)

# copy scripts
add_subdirectory (scripts)

# build tprof plugins
add_subdirectory (tprofplugin)

