cmake_minimum_required(VERSION 3.20)
project(CudaExecutabilityTest CUDA)
add_executable(CudaExecutabilityTest CudaExecutabilityTest.cu)
target_link_libraries(CudaExecutabilityTest PRIVATE SparQ_cuda)

# 目标配置统一由子模块 cmake/CUDA.cmake 的 sparq_setup_cuda_target 提供
# （目标架构继承顶层 CMAKE_CUDA_ARCHITECTURES，不再回退硬编码 75）。
include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
sparq_setup_cuda_target(CudaExecutabilityTest)
