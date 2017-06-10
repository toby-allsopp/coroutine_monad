cmake_minimum_required(VERSION 3.0)

project(coroutine_monad)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/return_object_holder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/maybe.h
    ${CMAKE_CURRENT_SOURCE_DIR}/expected.h
    ${CMAKE_CURRENT_SOURCE_DIR}/either.h)

add_executable(test_${PROJECT_NAME} main.cpp coroutine_monad.cpp)
target_link_libraries(test_${PROJECT_NAME} ${PROJECT_NAME})
if(MSVC)
target_compile_options(test_${PROJECT_NAME} PUBLIC /std:c++latest /await)
else()
target_compile_options(test_${PROJECT_NAME} PUBLIC -std=c++1z -fcoroutines-ts)
endif()

enable_testing()
add_test(test_${PROJECT_NAME} test_${PROJECT_NAME})
