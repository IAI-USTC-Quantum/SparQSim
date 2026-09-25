/**
 * @file cuda_utils.cu
 * @brief cuda_utils 的 CUDA 并行实现
 * @details 以 thrust 设备向量与 CUDA 内核实现 cuda_utils.cuh 中声明的 GPU 辅助函数（设备属性、内存与核启动辅助）（GPU 路径，当前 CMake 暂时屏蔽 GPU 构建）
 */
#include "cuda_utils.cuh"

#ifdef USE_CUDA

namespace qram_simulator {
    void throw_cuda_runtime_error()
    {
#ifndef QRAM_Release
        throw std::runtime_error("[CUDA Fatal] Cuda runtime error. Please check the code.");
#endif
    }

    void throw_cuda_runtime_error(const char* errinfo)
    {
#ifndef QRAM_Release
        throw std::runtime_error(errinfo);
#endif
    }

    void throw_cuda_runtime_error(const std::string& errinfo)
    {
#ifndef QRAM_Release
        throw std::runtime_error(errinfo);
#endif
    }

    void throw_cuda_runtime_error(std::string_view errinfo)
    {
        return throw_cuda_runtime_error(errinfo.data());
    }

    __global__ void hello_cuda() {
        printf("Hello from GPU thread %d!\n", threadIdx.x);
    }

    void run_cuda_kernel() {
        hello_cuda << <1, 5 >> > ();
        CUDA_CHECK(cudaDeviceSynchronize());
    }
} // namespace qram_simulator

#endif