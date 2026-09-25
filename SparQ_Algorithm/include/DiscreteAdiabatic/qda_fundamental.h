/**
 * @file qda_fundamental.h
 * @brief 量子离散绝热（QDA）线性系统求解器基础组件
 * @details 实现基于离散绝热定理的最优规模量子线性系统求解器
 *          （Optimal scaling quantum linear-systems solver via discrete adiabatic theorem,
 *          PRX Quantum, 2022, 3(4): 040303，构造细节见论文 Appendix F）的核心部件：
 *          插值哈密顿量 H(s) = (1-f(s))H₀ + f(s)H₁ 的块编码（Block_Encoding_Hs /
 *          Block_Encoding_Hs_PD）、单步量子游走算子（Walk_s）、游走幂次的 LCU 组合
 *          与 Dolph-Chebyshev 滤波（Filtering），以及保真度调试工具（QDADebugger /
 *          GetOutput）。插值参数 f(s) 取自论文 Eq. (69)，算法复杂度 O(κ log(κ/ε))；
 *          具体求解流程由 qda_tridiagonal.h / qda_via_QRAM.h 组合完成
 */

#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include "matrix.h"
#include "block_encoding.h"
#include "state_preparation.h"
#ifdef USE_CUDA
#include "cuda/cuda_utils.cuh"
#endif

/*****************************************************
				Quantum Walk Process
*****************************************************/
namespace qram_simulator {
	namespace QDA {
		/*
		block-encoding of H(s)
		H(s)=(1-f(s))H_0 + f(s)H_1
		reference: Optimal scaling quantum linear-systems solver via discrete adiabatic theorem[J]. PRX quantum, 2022, 3(4): 040303.
		doi: https://journals.aps.org/prxquantum/abstract/10.1103/PRXQuantum.3.040303
		Construction details: Appendix F
		*/
		/**
		 * @brief 插值哈密顿量 H(s) 的块编码（一般版本）
		 * @details H(s) = (1-f(s))H₀ + f(s)H₁：H₀ 由 |b⟩ 状态制备（enc_b）与主寄存器
		 *          反射构造，H₁ 由 A 的块编码（enc_A）构造（电路细节见上方论文
		 *          Appendix F）。插值通过旋转矩阵
		 *          R_s = [[√N(1-f), √N f], [√N f, √N(f-1)]]（√N = 1/√((1-f)²+f²)）
		 *          作用在 anc_2 上实现，配合受控 enc_A/enc_b 与各级反射，
		 *          整体构成 H(s) 的 (⟨0|⊗I) U (|0|⊗I) 型块编码。
		 *          支持条件控制（ClassControllable）
		 * @tparam Block_Encoding A 的块编码类型
		 * @tparam State_Prep |b⟩ 的状态制备类型
		 */
		template<typename Block_Encoding, typename State_Prep>
		struct Block_Encoding_Hs
		{
			/** @brief 插值参数 f(s) ∈ [0,1] */
			double fs;
			/** @brief 插值旋转矩阵 R_s（作用于 anc_2） */
			u22_t R_s;
			/** @brief 主数据寄存器名称 */
			std::string main_reg;
			/** @brief A 的块编码所用辅助寄存器名称 */
			std::string anc_UA;
			/** @brief 辅助寄存器 anc_1 名称 */
			std::string anc_1;
			/** @brief 辅助寄存器 anc_2 名称（插值旋转作用位） */
			std::string anc_2;
			/** @brief 辅助寄存器 anc_3 名称 */
			std::string anc_3;
			/** @brief 辅助寄存器 anc_4 名称 */
			std::string anc_4;
			// size_t data_size;
			// size_t rational_size;
			/** @brief 矩阵 A 的块编码算子 */
			Block_Encoding enc_A;
			/** @brief 右端项 |b⟩ 的状态制备算子 */
			State_Prep enc_b;
			ClassControllable

			/**
			 * @brief 构造函数（计算插值旋转矩阵 R_s）
			 * @param enc_A_ A 的块编码算子
			 * @param enc_b_ |b⟩ 的状态制备算子
			 * @param main_reg_ 主数据寄存器名称
			 * @param anc_UA_ A 的块编码辅助寄存器名称
			 * @param anc_1_ 辅助寄存器 anc_1 名称
			 * @param anc_2_ 辅助寄存器 anc_2 名称（插值旋转作用位）
			 * @param anc_3_ 辅助寄存器 anc_3 名称
			 * @param anc_4_ 辅助寄存器 anc_4 名称
			 * @param fs_ 插值参数 f(s)
			 */
			Block_Encoding_Hs(
				Block_Encoding enc_A_,
				State_Prep enc_b_,
				std::string_view main_reg_,
				std::string_view anc_UA_,
				std::string_view anc_1_,
				std::string_view anc_2_,
				std::string_view anc_3_,
				std::string_view anc_4_,
				double fs_) : enc_A(enc_A_), enc_b(enc_b_),
			main_reg(main_reg_), anc_UA(anc_UA_),
			anc_1(anc_1_), anc_2(anc_2_), anc_3(anc_3_), anc_4(anc_4_), fs(fs_)
			// data_size(dsz), rational_size(rsz)
			{
				double sqrt_N = 1.0 / sqrt((1 - fs) * (1 - fs) + fs * fs);
				double u00 = sqrt_N * (1 - fs),
					u01 = sqrt_N * fs,
					u10 = sqrt_N * fs,
					u11 = sqrt_N * (fs - 1);
				R_s = u22_t{ u00, u01, u10, u11 };
			};

