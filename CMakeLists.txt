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

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build (Debug/Release)" FORCE)
endif()

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

add_subdirectory(external/pybind11)
file(GLOB_RECURSE source "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

#Linking library ---
add_library(CGEngineCore STATIC ${source}) # Or SHARED if preferred
add_subdirectory(src/Bindings)
target_include_directories(CGEngineCore PUBLIC
    ${GLEW_INCLUDE_DIR}      # Manually set path - should be fine
    ${GLM_INCLUDE_DIR}       # Manually set path - should be fine
    # Use generator expression to get include dirs from the SFML target:
    $<TARGET_PROPERTY:SFML::Graphics,INTERFACE_INCLUDE_DIRECTORIES>
    ${OPENGL_INCLUDE_DIRS}  # Keep using the variable for OpenGL for now
    ${ASSIMP_INCLUDE_DIR}     # Manually set path - should be fine
)
target_link_libraries(CGEngineCore PUBLIC
    SFML::Graphics
    # Add other direct dependencies IF CGEngineCore code uses them directly:
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARY}
    ${ASSIMP_LIBRARY}
)
get_target_property(CGECORE_INCLUDES CGEngineCore INCLUDE_DIRECTORIES)
target_compile_features(CGEngineCore PRIVATE cxx_std_17)

add_executable(main src/main.cpp) # Assuming main.cpp is the entry point
target_link_libraries(main PRIVATE CGEngineCore SFML::Graphics ${OPENGL_LIBRARIES} ${GLEW_LIBRARY} ${ASSIMP_LIBRARY})

add_custom_command(TARGET main POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/ $<TARGET_FILE_DIR:main> COMMAND ${CMAKE_COMMAND} -E echo "Installed resources")
add_custom_command(TARGET main POST_BUILD COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile COMMAND ${CMAKE_COMMAND} -E echo "Built Doxygen documentation")

add_custom_target(update_resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "${CMAKE_BINARY_DIR}/bin"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Updating resource files in build directory"
)

# Optional: Set the DLL path as a variable
set(ASSIMP_DLL "${CMAKE_SOURCE_DIR}/external/assimp/lib/assimp-vc143-mt.dll")

# Verify DLL exists and add copy command
if(EXISTS ${ASSIMP_DLL})
    add_custom_command(
        TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            "${ASSIMP_DLL}"
            "$<TARGET_FILE_DIR:main>"
        COMMAND ${CMAKE_COMMAND} -E echo "Copied Assimp DLL to output directory"
    )
else()
    message(WARNING "Assimp DLL not found at: ${ASSIMP_DLL}")
endif()

# Add custom compile definitions based on build type
target_compile_definitions(main PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>
)

# Set compiler flags for different build types
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()