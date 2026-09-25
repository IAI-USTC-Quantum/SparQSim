cmake_minimum_required(VERSION 3.20)
project(CudaBenchmark CUDA)
add_executable(CudaBenchmark CudaBenchmark.cu)
target_link_libraries(CudaBenchmark PRIVATE SparQ_cuda)

# Target configuration is provided uniformly by sparq_setup_cuda_target in the submodule's cmake/CUDA.cmake
# (Target architectures inherit the top-level CMAKE_CUDA_ARCHITECTURES).
include(${QRAM_CORE_DIR}/cmake/CUDA.cmake)
sparq_setup_cuda_target(CudaBenchmark)
