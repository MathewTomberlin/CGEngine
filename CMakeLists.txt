cmake_minimum_required(VERSION 3.28)
project(CGEngine LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Modules)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(GLEW_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/glew-2.1.0/include/)
set(GLEW_LIBRARY ${CMAKE_SOURCE_DIR}/external/glew-2.1.0/lib/Release/x64/glew32s.lib)
set(GLM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/glm)
set(ASSIMP_LIBRARY ${CMAKE_SOURCE_DIR}/external/assimp/lib/assimp-vc143-mt.lib)
set(ASSIMP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/assimp/include)
set (CMAKE_FIND_FRAMEWORK "ONCE")

option(BUILD_DOC "Build documentation" ON)

find_package (OpenGL REQUIRED)

find_package(Doxygen)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

file(GLOB_RECURSE source "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

set (PROJECT_INCLUDES ${GLEW_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${SFML_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIR})
add_executable(main ${source})
include_directories(${PROJECT_INCLUDES})
add_custom_command(TARGET main POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/ $<TARGET_FILE_DIR:main> COMMAND ${CMAKE_COMMAND} -E echo "Installed resources")
add_custom_command(TARGET main POST_BUILD COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile COMMAND ${CMAKE_COMMAND} -E echo "Built Doxygen documentation")
target_compile_features(main PRIVATE cxx_std_17)
target_link_libraries(main PRIVATE SFML::Graphics)
target_link_libraries (main PRIVATE ${OPENGL_LIBRARIES})
target_link_libraries(main PRIVATE ${GLEW_LIBRARY})
target_link_libraries(main PRIVATE ${ASSIMP_LIBRARY})

