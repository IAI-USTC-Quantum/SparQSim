/**
 * @file block_encoding_tridiagonal.h
 * @brief 三对角矩阵的量子块编码
 * @details 实现对称三对角矩阵 A = αI + βT（T 为次对角线全 1 的移位矩阵）的块编码。
 *          基于 LCU（酉算子线性组合）分解 A = αI + βU₊ + βU₋：辅助寄存器制备
 *          LCU 振幅后由条件移位门（PlusOneAndOverflow）执行 +1/-1 移位分支，
 *          最终酉算子 U 满足 (⟨0|_{anc}⊗I) U (|0|_{anc}⊗I) = (αI + βU₊ + βU₋)/‖A‖_F。
 *          该块编码是 QDA 离散绝热求解器三对角版本（qda_tridiagonal.h）的核心子模块
 */

#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include <iostream>
#include <fstream>
#include "matrix.h"
#include <Eigen/Eigen>
#include <filesystem>


namespace qram_simulator {
	namespace block_encoding {
		namespace block_encoding_tridiagonal
		{
			/**
			 * @brief 加一并记录溢出的模移位门
			 * @details 对主寄存器执行 +1 操作：当主寄存器达到最大值 2^n - 1 时回绕到 0，
			 *          并翻转溢出位。该门对应移位矩阵 U₊/U₋ 的作用，是三对角块编码中
			 *          构造条件移位分支的基本部件。支持条件控制（ClassControllable）
			 */
			struct PlusOneAndOverflow : BaseOperator
			{
				using BaseOperator::operator();
				using BaseOperator::dag;

				ClassControllable
				/** @brief 被移位的主寄存器名称 */
				std::string main_reg;
				/** @brief 溢出位寄存器名称（回绕发生时翻转） */
				std::string overflow;
				/**
				 * @brief 构造函数
				 * @param main_reg_ 主寄存器名称
				 * @param overflow_ 溢出位寄存器名称
				 */
				PlusOneAndOverflow(std::string_view main_reg_, std::string_view overflow_) :
					main_reg(main_reg_), overflow(overflow_) {}
				/**
				 * @brief 应用加一移位操作
				 * @param state 系统状态向量
				 */
				void operator()(std::vector<System>& state) const;
				/**
				 * @brief 应用 dagger 操作（减一移位）
				 * @param state 系统状态向量
				 */
				void dag(std::vector<System>& state) const;
#ifdef USE_CUDA
				/**
				 * @brief CUDA 应用加一移位操作
				 * @param s CUDA 稀疏状态
				 */
				void operator()(CuSparseState& s) const;
				/**
				 * @brief CUDA 应用 dagger 操作（减一移位）
				 * @param s CUDA 稀疏状态
				 */
				void dag(CuSparseState& s) const;
#endif
			};

			/**
			 * @brief 三对角矩阵 A = αI + βT 的块编码算子
			 * @details 将 A = αI + βU₊ + βU₋ 分解为 LCU：在 4 位辅助寄存器 anc_UA 上制备
			 *          振幅向量 prep_state = {√|α|/s, √|β|/s, √|β|/s, √(1-(|α|+2|β|)/s)}，
			 *          其中 s = ‖A‖_F = sqrt(N|α|² + 2(N-1)|β|²) 为 Frobenius 范数
			 *          （N = 2^n 为主寄存器维度）。各分支分别执行恒等 / +1 移位 /
			 *          -1 移位 / 湮灭操作，使得酉算子 U 满足块编码定义
			 *          (⟨0|_{anc}⊗I) U (|0|_{anc}⊗I) = (αI + βU₊ + βU₋)/s，
			 *          即编码尺度因子为 s。当 β < 0 时插入额外条件反射修正移位分支符号。
			 *          支持条件控制（ClassControllable）
			 */
			struct Block_Encoding_Tridiagonal : BaseOperator
			{
				/** @brief 对角元系数 α */
				double alpha;
				/** @brief 次对角元系数 β */
				double beta;
				/** @brief 主寄存器名称 */
				std::string main_reg;
				/** @brief 块编码辅助寄存器名称（4 位） */
				std::string anc_UA;
				// std::vector<complex_t> matrix_elements;
				//DenseMatrix<complex_t> mat;
				/** @brief LCU 状态制备振幅向量（各分支系数的平方根） */
				std::vector<complex_t> prep_state;
				ClassControllable

				/**
				 * @brief 构造函数（计算 LCU 状态制备振幅）
				 * @param main_reg_ 主寄存器名称
				 * @param anc_UA_ 块编码辅助寄存器名称（4 位）
				 * @param alpha_ 对角元系数 α
				 * @param beta_ 次对角元系数 β
				 * @note 振幅计算的具体实现在 block_encoding_tridiagonal.cpp
				 */
				Block_Encoding_Tridiagonal(
					std::string_view main_reg_,
					std::string_view anc_UA_,
					double alpha_,
					double beta_);

