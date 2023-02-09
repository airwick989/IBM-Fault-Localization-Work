file(GLOB perfutil_SRC
    "*.h"
    "*.hpp"
    "*.cpp"
)

file(GLOB perfutil_stub_SRC
    "${OS_DIR}/*.h"
    "${OS_DIR}/*.hpp"
    "${OS_DIR}/*.cpp"
)

add_library (perfutil SHARED ${perfutil_SRC} ${perfutil_stub_SRC})

target_link_libraries (perfutil LINK_PUBLIC utils a2n ${EXTRA_EXE_LIBS})

target_include_directories (perfutil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


