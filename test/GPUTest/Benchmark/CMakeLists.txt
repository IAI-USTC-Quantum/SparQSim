cmake_minimum_required(VERSION 3.20)
project(CudaBenchmark CUDA)
add_executable(CudaBenchmark CudaBenchmark.cu)
target_link_libraries(CudaBenchmark PRIVATE SparQ_cuda)

# 目标配置统一由子模块 cmake/CUDA.cmake 的 sparq_setup_cuda_target 提供
# （目标架构继承顶层 CMAKE_CUDA_ARCHITECTURES）。
include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
sparq_setup_cuda_target(CudaBenchmark)
