add_library(modern-json INTERFACE)
target_include_directories(modern-json
    INTERFACE
        modern-json/include
)

add_library(CLI11 INTERFACE)
target_include_directories(CLI11
    INTERFACE
        CLI11/include
)