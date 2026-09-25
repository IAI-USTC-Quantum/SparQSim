/**
 * @file sort_state.cu
 * @brief CUDA parallel implementation of sort_state
 * @details Implements the basis-state sorting operators declared in sort_state.h (sorting/merging by the
 *          active register key) with thrust device vectors and CUDA kernels (GPU path; GPU builds are
 *          currently disabled in CMake)
 */
#include "sort_state.h"
#include "cuda_utils.cuh"

#ifdef USE_CUDA

namespace qram_simulator {

} // namespace qram_simulator

#endif // USE_CUDA