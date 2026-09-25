/**
 * @file condrot.cu
 * @brief CUDA parallel implementation of condrot
 * @details Implements the controlled rotation operators declared in condrot.h, such as CondRot_Rational_Bool
 *          and CondRot_General_Bool_Fast, with thrust device vectors and CUDA kernels (GPU path; GPU builds
 *          are currently disabled in CMake)
 */
#include "condrot.h"
#include "cuda_utils.cuh"
#include "cuda/condrot.cuh"

namespace qram_simulator {

} // namespace qram_simulator
