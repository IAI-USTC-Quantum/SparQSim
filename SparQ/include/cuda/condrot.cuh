/**
 * @file condrot.cuh
 * @brief 条件旋转算子的 CUDA 实现
 * @details 实现 CondRot_General_Bool_Fast<Callable>::operator()(CuSparseState&)
 *          的 GPU 路径：先按键排序并统计唯一键分组，对"分支孤单"（同一键下
 *          只有 out=0 或 out=1 一个分支）的组分裂出新分支，对"成对"（out=0/1
 *          两分支共存）的组直接做 2x2 酉混合，全程在 thrust 设备向量上并行。
 *          与 CPU 侧 SparQ/include/condrot.h 的语义保持一致
 */

#pragma once

#include "basic_components.cuh"
#include "quantum_interfere_basic.cuh"
#include "condrot.h"

namespace qram_simulator {

	/**
	 * @namespace qram_simulator::condrot_general_gpu_detail
	 * @brief 条件旋转 GPU 实现的内部细节（内核与辅助仿函数）
	 */
	namespace condrot_general_gpu_detail {
		/**
		 * @brief 设备侧角度函数包装器
		 * @details 把返回 u22_t（2x2 复矩阵）的 CPU 可调用对象包装为
		 *          __device__ 侧向裸 double 数组（按 [实,虚] 交错展开 8 元素）
		 *          写矩阵的仿函数，供 CUDA 内核使用
		 * @tparam Callable 返回 u22_t 的角度函数类型
		 */
		template<typename Callable>
		struct CuAngleFunction
		{
			/** @brief 被包装的角度函数 */
			Callable func;

			/**
			 * @brief 构造函数
			 * @param f 角度函数
			 */
			CuAngleFunction(Callable f) : func(f) {}

			/**
			 * @brief 计算输入值 v 对应的 2x2 酉矩阵并展开到裸数组
			 * @param v 输入寄存器值
			 * @param mat 输出数组（8 元素：4 个矩阵元的实部/虚部交错）
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
		 * @brief 单分支组的条件旋转内核
		 * @details 处理同一输入键下只存在一个 out 取值的"孤单"组：
		 *          在状态数组尾部分裂出一个 out 位翻转的新基态，
		 *          按 2x2 酉矩阵（由角度函数按输入值计算）混合原/新基态振幅
		 * @tparam CuAngleFunction 设备侧角度函数类型
		 * @param state 基态数组（设备指针）
		 * @param nsize 单分支组数
		 * @param unq_s 唯一键分组表（设备指针，前 nsize 项为单分支组）
		 * @param old_size 扩容前的基础态数（新基态从该下标起追加）
		 * @param in_id 输入寄存器 ID
		 * @param out_id 输出（布尔）寄存器 ID
		 * @param in_size 输入寄存器位宽
		 * @param func 设备侧角度函数
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
		 * @brief 成对分支组的条件旋转内核
		 * @details 处理同一输入键下 out=0/1 两分支共存的组：
		 *          直接对相邻的 (my_loc, my_loc+1) 两基态做 2x2 酉混合，
		 *          无需分裂新基态
		 * @tparam CuAngleFunction 设备侧角度函数类型
		 * @param state 基态数组（设备指针）
		 * @param nsize 成对组数
		 * @param unq_s 唯一键分组表（设备指针，自 num_one 起为成对组）
		 * @param old_size 扩容前的基础态数（此内核不使用，仅保持接口一致）
		 * @param in_id 输入寄存器 ID
		 * @param out_id 输出（布尔）寄存器 ID
		 * @param in_size 输入寄存器位宽
		 * @param func 设备侧角度函数
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
	 * @brief CondRot_General_Bool_Fast 的 GPU 执行入口
	 * @details 流程：状态迁上 GPU → 按 out 位外键排序 → 统计唯一键分组 →
	 *          扩容状态数组（容纳单分支组分裂出的新基态）→ 分别启动
	 *          operate_alone / operate_pair 内核 → 清除零振幅分支并更新规模统计
	 * @tparam Callable 角度函数类型（返回 u22_t）
	 * @param state GPU 稀疏态
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
