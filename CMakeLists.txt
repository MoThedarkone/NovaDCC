# CMakeList.txt : CMake project for NovaDcc, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.20)
project(NovaDCC LANGUAGES CXX)

# Ensure MSVC uses the dynamic CRT (MultiThreaded DLL) to avoid conflicts between static and dynamic CRT libs
if(MSVC)
    # Use the DLL CRT for release-like builds. This tells CMake to pass /MD or /MDd appropriately.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "MSVC runtime library" FORCE)
endif()

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect source files
file(GLOB_RECURSE NOVA_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.c
)

# If local external/imgui/backends exists, add backend cpp files to sources
if(EXISTS "${CMAKE_SOURCE_DIR}/external/imgui/backends")
    file(GLOB IMGUI_BACKENDS
        "${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp"
        "${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp"
    )
    list(APPEND NOVA_SOURCES ${IMGUI_BACKENDS})
endif()

# If the project contains a vendored ImGuizmo implementation, add its sources
if(EXISTS "${CMAKE_SOURCE_DIR}/external/imguizmo")
    file(GLOB IMGUZMO_SOURCES
        "${CMAKE_SOURCE_DIR}/external/imguizmo/*.cpp"
        "${CMAKE_SOURCE_DIR}/external/imguizmo/*.c"
    )
    if(IMGUZMO_SOURCES)
        list(APPEND NOVA_SOURCES ${IMGUZMO_SOURCES})
    endif()
endif()

add_executable(NovaDCC ${NOVA_SOURCES})

# Add src directory to the include paths for IDEs and tools
target_include_directories(NovaDCC PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Add local Assimp install include dir (from manual build)
target_include_directories(NovaDCC PRIVATE "${CMAKE_SOURCE_DIR}/external/assimp_install/include")
# Prefer generated assimp build include (contains config.h) if present
if(EXISTS "${CMAKE_SOURCE_DIR}/external/assimp_build/include")
    target_include_directories(NovaDCC PRIVATE "${CMAKE_SOURCE_DIR}/external/assimp_build/include")
endif()
# (Do not add external/assimp_src include - prefer installed headers)

# Link the manually built Assimp lib if present in common locations
# Search for common lib filenames (debug/release variants) and link the first match.
set(_assimp_lib_candidates
    "${CMAKE_SOURCE_DIR}/external/assimp_install/lib/assimp*.lib"
    "${CMAKE_SOURCE_DIR}/external/assimp_build/lib/Debug/assimp*.lib"
    "${CMAKE_SOURCE_DIR}/external/assimp_build/lib/Release/assimp*.lib"
    "${CMAKE_SOURCE_DIR}/external/assimp_build/bin/assimp*.lib"
)
set(_assimp_lib_found "")
foreach(_pat IN LISTS _assimp_lib_candidates)
    file(GLOB _matches LIST_DIRECTORIES false "${_pat}")
    if(_matches)
        list(GET _matches 0 _assimp_lib_found)
        break()
    endif()
endforeach()
if(_assimp_lib_found)
    message(STATUS "Linking assimp library: ${_assimp_lib_found}")
    target_link_libraries(NovaDCC PRIVATE "${_assimp_lib_found}")
else()
    message(WARNING "Assimp library not found in common locations. Continuing without linking assimp lib. If you need Assimp, install or point CMAKE_PREFIX_PATH.")
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# Helper: determine vcpkg root (prefer repo-local external/vcpkg)
if(EXISTS "${CMAKE_SOURCE_DIR}/external/vcpkg")
    set(_repo_vcpkg_root "${CMAKE_SOURCE_DIR}/external/vcpkg")
else()
    set(_repo_vcpkg_root "")
endif()

# Ensure a package vcpkg root and triplet are defined for fallback lookups
if(NOT "${_repo_vcpkg_root}" STREQUAL "")
    set(_pkg_vcpkg_root "${_repo_vcpkg_root}")
elseif(DEFINED VCPKG_ROOT)
    set(_pkg_vcpkg_root "${VCPKG_ROOT}")
elseif(DEFINED ENV{VCPKG_ROOT})
    set(_pkg_vcpkg_root "$ENV{VCPKG_ROOT}")
else()
    set(_pkg_vcpkg_root "${CMAKE_SOURCE_DIR}/external/vcpkg")
endif()
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(_pkg_triplet "x86-windows")
else()
    set(_pkg_triplet "${VCPKG_TARGET_TRIPLET}")
endif()

# Try to find glad via package config; if it fails, fall back to vcpkg installed path
find_package(glad CONFIG QUIET)
if(glad_FOUND)
    message(STATUS "Found glad via package config")
    target_link_libraries(NovaDCC PRIVATE glad::glad)
else()
    message(STATUS "glad not found via package config. Falling back to vcpkg installed location.")
    if(NOT "${_repo_vcpkg_root}" STREQUAL "")
        set(_vcpkg_root "${_repo_vcpkg_root}")
    elseif(DEFINED VCPKG_ROOT)
        set(_vcpkg_root "${VCPKG_ROOT}")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_root "$ENV{VCPKG_ROOT}")
    else()
        set(_vcpkg_root "${CMAKE_SOURCE_DIR}/external/vcpkg")
    endif()
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(_triplet "x86-windows")
    else()
        set(_triplet "${VCPKG_TARGET_TRIPLET}")
    endif()
    set(_glad_inc "${_vcpkg_root}/installed/${_triplet}/include")
    set(_glad_libdir "${_vcpkg_root}/installed/${_triplet}/lib")
    message(STATUS "Looking for glad headers in ${_glad_inc}")
    if(EXISTS "${_glad_inc}/glad/glad.h")
        target_include_directories(NovaDCC PRIVATE "${_glad_inc}")
        file(GLOB _glad_libs "${_glad_libdir}/glad*.lib")
        if(_glad_libs)
            message(STATUS "Linking glad libs: ${_glad_libs}")
            target_link_libraries(NovaDCC PRIVATE ${_glad_libs})
        else()
            message(FATAL_ERROR "glad libs not found in ${_glad_libdir}")
        endif()
    else()
        message(FATAL_ERROR "glad not found under ${_glad_inc}. Please install glad via vcpkg or set VCPKG_ROOT/VCPKG_TARGET_TRIPLET.")
    endif()
