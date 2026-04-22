# Fetch libigl pinned to v2.5.0 — last tag that still ships
#   include/igl/copyleft/comiso/{miq,nrosy,frame_field}.{h,cpp}
# libigl's own CMake will in turn FetchContent CoMISo, gmm, eigen, glfw, imgui.

include(FetchContent)

FetchContent_Declare(
    libigl
    GIT_REPOSITORY https://github.com/libigl/libigl.git
    GIT_TAG        v2.5.0          # DO NOT bump: v2.6.0 removed comiso/miq
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libigl)

# ---------------------------------------------------------------------------
# libQEx (Ebke et al. 2013) — converts MIQ's integer-grid map to a quad mesh.
# Requires OpenMesh (install via vcpkg: `vcpkg install openmesh:x64-windows`).
# Pinned to the last master commit (2020-08-22); project is dormant.
# ---------------------------------------------------------------------------
find_package(OpenMesh CONFIG QUIET)
if(NOT OpenMesh_FOUND)
    message(FATAL_ERROR
        "OpenMesh not found. Install via vcpkg:\n"
        "  <vcpkg-root>\\vcpkg install openmesh:x64-windows\n"
        "Then re-run cmake with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake")
endif()

FetchContent_Declare(
    libqex
    GIT_REPOSITORY https://github.com/hcebke/libQEx.git
    GIT_TAG        c0a4dc6         # master as of 2020-08-22
    GIT_SHALLOW    TRUE
)
# Don't let libQEx pull in its demo/cmdline_tool (uses POSIX getopt — fails MSVC)
set(BUILD_UNIT_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_GetProperties(libqex)
if(NOT libqex_POPULATED)
    FetchContent_Populate(libqex)
    # Only add the library itself; skip its demo/ which has MSVC-incompatible deps
    add_subdirectory(${libqex_SOURCE_DIR} ${libqex_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# --- Patch libQEx targets ---------------------------------------------------
# libQEx's own CMakeLists.txt was written against the legacy OpenMesh CMake
# (which set OPENMESH_INCLUDE_DIR/OPENMESH_LIBRARY_DIR as global vars). vcpkg
# provides the MODERN OpenMeshCore / OpenMeshTools imported targets instead,
# so QExStatic ends up with no OpenMesh include path. Inject it here.
foreach(_qex_tgt QEx QExStatic)
    if(TARGET ${_qex_tgt})
        target_link_libraries(${_qex_tgt} PUBLIC OpenMeshCore OpenMeshTools)
        # OpenMesh headers use <OpenMesh/Core/...> — target handles include dir.
        # Also silence MSVC's min/max macro and secure-CRT warnings QEx triggers:
        target_compile_definitions(${_qex_tgt} PRIVATE
            NOMINMAX
            _USE_MATH_DEFINES          # OpenMesh/Core/System/compiler.hh forces this
            _CRT_SECURE_NO_WARNINGS
            _SCL_SECURE_NO_WARNINGS)
    endif()
endforeach()
