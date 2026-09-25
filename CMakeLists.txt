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

# OpenMP 仅用于 CPU 并行；限定 CXX 组件——本项目只启用 CXX 语言，且启用
# CUDA 语言后 find_package 会额外去找不存在的 OpenMP_CUDA 而失败。
find_package(OpenMP REQUIRED COMPONENTS CXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
find_package(TBB QUIET)
if(TBB_FOUND)
    message(STATUS "Found TBB: ${TBB_VERSION}")
endif()

# CUDA GPU 后端：默认关闭（wheel / CI 恒为 CPU-only）。开启需本机 CUDA
# 工具链（CUDA 13 / CCCL 3 实测通过；生产 CondRot 原语的 GPU 内核仍待
# 补充，期间相关算子在 GPU 态自动回退 CPU 路径执行）。
option(SPARQ_ENABLE_CUDA "Build the CUDA GPU backend (requires a CUDA toolkit)" OFF)
set(QRAM_ENABLE_CUDA ${SPARQ_ENABLE_CUDA} CACHE BOOL "" FORCE)
if(SPARQ_ENABLE_CUDA)
    include(${CMAKE_CURRENT_SOURCE_DIR}/extern/qram-simulator/cmake/CUDA.cmake)
    sparq_prepare_cuda_env()   # VS 生成器：自动推导 CudaToolkitDir，免手工配置
    include(CheckLanguage)
    check_language(CUDA)
    if(NOT CMAKE_CUDA_COMPILER)
        message(FATAL_ERROR "SPARQ_ENABLE_CUDA=ON but no CUDA compiler (nvcc) found")
    endif()
    # 默认架构须在 enable_language 之前设置才会生效（否则 CMake 用工具包默认值）。
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_ARCHITECTURES 89)   # 默认 Ada（如 RTX 4090）；可 -D 覆盖
    endif()
    enable_language(CUDA)
    set(CUDA_FOUND TRUE)   # 既有子目录（SparQ/_cuda、GPUTest、Experiments）以该变量门控
    message(STATUS "CUDA backend enabled: ${CMAKE_CUDA_COMPILER} (arch ${CMAKE_CUDA_ARCHITECTURES})")
else()
    set(CUDA_FOUND FALSE)
endif()

# MSVC: force UTF-8 source interpretation (headers carry UTF-8 Chinese
# comments; legacy code pages such as CP936 mis-decode them).
# C++ 目标直接加 /utf-8；CUDA 目标必须经 -Xcompiler 转发给宿主编译器——
# nvcc 会把裸 "/utf-8" 当作第二个输入文件（"A single input file is
# required for a non-link phase"）。
if (MSVC)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
        "$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/utf-8>")
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
    # CUDA TU 以裸文件名 include "cuda_utils.cuh"（历史扁平布局），需单独暴露
    ${QRAM_CORE_DIR}/Common/include/cuda
    ${QRAM_CORE_DIR}/QRAM/include
    ${QRAM_CORE_DIR}/ThirdParty
    ${QRAM_CORE_DIR}/ThirdParty/argparse
    ${QRAM_CORE_DIR}/ThirdParty/fmt/include
    ${QRAM_CORE_DIR}/ThirdParty/eigen-3.4.0
)
include_directories(${SPARQSIM_INCLUDE_DIRS})

# Python 扩展是共享模块（.so/.pyd）；Linux 上链入的静态库
# （SparQ_Algorithm / SparQ_Simulator 及 submodule 的核心库）必须以
# -fPIC 编译，否则链接报 "relocation R_X86_64_PC32 ... recompile
# with -fPIC"（Windows/MSVC 无此概念，本地不复现）。
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

# CUDA 伞目标：GPUTest / Experiments 的既有链接名，聚合全部 *_cuda 变体
# （各变体由 SparQ/src、SparQ_Algorithm/src 及子模块在 CUDA_FOUND 时创建）。
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
# pybind11_add_module 默认给模块链接 pybind11::lto（MSVC 上 = /GL + /LTCG）；
# PySparQ/_core 单编译单元吸入全部 SparQ 头文件后，MSVC 链接期代码生成
# 阶段间歇性触发内部编译器错误 C1001（本机 4 次构建 3 次崩于此，崩点
# 各不相同：type_caster_base.h / pybind11.h / quantum_arithmetic.h）。
# 显式定义 CMAKE_INTERPROCEDURAL_OPTIMIZATION 可让 pybind11 跳过 lto
# 链接（官方逃生口），以牺牲少量跨翻译单元优化换取可构建性。
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
