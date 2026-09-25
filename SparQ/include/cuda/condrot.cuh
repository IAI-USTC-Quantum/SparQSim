/**
 * @file condrot.cuh
 * @brief CUDA implementation of the controlled rotation operator
 * @details Implements the GPU path of CondRot_General_Bool_Fast<Callable>::operator()(CuSparseState&):
 *          first sort by key and collect unique-key groups; groups with a "lone branch" (only one
 *          of out=0 or out=1 exists under the same key) are split to create a new branch, while
 *          "paired" groups (both out=0/1 branches coexist) directly undergo a 2x2 unitary mixing,
 *          all in parallel on thrust device vectors.
 *          Semantics are kept consistent with the CPU-side SparQ/include/condrot.h
 */

#pragma once

#include "basic_components.cuh"
#include "quantum_interfere_basic.cuh"
#include "condrot.h"

namespace qram_simulator {

	/**
	 * @namespace qram_simulator::condrot_general_gpu_detail
	 * @brief Internal details of the GPU implementation of controlled rotation (kernels and helper functors)
	 */
	namespace condrot_general_gpu_detail {
		/**
		 * @brief Device-side angle-function wrapper
		 * @details Wraps a CPU callable returning u22_t (2x2 complex matrix) into a functor
		 *          that, on the __device__ side, writes the matrix into a raw double array
		 *          (8 elements, interleaved as [real,imag]), for use by CUDA kernels
		 * @tparam Callable Type of the angle function returning u22_t
		 */
		template<typename Callable>
		struct CuAngleFunction
		{
			/** @brief The wrapped angle function */
			Callable func;

			/**
			 * @brief Constructor
			 * @param f Angle function
			 */
			CuAngleFunction(Callable f) : func(f) {}

			/**
			 * @brief Compute the 2x2 unitary matrix for input value v and expand it into the raw array
			 * @param v Input register value
			 * @param mat Output array (8 elements: real/imaginary parts of the 4 matrix entries, interleaved)
			 */
			__device__ void operator()(size_t v, double* mat) const {
				u22_t mat_u22 = func(v);
				mat[0] = mat_u22[0].real();
				mat[1] = mat_u22[0].imag();
				mat[2] = mat_u22[1].real();
				mat[3] = mat_u22[1].imag();
				mat[4] = mat_u22[2].real();
				mat[5] = mat_u22[2].imag();
				mat[6] = mat_u22[3].real();
				mat[7] = mat_u22[3].imag();
			}
		};

