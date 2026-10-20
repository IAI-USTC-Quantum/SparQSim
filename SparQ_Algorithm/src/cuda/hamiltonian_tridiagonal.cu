/**
 * @file hamiltonian_tridiagonal.cu
 * @brief CUDA implementations of the direct tridiagonal-walk oracles
 * @details See SparQ_Algorithm/include/hamiltonian_simulation_tridiagonal.h:
 *          bit-exact arithmetic mirrors of the QRAM-path composite register
 *          maps, element-parallel one-thread-per-system kernels over
 *          CuSparseState; T_Tridiagonal itself is a composition of operators
 *          that already provide CUDA overloads (CU_IMPL dispatches to the
 *          generic impl<CuSparseState>).
 */

#include "hamiltonian_simulation_tridiagonal.h"
#include "cuda_utils.cuh"
#include "cuda/sparse_state_simulator.cuh"

namespace qram_simulator {

	namespace CKS {

		namespace tridiag_walk_gpu_detail
		{
			struct TridiagOracle1Functor
			{
				size_t offset_id;
				size_t row_id;
				size_t slot_id;
				size_t out_id;
				TridiagonalSpec spec;

				__device__ void operator()(System& s) const
				{
					uint64_t off = CuGetAsUint64(s, offset_id, 64);
					uint64_t j = CuGetAsUint64(s, row_id, 64);
					uint64_t slot = CuGetAsUint64(s, slot_id, 64);
					CuGet(s, out_id).value ^=
						spec.combined_mem(off + TridiagonalSpec::slot_count * j + slot);
				}
			};

			struct TridiagOracle2FwdFunctor
			{
				size_t offset_id;
				size_t row_id;
				size_t k_id;
				size_t sr_id;
				TridiagonalSpec spec;

				__device__ void operator()(System& s) const
				{
					uint64_t j = CuGetAsUint64(s, row_id, 64);
					uint64_t off = CuGetAsUint64(s, offset_id, 64);
					uint64_t k = CuGetAsUint64(s, k_id, 64);
					uint64_t sr = CuGetAsUint64(s, sr_id, 64);
					uint64_t base = spec.window_base(size_t(j));
					uint64_t row_addr = off + TridiagonalSpec::slot_count * j;
					bool hit = k >= base && k < base + TridiagonalSpec::slot_count;
					uint64_t sr1 = sr ^ (hit ? row_addr + (k - base) : 0);
					uint64_t k1 = k ^ spec.combined_mem(sr1);
					uint64_t kmask = (uint64_t(1) << spec.k_width) - 1;
					CuGet(s, k_id).value = (sr1 - row_addr) & kmask;
					CuGet(s, sr_id).value = k1 & kmask;
				}
			};

			struct TridiagOracle2DagFunctor
			{
				size_t offset_id;
				size_t row_id;
				size_t k_id;
				size_t sr_id;
				TridiagonalSpec spec;

				__device__ void operator()(System& s) const
				{
					uint64_t j = CuGetAsUint64(s, row_id, 64);
					uint64_t off = CuGetAsUint64(s, offset_id, 64);
					uint64_t k = CuGetAsUint64(s, k_id, 64);
					uint64_t sr = CuGetAsUint64(s, sr_id, 64);
					uint64_t base = spec.window_base(size_t(j));
					uint64_t row_addr = off + TridiagonalSpec::slot_count * j;
					uint64_t kmask = (uint64_t(1) << spec.k_width) - 1;
					uint64_t k1 = (k + row_addr) & kmask;
					uint64_t k2 = sr ^ spec.combined_mem(k1);
					bool hit = k2 >= base && k2 < base + TridiagonalSpec::slot_count;
					CuGet(s, k_id).value = k2;
					CuGet(s, sr_id).value = k1 ^ (hit ? row_addr + (k2 - base) : 0);
				}
			};

			struct TridiagParityPhaseFunctor
			{
				size_t reg_id;

				__device__ void operator()(System& s) const
				{
					if (CuGetAsUint64(s, reg_id, 64) & 1)
					{
						CuSystemAmplitude(s)[0] = -CuSystemAmplitude(s)[0];
						CuSystemAmplitude(s)[1] = -CuSystemAmplitude(s)[1];
					}
				}
			};
		}

		using namespace tridiag_walk_gpu_detail;

		void TridiagOracle1::operator()(CuSparseState& state) const
		{
			profiler _("TridiagOracle1 cuda");
			state.move_to_gpu();
			thrust::for_each(thrust::device,
				state.sparse_state_gpu.begin(), state.sparse_state_gpu.end(),
				TridiagOracle1Functor{ offset_id, row_id, slot_id, out_id, spec }
			);
		}

		void TridiagOracle2::operator()(CuSparseState& state) const
		{
			profiler _("TridiagOracle2 cuda");
			state.move_to_gpu();
			thrust::for_each(thrust::device,
				state.sparse_state_gpu.begin(), state.sparse_state_gpu.end(),
				TridiagOracle2FwdFunctor{ offset_id, row_id, k_id, sr_id, spec }
			);
		}

		void TridiagOracle2::dag(CuSparseState& state) const
		{
			profiler _("TridiagOracle2.dag cuda");
			state.move_to_gpu();
			thrust::for_each(thrust::device,
				state.sparse_state_gpu.begin(), state.sparse_state_gpu.end(),
				TridiagOracle2DagFunctor{ offset_id, row_id, k_id, sr_id, spec }
			);
		}

		void TridiagParityPhase::operator()(CuSparseState& state) const
		{
			profiler _("TridiagParityPhase cuda");
			state.move_to_gpu();
			thrust::for_each(thrust::device,
				state.sparse_state_gpu.begin(), state.sparse_state_gpu.end(),
				TridiagParityPhaseFunctor{ reg_id }
			);
		}

		CU_IMPL(T_Tridiagonal)

	}

}