			/**
			 * @brief 块编码电路实现（正向）
			 * @param state 系统状态向量
			 * @details 电路序列：H(anc_3) → enc_b† → 主寄存器反射（受控）→ enc_b →
			 *          R_s 插值旋转（受控 anc_4）→ H(anc_2) → 受控 enc_A 与反射 →
			 *          逆插值旋转 → 第二轮 enc_b 反射序列 → H(anc_3)
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				profiler _("Block_Encoding_Hs");
				SPLIT_BY_CONDITIONS {
					Block_Encoding enc_A_copy = enc_A;

					(Hadamard_Bool(anc_3))(state);
					enc_b.dag(state);
					X_Bool(anc_1, 0)(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_1, anc_3, anc_4 })(state);
					X_Bool(anc_1, 0)(state);
					enc_b(state);
					X_Bool(anc_4, 0)(state);
					Rot_Bool(anc_2, R_s).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					Hadamard_Bool(anc_2).conditioned_by_all_ones(anc_4)(state);
					enc_A_copy.conditioned_by_all_ones({ anc_1, anc_2 })(state);
					X_Bool(anc_1, 0).conditioned_by_all_ones(anc_2)(state);
					Reflection_Bool(anc_2, true).conditioned_by_all_ones(anc_1)(state);
					enc_A_copy.conditioned_by_all_ones({ anc_1, anc_2 }).dag(state);
					X_Bool(anc_4, 0)(state);
					Hadamard_Bool(anc_2).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					Rot_Bool(anc_2, R_s).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					enc_b.dag(state);
					X_Bool(anc_1, 0)(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_1, anc_3, anc_4 })(state);
					X_Bool(anc_1, 0)(state);
					enc_b(state);
					(Hadamard_Bool(anc_3))(state);
				}
				MERGE_BY_CONDITIONS
			}

			/**
			 * @brief 块编码电路实现（dagger，逆向）
			 * @param state 系统状态向量
			 * @note 当前实现入口处即抛出运行时异常（尚未完成），调用会直接失败
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				throw_general_runtime_error();
				profiler _("Block_Encoding_Hs::dag");
				
				SPLIT_BY_CONDITIONS {
					Block_Encoding enc_A_copy = enc_A;

					(Hadamard_Bool(anc_3))(state);
					enc_b.dag(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_1, anc_3, anc_4 })(state);
					enc_b(state);
					X_Bool(anc_4, 0)(state);
					Rot_Bool(anc_2, R_s).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					Hadamard_Bool(anc_2).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					enc_A_copy.conditioned_by_all_ones({ anc_1, anc_2 })(state);
					Reflection_Bool(anc_2, true).conditioned_by_all_ones(anc_1)(state);
					X_Bool(anc_1, 0).conditioned_by_all_ones(anc_2)(state);
					enc_A_copy.conditioned_by_all_ones({ anc_1, anc_2 }).dag(state);
					Hadamard_Bool(anc_2).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					Rot_Bool(anc_2, R_s).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					enc_b.dag(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_1, anc_3, anc_4 })(state);
					enc_b(state);
					(Hadamard_Bool(anc_3))(state);
				}
				MERGE_BY_CONDITIONS
			}

			COMPOSITE_OPERATION
		};

		/*
		block-encoding of H(s): positive-definite version
		H(s)=(1-f(s))H_0 + f(s)H_1
		reference: Optimal scaling quantum linear-systems solver via discrete adiabatic theorem[J]. PRX quantum, 2022, 3(4): 040303.
		doi: https://journals.aps.org/prxquantum/abstract/10.1103/PRXQuantum.3.040303
		Construction details: Appendix F
		*/
		/**
		 * @brief 插值哈密顿量 H(s) 的块编码（正定版本）
		 * @details 适用于 A 为正定矩阵的情形，电路比一般版本精简（省去部分受控层，
		 *          插值旋转 R_s 改为作用于 anc_1，enc_A 由 {anc_1, anc_3} 控制），
		 *          H(s) = (1-f(s))H₀ + f(s)H₁ 的构造思路与一般版本一致。
		 *          支持条件控制（ClassControllable）
		 * @tparam Block_Encoding A 的块编码类型
		 * @tparam State_Prep |b⟩ 的状态制备类型
		 */
		template<typename Block_Encoding, typename State_Prep>
		struct Block_Encoding_Hs_PD
		{
			/** @brief 插值参数 f(s) ∈ [0,1] */
			double fs;
			/** @brief 插值旋转矩阵 R_s（作用于 anc_1） */
			u22_t R_s;
			/** @brief 主数据寄存器名称 */
			std::string main_reg;
			/** @brief A 的块编码所用辅助寄存器名称 */
			std::string anc_UA;
			/** @brief 辅助寄存器 anc_1 名称（插值旋转作用位） */
			std::string anc_1;
			/** @brief 辅助寄存器 anc_2 名称 */
			std::string anc_2;
			/** @brief 辅助寄存器 anc_3 名称 */
			std::string anc_3;
			/** @brief 辅助寄存器 anc_4 名称 */
			std::string anc_4;
			// size_t data_size;
			// size_t rational_size;
			/** @brief 矩阵 A 的块编码算子 */
			Block_Encoding enc_A;
			/** @brief 右端项 |b⟩ 的状态制备算子 */
			State_Prep enc_b;
			ClassControllable

			/**
			 * @brief 构造函数（计算插值旋转矩阵 R_s）
			 * @param enc_A_ A 的块编码算子
			 * @param enc_b_ |b⟩ 的状态制备算子
			 * @param main_reg_ 主数据寄存器名称
			 * @param anc_UA_ A 的块编码辅助寄存器名称
			 * @param anc_1_ 辅助寄存器 anc_1 名称（插值旋转作用位）
			 * @param anc_2_ 辅助寄存器 anc_2 名称
			 * @param anc_3_ 辅助寄存器 anc_3 名称
			 * @param anc_4_ 辅助寄存器 anc_4 名称（正定版本未使用）
			 * @param fs_ 插值参数 f(s)
			 */
			Block_Encoding_Hs_PD(
				Block_Encoding enc_A_,
				State_Prep enc_b_,
				std::string_view main_reg_,
				std::string_view anc_UA_,
				std::string_view anc_1_,
				std::string_view anc_2_,
				std::string_view anc_3_,//a_h
				std::string_view anc_4_,//none
				double fs_) : enc_A(enc_A_), enc_b(enc_b_),
			main_reg(main_reg_), anc_UA(anc_UA_),
			anc_1(anc_1_), anc_2(anc_2_), anc_3(anc_3_), anc_4(anc_4_), fs(fs_)
				// data_size(dsz), rational_size(rsz)
			{
				double sqrt_N = 1.0 / sqrt((1 - fs) * (1 - fs) + fs * fs);
				double u00 = sqrt_N * (1 - fs),
					u01 = sqrt_N * fs,
					u10 = sqrt_N * fs,
					u11 = sqrt_N * (fs - 1);
				R_s = u22_t{ u00, u01, u10, u11 };
			};