		// CUDA kernel: Hadamard_Bool::operate_alone_zero + Hadamard_Bool::operate_alone_one
		/**
		 * @brief Controlled-rotation kernel for single-branch groups
		 * @details Handles "lone" groups where only one out value exists under the same input key:
		 *          splits a new basis state with the out bit flipped at the tail of the state array,
		 *          then mixes the original/new basis-state amplitudes by the 2x2 unitary matrix
		 *          (computed by the angle function from the input value)
		 * @tparam CuAngleFunction Device-side angle-function type
		 * @param state Basis-state array (device pointer)
		 * @param nsize Number of single-branch groups
		 * @param unq_s Unique-key group table (device pointer, the first nsize entries are single-branch groups)
		 * @param old_size Number of basis states before expansion (new basis states are appended from this index)
		 * @param in_id Input register ID
		 * @param out_id Output (boolean) register ID
		 * @param in_size Input register bit width
		 * @param func Device-side angle function
		 */
		template<typename CuAngleFunction>
		__global__ void CondRot_General_Bool_operate_alone(System* state, size_t nsize, unq_ele* unq_s, size_t old_size,
			int in_id, int out_id, size_t in_size, CuAngleFunction func)
		{
			size_t tid = blockIdx.x * blockDim.x + threadIdx.x;
			size_t my_loc;

			if (tid < nsize) {
				my_loc = unq_s[tid].sptr;
				state[old_size + tid] = state[my_loc];

				size_t v = CuGetAsUint64(state[my_loc], in_id, in_size);

				double mat[8];
				func(v, mat);
				double u00_real = mat[0];
				double u00_imag = mat[1];
				double u01_real = mat[2];
				double u01_imag = mat[3];
				double u10_real = mat[4];
				double u10_imag = mat[5];
				double u11_real = mat[6];
				double u11_imag = mat[7];
				//printf("CondRot mat: %lf %lf %lf %lf %lf %lf %lf %lf\n", mat[0], mat[1], mat[2], mat[3], mat[4], mat[5], mat[6], mat[7]);

				if (CuGetAsBool(state[my_loc], out_id, 1) == 0) {
					CuGet(state[old_size + tid], out_id).value = 1;

					double a_real = CuSystemAmplitude(state[my_loc])[0];
					double a_imag = CuSystemAmplitude(state[my_loc])[1];
					double b_real = 0;
					double b_imag = 0;

					CuSystemAmplitude(state[my_loc])[0] = (u00_real * a_real - u00_imag * a_imag) + (u01_real * b_real - u01_imag * b_imag);
					CuSystemAmplitude(state[my_loc])[1] = (u00_real * a_imag + u00_imag * a_real) + (u01_real * b_imag + u01_imag * b_real);
					CuSystemAmplitude(state[old_size + tid])[0] = (u10_real * a_real - u10_imag * a_imag) + (u11_real * b_real - u11_imag * b_imag);
					CuSystemAmplitude(state[old_size + tid])[1] = (u10_real * a_imag + u10_imag * a_real) + (u11_real * b_imag + u11_imag * b_real);
				}
				else {
					CuGet(state[old_size + tid], out_id).value = 0;
					double a_real = 0;
					double a_imag = 0;
					double b_real = CuSystemAmplitude(state[my_loc])[0];
					double b_imag = CuSystemAmplitude(state[my_loc])[1];

					CuSystemAmplitude(state[old_size + tid])[0] = (u00_real * a_real - u00_imag * a_imag) + (u01_real * b_real - u01_imag * b_imag);
					CuSystemAmplitude(state[old_size + tid])[1] = (u00_real * a_imag + u00_imag * a_real) + (u01_real * b_imag + u01_imag * b_real);
					CuSystemAmplitude(state[my_loc])[0] = (u10_real * a_real - u10_imag * a_imag) + (u11_real * b_real - u11_imag * b_imag);
					CuSystemAmplitude(state[my_loc])[1] = (u10_real * a_imag + u10_imag * a_real) + (u11_real * b_imag + u11_imag * b_real);
				}
			}
		}

		/**
		 * @brief Controlled-rotation kernel for paired-branch groups
		 * @details Handles groups where both out=0/1 branches coexist under the same input key:
		 *          directly applies a 2x2 unitary mixing to the two adjacent basis states
		 *          (my_loc, my_loc+1), with no need to split a new basis state
		 * @tparam CuAngleFunction Device-side angle-function type
		 * @param state Basis-state array (device pointer)
		 * @param nsize Number of paired groups
		 * @param unq_s Unique-key group table (device pointer, paired groups start at num_one)
		 * @param old_size Number of basis states before expansion (unused by this kernel, kept for interface consistency)
		 * @param in_id Input register ID
		 * @param out_id Output (boolean) register ID
		 * @param in_size Input register bit width
		 * @param func Device-side angle function
		 */
		template<typename CuAngleFunction>
		__global__ void CondRot_General_Bool_operate_pair(System* state, size_t nsize, unq_ele* unq_s, size_t old_size,
			int in_id, int out_id, size_t in_size, CuAngleFunction func)
		{
			size_t tid = blockIdx.x * blockDim.x + threadIdx.x;
			size_t my_loc;

			if (tid < nsize) {
				my_loc = unq_s[tid].sptr;
				size_t v = CuGetAsUint64(state[my_loc], in_id, in_size);

				double mat[8];
				func(v, mat);
				double u00_real = mat[0];
				double u00_imag = mat[1];
				double u01_real = mat[2];
				double u01_imag = mat[3];
				double u10_real = mat[4];
				double u10_imag = mat[5];
				double u11_real = mat[6];
				double u11_imag = mat[7];
				//printf("CondRot mat: %lf %lf %lf %lf %lf %lf %lf %lf\n", mat[0], mat[1], mat[2], mat[3], mat[4], mat[5], mat[6], mat[7]);

				double a_real = CuSystemAmplitude(state[my_loc])[0];
				double a_imag = CuSystemAmplitude(state[my_loc])[1];
				double b_real = CuSystemAmplitude(state[my_loc + 1])[0];
				double b_imag = CuSystemAmplitude(state[my_loc + 1])[1];

				CuSystemAmplitude(state[my_loc])[0] = (u00_real * a_real - u00_imag * a_imag) + (u01_real * b_real - u01_imag * b_imag);
				CuSystemAmplitude(state[my_loc])[1] = (u00_real * a_imag + u00_imag * a_real) + (u01_real * b_imag + u01_imag * b_real);
				CuSystemAmplitude(state[my_loc + 1])[0] = (u10_real * a_real - u10_imag * a_imag) + (u11_real * b_real - u11_imag * b_imag);
				CuSystemAmplitude(state[my_loc + 1])[1] = (u10_real * a_imag + u10_imag * a_real) + (u11_real * b_imag + u11_imag * b_real);
			}
		}
	}

