
file (GLOB JPROF_SRC
   "*.h"
   "*.c"
   "plugin/*.h"
   "plugin/*.cpp"
)

if (LINUX)
    set (JPROF_STUB_SRC )
    set (EXTRA_EXE_LIBS pthread dl)
endif (LINUX)

add_library (jprof SHARED ${JPROF_SRC} ${JPROF_STUB_SRC})

target_link_libraries (jprof LINK_PUBLIC utils perfutil a2n ${EXTRA_EXE_LIBS})

target_include_directories (jprof PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