			/**
			 * @brief 正定版块编码电路实现（正向）
			 * @param state 系统状态向量
			 * @details 电路序列：H(anc_2) → enc_b† → 主寄存器反射（受控 {anc_2, anc_3}）→
			 *          enc_b → R_s 插值旋转（受控 anc_3）→ H(anc_1) → 受控 enc_A 与
			 *          enc_A† → 逆插值旋转 → 第二轮 enc_b 反射序列 → H(anc_2)
			 */
			template<typename Ty>
			void operator()(Ty& state) const
			{
				profiler _("Block_Encoding_Hs_PD");
					
				SPLIT_BY_CONDITIONS
				{
					Block_Encoding enc_A_copy = enc_A;
					(Hadamard_Bool(anc_2))(state);
					enc_b.dag(state);					
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_2, anc_3 })(state);
					enc_b(state);
					X_Bool(anc_3, 0)(state);
					Rot_Bool(anc_1, R_s).conditioned_by_all_ones(anc_3)(state);
					X_Bool(anc_3, 0)(state);
					Hadamard_Bool(anc_1).conditioned_by_all_ones(anc_3)(state);
					enc_A_copy.conditioned_by_all_ones(std::vector<std::string>{ anc_1, anc_3 })(state);
					X_Bool(anc_3, 0)(state);
					enc_A_copy.conditioned_by_all_ones(std::vector<std::string>{ anc_1, anc_3 }).dag(state);
					Hadamard_Bool(anc_1).conditioned_by_all_ones(anc_3)(state);
					X_Bool(anc_3, 0)(state);
					Rot_Bool(anc_1, R_s).conditioned_by_all_ones(anc_3)(state);
					X_Bool(anc_3, 0)(state);
					enc_b.dag(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_2, anc_3 })(state);
					enc_b(state);
					(Hadamard_Bool(anc_2))(state);
				}
				MERGE_BY_CONDITIONS
			}

		};
		//bool PD = false;
		// template<typename Block_Encoding, typename State_Prep>
		/**
		 * @brief 参数 s 处的单步量子游走算子 W(s)
		 * @details 离散绝热演化的单步实现：W(s) = i · R · U_H(s)，其中 U_H(s) 为插值
		 *          哈密顿量 H(s) = (1-f(s))H₀ + f(s)H₁ 的块编码，R 为关于块编码辅助
		 *          寄存器的反射（PD = true 时作用在 {anc_UA, anc_1, anc_2}，
		 *          否则作用在 {anc_UA, anc_2, anc_3}），整体再乘全局相位 i。
		 *          插值参数按论文 Eq. (69) 计算：
		 *          fs = κ/(κ-1) · (1 - (1 + s(κ^(p-1) - 1))^(1/(1-p)))。
		 *          支持条件控制（ClassControllable）
		 * @tparam Block_Encoding A 的块编码类型
		 * @tparam State_Prep |b⟩ 的状态制备类型
		 * @tparam PD 是否采用正定（positive-definite）变体的反射寄存器组合
		 * @note PD 仅切换反射所用寄存器；enc_Hs 统一使用一般版块编码 Block_Encoding_Hs
		 *       （正定块编码的切换见被注释的 EncHs 条件类型别名）
		 */
		template<typename Block_Encoding, typename State_Prep, bool PD = false>
		struct Walk_s
		{
			/** @brief 绝热演化离散化参数 s ∈ [0,1] */
			double s;
			/** @brief 线性系统条件数 κ */
			double kappa;
			/** @brief 绝热调度参数 p */
			double p;
			/** @brief 插值参数 f(s)（由 s、κ、p 按论文 Eq. (69) 计算） */
			double fs;
			// double temp;
			/** @brief 全局相位因子（默认 i） */
			complex_t phase = complex_t(0, 1.0);
			/** @brief 主数据寄存器名称 */
			std::string main_reg;
			/** @brief A 的块编码所用辅助寄存器名称 */
			std::string anc_UA;
			/** @brief 辅助寄存器 anc_1 名称 */
			std::string anc_1;
			/** @brief 辅助寄存器 anc_2 名称 */
			std::string anc_2;
			/** @brief 辅助寄存器 anc_3 名称 */
			std::string anc_3;
			/** @brief 辅助寄存器 anc_4 名称 */
			std::string anc_4;
			/** @brief 矩阵 A 的块编码算子 */
			Block_Encoding enc_A;
			/** @brief 右端项 |b⟩ 的状态制备算子 */
			State_Prep enc_b;
			/** @brief 正定变体标志（编译期常量，来自模板参数 PD） */
			constexpr static bool is_positive_definite = PD;
			/** @brief H(s) 块编码类型别名 */
			using EncHs = Block_Encoding_Hs<Block_Encoding, State_Prep>;
			//using EncHs = std::conditional<PD, Block_Encoding_Hs_PD<Block_Encoding, State_Prep>, Block_Encoding_Hs<Block_Encoding, State_Prep>>;
			/** @brief H(s) 的块编码算子实例 */
			EncHs enc_Hs;

			ClassControllable

			/**
			 * @brief 构造函数（内部推导 f(s) 并组装 H(s) 块编码）
			 * @param enc_A_ A 的块编码算子
			 * @param enc_b_ |b⟩ 的状态制备算子
			 * @param main_reg_ 主数据寄存器名称
			 * @param anc_UA_ A 的块编码辅助寄存器名称
			 * @param anc_1_ 辅助寄存器 anc_1 名称
			 * @param anc_2_ 辅助寄存器 anc_2 名称
			 * @param anc_3_ 辅助寄存器 anc_3 名称
			 * @param anc_4_ 辅助寄存器 anc_4 名称
			 * @param s_ 绝热演化离散化参数 s ∈ [0,1]
			 * @param kappa_ 条件数 κ
			 * @param p_ 绝热调度参数 p
			 */
			Walk_s(Block_Encoding enc_A_,
				State_Prep enc_b_,
				std::string_view main_reg_,
				std::string_view anc_UA_,
				std::string_view anc_1_,
				std::string_view anc_2_,
				std::string_view anc_3_,
				std::string_view anc_4_,
				double s_,
				double kappa_,
				double p_) :
			main_reg(main_reg_), anc_UA(anc_UA_),
			anc_1(anc_1_), anc_2(anc_2_), anc_3(anc_3_), anc_4(anc_4_),
			s(s_), kappa(kappa_), p(p_), 
			enc_A(enc_A_), enc_b(enc_b_),
			/* fs (Eq. (69)) page 11 */
			fs(kappa_ / (kappa_ - 1) * (1 - pow(1 + s_ * (pow(kappa_, p_ - 1) - 1), 1 / (1 - p_)))),
			enc_Hs(enc_A_, enc_b_, main_reg, anc_UA, anc_1, anc_2, anc_3, anc_4, fs)
			{
			};

			/**
			 * @brief 单步游走电路实现（正向）：H(s) 块编码 → 反射 → 全局相位
			 * @param state 系统状态向量
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				profiler _("Walk_s");

				SPLIT_BY_CONDITIONS
				{
					(EncHs(enc_Hs))(state);

					if constexpr (!is_positive_definite)
						Reflection_Bool({ anc_UA, anc_2, anc_3 }, false)(state);
					else {
						Reflection_Bool({ anc_UA, anc_1, anc_2 }, false)(state);
					}
				}
				MERGE_BY_CONDITIONS

				(GlobalPhase(phase))(state);
			}

			/**
			 * @brief 单步游走电路实现（dagger，逆向）：全局相位⁻¹ → 反射 → H(s) 块编码†
			 * @param state 系统状态向量
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				profiler _("Walk_s::dag");

				(GlobalPhase(-phase))(state);

				SPLIT_BY_CONDITIONS
				{
					if constexpr (!is_positive_definite) {
						Reflection_Bool({ anc_UA, anc_2, anc_3 }, false)(state);
					}
					else {
						Reflection_Bool({ anc_UA, anc_1, anc_2 }, false)(state);
					}

					(EncHs(enc_Hs)).dag(state);
				}
				MERGE_BY_CONDITIONS
			}

			COMPOSITE_OPERATION
		};


		/**
		 * @brief QDA 经典参考解调试器
		 * @details 保存原始矩阵 A 与右端项 b，在经典侧计算离散绝热演化各阶段的
		 *          理想结果：Hermitian 扩展插值矩阵 A_f、理想初态 |0⟩⊗|b⟩ 与 |1⟩⊗|b⟩
		 *          以及中间时刻的理想本征态（经经典线性求解器求得），
		 *          用于与量子模拟结果做保真度对比
		 */
		struct QDADebugger
		{
			/** @brief 原始矩阵 A */
			DenseMatrix<double> matrix_A;
			/** @brief 原始右端项向量 b */
			DenseVector<double> vector_b;
			/** @brief 插值参数 f(s) */
			double fs;
			/** @brief b 的维数 */
			size_t row_size;

			/**
			 * @brief 构造函数（按 Eq. (69) 由 s、κ、p 计算 f(s)）
			 * @param matrix_A_ 原始矩阵 A
			 * @param vector_b_ 原始右端项向量 b
			 * @param s_ 绝热演化离散化参数 s
			 * @param kappa_ 条件数 κ
			 * @param p_ 绝热调度参数 p
			 */
			QDADebugger(
				const DenseMatrix<double>& matrix_A_,
				const DenseVector<double>& vector_b_,
				double s_,
				double kappa_,
				double p_
			) :
				matrix_A(matrix_A_),
				vector_b(vector_b_),
				fs(kappa_ / (kappa_ - 1) * (1 - pow(1 + s_ * (pow(kappa_, p_ - 1) - 1), 1 / (1 - p_)))),
				row_size(vector_b_.size)
			{}

			//void init_eigenstate(std::vector<System>& state);

			/**
			 * @brief 计算 Hermitian 扩展插值矩阵 A_f
			 * @return 2n×2n 矩阵 [[(1-f)I, fA], [fA†, -(1-f)I]]
			 */
			DenseMatrix<double> get_matrix_Af();
			/**
			 * @brief 理想初态向量 |0⟩⊗|b⟩（扩展空间）
			 * @return 2n 维向量，前 n 个分量为 b，后 n 个为 0
			 */
			DenseVector<double> get_vector_0b();
			/**
			 * @brief 理想向量 |1⟩⊗|b⟩（扩展空间）
			 * @return 2n 维向量，前 n 个分量为 0，后 n 个为 b
			 */
			DenseVector<double> get_vector_1b();
			/**
			 * @brief 计算中间时刻 s 的理想本征态（保真度参考态）
			 * @param is_PD 是否正定情形（当前实现未使用）
			 * @return 长度 4n 的实数向量（按主寄存器 + 辅助位布局补零，
			 *         便于与量子态直接对比）
			 * @details f(s) ≈ 0 时返回初态 |0⟩⊗|b⟩；f(s) ≈ 1 时返回 A x = b 的
			 *          归一化解（置于 |1⟩ 分支）；否则求解 A_f y = (|0⟩⊗|b⟩) 的
			 *          归一化解作为中间本征态
			 */
			std::vector<double> get_mid_eigenstate(bool is_PD=false);
			//std::vector<double> get_matrix_dag();
		};

		/**
		 * @brief 后选择（post-selection）读出算子
		 * @details 从演化末态中筛选所有指定辅助寄存器（anc_registers）取值均为 0 的
		 *          分支，返回归一化后的振幅向量（主寄存器 + anc_1 + anc_4 布局）与
		 *          成功概率（命中分支的权重和），用于读取离散绝热演化的解并做保真度验证
		 */
		struct GetOutput {
			/** @brief 主寄存器 ID */
			size_t main_reg;
			/** @brief A 块编码辅助寄存器 ID */
			size_t anc_UA;
			/** @brief 辅助寄存器 anc_4 ID */
			size_t anc_4;
			/** @brief 辅助寄存器 anc_3 ID */
			size_t anc_3;
			/** @brief 辅助寄存器 anc_2 ID */
			size_t anc_2;
			/** @brief 辅助寄存器 anc_1 ID */
			size_t anc_1;
			/** @brief LCU 索引寄存器 ID（滤波流程用） */
			size_t index;
			/** @brief 滤波辅助寄存器 anc_h ID */
			size_t anc_h;
			/** @brief 参与后选择的辅助寄存器 ID 列表 */
			std::vector<size_t> anc_registers;
			//std::vector<double> weights;
			/**
			 * @brief 构造函数（基本版本，后选择 {anc_UA, anc_3, anc_2}）
			 * @param main_reg 主寄存器名称
			 * @param anc_UA A 块编码辅助寄存器名称
			 * @param anc_4 辅助寄存器 anc_4 名称
			 * @param anc_3 辅助寄存器 anc_3 名称
			 * @param anc_2 辅助寄存器 anc_2 名称
			 * @param anc_1 辅助寄存器 anc_1 名称
			 * @param is_PD 正定模式标志（当前实现未使用，两种模式后选择同一组寄存器）
			 */
			GetOutput(
				std::string main_reg,
				std::string anc_UA,
				std::string anc_4,
				std::string anc_3,
				std::string anc_2,
				std::string anc_1,
				bool is_PD=false) :
				main_reg(System::get(main_reg)), anc_UA(System::get(anc_UA)),
				anc_4(System::get(anc_4)), anc_3(System::get(anc_3)), anc_2(System::get(anc_2)), anc_1(System::get(anc_1))
				//,index(index), anc_h(anc_h)
			{
				/* Mode 1: 5 ancillas are included to post-select */
				//if (!is_PD)
				//{
				//	anc_registers = { anc_UA, anc_3, anc_2, index, anc_h };
				//}
				//else {
				//	anc_registers = { anc_UA, anc_3, anc_2 };
				//}
				anc_registers = { System::get(anc_UA), System::get(anc_3), System::get(anc_2) };
			};
			/**
			 * @brief 构造函数（滤波版本，后选择 {anc_UA, anc_3, anc_2, index, anc_h}）
			 * @param main_reg 主寄存器名称
			 * @param anc_UA A 块编码辅助寄存器名称
			 * @param anc_4 辅助寄存器 anc_4 名称
			 * @param anc_3 辅助寄存器 anc_3 名称
			 * @param anc_2 辅助寄存器 anc_2 名称
			 * @param anc_1 辅助寄存器 anc_1 名称
			 * @param index LCU 索引寄存器名称
			 * @param anc_h 滤波辅助寄存器名称
			 */
			GetOutput(
				std::string main_reg,
				std::string anc_UA,
				std::string anc_4,
				std::string anc_3,
				std::string anc_2,
				std::string anc_1,
				std::string index,
				std::string anc_h) :
				main_reg(System::get(main_reg)), anc_UA(System::get(anc_UA)),
				anc_4(System::get(anc_4)), anc_3(System::get(anc_3)), anc_2(System::get(anc_2)), anc_1(System::get(anc_1)),
				index(System::get(index)), anc_h(System::get(anc_h))
			{
				anc_registers = { System::get(anc_UA), System::get(anc_3), System::get(anc_2), 
					System::get(index), System::get(anc_h) };
			}

			/**
			 * @brief 从系统状态向量提取后选择子空间（具体实现在 qda_fundamental.cpp）
			 * @param state 系统状态向量
			 * @return {归一化振幅向量（索引 = main_reg 值 + anc_1·2^n + anc_4·2^(n+1)）,
			 *          成功概率}
			 */
			std::pair<std::vector<complex_t>, double> operator()(const std::vector<System>& state) const;
			
			/**
			 * @brief 从稀疏态提取后选择子空间（委托给基矢列表版本）
			 * @param state 稀疏态
			 * @return {归一化振幅向量, 成功概率}
			 */
			std::pair<std::vector<complex_t>, double> operator()(const SparseState& state) const
			{
				return (*this)(state.basis_states);
			}
#ifdef USE_CUDA
			/**
			 * @brief 从 CUDA 稀疏态提取后选择子空间
			 * @param state CUDA 稀疏态
			 * @return {归一化振幅向量, 成功概率}
			 */
			std::pair<std::vector<complex_t>, double> operator()(const CuSparseState& state) const;
#endif

			/**
			 * @brief 检查指定辅助寄存器在所有分支中是否均已归零
			 * @param state 系统状态向量
			 * @return {anc_UA, anc_3, anc_2, index, anc_h} 全为 0 时返回 true
			 * @note 使用 index/anc_h 成员，需以包含它们的构造函数构造才有意义
			 */
			template<typename Ty>
			bool check_removable(Ty& state)
			{
				std::vector<size_t> anc_registers = { anc_UA, anc_3, anc_2, index, anc_h };
				for (size_t i = 0; i < state.size(); ++i)
				{
					System& s = state[i];
					for (auto &reg_id : anc_registers)
					{
						if (s.GetAs(reg_id, size_t) != 0)
							return false;
					}
				}
				return true;
			}

			/**
			 * @brief 提取辅助寄存器（anc_registers）全零的子空间并归一化
			 * @param state 系统状态向量
			 * @return {子空间状态列表（权重和不为 0 时已归一化）, 子空间概率和}
			 */
			template<typename Ty>
			std::pair<Ty, double> get_subspace(Ty& state)
			{
				size_t size_mreg = System::size_of(main_reg);
				// The size of anc_4/anc_1 is 1. The length of state_ps is pow2(size_mreg + 1 + 1).
				std::vector<System> state_ps;
				double sum = 0.0;
				for (int i = 0; i < state.size(); ++i)
				{
					System& s = state[i];

					bool is_zero = std::all_of(anc_registers.begin(), anc_registers.end(),
						[&](const size_t& reg)
						{
							size_t v = s.GetAs(reg, size_t);
							return v == 0;
						});
					if (!is_zero)
						continue;
					else {
						state_ps.push_back(s);
						sum += abs_sqr(s.amplitude);
					}
				}
				if (std::abs(sum - 0.0) < epsilon) {
					return { state_ps, sum };
				}
				else {
					double _sqr = sum != 0 ? 1.0 / std::sqrt(sum) : 1.0;
					for (int i = 0; i < state_ps.size(); i++) {
						state_ps[i].amplitude *= _sqr;
					}
					return { state_ps, sum };
				}
			}
		};

		/**
		 * @brief 构造投影补算符矩阵 Q_b = I - (|0⟩⟨0|)⊗(|b⟩⟨b|)
		 * @param b 归一化右端项向量
		 * @return Q_b 对应的复数矩阵（扩展空间）
		 * @tparam Ty Eigen 向量表达式类型
		 */
		template<typename Ty>
		auto GetQb(const Eigen::MatrixBase<Ty>& b) -> EigenMat<complex_t>
		{
			//auto Vec0 = GetVec0();
			auto Vec = GetVec0<complex_t>();
			//fmt::print("Vec0={}\n", eigenmat2str(Vec0));
			auto Vec0b = kroneckerProduct(Vec, b);
			//fmt::print("Vec0b={}\n", eigenmat2str(Vec0b));
			auto Mat0b = Vec0b * Vec0b.adjoint();
			//fmt::print("Mat0b={}\n", eigenmat2str(Mat0b));
			auto MatI = eyes_like(Mat0b);
			//fmt::print("MatI={}\n", eigenmat2str(MatI));
			auto MatQb = MatI - Mat0b;
			//fmt::print("MatQb={}\n", eigenmat2str(MatQb));
			return MatQb;
		}

		/**
		 * @brief 构造 Hermitian 扩展插值算符 A_f
		 * @param A 原始矩阵
		 * @param fs 插值参数 f(s)
		 * @return A_f = (1-f)·σz⊗I + f·[[0, A], [A†, 0]] 对应的复数矩阵
		 * @tparam Ty Eigen 矩阵表达式类型
		 */
		template<typename Ty>
		EigenMat<complex_t> GetAf(const Eigen::MatrixBase<Ty>& A, double fs)
		{
			return (1 - fs) * kroneckerProduct(GetSigmaZ(), eyes_like(A))
				+ fs * HermitianA(A);
		}

		/**
		 * @brief 构造插值哈密顿量矩阵 H(s)（非对角块形式）
		 * @param A 原始矩阵
		 * @param fs 插值参数 f(s)
		 * @param b 归一化右端项向量
		 * @return H(s) = c·[[0, A_f·Q_b], [Q_b·A_f, 0]]，其中 c = 1/√(2f² + 2(1-f)²)
		 * @tparam Ty1 Eigen 矩阵表达式类型
		 * @tparam Ty2 Eigen 向量表达式类型
		 */
		template<typename Ty1, typename Ty2>
		EigenMat<complex_t> GetHs(const Eigen::MatrixBase<Ty1>& A, double fs, const Eigen::MatrixBase<Ty2>& b)
		{
			double scalar = 1.0 / std::sqrt(2.0 * fs * fs + 2.0 * (1 - fs) * (1 - fs));
			auto Af = GetAf(A, fs);
			auto Qb = GetQb(b);
			auto AfQb = scalar * Af * Qb;
			auto QbAf = scalar * Qb * Af;
			auto AfQb01 = kroneckerProduct(GetMat01<complex_t>(), AfQb);
			auto QbAf10 = kroneckerProduct(GetMat10<complex_t>(), QbAf);

			return AfQb01 + QbAf10;
		}
	}
}