endif()

# Try to find glfw3, imgui, glm via package config but accept fallback to vcpkg installed libs
find_package(glfw3 CONFIG QUIET)
if(glfw3_FOUND)
    message(STATUS "Found glfw3 via package config")
    list(APPEND _pkg_link_targets glfw)
else()
    message(STATUS "glfw3 package config not found / incompatible; using vcpkg-installed libs")
    if(NOT "${_repo_vcpkg_root}" STREQUAL "")
        set(_pkg_vcpkg_root "${_repo_vcpkg_root}")
    elseif(DEFINED VCPKG_ROOT)
        set(_pkg_vcpkg_root "${VCPKG_ROOT}")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(_pkg_vcpkg_root "$ENV{VCPKG_ROOT}")
    else()
        set(_pkg_vcpkg_root "${CMAKE_SOURCE_DIR}/external/vcpkg")
    endif()
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(_pkg_triplet "x86-windows")
    else()
        set(_pkg_triplet "${VCPKG_TARGET_TRIPLET}")
    endif()
    set(_glfw_inc "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include")
    set(_glfw_libdir "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/lib")
    message(STATUS "Looking for glfw headers in ${_glfw_inc}")
    if(EXISTS "${_glfw_inc}/GLFW/glfw3.h")
        target_include_directories(NovaDCC PRIVATE "${_glfw_inc}")
        file(GLOB _glfw_libs "${_glfw_libdir}/glfw3*.lib")
        if(_glfw_libs)
            message(STATUS "Linking glfw libs: ${_glfw_libs}")
            target_link_libraries(NovaDCC PRIVATE ${_glfw_libs})
        else()
            message(FATAL_ERROR "glfw libs not found in ${_glfw_libdir}")
        endif()
    else()
        message(FATAL_ERROR "glfw headers not found in ${_glfw_inc}")
    endif()
