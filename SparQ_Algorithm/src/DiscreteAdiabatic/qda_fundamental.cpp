/**
 * @file qda_fundamental.cpp
 * @brief QDA 基础组件的实现
 * @details 实现 QDADebugger 的经典参考解计算（Hermitian 扩展矩阵 A_f、理想
 *          初/终态向量、中间本征态）与 GetOutput 的后选择读出
 */

#include "DiscreteAdiabatic/qda_fundamental.h"

namespace qram_simulator {
	namespace QDA {

		/**
		 * @brief 计算 Hermitian 扩展插值矩阵 A_f
		 * @return 2n×2n 矩阵 [[(1-f)I, fA], [fA†, -(1-f)I]]
		 */
		DenseMatrix<double> QDADebugger::get_matrix_Af()
		{
			size_t newSize = 2 * row_size;

			std::vector<double> Ad(newSize * newSize, 0.0);
			for (size_t i = 0; i < row_size; ++i) {
				for (size_t j = 0; j < row_size; ++j) {
					double A_ij = matrix_A(i, j);
					double A_dag_ji = matrix_A(j, i);

					Ad[i * newSize + j] = (1 - fs) * (i == j); // (1-f)I 
					Ad[i * newSize + (j + row_size)] = fs * A_ij;     // fA 
					Ad[(i + row_size) * newSize + j] = fs * A_dag_ji; // fA^dagger
					Ad[(i + row_size) * newSize + (j + row_size)] = -(1 - fs) * (i == j); // -(1-f)I 
				}
			}
			DenseMatrix<double> Af(newSize, Ad);
			return Af;
		}

		/**
		 * @brief 理想初态向量 |0⟩⊗|b⟩
		 * @return 2n 维向量，前 n 个分量为 b
		 */
		DenseVector<double> QDADebugger::get_vector_0b()
		{
			size_t newSize = 2 * row_size;
			std::vector<double> new_b(newSize, 0.0);
			for (size_t i = 0; i < row_size; ++i) {
				new_b[i] = vector_b[i];
			}
			DenseVector<double> vec(newSize, new_b);
			return vec;
		}

		/**
		 * @brief 理想向量 |1⟩⊗|b⟩
		 * @return 2n 维向量，后 n 个分量为 b
		 */
		DenseVector<double> QDADebugger::get_vector_1b()
		{
			size_t newSize = 2 * row_size;
			std::vector<double> new_b(newSize, 0.0);
			for (size_t i = 0; i < row_size; ++i) {
				new_b[row_size + i] = vector_b[i];
			}
			DenseVector<double> vec(newSize, new_b);
			return vec;
		}

		/**
		 * @brief 计算中间时刻 s 的理想本征态（保真度参考态）
		 * @param is_PD 是否正定情形（当前实现未使用）
		 * @return 长度 4n 的实数向量（按主寄存器 + 辅助位布局补零，便于与量子态对比）
		 * @details f(s) ≈ 0 时返回初态 |0⟩⊗|b⟩；f(s) ≈ 1 时返回 A x = b 的归一化解
		 *          （置于 |1⟩ 分支）；否则求解 A_f y = (|0⟩⊗|b⟩) 的归一化解
		 */
		std::vector<double> QDADebugger::get_mid_eigenstate(bool is_PD)
		{
			if (fs < epsilon) {
				DenseVector<double> sol = get_vector_0b();
				std::vector<double> sol_vec = sol.data;
				sol_vec.resize(row_size * 4, 0.0);
				return sol_vec;
			}
			else if (std::abs(fs - 1.0) < epsilon) {
				DenseMatrix<double> A(matrix_A);
				DenseVector<double> b(vector_b);
				DenseVector<double> sol = my_linear_solver(A, b);
				sol = sol / sol.norm2();
				std::vector<double> _vec = sol.data;
				std::vector<double> sol_vec(row_size * 4, 0.0);
				for (int j = 0; j < row_size; j++) {
					sol_vec[row_size + j] = _vec[j];
				}
				return sol_vec;
			}
			else {
				DenseMatrix<double> Af = get_matrix_Af();
				//fmt::print("{}\n", Af.to_string());
				DenseVector<double> b = get_vector_0b();
				DenseVector<double> sol = my_linear_solver(Af, b);
				sol = sol / sol.norm2();
				std::string sol_str = sol.to_string();

				std::vector<double> sol_vec = sol.data;
				sol_vec.resize(row_size * 4, 0.0);
				return sol_vec;
			}
			
		}

		/**
		 * @brief 从系统状态向量提取后选择子空间
		 * @param state 系统状态向量
		 * @return {归一化振幅向量（索引 = main_reg 值 + anc_1·2^n + anc_4·2^(n+1)）,
		 *          成功概率}
		 * @details 筛选 anc_registers 中所有寄存器取值均为 0 的分支，
		 *          振幅按命中分支权重和归一化
		 */
		std::pair<std::vector<complex_t>, double> GetOutput::operator()(const std::vector<System>& state) const
		{
			profiler _("GetOutput");
			// The size of anc_4/anc_1 is 1. The length of state_ps is pow2(size_mreg + 1 + 1).

			size_t main_reg_num = System::size_of(main_reg);
			size_t anc_1_num = System::size_of(anc_1);
			size_t anc_4_num = System::size_of(anc_4);
			std::vector<complex_t> state_ps(pow2(main_reg_num + 2), complex_t(0, 0));

			double sum = 0.0;
			for (int64_t i = 0; i < state.size(); ++i)
			{
				const System& s = state[i];

				bool is_zero = std::all_of(anc_registers.begin(), anc_registers.end(),
					[&](size_t reg)
					{
						uint64_t v = s.GetAs(reg, uint64_t);
						return v == 0;
					});
				if (!is_zero)
					continue;
				else {
					uint64_t _id = concat_value(
						{
							{ s.GetAs(main_reg, uint64_t), main_reg_num},
							{ s.GetAs(anc_1, uint64_t), anc_1_num},
							{ s.GetAs(anc_4, uint64_t), anc_4_num},
						}
						);

					state_ps[_id] = s.amplitude;
					sum += abs_sqr(s.amplitude);
				}
			}
			if (std::abs(sum - 1.0) < epsilon) {
				return { state_ps, sum };
			}
			else {
				double _sqr = sum != 0 ? 1.0 / std::sqrt(sum) : 1.0;
				for (int i = 0; i < state_ps.size(); i++) {
					state_ps[i] *= _sqr;
				}
				return { state_ps, sum };
			}
		}

	}
}