
set (POST_SRC bputil.c hash.c i386dis.c main.c tree.c tree2.c post.c)

if (LINUX)
    set (POST_STUB_SRC )
    set (EXTRA_EXE_LIBS iberty dl c)
endif (LINUX)

add_executable (post ${POST_SRC})

target_link_libraries (post LINK_PUBLIC utils perfutil a2n ${EXTRA_EXE_LIBS})
