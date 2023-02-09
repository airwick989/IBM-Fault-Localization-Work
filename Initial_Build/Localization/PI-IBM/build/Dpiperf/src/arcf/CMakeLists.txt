
set (ARCF_SRC arc.c arc4a.c comm.c ctr.c g.c input.c input2.c itr.c jhash.c jraw.c nrmhdf.c nrmrd.c util.c)

if (LINUX)
   set (ARCF_STUB_SRC )
   set (EXTRA_EXE_LIBS )
endif (LINUX)

add_executable (arcf ${ARCF_SRC})

target_link_libraries (arcf LINK_PUBLIC utils ${EXTRA_EXE_LIBS})