endif()

find_package(imgui CONFIG QUIET)
if(imgui_FOUND)
    message(STATUS "Found imgui via package config")
    list(APPEND _pkg_link_targets imgui::imgui)
else()
    message(STATUS "imgui package config not found; using vcpkg-installed libs")
    set(_img_inc "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include")
    set(_img_libdir "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/lib")
    message(STATUS "Looking for imgui headers in ${_img_inc}")
    if(EXISTS "${_img_inc}/imgui.h")
        target_include_directories(NovaDCC PRIVATE "${_img_inc}")
        file(GLOB _img_libs "${_img_libdir}/imgui*.lib")
        if(_img_libs)
            message(STATUS "Linking imgui libs: ${_img_libs}")
            target_link_libraries(NovaDCC PRIVATE ${_img_libs})
        else()
            message(FATAL_ERROR "imgui libs not found in ${_img_libdir}")
        endif()
    else()
        message(FATAL_ERROR "imgui headers not found in ${_img_inc}")
    endif()
endif()

find_package(glm CONFIG QUIET)
if(glm_FOUND)
    message(STATUS "Found glm via package config")
    list(APPEND _pkg_link_targets glm::glm)
else()
    message(STATUS "glm package config not found; using vcpkg-installed include")
    set(_glm_inc "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include")
    message(STATUS "Looking for glm headers in ${_glm_inc}")
    if(EXISTS "${_glm_inc}/glm/glm.hpp")
        target_include_directories(NovaDCC PRIVATE "${_glm_inc}")
    else()
        message(FATAL_ERROR "glm headers not found in ${_glm_inc}")
    endif()
endif()

# If local external/imgui exists, add its include dirs so backends headers are found
if(EXISTS "${CMAKE_SOURCE_DIR}/external/imgui")
    target_include_directories(NovaDCC PRIVATE
        ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/external/imgui
        ${CMAKE_SOURCE_DIR}/external/imgui/backends
    )
endif()

