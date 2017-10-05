cmake_minimum_required (VERSION 2.6)
project (VulkanTest)

#set c++ standard
set (CMAKE_CXX_STANDARD 11)

# add the binary tree to the search path for include files
include_directories ("${PROJECT_BINARY_DIR}")

include(DownloadProject.cmake)

# don't download glfw project shit
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_DOCUMENT_INTERNALS "" OFF)
option(GLFW_INSTALL "" OFF)

#get glfw
download_project(PROJ glfw
  GIT_REPOSITORY      https://github.com/glfw/glfw.git
  GIT_TAG             0f488ac
  ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)

add_subdirectory(${glfw_SOURCE_DIR} ${glfw_BINARY_DIR})
include_directories("${glfw_SOURCE_DIR}/include")

#find vulkan sdk
include_directories("$ENV{VULKAN_SDK}/include")
link_directories("$ENV{VULKAN_SDK}/lib")


#get glm
download_project(PROJ glm
  GIT_REPOSITORY      https://github.com/g-truc/glm.git
  GIT_TAG             6a1e2ec
  ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)
add_subdirectory(${glm_SOURCE_DIR} ${glm_BINARY_DIR})
include_directories("${glm_SOURCE_DIR}")

# add the exes
# Add all files from a src folder
file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

#file(GLOB_RECURSE SHADERS res/shaders/*.vert res/shaders/*.frag)

add_executable(VulkanTest ${SOURCES})
target_link_libraries (VulkanTest  ${EXTRA_LIBS})

target_link_libraries(VulkanTest glfw)
target_link_libraries(VulkanTest vulkan-1)
