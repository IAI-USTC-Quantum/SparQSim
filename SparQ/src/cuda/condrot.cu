/**
 * @file condrot.cu
 * @brief condrot 的 CUDA 并行实现
 * @details 以 thrust 设备向量与 CUDA 内核实现 condrot.h 中声明的 CondRot_Rational_Bool、CondRot_General_Bool_Fast 等条件旋转算子（GPU 路径，当前 CMake 暂时屏蔽 GPU 构建）
 */
#include "condrot.h"
#include "cuda_utils.cuh"
#include "cuda/condrot.cuh"

namespace qram_simulator {

} // namespace qram_simulator
