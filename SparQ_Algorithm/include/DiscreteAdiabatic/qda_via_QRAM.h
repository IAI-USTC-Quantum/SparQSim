/**
 * @file qda_via_QRAM.h
 * @brief 基于 QRAM 的通用矩阵 QDA 线性系统求解器
 * @details 将 qda_fundamental.h 的通用单步离散绝热游走 Walk_s 实例化为
 *          QRAM 场景：矩阵 A 的块编码用 Block_Encoding_via_QRAM（数据来自
 *          qram_A 层级树），右端项 b 的编码用 State_Prep_via_QRAM（数据来自
 *          qram_b 层级树）。含可定制 b 编码的模板版（Walk_s_via_QRAM_A）、
 *          标准版（Walk_s_via_QRAM）、调试版（Walk_s_via_QRAM_Debug）与
 *          多步完整求解序列（WalkSequence_via_QRAM_Debug，含逐步保真度统计）。
 *          对应的 Python 实现见 pysparq.algorithms.qda_solver，
 *          C++ 实验入口见 Experiments/QDA
 */

#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include "matrix.h"
#include "BlockEncoding/block_encoding_via_QRAM.h"
#include "state_preparation.h"
#include "qda_fundamental.h"
#include <Eigen/Eigen>

namespace qram_simulator {
	namespace QDA {
		/**
		 * @namespace qram_simulator::QDA::QDA_via_QRAM
		 * @brief 基于 QRAM 的 QDA 求解器
		 */
		namespace QDA_via_QRAM {
			using namespace block_encoding::block_encoding_via_QRAM;
			using namespace state_prep;

			/**
			 * @brief QRAM 场景的单步游走（b 编码可定制的模板版）
			 * @details 矩阵块编码固定为 Block_Encoding_via_QRAM（数据来自 qram_A），
			 *          右端项编码类型由模板参数 Encb_type 指定
			 * @tparam Encb_type 右端项 b 的编码算子类型
			 */
			template<typename Encb_type>
			struct Walk_s_via_QRAM_A : Walk_s<Block_Encoding_via_QRAM, Encb_type>
			{
				/** @brief 矩阵 A 的 QRAM 电路指针（层级树数据） */
				qram_qutrit::QRAMCircuit* qram_A;
				/** @brief 数据寄存器位宽 */
				size_t data_size;
				/** @brief 有理数寄存器位宽 */
				size_t rational_size;
				/** @brief 矩阵块编码类型 */
				using EncA = Block_Encoding_via_QRAM;
				/** @brief 右端项编码类型 */
				using Encb = Encb_type;

				/**
				 * @brief 构造函数
				 * @param qram_A_ 矩阵 A 的 QRAM 电路指针
				 * @param encb_ 右端项编码算子实例
				 * @param main_reg_ 主寄存器名称
				 * @param anc_UA_ 块编码辅助寄存器名称
				 * @param anc_1_ 辅助寄存器 1 名称
				 * @param anc_2_ 辅助寄存器 2 名称
				 * @param anc_3_ 辅助寄存器 3 名称
				 * @param anc_4_ 辅助寄存器 4 名称
				 * @param s_ 插值参数 s ∈ [0, 1]
				 * @param kappa_ 条件数 κ
				 * @param p_ 成功概率参数
				 * @param dsz 数据寄存器位宽
				 * @param rsz 有理数寄存器位宽
				 */
				Walk_s_via_QRAM_A(
					qram_qutrit::QRAMCircuit* qram_A_,
					Encb_type encb_,
					std::string_view main_reg_,
					std::string_view anc_UA_,
					std::string_view anc_1_,
					std::string_view anc_2_,
					std::string_view anc_3_,
					std::string_view anc_4_,
					double s_,
					double kappa_,
					double p_,
					size_t dsz,
					size_t rsz
				) :
					qram_A(qram_A_), data_size(dsz), rational_size(rsz),
					Walk_s<Block_Encoding_via_QRAM, Encb_type>(
						Block_Encoding_via_QRAM(qram_A_, main_reg_, anc_UA_, dsz, rsz),
						encb_,
						main_reg_, anc_UA_, anc_1_, anc_2_, anc_3_, anc_4_,
						s_, kappa_, p_)
				{
				}

			};

