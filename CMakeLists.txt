cmake_minimum_required(VERSION 3.20)
project(LiteSR VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "LiteSR currently targets Windows only")
endif()

add_executable(litesr src/main.cpp)
target_compile_features(litesr PRIVATE cxx_std_17)
target_compile_definitions(litesr PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(litesr PRIVATE shell32)
if(MSVC)
  target_compile_options(litesr PRIVATE /W4 /permissive-)
endif()
