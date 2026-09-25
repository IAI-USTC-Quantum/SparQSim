# SparQSim — the pysparq Python package + the SparQ C++ framework.
#
# The QRAM C++ core (Common + QRAM + ThirdParty) is consumed from the
# QRAM-Simulator git submodule (relative URL ../QRAM-Simulator.git, resolves
# on both Gitea and GitHub); the SparQ simulator, the algorithm library and
# the pysparq bindings live HERE. scikit-build-core drives this file when
# building the wheel; everything compiles in one CMake invocation, exactly
# like the former monorepo build. The separate qram_simulator thin binding
# is packaged in the QRAM-Simulator repo.
cmake_minimum_required(VERSION 3.18)

project(SparQSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Centralized output layout (matches the former monorepo convention).
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/lib)

# Turn off everything the core repo builds for its own development; these
# must be cache FORCE so option() inside the core does not re-default them.
set(QRAM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(QRAM_BUILD_EXPERIMENTS OFF CACHE BOOL "" FORCE)

# Development-only target groups (the wheel build leaves all of them OFF).
option(SPARQ_BUILD_TESTS "Build C++ test targets (SparQ/test, test/)" OFF)
option(SPARQ_BUILD_EXPERIMENTS "Build experiment programs (Experiments/)" OFF)
option(SPARQ_BUILD_EXAMPLES "Build example programs (examples/)" OFF)

if(SPARQ_BUILD_TESTS)
    enable_testing()
endif()

# Find pybind11 in the TOP scope: its imported targets (pybind11::module …)
# are directory-scoped, so a find_package inside a subdirectory would be
# invisible to sibling scopes. (scikit-build-core provides pybind11 in the
# build env; manual builds need `pip install pybind11`.)
find_package(pybind11 CONFIG REQUIRED)

# OpenMP is used only for CPU parallelism; restrict to the CXX component —
# this project enables only the CXX language, and once the CUDA language is
# enabled find_package would additionally look up the non-existent OpenMP_CUDA
# and fail.
find_package(OpenMP REQUIRED COMPONENTS CXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
find_package(TBB QUIET)
if(TBB_FOUND)
    message(STATUS "Found TBB: ${TBB_VERSION}")
endif()

# CUDA GPU backend: OFF by default (wheel / CI builds are always CPU-only).
# Enabling it requires a local CUDA toolkit (tested with CUDA 13 / CCCL 3;
# the GPU kernels for the production CondRot primitives are still pending —
# meanwhile the affected operators automatically fall back to the CPU path
# when running in GPU mode).
option(SPARQ_ENABLE_CUDA "Build the CUDA GPU backend (requires a CUDA toolkit)" OFF)
set(QRAM_ENABLE_CUDA ${SPARQ_ENABLE_CUDA} CACHE BOOL "" FORCE)
if(SPARQ_ENABLE_CUDA)
    include(${CMAKE_CURRENT_SOURCE_DIR}/extern/qram-simulator/cmake/CUDA.cmake)
    sparq_prepare_cuda_env()   # VS generator: auto-derives CudaToolkitDir, no manual setup needed
    include(CheckLanguage)
    check_language(CUDA)
    if(NOT CMAKE_CUDA_COMPILER)
        message(FATAL_ERROR "SPARQ_ENABLE_CUDA=ON but no CUDA compiler (nvcc) found")
    endif()
    # The default architecture must be set before enable_language to take
    # effect (otherwise CMake falls back to the toolkit's default).
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_ARCHITECTURES 89)   # default Ada (e.g. RTX 4090); override with -D
    endif()
    enable_language(CUDA)
    set(CUDA_FOUND TRUE)   # existing subdirectories (SparQ/_cuda, GPUTest, Experiments) gate on this variable
    message(STATUS "CUDA backend enabled: ${CMAKE_CUDA_COMPILER} (arch ${CMAKE_CUDA_ARCHITECTURES})")
else()
    set(CUDA_FOUND FALSE)
endif()

# MSVC: force UTF-8 source interpretation for C++ targets (headers carry
# UTF-8 Chinese comments; legacy code pages such as CP936 mis-decode them).
# CUDA targets must NOT forward /utf-8: a bare "/utf-8" is treated by nvcc as
# a second input file, and when forwarded via -Xcompiler nvcc intercepts the
# switch and breaks the frontend's parsing of BOM+UTF-8 literals
# ("unrecognized token / missing closing quote", observed in practice). The
# Chinese in CUDA sources relies on the UTF-8 BOM; the host pass degrades to
# a C4819 warning (harmless).
if (MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
endif()

# Flat include layout = local framework headers + the QRAM core's headers and
# ThirdParty (from the submodule). Mirrors the former monorepo layout and the
# wheel's installed site-packages/include/ (which the dynamic-operator JIT
# compiles against).
set(QRAM_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/qram-simulator)
set(SPARQSIM_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/SparQ/include
    ${CMAKE_CURRENT_SOURCE_DIR}/SparQ_Algorithm/include
    ${QRAM_CORE_DIR}/Common/include
    # CUDA TUs include "cuda_utils.cuh" by bare filename (legacy flat
    # layout); expose that directory separately
    ${QRAM_CORE_DIR}/Common/include/cuda
    ${QRAM_CORE_DIR}/QRAM/include
    ${QRAM_CORE_DIR}/ThirdParty
    ${QRAM_CORE_DIR}/ThirdParty/argparse
    ${QRAM_CORE_DIR}/ThirdParty/fmt/include
    ${QRAM_CORE_DIR}/ThirdParty/eigen-3.4.0
)
include_directories(${SPARQSIM_INCLUDE_DIRS})

# The Python extension is a shared module (.so/.pyd); the static libraries
# linked into it on Linux (SparQ_Algorithm / SparQ_Simulator and the
# submodule's core libraries) must be compiled with -fPIC, otherwise linking
# fails with "relocation R_X86_64_PC32 ... recompile with -fPIC" (no such
# concept on Windows/MSVC, so it does not reproduce locally).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The C++ core from the submodule: provides SparQ_Common, SparQ_QRAMSimulator
# and fmt, and installs the core headers into the flat include/ layout.
add_subdirectory(extern/qram-simulator)

# The SparQ framework (moved here from the former monorepo).
add_subdirectory(SparQ)
add_subdirectory(SparQ_Algorithm)

# The umbrella target — formerly defined in the monorepo's
# SparQ_Algorithm/src/CMakeLists.txt; consumers (PySparQ, tests, experiments,
# examples) link only this.
add_library(SparQ INTERFACE)
target_link_libraries(SparQ INTERFACE
    SparQ_Algorithm
    SparQ_Simulator
    SparQ_QRAMSimulator
    SparQ_Common
    fmt
)
if (TBB_FOUND)
    target_link_libraries(SparQ INTERFACE TBB::tbb)
endif()
target_include_directories(SparQ INTERFACE
    "$<BUILD_INTERFACE:${SPARQSIM_INCLUDE_DIRS}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# CUDA umbrella target: the existing link name for GPUTest / Experiments,
# aggregating all *_cuda variants (each variant is created by SparQ/src,
# SparQ_Algorithm/src and the submodule when CUDA_FOUND).
if(CUDA_FOUND)
    add_library(SparQ_cuda INTERFACE)
    target_link_libraries(SparQ_cuda INTERFACE
        SparQ_Simulator_cuda
        SparQ_Algorithm_cuda
        SparQ_Common_cuda
        SparQ_QRAMSimulator_cuda
    )
endif()

# Flat framework headers for the wheel's include/ (the core's install rules
# already cover Common/QRAM/eigen/fmt).
include(GNUInstallDirs)
install(DIRECTORY
    SparQ/include/
    SparQ_Algorithm/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Full-featured pysparq bindings. (The thin qram_simulator binding is built
# and published from the QRAM-Simulator core repo itself.)
#
# pybind11_add_module links pybind11::lto into the module by default
# (= /GL + /LTCG on MSVC); after PySparQ/_core pulls all SparQ headers into a
# single translation unit, the MSVC link-time code generation phase
# intermittently triggers internal compiler error C1001 (3 of 4 local builds
# crashed there, each at a different point: type_caster_base.h /
# pybind11.h / quantum_arithmetic.h).
# Explicitly defining CMAKE_INTERPROCEDURAL_OPTIMIZATION makes pybind11 skip
# the lto link (the official escape hatch), trading a small amount of
# cross-translation-unit optimization for buildability.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
add_subdirectory(PySparQ)

if(SPARQ_BUILD_TESTS)
    # Vendored googletest comes from the core submodule's ThirdParty.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(${QRAM_CORE_DIR}/ThirdParty/googletest EXCLUDE_FROM_ALL)
    add_subdirectory(test)
endif()

if(SPARQ_BUILD_EXPERIMENTS)
    add_subdirectory(Experiments)
endif()

if(SPARQ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