	/**
	 * @brief GPU execution entry point of CondRot_General_Bool_Fast
	 * @details Flow: move the state onto the GPU → sort by key excluding the out bit →
	 *          collect unique-key groups → expand the state array (to hold the new basis
	 *          states split from single-branch groups) → launch the operate_alone /
	 *          operate_pair kernels separately → prune zero-amplitude branches and update
	 *          the size statistics
	 *          Status note: this template currently has no callers in the repository —
	 *          it is kept as the reference implementation for the pending GPU kernel
	 *          of the production CondRot primitives (CondRot_Rational/Fixed_Bool),
	 *          whose absence is the reason the GPU path once stayed disabled.
	 * @tparam Callable Angle-function type (returns u22_t)
	 * @param state GPU sparse state
	 */
	template<typename Callable>
	void CondRot_General_Bool_Fast<Callable>::operator()(CuSparseState& state) const
	{
		profiler _("CondRot_General_Bool cuda");
		using namespace condrot_general_gpu_detail;

		state.move_to_gpu();
		SortExceptKey_devfunc(out_id, state.sparse_state_gpu);

		thrust::device_vector<unq_ele> unique_s;
		Unique_count_elem(state, unique_s, out_id);
		//SortUniqueElements(unique_s);

		size_t num_one = EleNum_MoreThanOne(unique_s);
		size_t unique_state_num = unique_s.size();
		size_t num_not_one = unique_state_num - num_one;
		/* expand state size */
		size_t state_size_0 = state.sparse_state_gpu.size();

		state.sparse_state_gpu.resize(state_size_0 + num_one);
		unq_ele* s_ptr;
		size_t blocksize = CUDA_BLOCK_SIZE;
		size_t nblock = 0;

		size_t in_size = System::size_of(in_id);

		CuAngleFunction<Callable> cu_func(func);

		if (num_one > 0)
		{
			s_ptr = unique_s.data().get();
			nblock = (num_one - 1) / blocksize + 1;
			CondRot_General_Bool_operate_alone << <nblock, blocksize >> > (state.sparse_state_gpu.data().get(), num_one, s_ptr, state_size_0,
				in_id, out_id, in_size, cu_func);
		}
		if (num_not_one > 0)
		{
			s_ptr = thrust::raw_pointer_cast(&(unique_s[num_one]));
			nblock = (num_not_one - 1) / blocksize + 1;
			CondRot_General_Bool_operate_pair << <nblock, blocksize >> > (state.sparse_state_gpu.data().get(), num_not_one, s_ptr, state_size_0,
				in_id, out_id, in_size, cu_func);
		}

		ClearZero()(state);
		System::update_max_size(state.size());
	}

} // namespace qram_simulator
