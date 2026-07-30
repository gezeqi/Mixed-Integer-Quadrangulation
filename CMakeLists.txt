cmake_minimum_required(VERSION 3.16)

# --- CMake 4.x compatibility shim ----------------------------------------
# libigl v2.5.0 and its transitive deps (CoMISo, gmm, ...) still declare
# cmake_minimum_required(VERSION <3.5) in places. CMake 4 refuses those.
# Setting this BEFORE project() makes CMake treat every sub-project as if it
# required at least 3.5, which is enough to silence the hard error.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(miq_repro LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- libigl options (must be set BEFORE include(libigl)) ---
option(LIBIGL_COPYLEFT_COMISO "Build libigl/CoMISo bindings (MIQ, NRosy)" ON)
option(LIBIGL_GLFW            "Build libigl GLFW viewer"                  ON)
option(LIBIGL_OPENGL          "Build libigl OpenGL"                       ON)
option(LIBIGL_IMGUI           "Build libigl ImGui plugin"                 ON)

# CMakeLists will fetch libigl v2.5.0 via CPM/FetchContent (helper below).
include(cmake/libigl.cmake)

# --- Targets -------------------------------------------------------------
add_executable(01_cross_field  src/01_cross_field.cpp)
target_link_libraries(01_cross_field  PRIVATE
    igl::core igl::glfw igl::imgui igl_copyleft::comiso)

add_executable(02_miq          src/02_miq.cpp)
target_link_libraries(02_miq          PRIVATE
    igl::core igl::glfw igl::imgui igl_copyleft::comiso)

add_executable(03_quad_extract src/03_quad_extract.cpp)
target_link_libraries(03_quad_extract PRIVATE
    igl::core igl::glfw igl::imgui igl_copyleft::comiso
    QExStatic)    # libQEx static lib — avoids MSVC dll-export issues
target_include_directories(03_quad_extract PRIVATE
    ${libqex_SOURCE_DIR}/interfaces/c)
target_compile_definitions(03_quad_extract PRIVATE
    NOMINMAX _USE_MATH_DEFINES    # qex.h pulls OpenMesh headers
    DLLEXPORT=)                   # static link: neutralize qex.h's dllimport/dllexport

add_executable(04_quad_extract_load_frames src/04_quad_extract_load_frames.cpp)
target_link_libraries(04_quad_extract_load_frames PRIVATE
        igl::core igl::glfw igl::imgui igl_copyleft::comiso
        QExStatic)    # libQEx static lib — avoids MSVC dll-export issues
target_include_directories(04_quad_extract_load_frames PRIVATE
        ${libqex_SOURCE_DIR}/interfaces/c)
target_compile_definitions(04_quad_extract_load_frames PRIVATE
        NOMINMAX _USE_MATH_DEFINES    # qex.h pulls OpenMesh headers
        DLLEXPORT=)                   # static link: neutralize qex.h's dllimport/dllexport

# Copy data folder next to binaries so relative paths just work
add_custom_target(copy_data ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:01_cross_field>/data)

# --- Runtime DLL deployment (Windows) ------------------------------------
# CoMISo ships a prebuilt MinGW OpenBLAS 0.2.14 (from 2014). That DLL
# access-violates on modern Windows the moment the MI solver calls into
# factorization (confirmed crash with exit code 0xC0000005 in MIQ).
#
# Fix: if vcpkg has installed a modern openblas.dll, use THAT and rename it
# to libopenblas.dll to satisfy CoMISo's import table. We still copy the
# rest of CoMISo's ext DLLs as a fallback for deps like pthreads.
if(WIN32)
    set(_comiso_openblas_bin
        "${CMAKE_BINARY_DIR}/_deps/comiso-src/ext/OpenBLAS-v0.2.14-Win64-int64/bin")
    set(_vcpkg_bin "")
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
        get_filename_component(_vcpkg_root "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
        get_filename_component(_vcpkg_root "${_vcpkg_root}/../.."   ABSOLUTE)
        set(_vcpkg_bin "${_vcpkg_root}/installed/${VCPKG_TARGET_TRIPLET}/bin")
    endif()

    foreach(_tgt 01_cross_field 02_miq 03_quad_extract 04_quad_extract_load_frames)
        # 1. CoMISo's bundled ext DLLs first (baseline)
        if(EXISTS "${_comiso_openblas_bin}")
            file(GLOB _comiso_dlls "${_comiso_openblas_bin}/*.dll")
            foreach(_dll ${_comiso_dlls})
                add_custom_command(TARGET ${_tgt} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:${_tgt}>")
            endforeach()
        endif()

        # 2. vcpkg DLLs OVERWRITE the bad MinGW openblas + deps
        if(_vcpkg_bin AND EXISTS "${_vcpkg_bin}")
            file(GLOB _vcpkg_dlls "${_vcpkg_bin}/*.dll")
            foreach(_dll ${_vcpkg_dlls})
                add_custom_command(TARGET ${_tgt} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_dll}" "$<TARGET_FILE_DIR:${_tgt}>")
            endforeach()
            # Rename openblas.dll -> libopenblas.dll so CoMISo's import resolves
            add_custom_command(TARGET ${_tgt} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_vcpkg_bin}/openblas.dll"
                    "$<TARGET_FILE_DIR:${_tgt}>/libopenblas.dll")
        endif()
    endforeach()
endif()
