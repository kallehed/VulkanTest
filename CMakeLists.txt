cmake_minimum_required(VERSION 3.12)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 17)

find_package(glfw3 REQUIRED)

set(SOURCES
  main.cpp
)
if(MSVC)
  add_compile_options(${PROJECT_NAME} /W4 /WX)
else()
  add_compile_options(${PROJECT_NAME} -Wall -Wextra -Wpedantic)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} glfw vulkan)
