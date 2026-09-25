/**
 * @file sort_state.cu
 * @brief sort_state 的 CUDA 并行实现
 * @details 以 thrust 设备向量与 CUDA 内核实现 sort_state.h 中声明的基态排序算子（按激活寄存器键排序/合并）（GPU 路径，当前 CMake 暂时屏蔽 GPU 构建）
 */
#include "sort_state.h"
#include "cuda_utils.cuh"

#ifdef USE_CUDA

namespace qram_simulator {

} // namespace qram_simulator

#endif // USE_CUDA