			/**
			 * @brief QRAM 场景的标准单步游走
			 * @details 矩阵块编码用 Block_Encoding_via_QRAM（qram_A），
			 *          右端项编码用 State_Prep_via_QRAM（qram_b，
			 *          经典分布的 QRAM 态制备）
			 */
			struct Walk_s_via_QRAM : Walk_s<Block_Encoding_via_QRAM, State_Prep_via_QRAM>
			{
				/** @brief 矩阵 A 的 QRAM 电路指针 */
				qram_qutrit::QRAMCircuit* qram_A;
				/** @brief 右端项 b 的 QRAM 电路指针 */
				qram_qutrit::QRAMCircuit* qram_b;
				/** @brief 数据寄存器位宽 */
				size_t data_size;
				/** @brief 有理数寄存器位宽 */
				size_t rational_size;

				/** @brief 矩阵块编码类型 */
				using EncA = Block_Encoding_via_QRAM;
				/** @brief 右端项编码类型 */
				using Encb = State_Prep_via_QRAM;

				/**
				 * @brief 构造函数
				 * @param qram_A_ 矩阵 A 的 QRAM 电路指针
				 * @param qram_b_ 右端项 b 的 QRAM 电路指针
				 * @param main_reg_ 主寄存器名称
				 * @param anc_UA_ 块编码辅助寄存器名称
				 * @param anc_1_ 辅助寄存器 1 名称
				 * @param anc_2_ 辅助寄存器 2 名称
				 * @param anc_3_ 辅助寄存器 3 名称
				 * @param anc_4_ 辅助寄存器 4 名称
				 * @param s_ 插值参数 s ∈ [0, 1]
				 * @param kappa_ 条件数 κ
				 * @param p_ 成功概率参数
				 * @param dsz 数据寄存器位宽
				 * @param rsz 有理数寄存器位宽
				 */
				Walk_s_via_QRAM(
					qram_qutrit::QRAMCircuit* qram_A_,
					qram_qutrit::QRAMCircuit* qram_b_,
					std::string_view main_reg_,
					std::string_view anc_UA_,
					std::string_view anc_1_,
					std::string_view anc_2_,
					std::string_view anc_3_,
					std::string_view anc_4_,
					double s_,
					double kappa_,
					double p_,
					size_t dsz,
					size_t rsz
				) :
					qram_A(qram_A_), qram_b(qram_b_), data_size(dsz), rational_size(rsz),
					Walk_s(
						Block_Encoding_via_QRAM(qram_A_, main_reg_, anc_UA_, dsz, rsz),
						State_Prep_via_QRAM(qram_b_, main_reg_, dsz, rsz),
						main_reg_, anc_UA_, anc_1_, anc_2_, anc_3_, anc_4_,
						s_, kappa_, p_)
				{}
			};


			/**
			 * @brief QRAM 场景 QDA 游走的调试版
			 * @details 在 Walk_s_via_QRAM 基础上附带 QDADebugger：
			 *          持有经典矩阵/向量副本，用于与理想中间本征态做保真度对比
			 */
			struct Walk_s_via_QRAM_Debug : public Walk_s_via_QRAM, QDADebugger
			{
				/**
				 * @brief 构造函数
				 * @param qram_A_ 矩阵 A 的 QRAM 电路指针
				 * @param qram_b_ 右端项 b 的 QRAM 电路指针
				 * @param matrix_A_ 经典矩阵副本（保真度对比用）
				 * @param vector_b_ 经典右端项副本（保真度对比用）
				 * @param main_reg_ 主寄存器名称
				 * @param anc_UA_ 块编码辅助寄存器名称
				 * @param anc_1_ 辅助寄存器 1 名称
				 * @param anc_2_ 辅助寄存器 2 名称
				 * @param anc_3_ 辅助寄存器 3 名称
				 * @param anc_4_ 辅助寄存器 4 名称
				 * @param s_ 插值参数 s ∈ [0, 1]
				 * @param kappa_ 条件数 κ
				 * @param p_ 成功概率参数
				 * @param is_PD 矩阵是否正定（选择 H(s) 构造路径）
				 * @param dsz 数据寄存器位宽
				 * @param rsz 有理数寄存器位宽
				 */
				Walk_s_via_QRAM_Debug(qram_qutrit::QRAMCircuit* qram_A_,
					qram_qutrit::QRAMCircuit* qram_b_,
					const DenseMatrix<double>& matrix_A_,
					const DenseVector<double>& vector_b_,
					std::string main_reg_,
					std::string anc_UA_,
					std::string anc_1_,
					std::string anc_2_,
					std::string anc_3_,
					std::string anc_4_,
					double s_,
					double kappa_,
					double p_,
					bool is_PD,
					size_t dsz,
					size_t rsz) :
					Walk_s_via_QRAM(qram_A_, qram_b_, main_reg_, anc_UA_,
						anc_1_, anc_2_, anc_3_, anc_4_,
						s_, kappa_, p_, dsz, rsz),
					QDADebugger(matrix_A_, vector_b_, s_, kappa_, p_)
				{
				};
			};

