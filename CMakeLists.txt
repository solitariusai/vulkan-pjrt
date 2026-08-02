cmake_minimum_required(VERSION 3.16)
project(vulkan_pjrt C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Vulkan REQUIRED)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${Vulkan_INCLUDE_DIRS}
)

add_library(vulkan_pjrt SHARED
  src/vulkan_backend.cc
  src/vulkan_buffer.cc
  src/vulkan_compiler.cc
  src/vulkan_executable.cc
  src/vulkan_client.cc
  src/pjrt_c_api.cc
)

target_link_libraries(vulkan_pjrt PRIVATE ${Vulkan_LIBRARIES} pthread)

set_target_properties(vulkan_pjrt PROPERTIES
  PREFIX "lib"
  SUFFIX ".so"
)
