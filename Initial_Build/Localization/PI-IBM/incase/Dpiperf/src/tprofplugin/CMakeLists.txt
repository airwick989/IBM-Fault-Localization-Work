
file (GLOB TPROF_PLUGIN_SRC
   "*.h*"
   "*.c*"
)

if (LINUX)
    set (EXTRA_EXE_LIBS pthread dl)
endif (LINUX)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PI_LIB_LOC}/plugins")

add_library (tprof SHARED ${TPROF_PLUGIN_SRC})

target_link_libraries (tprof LINK_PUBLIC utils perfutil a2n ${EXTRA_EXE_LIBS})

target_include_directories (tprof PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