			/**
			 * @brief QRAM 场景 QDA 完整求解序列（调试驱动）
			 * @details 逐步执行 s = n/steps 的单步游走并清理零振幅分支；
			 *          每隔若干步用 GetOutput 读出中间态、与理想本征态
			 *          （QDADebugger::get_mid_eigenstate）做保真度对比，
			 *          将进度/保真度/最大寄存器规模等统计追加写入
			 *          stdout_filename 与 fidelity_filename（由 stdout 文件名
			 *          替换 "stdout" 为 "fidelity" 生成）两个文件
			 */
			struct WalkSequence_via_QRAM_Debug
			{
				/** @brief 离散绝热总步数 */
				size_t steps;
				/** @brief 条件数 κ */
				double kappa;
				/** @brief 成功概率参数 */
				double p;
				/** @brief 主寄存器名称 */
				std::string main_reg;
				/** @brief 块编码辅助寄存器名称 */
				std::string anc_UA;
				/** @brief 辅助寄存器 1-4 名称 */
				std::string anc_1;
				std::string anc_2;
				std::string anc_3;
				std::string anc_4;
				/** @brief 矩阵 A 的 QRAM 电路指针 */
				qram_qutrit::QRAMCircuit* qram_A;
				/** @brief 右端项 b 的 QRAM 电路指针 */
				qram_qutrit::QRAMCircuit* qram_b;
				/** @brief 经典矩阵副本（保真度对比用） */
				DenseMatrix<double> matrix_A;
				/** @brief 经典右端项副本（保真度对比用） */
				DenseVector<double> vector_b;
				/** @brief 数据寄存器位宽 */
				size_t data_size;
				/** @brief 有理数寄存器位宽 */
				size_t rational_size;
				/** @brief 运行统计输出文件名（保真度文件名由其派生） */
				std::string stdout_filename;

				/**
				 * @brief 构造函数
				 * @param qram_A_ 矩阵 A 的 QRAM 电路指针
				 * @param qram_b_ 右端项 b 的 QRAM 电路指针
				 * @param matrix_A 经典矩阵副本
				 * @param vector_b 经典右端项副本
				 * @param main_reg_ 主寄存器名称
				 * @param anc_UA_ 块编码辅助寄存器名称
				 * @param anc_1_ 辅助寄存器 1 名称
				 * @param anc_2_ 辅助寄存器 2 名称
				 * @param anc_3_ 辅助寄存器 3 名称
				 * @param anc_4_ 辅助寄存器 4 名称
				 * @param steps_ 离散绝热总步数
				 * @param kappa_ 条件数 κ
				 * @param p_ 成功概率参数
				 * @param dsz 数据寄存器位宽
				 * @param rsz 有理数寄存器位宽
				 * @param stdout_filename_ 运行统计输出文件名
				 */
				WalkSequence_via_QRAM_Debug(qram_qutrit::QRAMCircuit* qram_A_,
					qram_qutrit::QRAMCircuit* qram_b_,
					const DenseMatrix<double>& matrix_A,
					const DenseVector<double>& vector_b,
					std::string main_reg_,
					std::string anc_UA_,
					std::string anc_1_,
					std::string anc_2_,
					std::string anc_3_,
					std::string anc_4_,
					size_t steps_,
					double kappa_,
					double p_,
					size_t dsz,
					size_t rsz,
					std::string stdout_filename_) : qram_A(qram_A_), qram_b(qram_b_), matrix_A(matrix_A), vector_b(vector_b),
					main_reg(main_reg_), anc_UA(anc_UA_), anc_1(anc_1_), anc_2(anc_2_), anc_3(anc_3_), anc_4(anc_4_),
					steps(steps_), kappa(kappa_), p(p_), data_size(dsz), rational_size(rsz), stdout_filename(stdout_filename_)
				{
				};

