add_library(strings INTERFACE)

target_compile_features(strings INTERFACE cxx_std_20)
target_compile_definitions(strings INTERFACE LIBCXX_ENABLE_INCOMPLETE_FEATURES)
target_include_directories(strings INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_compile_definitions(strings INTERFACE 
        STRINGS_USE_TOCHARS_FLOAT_STUB
        STRINGS_APPLE_CLANG_RANGES)
endif()