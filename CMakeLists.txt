cmake_minimum_required(VERSION 3.12)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 17)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

set(SOURCES
  main.cpp
)
if(MSVC)
  add_compile_options(${PROJECT_NAME} /W4 /WX)
else()
  add_compile_options(${PROJECT_NAME} -Wall -Wextra -Wpedantic)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE glfw ${Vulkan_LIBRARIES})