				/**
				 * @brief 执行完整离散绝热序列（正向）
				 * @tparam Ty 状态类型
				 * @param state 系统状态
				 */
				template<typename Ty>
				void operator()(Ty& state)
				{
					std::regex _pattern("stdout");
					std::string fidelity_filename = std::regex_replace(stdout_filename, _pattern, "fidelity");
					if (stdout_filename == fidelity_filename)
					{
						fmt::print("\nstdout filename `{}` is invalid!\n", stdout_filename);
						std::exit(5);
					}

					fmt::print("\nfilename for fidelity saving: {}\n", fidelity_filename);
					fmt::print("\nfilename for std output: {}\n", stdout_filename);

					{
						std::ofstream f_fidelity(fidelity_filename);
						if (!f_fidelity.is_open()) {
							throw std::runtime_error("Failed to open file.");
						}
					}

					for (size_t n = 0; n < steps; n++)
					{
						double s = double(n) / steps;
						auto walk = Walk_s_via_QRAM_Debug(qram_A, qram_b, matrix_A, vector_b,
							main_reg, anc_UA, anc_1, anc_2, anc_3, anc_4,
							s, kappa, p, false, data_size, rational_size);
						walk(state);
						ClearZero()(state);

						if ((n + 1) % 2 == 0)
						{
							auto mid_state = GetOutput(main_reg, anc_UA, anc_4, anc_3, anc_2, anc_1)(state);
							std::vector<double> ideal_state = walk.get_mid_eigenstate();

							double fidelity = get_fidelity(ideal_state, mid_state.first);


							{// stdout writing
								std::ofstream f_stdout(stdout_filename, std::ios::app);
								if (!f_stdout.is_open()) {
									throw std::runtime_error("Failed to open file.");
								}
								f_stdout << fmt::format("step: {} / {}, fidelity: {}, p_success: {}\n", n, steps, fidelity, mid_state.second);
								f_stdout << fmt::format("Maximum Qubit Count = {}\n", System::max_qubit_count);
								f_stdout << fmt::format("Maximum Register Count = {}\n", System::max_register_count);
								f_stdout << fmt::format("Maximum System Size = {}\n\n", System::max_system_size);
								f_stdout.close();
							}

							{// fidelity writing
								std::ofstream f_fidelity(fidelity_filename, std::ios::app);
								if (!f_fidelity.is_open()) {
									throw std::runtime_error("Failed to open file.");
								}
								f_fidelity << fmt::format("step: {} / {}, fidelity: {}, p_success: {}, max_system_size: {}\n",
									n, steps, fidelity, mid_state.second, System::max_system_size);
								f_fidelity.close();
							}
						}
						auto now = std::chrono::system_clock::now();
						fmt::print("{:%Y-%m-%d %H:%M:%S} | step: {:>7} / {:>7}, max_system_size: {}\n", now, n, steps, System::max_system_size);
					}
				}

				/**
				 * @brief 执行离散绝热序列的逆（dagger）
				 * @details 逆序施加各步游走的 dagger（s 从 1 回到 0）
				 * @tparam Ty 状态类型
				 * @param state 系统状态
				 */
				template<typename Ty>
				void dag(Ty& state)
				{
					for (size_t n = 0; n < steps; n++) {
						if ((n + 1) % 10 == 0) fmt::print("n: {:>5}\n", n);
						double s = double(steps - n - 1) / steps;
						auto walk = Walk_s_via_QRAM_Debug(qram_A, qram_b, matrix_A, vector_b,
							main_reg, anc_UA, anc_1, anc_2, anc_3, anc_4,
							s, kappa, p, data_size, rational_size);
						walk.dag(state);

						ClearZero()(state);
					}
				}
			};

		} // namespace QDA_via_QRAM
	} // namespace QDA
}