/*****************************************************
				Filtering Process
*****************************************************/
namespace qram_simulator{
	namespace QDA
	{
		/**
		 * @brief 游走算子幂次的 LCU（酉算子线性组合）容器
		 * @details 对索引寄存器的第 i 位，构造以该位为控制的游走算子并重复执行
		 *          2^(i+1) 次；与索引寄存器上的系数状态制备（State_Prep_via_QRAM）
		 *          配合，实现 Σ_k c_k W^k 型的游走幂次展开（LCU）。每步进度同时
		 *          打印并写入日志文件。支持条件控制（ClassControllable）
		 * @tparam Walk_s 游走算子类型
		 */
		template<typename Walk_s>
		struct LCU
		{
			/** @brief LCU 索引寄存器 ID */
			size_t index;
			/** @brief 游走算子实例 */
			Walk_s Walk;
			/** @brief 索引寄存器位宽 */
			size_t index_size;
			/** @brief 日志文件路径 */
			std::string filename;
			ClassControllable

			/**
			 * @brief 构造函数（索引以寄存器 ID 给出）
			 * @param Walk 游走算子实例
			 * @param index 索引寄存器 ID
			 * @param filename_ 日志文件路径
			 */
			LCU(Walk_s Walk, size_t index, std::string filename_) :
				Walk(Walk), index(index), filename(filename_)
			{
				index_size = System::size_of(index);
			};

