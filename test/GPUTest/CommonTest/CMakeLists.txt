cmake_minimum_required(VERSION 3.20)
project(CudaExecutabilityTest CUDA)
add_executable(CudaExecutabilityTest CudaExecutabilityTest.cu)
target_link_libraries(CudaExecutabilityTest PRIVATE SparQ_cuda)

# Target configuration is provided uniformly by sparq_setup_cuda_target in the submodule's cmake/CUDA.cmake
# (Target architectures inherit the top-level CMAKE_CUDA_ARCHITECTURES; no longer falls back to hardcoded 75).
include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
sparq_setup_cuda_target(CudaExecutabilityTest)
