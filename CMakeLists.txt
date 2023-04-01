add_library(strings INTERFACE)

target_compile_features(strings INTERFACE cxx_std_20)
target_include_directories(strings INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")