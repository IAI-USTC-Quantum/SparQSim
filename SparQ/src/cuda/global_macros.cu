/**
 * @file global_macros.cu
 * @brief global_macros 的 CUDA 并行实现
 * @details 以 thrust 设备向量与 CUDA 内核实现 global_macros.h 中宏在 CUDA 编译单元所需的定义（GPU 路径，当前 CMake 暂时屏蔽 GPU 构建）
 */
#include "global_macros.h"
#include "cuda_utils.cuh"

#ifdef USE_CUDA

namespace qram_simulator {

} // namespace qram_simulator

#endif // USE_CUDA