
add_executable (swtrace swtrace.c)

target_link_libraries (swtrace LINK_PUBLIC perfutil a2n ${EXTRA_EXE_LIBS})
