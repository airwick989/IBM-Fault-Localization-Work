file(GLOB utils_SRC
    "*.h"
    "*.hpp"
    "*.c"
    "*.cpp"
)

add_library (utils STATIC ${utils_SRC})
target_include_directories (utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
