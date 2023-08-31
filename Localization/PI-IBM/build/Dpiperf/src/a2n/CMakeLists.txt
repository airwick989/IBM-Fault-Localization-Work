
set (A2N_SRC util.c a2n.c a2nint.c initterm.c saveres.c)

if (LINUX)
    set (A2N_STUB_SRC linux/linuxelf.c linux/linuxelf64.c linux/linuxmap.c linux/linuxsyms.c linux/linuxval.c)
    set (EXTRA_EXE_LIBS pthread dl c bfd iberty)
endif (LINUX)

add_definitions(-DDEBUG)

add_library (a2n SHARED ${A2N_SRC} ${A2N_STUB_SRC})

target_link_libraries (a2n LINK_PUBLIC utils ${EXTRA_EXE_LIBS})

target_include_directories (a2n PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
