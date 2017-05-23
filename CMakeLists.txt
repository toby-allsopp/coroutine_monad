cmake_minimum_required(VERSION 3.0)

project(coroutine_monad)
add_executable(${PROJECT_NAME} main.cpp coroutine_monad.cpp maybe.h)
if(MSVC)
target_compile_options(${PROJECT_NAME} PUBLIC /std:c++latest /await)
else()
target_compile_options(${PROJECT_NAME} PUBLIC -std=c++1z -fcoroutines-ts)
endif()