# Define that ImGui's OpenGL backend uses GLAD
target_compile_definitions(NovaDCC PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# If local vendor ImGuizmo exists, include it and enable USE_IMGUZMO
if(EXISTS "${CMAKE_SOURCE_DIR}/external/imguizmo")
    message(STATUS "ImGuizmo detected in external/imguizmo; enabling USE_IMGUZMO")
    target_include_directories(NovaDCC PRIVATE "${CMAKE_SOURCE_DIR}/external/imguizmo")
    target_compile_definitions(NovaDCC PRIVATE USE_IMGUZO)
endif()

# Link against other vcpkg targets and OpenGL
if(glfw3_FOUND)
    target_link_libraries(NovaDCC PRIVATE glfw)
endif()
if(imgui_FOUND)
    target_link_libraries(NovaDCC PRIVATE imgui::imgui)
endif()
if(glm_FOUND)
    target_link_libraries(NovaDCC PRIVATE glm::glm)
endif()

# Find Assimp (via package config or vcpkg)
find_package(assimp CONFIG QUIET)
if(assimp_FOUND)
    message(STATUS "Found assimp via package config")
    target_link_libraries(NovaDCC PRIVATE assimp::assimp)
else()
    message(STATUS "assimp package config not found; using vcpkg-installed libs")
    set(_assimp_inc "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include")
    set(_assimp_libdir "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/lib")
    message(STATUS "Looking for assimp headers in ${_assimp_inc}")
    if(EXISTS "${_assimp_inc}/assimp/scene.h")
        target_include_directories(NovaDCC PRIVATE "${_assimp_inc}")
        file(GLOB _assimp_libs "${_assimp_libdir}/assimp*.lib")
        if(_assimp_libs)
            message(STATUS "Linking assimp libs: ${_assimp_libs}")
            target_link_libraries(NovaDCC PRIVATE ${_assimp_libs})
        else()
            message(WARNING "assimp libs not found in ${_assimp_libdir}. Continuing without assimp.")
        endif()
    else()
        message(WARNING "assimp headers not found in ${_assimp_inc}. Continuing without assimp.")
    endif()
endif()

# tinygltf (header-only) — prefer vcpkg-installed include; avoid FetchContent build failures
set(_tiny_inc1 "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include/tinygltf/tiny_gltf.h")
set(_tiny_inc2 "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include/tiny_gltf.h")
if(EXISTS "${_tiny_inc1}")
    target_include_directories(NovaDCC PRIVATE "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include")
    message(STATUS "Using tinygltf from vcpkg include")
elseif(EXISTS "${_tiny_inc2}")
    target_include_directories(NovaDCC PRIVATE "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include")
    message(STATUS "Using tinygltf from vcpkg include")
else()
    message(WARNING "tinygltf not found in vcpkg installed include. Skipping FetchContent to avoid build failures. If you need tinygltf, install via vcpkg or add it to external/.")
endif()

# If a local tinygltf include exists in external/, add it so real headers are used instead of stubs
if(EXISTS "${CMAKE_SOURCE_DIR}/external/tinygltf/include")
    target_include_directories(NovaDCC PRIVATE "${CMAKE_SOURCE_DIR}/external/tinygltf/include")
endif()

# tinyfiledialogs: prefer vcpkg-installed include, otherwise accept local external/ location
if(EXISTS "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include/tinyfiledialogs.h")
    message(STATUS "Using tinyfiledialogs from vcpkg include")
    target_include_directories(NovaDCC PRIVATE "${_pkg_vcpkg_root}/installed/${_pkg_triplet}/include")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs/include/tinyfiledialogs.h")
    message(STATUS "Using tinyfiledialogs from external/tinyfiledialogs/include")
    target_include_directories(NovaDCC PRIVATE "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs/include")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs/tinyfiledialogs.h")
    message(STATUS "Using tinyfiledialogs from external/tinyfiledialogs")
    target_include_directories(NovaDCC PRIVATE "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs")
else()
    message(WARNING "tinyfiledialogs header not found. If you need tinyfiledialogs, install via vcpkg or add it to external/tinyfiledialogs.")
endif()

# Ensure include dirs are present for the compiler (add both possible local locations)
target_include_directories(NovaDCC PRIVATE
    "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs/include"
    "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs"
)

target_link_libraries(NovaDCC
    PRIVATE
        OpenGL::GL
)

# Enable experimental GLM extensions used by some gtx headers
add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

# Set output directory for executable
set_target_properties(NovaDCC PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Configured NovaDCC with ${NOVA_SOURCES}")

# Post-build: ensure glfw3.dll is copied to the executable output folder if present in common vcpkg locations
if(TARGET NovaDCC)
  set(_glfw_dll_paths
    "${CMAKE_SOURCE_DIR}/external/vcpkg/installed/x64-windows/bin/glfw3.dll"
    "${CMAKE_SOURCE_DIR}/external/vcpkg/installed/x86-windows/bin/glfw3.dll"
    "${CMAKE_SOURCE_DIR}/external/vcpkg/buildtrees/glfw3/x64-windows-rel/src/glfw3.dll"
    "${CMAKE_SOURCE_DIR}/external/vcpkg/buildtrees/glfw3/x86-windows-rel/src/glfw3.dll"
    "${CMAKE_SOURCE_DIR}/external/vcpkg/buildtrees/glfw3/x64-windows-dbg/src/glfw3.dll"
    "${CMAKE_SOURCE_DIR}/external/vcpkg/buildtrees/glfw3/x86-windows-dbg/src/glfw3.dll"
  )
  foreach(_p IN LISTS _glfw_dll_paths)
    if(EXISTS "${_p}")
      add_custom_command(TARGET NovaDCC POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_p}" $<TARGET_FILE_DIR:NovaDCC>
        COMMENT "Copying glfw3.dll to target output"
      )
      break()
    endif()
  endforeach()
endif()
