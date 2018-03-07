cmake_minimum_required(VERSION 3.0)

project(coroutine_monad)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/std-make/include)
target_sources(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/return_object_holder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/maybe.h
    ${CMAKE_CURRENT_SOURCE_DIR}/expected.h
    ${CMAKE_CURRENT_SOURCE_DIR}/either.h
    ${CMAKE_CURRENT_SOURCE_DIR}/state.h
    ${CMAKE_CURRENT_SOURCE_DIR}/monad_promise.h
)
if(MSVC)
target_compile_options(${PROJECT_NAME} INTERFACE /std:c++latest /await)
else()
target_compile_options(${PROJECT_NAME} INTERFACE -std=c++1z -fcoroutines-ts)
endif()

add_executable(test_${PROJECT_NAME}
    test_main.cpp
    test_optional.cpp
    test_expected.cpp
    test_state.cpp
)
target_link_libraries(test_${PROJECT_NAME} ${PROJECT_NAME})

enable_testing()
add_test(test_${PROJECT_NAME} test_${PROJECT_NAME})
