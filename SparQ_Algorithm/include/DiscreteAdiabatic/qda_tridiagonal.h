/**
 * @file qda_tridiagonal.h
 * @brief 三对角矩阵版 QDA 线性系统求解器
 * @details 将 qda_fundamental.h 的通用单步离散绝热游走 Walk_s 实例化为
 *          三对角场景：矩阵 A = αI + βT 的块编码用 Block_Encoding_Tridiagonal，
 *          右端项 b 的编码用 Hadamard_Int_Full（均匀分布）。
 *          提供 Walk_s_Tridiagonal（标准版）与 Walk_s_Tridiagonal_Debug
 *          （附带保真度对比的调试版）。对应的 Python 实现见 pysparq.algorithms.qda_solver，
 *          C++ 实验入口见 Experiments/QDA
 */

#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include "matrix.h"
#include "BlockEncoding/block_encoding_tridiagonal.h"
#include "qda_fundamental.h"
#include <Eigen/Eigen>

namespace qram_simulator {
	using namespace block_encoding::block_encoding_tridiagonal;

	/**
	 * @namespace qram_simulator::QDA
	 * @brief 离散绝热（QDA）线性系统求解器
	 */
	namespace QDA {
		/**
		 * @namespace qram_simulator::QDA::QDA_tridiagonal
		 * @brief 三对角矩阵版 QDA 求解器
		 */
		namespace QDA_tridiagonal {

			/**
			 * @brief 三对角场景的单步离散绝热游走
			 * @details 组合 Block_Encoding_Tridiagonal（A = αI + βT 的块编码）
			 *          与 Hadamard_Int_Full（b 的均匀叠加编码），
			 *          按 qda_fundamental.h 的 Walk_s 模板实现插值哈密顿量
			 *          H(s) 的单步游走
			 */
			struct Walk_s_Tridiagonal : Walk_s<Block_Encoding_Tridiagonal, Hadamard_Int_Full>
			{
				/** @brief 矩阵块编码类型 */
				using EncA = Block_Encoding_Tridiagonal;
				/** @brief 右端项编码类型 */
				using Encb = Hadamard_Int_Full;

				/**
				 * @brief 构造函数
				 * @param main_reg_ 主寄存器名称
				 * @param anc_UA_ 块编码辅助寄存器名称
				 * @param anc_1_ 辅助寄存器 1 名称
				 * @param anc_2_ 辅助寄存器 2 名称
				 * @param anc_3_ 辅助寄存器 3 名称
				 * @param anc_4_ 辅助寄存器 4 名称
				 * @param s_ 插值参数 s ∈ [0, 1]
				 * @param kappa_ 条件数 κ
				 * @param p_ 成功概率参数
				 * @param alpha_ 三对角对角元系数 α
				 * @param beta_ 三对角次对角元系数 β
				 */
				Walk_s_Tridiagonal(
					std::string_view main_reg_,
					std::string_view anc_UA_,
					std::string_view anc_1_,
					std::string_view anc_2_,
					std::string_view anc_3_,
					std::string_view anc_4_,
					double s_,
					double kappa_,
					double p_,
					double alpha_,
					double beta_
				) :
					Walk_s(
						Block_Encoding_Tridiagonal(main_reg_, anc_UA_, alpha_, beta_),
						Hadamard_Int_Full(main_reg_),
						main_reg_, anc_UA_, anc_1_, anc_2_, anc_3_, anc_4_,
						s_, kappa_, p_)
				{}
			};

			/**
			 * @brief 三对角 QDA 游走的调试版
			 * @details 在 Walk_s_Tridiagonal 基础上附带 QDADebugger：
			 *          持有经典矩阵/向量副本，用于与理想中间本征态做保真度对比
			 */
			struct Walk_s_Tridiagonal_Debug : public Walk_s_Tridiagonal, QDADebugger
			{
				// size_t row_size;

				/**
				 * @brief 构造函数
				 * @param matrix 经典三对角矩阵副本（保真度对比用）
				 * @param vec 经典右端项副本（保真度对比用）
				 * @param main_reg_ 主寄存器名称
				 * @param anc_UA_ 块编码辅助寄存器名称
				 * @param anc_1_ 辅助寄存器 1 名称
				 * @param anc_2_ 辅助寄存器 2 名称
				 * @param anc_3_ 辅助寄存器 3 名称
				 * @param anc_4_ 辅助寄存器 4 名称
				 * @param s_ 插值参数 s ∈ [0, 1]
				 * @param kappa_ 条件数 κ
				 * @param p_ 成功概率参数
				 * @param alpha_ 三对角对角元系数 α
				 * @param beta_ 三对角次对角元系数 β
				 */
				Walk_s_Tridiagonal_Debug(
					const DenseMatrix<double>& matrix,
					const DenseVector<double>& vec,
					std::string_view main_reg_,
					std::string_view anc_UA_,
					std::string_view anc_1_,
					std::string_view anc_2_,
					std::string_view anc_3_,
					std::string_view anc_4_,
					double s_,
					double kappa_,
					double p_,
					double alpha_,
					double beta_) :
					Walk_s_Tridiagonal(main_reg_, anc_UA_,
						anc_1_, anc_2_, anc_3_, anc_4_,
						s_, kappa_, p_, alpha_, beta_),
					QDADebugger(
						matrix, vec,
						s_, kappa_, p_)
				{
				};
			};
		}

	}
}
