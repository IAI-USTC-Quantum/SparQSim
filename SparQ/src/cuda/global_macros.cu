/**
 * @file global_macros.cu
 * @brief CUDA parallel implementation of global_macros
 * @details Implements with thrust device vectors and CUDA kernels the definitions required by the macros of
 *          global_macros.h in CUDA translation units (GPU path; the GPU build is temporarily disabled in CMake)
 */
#include "global_macros.h"
#include "cuda_utils.cuh"

#ifdef USE_CUDA

namespace qram_simulator {

} // namespace qram_simulator

#endif // USE_CUDA