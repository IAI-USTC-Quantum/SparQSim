/**
 * @file dark_magic.cu
 * @brief dark_magic 的 CUDA 并行实现
 * @details 以 thrust 设备向量与 CUDA 内核实现 dark_magic.h 中声明的特殊优化算子（针对特定电路结构的手工优化路径）（GPU 路径，当前 CMake 暂时屏蔽 GPU 构建）
 */
#include "dark_magic.h"
#include "cuda_utils.cuh"
#include "cuda/basic_components.cuh"

#ifdef USE_CUDA

namespace qram_simulator {
	void Normalize::operator()(CuSparseState& state) const
	{
		profiler _("Normalize cuda");

		// 1. Calculate sum of probabilities on GPU
		double sum_prob_sqr = thrust::transform_reduce(
			thrust::device,
			state.sparse_state_gpu.begin(),
			state.sparse_state_gpu.end(),
			AbsSqrFunctor(),
			0.0,
			thrust::plus<double>()
		);

		// 2. Calculate normalization factor
		double sum_prob = 1.0 / std::sqrt(sum_prob_sqr);

		if (std::isnan(sum_prob))
			throw_bad_result();

		// 3. Renormalize on GPU
		thrust::for_each(
			thrust::device,
			state.sparse_state_gpu.begin(),
			state.sparse_state_gpu.end(),
			Normalize_Functor(sum_prob)
		);

		// CheckNormalization()(state);
	}

	struct Init_Unsafe_Functor {
		int id;
		uint64_t value;

		Init_Unsafe_Functor(int id_, uint64_t value_) : id(id_), value(value_) {}


		__host__ __device__ void operator()(System& s) const {
			CuGet(s, id).value = value;
		}
	};

	void Init_Unsafe::operator()(CuSparseState& state) const
	{
		state.move_to_gpu();

		thrust::for_each(
			thrust::device,
			state.sparse_state_gpu.begin(),
			state.sparse_state_gpu.end(),
			Init_Unsafe_Functor(id, value)
		);
	}
} // namespace qram_simulator

#endif // USE_CUDA