			/**
			 * @brief 构造函数（索引以寄存器名称给出）
			 * @param Walk 游走算子实例
			 * @param index 索引寄存器名称
			 * @param filename_ 日志文件路径
			 */
			LCU(Walk_s Walk, std::string index, std::string filename_) :
				Walk(Walk), index(System::get(index)), filename(filename_)
			{
				index_size = System::size_of(index);

			}

			/**
			 * @brief 执行 LCU 组合（正向）
			 * @param state 系统状态向量
			 */
			template<typename Ty>
			void operator()(Ty& state)
			{
				for (size_t i = 0; i < index_size; i++)
				{
					fmt::print("LCU step {} / {}\n", i, index_size);
					{
						std::ofstream f_stdout(filename, std::ios::app);
						if (!f_stdout.is_open()) {
							throw std::runtime_error("Failed to open file.");
						}
						f_stdout << fmt::format("LCU step {} / {}\n", i, index_size);
						f_stdout.close();
					}
					Walk.clear_control_by_bit();
					auto _walk = Walk.conditioned_by_bit(index, i).conditioned_by_all_ones(condition_variable_all_ones);
					for (int j = 0; j < pow2(i + 1); j++)
					{
						_walk(state);
					}
				}
			}

			/**
			 * @brief 执行 LCU 组合（dagger，逆向）
			 * @param state 系统状态向量
			 */
			template<typename Ty>
			void dag(Ty& state)
			{
				for (size_t i = 0; i < index_size; i++)
				{
					fmt::print("LCUdag step {} / {}\n", i, index_size);
					{
						std::ofstream f_stdout(filename, std::ios::app);
						if (!f_stdout.is_open()) {
							throw std::runtime_error("Failed to open file.");
						}
						f_stdout << fmt::format("LCUdag step {} / {}\n", i, index_size);
						f_stdout.close();
					}
					Walk.clear_control_by_bit();
					auto _walk = Walk.conditioned_by_bit(index, i).conditioned_by_all_ones(condition_variable_all_ones);
					for (int j = 0; j < pow2(i + 1); j++)
					{
						_walk.dag(state);
					}
				}
			}
		};


