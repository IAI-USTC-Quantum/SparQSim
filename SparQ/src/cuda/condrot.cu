/**
 * @file condrot.cu
 * @brief CUDA implementation of the rational controlled rotation
 * @details CondRot_Rational_Bool rotates the Boolean output register by the angle
 *          encoded in the Rational input register, pairing basis states that are
 *          equal in every register except the output bit:
 *          (a0, a1) -> (a0*cos - a1*sin, a0*sin + a1*cos).
 *
 *          GPU strategy (2026-10-20): each input system scatters its amplitude into
 *          the two output-bit branches (a*cos on its own branch, +-a*sin on the
 *          flipped branch), then equal keys are merged by summing amplitudes
 *          (CuSortMergeUniqueErase). For a key with both branches present this
 *          reproduces the CPU pairwise arithmetic bit-exactly (IEEE addition is
 *          commutative, each branch receives at most two contributions); the
 *          epsilon-pruning in the merge matches the CPU trailing ClearZero.
 */

#include "condrot.h"
#include "cuda/basic_components.cuh"
#include "cuda/sparse_state_simulator.cuh"

namespace qram_simulator {

	namespace {

		struct CondRotScatterFunctor
		{
			size_t in_id;
			size_t in_sz;
			size_t out_id;
			double sign;

			/**
			 * @param t (input system, own-branch output slot, flipped-branch output slot)
			 * @details keep holds the input configuration with amplitude scaled by cos;
			 *          flip holds the output-bit-flipped configuration with amplitude
			 *          scaled by +-sin (sign = -1 implements the dagger)
			 */
			__device__ void operator()(thrust::tuple<const System&, System&, System&> t) const
			{
				const System& in = thrust::get<0>(t);
				System& keep = thrust::get<1>(t);
				System& flip = thrust::get<2>(t);
				uint64_t rv = CuGet(in, in_id).value & width_mask(in_sz);
				double theta = 1.0 * double(rv) / double(pow2(in_sz)) * (2.0 * pi);
				double c = std::cos(theta);
				double s = std::sin(theta) * sign;
				bool bit = CuGetAsBool(in, out_id, 1);

				keep = in;
				flip = in;
				CuGet(flip, out_id).value ^= 1; // Boolean register: flip bit 0

				double* ak = CuSystemAmplitude(keep);
				double* af = CuSystemAmplitude(flip);
				double sf = bit ? -s : s;
				ak[0] *= c; ak[1] *= c;
				af[0] *= sf; af[1] *= sf;
			}
		};
	}

	static void condrot_impl(CuSparseState& state, size_t in_id, size_t out_id, double sign)
	{
		profiler(sign > 0 ? "CondRot cuda" : "CondRot_dag cuda");
		state.move_to_gpu();
		auto& v = state.sparse_state_gpu;
		size_t n = v.size();
		if (!n) return;

		size_t in_sz = System::size_of(in_id);
		thrust::device_vector<System> out(2 * n);
		thrust::for_each(thrust::device,
			thrust::make_zip_iterator(thrust::make_tuple(
				v.begin(), out.begin(), out.begin() + n)),
			thrust::make_zip_iterator(thrust::make_tuple(
				v.end(), out.begin() + n, out.end())),
			CondRotScatterFunctor{ in_id, in_sz, out_id, sign });

		CuSortMergeUniqueErase(out);
		v.swap(out);
	}

	void CondRot_Rational_Bool::operator()(CuSparseState& state) const
	{
		condrot_impl(state, register_in, register_out, 1.0);
	}

	void CondRot_Rational_Bool::dag(CuSparseState& state) const
	{
		condrot_impl(state, register_in, register_out, -1.0);
	}
}
