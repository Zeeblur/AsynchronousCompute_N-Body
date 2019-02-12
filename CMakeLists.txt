cmake_minimum_required (VERSION 2.6)
project (VulkanTest)

#set c++ standard
set (CMAKE_CXX_STANDARD 11)

# add the binary tree to the search path for include files
include_directories ("${PROJECT_BINARY_DIR}")

SET(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})


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


## TRIANGLE TEST:
file(GLOB_RECURSE SOURCES src/triangle/*.cpp src/triangle/*.h external/*.h)

add_executable(vulkantest ${SOURCES})
target_link_libraries (vulkantest  ${EXTRA_LIBS})
  
target_link_libraries(vulkantest glfw)
target_link_libraries(vulkantest vulkan-1)
target_include_directories(vulkantest PRIVATE external/)

## PARTICLES:

file(GLOB_RECURSE SOURCES2 src/particles_compute_only/*.cpp src/particles_compute_only/*.h external/*.h)

add_executable(particles_compute_only ${SOURCES2})
target_link_libraries (particles_compute_only  ${EXTRA_LIBS})


## ASYNC:

file(GLOB_RECURSE SOURCES3 src/asyncParticles_v1_transfer/*.cpp src/asyncParticles_v1_transfer/*.h external/*.h)
add_executable(asyncParticles_v1_transfer ${SOURCES3})
target_link_libraries (asyncParticles_v1_transfer  ${EXTRA_LIBS})

## sim:

file(GLOB_RECURSE SOURCES4 src/simulation_test/*.cpp src/simulation_test/*.h external/*.h)
add_executable(simulation_test ${SOURCES4})
target_link_libraries (simulation_test  ${EXTRA_LIBS})
  
 ## LINK:
 
## ASYNC2:

file(GLOB_RECURSE SOURCES3 src/asyncParticles_v2_double/*.cpp src/asyncParticles_v2_double/*.h external/*.h)
add_executable(asyncParticles_v2_double ${SOURCES3})
target_link_libraries (asyncParticles_v2_double  ${EXTRA_LIBS})
  
target_link_libraries(particles_compute_only glfw)
target_link_libraries(particles_compute_only vulkan-1)
target_include_directories(particles_compute_only PRIVATE external/)

target_link_libraries(asyncParticles_v1_transfer glfw)
target_link_libraries(asyncParticles_v1_transfer vulkan-1)
target_include_directories(asyncParticles_v1_transfer PRIVATE external/)

target_link_libraries(asyncParticles_v2_double glfw)
target_link_libraries(asyncParticles_v2_double vulkan-1)
target_include_directories(asyncParticles_v2_double PRIVATE external/)

target_link_libraries(simulation_test glfw)
target_link_libraries(simulation_test vulkan-1)
target_include_directories(simulation_test PRIVATE external/)


# run shader compile and copy
add_dependencies(simulation_test copy_res)
  
add_custom_target(copy_res ALL
COMMAND "${PROJECT_SOURCE_DIR}/res/shaders/shadercompile.bat" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/res/shaders"
COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/res") 


# set WORKING_DIRECTORY
set_target_properties(simulation_test 
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$(Configuration)
)