				/**
				 * @brief 块编码电路实现（正向）
				 * @param state 系统状态向量
				 * @details 流程：拆分辅助寄存器 → LCU 状态制备 → 条件 ±1 移位
				 *          （β < 0 时附加反射修正符号）→ 湮灭分支 → 逆状态制备并合并寄存器
				 */
				template<typename Ty>
				void impl(Ty& state) const
				{
					profiler _("Block_Encoding_Tridiagonal");
					SPLIT_BY_CONDITIONS
					{
						auto overflow = SplitRegister(anc_UA, "overflow", 1)(state);
						auto other = SplitRegister(anc_UA, "other", 1)(state);
						Rot_GeneralStatePrep stateprep(anc_UA, prep_state);
						stateprep(state);

						(PlusOneAndOverflow(main_reg, "overflow")).conditioned_by_value(anc_UA, 1)(state);
						if (beta < 0)
						{
							Reflection_Bool({ main_reg, "overflow" }, false).conditioned_by_value(anc_UA, 1)(state);
							Reflection_Bool({ main_reg, "overflow" }, false).conditioned_by_value(anc_UA, 2)(state);
						}
						(PlusOneAndOverflow(main_reg, "overflow")).conditioned_by_value(anc_UA, 2).dag(state);

						X_Bool("other", 0).conditioned_by_all_ones(anc_UA)(state);

						stateprep.dag(state);
						CombineRegister(anc_UA, "other")(state);
						CombineRegister(anc_UA, "overflow")(state);
					}
					MERGE_BY_CONDITIONS
				}

				/**
				 * @brief 块编码电路实现（dagger，逆向）
				 * @param state 系统状态向量
				 * @details 与 impl 相同的电路组成，但移位分支的执行顺序与反射顺序相反
				 */
				template<typename Ty>
				void impl_dag(Ty& state) const
				{
					profiler _("Block_Encoding_Tridiagonal");

					SPLIT_BY_CONDITIONS
					{
						auto overflow = SplitRegister(anc_UA, "overflow", 1)(state);
						auto other = SplitRegister(anc_UA, "other", 1)(state);
						Rot_GeneralStatePrep stateprep(anc_UA, prep_state);
						stateprep(state);

						(PlusOneAndOverflow(main_reg, "overflow")).conditioned_by_value(anc_UA, 2)(state);
						if (beta < 0)
						{
							Reflection_Bool({ main_reg, "overflow" }, false).conditioned_by_value(anc_UA, 2)(state);
							Reflection_Bool({ main_reg, "overflow" }, false).conditioned_by_value(anc_UA, 1)(state);
						}
						(PlusOneAndOverflow(main_reg, "overflow")).conditioned_by_value(anc_UA, 1).dag(state);

						X_Bool("other", 0).conditioned_by_all_ones(anc_UA)(state);

						stateprep.dag(state);
						CombineRegister(anc_UA, "other")(state);
						CombineRegister(anc_UA, "overflow")(state);
					}
					MERGE_BY_CONDITIONS
				}

				COMPOSITE_OPERATION
			};

			/**
			 * @brief 提取三对角块编码的编码块矩阵（数值验证辅助）
			 * @param qubit_num 主寄存器量子位数 n
			 * @param alpha 对角元系数 α
			 * @param beta 次对角元系数 β
			 * @return 编码块 (⟨0|_{anc}⊗I) U (|0|_{anc}⊗I) 的实数矩阵，
			 *         维度 2^n × 2^n，理论值为 (αI + βT)/‖αI + βT‖_F
			 */
			inline DenseMatrix<double> get_block_encoding_tridiagonal(size_t qubit_num, double alpha, double beta)
			{
				System::add_register("main_reg", UnsignedInteger, qubit_num);
				System::add_register("anc_UA", UnsignedInteger, 4);
				Block_Encoding_Tridiagonal block_enc("main_reg", "anc_UA", alpha, beta);

				DenseMatrix<complex_t> mat = extract_block_encoding(block_enc, "main_reg", "anc_UA", qubit_num);
				DenseMatrix<double> ret(mat.size);
				for (int i = 0; i < pow2(qubit_num); ++i)
				{
					for (int j = 0; j < pow2(qubit_num); ++j)
					{
						ret(i, j) = mat(i, j).real();
					}
				}

				System::clear();
				return ret;
			}

			/**
			 * @brief 构造经典三对角矩阵 αI + βT
			 * @param alpha 对角元系数 α
			 * @param beta 次对角元系数 β
			 * @param dim 矩阵维度
			 * @return dim × dim 三对角矩阵，主对角线为 α、次对角线为 β
			 */
			inline DenseMatrix<double> get_tridiagonal_matrix(double alpha, double beta, size_t dim)
			{
				DenseMatrix<double> mat(dim);
				for (size_t i = 0; i < dim; ++i)
				{
					mat(i, i) = alpha;
					if (i > 0)
						mat(i - 1, i) = beta;
					if (i < (dim - 1))
						mat(i + 1, i) = beta;
				}
				/*fmt::print("{}", mat.to_string());*/
				return mat;
			}

			/**
			 * @brief 构造下移位矩阵 U₊（U₊[i, i-1] = 1，即子对角线为 1）
			 * @tparam Ty 矩阵元素类型
			 * @param size 矩阵维度
			 * @return size × size 下移位矩阵
			 */
			template<typename Ty>
			DenseMatrix<Ty> Get_U_plus(size_t size)
			{
				DenseMatrix<Ty> U_plus(size);
				for (size_t i = 1; i < size; ++i)
				{
					U_plus(i, i - 1) = 1;
				}
				return U_plus;
			}

			/**
			 * @brief 构造上移位矩阵 U₋（U₋[i, i+1] = 1，即超对角线为 1）
			 * @tparam Ty 矩阵元素类型
			 * @param size 矩阵维度
			 * @return size × size 上移位矩阵
			 */
			template<typename Ty>
			DenseMatrix<Ty> Get_U_minus(size_t size)
			{
				DenseMatrix<Ty> U_minus(size);
				for (size_t i = 0; i < size - 1; ++i)
				{
					U_minus(i, i + 1) = 1;
				}
				return U_minus;
			}

		}
	}
}