		/**
		 * @brief 由非负系数序列计算顺序状态制备的旋转角序列
		 * @param coeffs 非负系数列表
		 * @return 旋转角列表 θ_i = 2·arccos(√(c_i / Σ_{j≥i} c_j))
		 * @throws 当系数出现负值时抛出异常
		 */
		inline std::vector<double> CalculateAngles(std::vector<double>& coeffs) {
			auto l = coeffs.size();
			std::vector<double> angles(l);
			double sum = std::accumulate(coeffs.begin(), coeffs.end(), 0.0);
			for (size_t i = 0; i < l; ++i)
			{
				double s = coeffs[i];
				if (s < 0) { throw_invalid_input(); }
				else {
					double cos_theta_2 = sqrt(s / std::accumulate(coeffs.begin() + i, coeffs.end(), 0.0));
					angles[i] = 2 * acos(cos_theta_2);
				}
			}
			return angles;
		};

		/**
		 * @brief 计算第一类 Chebyshev 多项式 T_n(x)（迭代实现）
		 * @param n 多项式阶数
		 * @param x 输入值
		 * @return T_n(x)
		 */
		inline double chebyshevT(size_t n, double x) {
			// Base cases
			if (n == 0) return 1;
			if (n == 1) return x;
			double T0 = 1;
			double T1 = x;
			double Tn = 0;
			for (size_t k = 2; k <= n; ++k) {
				Tn = 2 * x * T1 - T0;
				T0 = T1;
				T1 = Tn;
			}
			return Tn;
		}

		/**
		 * @brief 计算 Dolph-Chebyshev 窗函数值
		 * @param epsilon_ 误差容限 ε
		 * @param l_ 窗长度参数 l
		 * @param phi_ 相位角 φ
		 * @return ε·T_l(cosh(acosh(1/ε)/l)·cos φ)
		 */
		inline double DolphChebyshev(double epsilon_, int l_, double phi_) {
			double beta = cosh(acosh(1.0 / epsilon_) / l_);
			double x = epsilon_ * chebyshevT(l_, beta * cos(phi_));
			return x;
		}

		/**
		 * @brief 求值偶函数 Fourier 级数
		 * @param weights 系数列表（w_0 为常数项）
		 * @param x 求值点
		 * @return w_0 + Σ_{i≥1} 2·w_i·cos(i·x)
		 */
		inline double FourierSeries(std::vector<double> weights, double x)
		{
			auto l = weights.size();
			double sum = weights[0];
			for (int i = 1; i < l; i++)
			{
				sum += weights[i] * 2 * cos(i * x);
			}
			return sum;
		}
		// Function to compute the coefficients of the Fourier series
		/**
		 * @brief 数值计算 Dolph-Chebyshev 滤波器的 Fourier 系数
		 * @param epsilon_ 误差容限 ε
		 * @param l_ 滤波器长度参数 l
		 * @return 偶数阶系数列表（对窗函数做数值积分，仅保留 j 为偶数的项）
		 */
		inline std::vector<double> ComputeFourierCoeffs(double epsilon_, int l_) {
			std::vector<double> coeffs; // Initialize coefficients array
			double P_ = 2 * pi;

			// Calculate each coefficient from w_0 to w_l
			for (int j = 0; j <= l_; ++j) {
				double coeff = 0.0;
				double integral = 0.0;
				double delta_phi = P_ / 10000.0; // Set a small delta_phi for numerical integration

				// Perform numerical integration (e.g., using the trapezoidal rule)
				for (double phi = 0; phi <= P_ / 2; phi += delta_phi) {
					double cos_term = cos(2 * pi * j * phi / P_);
					double func_value = DolphChebyshev(epsilon_, l_, phi);
					double term = func_value * cos_term;

					// Use trapezoidal rule for integration
					integral += term;
				}
				coeff = integral * delta_phi / P_; // Finalize the coefficient
				// Since the function is even, double the coefficient
				//std::cout << "Coefficient a" << j << ": " << 2*coeff << std::endl;
				if ((j % 2) == 0)
					coeffs.push_back(2 * coeff);
			}
			return coeffs;
		}

