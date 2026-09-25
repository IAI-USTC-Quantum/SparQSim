/**
 * @file qft.cu
 * @brief qft 的 CUDA 并行实现
 * @details 以 thrust 设备向量与 CUDA 内核实现 qft.h 中声明的 QFT、InverseQFT、QFT_Full（GPU 路径，当前 CMake 暂时屏蔽 GPU 构建）
 */
#include "qft.h"
#include "cuda_utils.cuh"

#ifdef USE_CUDA

namespace qram_simulator {

} // namespace qram_simulator

#endif // USE_CUDA