		/**
		 * @brief Dolph-Chebyshev 滤波算子（QDA 精度增强）
		 * @details 对游走序列施加滤波以放大成功分支的振幅：先用 QRAM（qram_w）在
		 *          索引寄存器上制备滤波系数态，对 anc_h 做 Hadamard 后施加受控 LCU
		 *          游走幂次展开，再经 X(anc_h) 与 LCU† 的交替组合实现反射式滤波，
		 *          最后逆制备并读取成功概率。滤波系数由 ComputeFourierCoeffs
		 *          （Dolph-Chebyshev 窗）给出。支持条件控制（ClassControllable）
		 * @tparam Walk_type 游走算子类型
		 */
		template<typename Walk_type>
		struct Filtering
		{
			/** @brief 存储滤波系数的 QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram_w;
			/** @brief 主数据寄存器名称 */
			std::string main_reg;
			/** @brief A 的块编码所用辅助寄存器名称 */
			std::string anc_UA;
			/** @brief 辅助寄存器 anc_4 名称 */
			std::string anc_4;
			/** @brief 辅助寄存器 anc_3 名称 */
			std::string anc_3;
			/** @brief 辅助寄存器 anc_2 名称 */
			std::string anc_2;
			/** @brief 辅助寄存器 anc_1 名称 */
			std::string anc_1;
			/** @brief LCU 索引寄存器名称 */
			std::string index;
			/** @brief 滤波辅助寄存器名称 */
			std::string anc_h;
			/** @brief 数据寄存器位宽（定点量化位数） */
			size_t data_size;
			/** @brief 有理数（旋转角）寄存器位宽 */
			size_t rational_size;
			/** @brief 游走算子实例 */
			Walk_type Walk;
			/** @brief 索引寄存器位宽 */
			int index_size;
			/** @brief 运行日志文件路径 */
			std::string stdout_filename;
			/**
			 * @brief 构造函数
			 * @param qram_w 存储滤波系数的 QRAM 电路指针
			 * @param Walk 游走算子实例
			 * @param main_reg 主数据寄存器名称
			 * @param anc_UA A 的块编码辅助寄存器名称
			 * @param anc_4 辅助寄存器 anc_4 名称
			 * @param anc_3 辅助寄存器 anc_3 名称
			 * @param anc_2 辅助寄存器 anc_2 名称
			 * @param anc_1 辅助寄存器 anc_1 名称
			 * @param index LCU 索引寄存器名称
			 * @param anc_h 滤波辅助寄存器名称
			 * @param ds 数据寄存器位宽
			 * @param rs 有理数寄存器位宽
			 * @param stdout_filename_ 运行日志文件路径
			 */
			Filtering(qram_qutrit::QRAMCircuit* qram_w,
				Walk_type Walk,
				std::string main_reg,
				std::string anc_UA,
				std::string anc_4,
				std::string anc_3,
				std::string anc_2,
				std::string anc_1,
				std::string index,
				std::string anc_h,
				size_t ds,
				size_t rs,
				std::string stdout_filename_) :
				qram_w(qram_w), Walk(Walk), main_reg(main_reg), anc_UA(anc_UA),
				anc_4(anc_4), anc_3(anc_3), anc_2(anc_2), anc_1(anc_1),
				index(index), anc_h(anc_h), data_size(ds), rational_size(rs), stdout_filename(stdout_filename_)
			{
				index_size = System::size_of(index);
			};

			/**
			 * @brief 生成随机初态（调试用）
			 * @param state 系统状态向量
			 * @details 对 anc_1 与主寄存器施加 Hadamard，再为每个分支注入随机实振幅
			 *          并归一化
			 */
			template<typename Ty>
			void random_state_generate(Ty& state)
			{
				//Hadamard_Int(anc_4, 1)(state);
				Hadamard_Int(anc_1, 1)(state);
				Hadamard_Int(main_reg, System::size_of(main_reg))(state);

				for (auto& s : state)
				{
					s.amplitude = random_engine::rng() * 2 - 1;
				}
				Normalize()(state);
			}
			//std::string dump_format() const
			//{
			//	return fmt::format("Filtering");
			//}

			/**
			 * @brief 执行滤波流程并返回成功概率
			 * @param state 系统状态向量
			 * @return 后选择概率（anc_h 与 index 均为 0 的分支），由偏迹振幅取平方得到
			 * @details 流程：系数态制备 → H(anc_h) → LCU（受控）→ X(anc_h) →
			 *          LCU†（受控）→ X(anc_h) → H(anc_h) → 逆制备；运行后将
			 *          峰值资源统计写入日志文件
			 */
			template<typename Ty>
			double operator()(Ty& state)
			{
				using namespace state_prep;
				State_Prep_via_QRAM(qram_w, index, data_size, rational_size)(state);
				// StatePrint(0, 16)(state);
				(Hadamard_Int_Full(anc_h))(state);
				LCU<Walk_type>(Walk, index, stdout_filename).conditioned_by_all_ones(anc_h)(state);

				X_Bool(anc_h, 0)(state);
				LCU<Walk_type>(Walk, index, stdout_filename).conditioned_by_all_ones(anc_h).dag(state);
				X_Bool(anc_h, 0)(state);

				(Hadamard_Int_Full(anc_h))(state);
				State_Prep_via_QRAM(qram_w, index, data_size, rational_size).dag(state);

				{// stdout writing
					std::ofstream f_stdout(stdout_filename, std::ios::app);
					if (!f_stdout.is_open()) {
						throw std::runtime_error("Failed to open file.");
					}
					f_stdout << fmt::format("\nAfter filtering: \n");
					f_stdout << fmt::format("Maximum Qubit Count = {}\n", System::max_qubit_count);
					f_stdout << fmt::format("Maximum Register Count = {}\n", System::max_register_count);
					f_stdout << fmt::format("Maximum System Size = {}\n\n", System::max_system_size);
					f_stdout.close();
				}

				double prob_inv = PartialTraceSelect({ System::get(anc_h), System::get(index) }, { 0, 0 })(state);
				double prob = (1.0 / prob_inv) * (1.0 / prob_inv);
				return prob;
			}
		};
	} // namespace QDA

	namespace QDA
	{
		/* Extract full unitary of BlockEncoding_Hs */
		/**
		 * @brief 提取 Block_Encoding_Hs 的完整幺正矩阵（调试用）
		 * @param encHs H(s) 块编码算子
		 * @param main_reg 主数据寄存器名称
		 * @param anc_UA A 的块编码辅助寄存器名称
		 * @param anc_1 辅助寄存器 anc_1 名称
		 * @param anc_2 辅助寄存器 anc_2 名称
		 * @param anc_3 辅助寄存器 anc_3 名称
		 * @param anc_4 辅助寄存器 anc_4 名称
		 * @return 2^(主寄存器位数 + anc_UA 位数 + 4) 维完整幺正矩阵
		 * @tparam Block_Encoding A 的块编码类型
		 * @tparam State_Prep |b⟩ 的状态制备类型
		 * @tparam StateType 状态容器类型（默认 SparseState）
		 */
		template <typename Block_Encoding, typename State_Prep, typename StateType = SparseState>
		DenseMatrix<complex_t> _extract_full_unitary(
			Block_Encoding_Hs<Block_Encoding, State_Prep> encHs,
			std::string main_reg, std::string anc_UA,
			std::string anc_1, std::string anc_2, std::string anc_3, std::string anc_4)
		{
			size_t main_reg_num = System::size_of(main_reg);
			size_t anc_UA_num = System::size_of(anc_UA);
			size_t qubit_num = main_reg_num + anc_UA_num + 4;
			fmt::print("main_reg_num = {}, anc_UA_num = {}", main_reg_num, anc_UA_num);

			DenseMatrix<complex_t> ret(pow2(qubit_num));
			int main_reg_pos = System::get(main_reg);
			int anc_UA_pos = System::get(anc_UA);
			int anc_1_pos = System::get(anc_1);
			int anc_2_pos = System::get(anc_2);
			int anc_3_pos = System::get(anc_3);
			int anc_4_pos = System::get(anc_4);

			auto i_range = range(pow2(main_reg_num));
			auto a_A_range = range(pow2(anc_UA_num));
			auto b1_range = range(2);
			auto b2_range = range(2);
			auto b3_range = range(2);
			auto b4_range = range(2);

			for (auto [i, a_A, b1, b2, b3, b4] : product(i_range, a_A_range, b1_range, b2_range, b3_range, b4_range)) {
				/*std::vector<System> state;
				state.emplace_back();*/
				StateType state(1);
				state.back().get(main_reg_pos).value = i;
				state.back().get(anc_UA_pos).value = a_A;
				state.back().get(anc_1_pos).value = b1;
				state.back().get(anc_2_pos).value = b2;
				state.back().get(anc_3_pos).value = b3;
				state.back().get(anc_4_pos).value = b4;

				encHs(state);

				std::vector<complex_t> vec(pow2(qubit_num), 0);

				for (auto& s : state)
				{
					size_t index = concat_value(
						{
							{s.get(main_reg_pos).value, main_reg_num},
							{s.get(anc_UA_pos).value, anc_UA_num},
							{s.get(anc_1_pos).value, 1},
							{s.get(anc_2_pos).value, 1},
							{s.get(anc_3_pos).value, 1},
							{s.get(anc_4_pos).value, 1}
						}
					);
					vec[index] = s.amplitude;
				}
				for (size_t j = 0; j < pow2(qubit_num); ++j)
				{
					size_t index = concat_value(
						{
							{i, main_reg_num},
							{a_A, anc_UA_num},
							{b1, 1},
							{b2, 1},
							{b3, 1},
							{b4, 1}
						}
					);

					ret(j, index) = vec[j];
				}
			}
			return ret;
		}

		/**
		 * @brief 提取 Block_Encoding_Hs 的完整幺正矩阵（SparseState 版便捷封装）
		 * @param encHs H(s) 块编码算子
		 * @param main_reg 主数据寄存器名称
		 * @param anc_UA A 的块编码辅助寄存器名称
		 * @param anc_1 辅助寄存器 anc_1 名称
		 * @param anc_2 辅助寄存器 anc_2 名称
		 * @param anc_3 辅助寄存器 anc_3 名称
		 * @param anc_4 辅助寄存器 anc_4 名称
		 * @return 完整幺正矩阵
		 * @tparam Block_Encoding A 的块编码类型
		 * @tparam State_Prep |b⟩ 的状态制备类型
		 */
		template <typename Block_Encoding, typename State_Prep>
		DenseMatrix<complex_t> extract_full_unitary(
			Block_Encoding_Hs<Block_Encoding, State_Prep> encHs,
			std::string main_reg, std::string anc_UA,
			std::string anc_1, std::string anc_2, std::string anc_3, std::string anc_4)
		{
			return _extract_full_unitary<Block_Encoding, State_Prep, SparseState>(encHs, main_reg, anc_UA, anc_1, anc_2, anc_3, anc_4);
		}
		

		/* Extract the block encoding part of the Hs */
		/**
		 * @brief 提取 H(s) 块编码的有效编码块（调试用）
		 * @param encHs H(s) 块编码算子
		 * @param main_reg 主数据寄存器名称
		 * @param anc_UA A 的块编码辅助寄存器名称
		 * @param anc_1 辅助寄存器 anc_1 名称
		 * @param anc_2 辅助寄存器 anc_2 名称
		 * @param anc_3 辅助寄存器 anc_3 名称
		 * @param anc_4 辅助寄存器 anc_4 名称
		 * @param qubit_num 主寄存器量子位数 n
		 * @return 后选择（anc_UA / anc_2 / anc_3 全 0）并按成功概率归一化的
		 *         2^(n+2) 维矩阵，行列索引包含 anc_1 与 anc_4 两个外层块指标
		 * @tparam Block_Encoding A 的块编码类型
		 * @tparam State_Prep |b⟩ 的状态制备类型
		 * @tparam StateType 状态容器类型（默认 SparseState）
		 */
		template <typename Block_Encoding, typename State_Prep, typename StateType = SparseState>
		DenseMatrix<complex_t> _extract_block_encoding_Hs(
			Block_Encoding_Hs<Block_Encoding, State_Prep> encHs,
			std::string main_reg, std::string anc_UA,
			std::string anc_1, std::string anc_2, std::string anc_3, std::string anc_4,
			size_t qubit_num)
		{
			DenseMatrix<complex_t> ret(pow2(qubit_num + 2));
			auto main_reg_pos = System::get(main_reg);
			auto anc_UA_pos = System::get(anc_UA);
			auto anc_1_pos = System::get(anc_1);
			auto anc_2_pos = System::get(anc_2);
			auto anc_3_pos = System::get(anc_3);
			auto anc_4_pos = System::get(anc_4);

			auto i_range = range(pow2(qubit_num));
			auto b1_range = range(2);
			auto b4_range = range(2);

			for (auto [i, b1, b4] : product(i_range, b1_range, b4_range))
			{
				//std::vector<System> state;
				//state.emplace_back();
				StateType state(1);
				state.back().get(main_reg_pos).value = i;
				state.back().get(anc_UA_pos).value = 0;
				state.back().get(anc_1_pos).value = b1;
				state.back().get(anc_2_pos).value = 0;
				state.back().get(anc_3_pos).value = 0;
				state.back().get(anc_4_pos).value = b4;

				encHs(state);
				//StatePrint()(state);
				double prob = PartialTraceSelect({ { anc_UA, 0 },{ anc_2, 0 },{ anc_3, 0 } })(state);

				std::vector<complex_t> vec(pow2(qubit_num + 2), 0);
				for (auto& s : state)
				{
					vec[s.get(main_reg_pos).value
						+ (s.get(anc_1_pos).value << qubit_num)
						+ (s.get(anc_4_pos).value << (qubit_num + 1))
					] = s.amplitude;
				}
				for (size_t j = 0; j < pow2(qubit_num + 2); ++j)
				{
					ret(j, (b4 << (qubit_num + 1)) + (b1 << qubit_num) + i) = vec[j] / prob;
				}
			}
			return ret;
		}


		/**
		 * @brief 提取 H(s) 块编码的有效编码块（SparseState 版便捷封装）
		 * @param encHs H(s) 块编码算子
		 * @param main_reg 主数据寄存器名称
		 * @param anc_UA A 的块编码辅助寄存器名称
		 * @param anc_1 辅助寄存器 anc_1 名称
		 * @param anc_2 辅助寄存器 anc_2 名称
		 * @param anc_3 辅助寄存器 anc_3 名称
		 * @param anc_4 辅助寄存器 anc_4 名称
		 * @param qubit_num 主寄存器量子位数
		 * @return 后选择归一化后的编码块矩阵
		 * @tparam Block_Encoding A 的块编码类型
		 * @tparam State_Prep |b⟩ 的状态制备类型
		 */
		template <typename Block_Encoding, typename State_Prep>
		DenseMatrix<complex_t> extract_block_encoding_Hs(
			Block_Encoding_Hs<Block_Encoding, State_Prep> encHs,
			std::string main_reg, std::string anc_UA,
			std::string anc_1, std::string anc_2, std::string anc_3, std::string anc_4,
			size_t qubit_num)
		{
			return _extract_block_encoding_Hs<Block_Encoding, State_Prep, SparseState>(encHs, main_reg, anc_UA, anc_1, anc_2, anc_3, anc_4, qubit_num);
		}
	}
}