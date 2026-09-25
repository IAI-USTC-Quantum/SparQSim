/**
 * @file block_encoding_tridiagonal.cpp
 * @brief 三对角矩阵块编码的实现
 * @details 实现 PlusOneAndOverflow（模加一移位门）的正向/dagger 操作，以及
 *          Block_Encoding_Tridiagonal 构造函数中 LCU 状态制备振幅的计算
 */

#include "BlockEncoding/block_encoding_tridiagonal.h"

namespace qram_simulator {
	namespace block_encoding {
		namespace block_encoding_tridiagonal {
			/**
			 * @brief 应用加一移位操作
			 * @param state 系统状态向量
			 * @details 主寄存器值 +1；当值已达 2^n - 1 时回绕为 0 并翻转溢出位
			 */
			void PlusOneAndOverflow::operator()(std::vector<System>& state) const {
				profiler _("Quantum_Modulo_Adder");
				auto main_reg_idx = System::get(main_reg);
				auto overflow_reg_idx = System::get(overflow);
				auto main_reg_num = System::size_of(main_reg);
#ifdef SINGLE_THREAD
				for (auto& s : state)
				{
#else
#pragma omp parallel for
				for (int i = 0; i < state.size(); ++i)
				{
					auto& s = state[i];
#endif
					if (!ConditionSatisfied(s))
						continue;
					auto& val = s.get(main_reg_idx).value;
					auto& overflow_val = s.get(overflow_reg_idx).value;
					if (val == pow2(main_reg_num) - 1)
					{
						overflow_val = (overflow_val + 1) % 2;
						val = 0;
					}
					else {
						val += 1;
					}
				}
			}

			/**
			 * @brief 应用 dagger 操作（减一移位）
			 * @param state 系统状态向量
			 * @details 主寄存器值 -1；当值已为 0 时回绕到 2^n - 1 并翻转溢出位
			 */
			void PlusOneAndOverflow::dag(std::vector<System>& state) const {
				profiler _("Quantum_Modulo_Adder");
				auto main_reg_idx = System::get(main_reg);
				auto overflow_reg_idx = System::get(overflow);
				auto main_reg_num = System::size_of(main_reg);
#ifdef SINGLE_THREAD
				for (auto& s : state)
				{
#else
#pragma omp parallel for
				for (int i = 0; i < state.size(); ++i)
				{
					auto& s = state[i];
#endif
					if (!ConditionSatisfied(s))
						continue;
					
					auto& val = s.get(main_reg_idx).value;
					auto& overflow_val = s.get(overflow_reg_idx).value;
					if (val == 0)
					{
						overflow_val = (overflow_val + 1) % 2;
						val = pow2(main_reg_num) - 1;
					}
					else {
						val -= 1;
					}

				}
			}

			/**
			 * @brief 构造函数：计算 LCU 状态制备振幅
			 * @param main_reg_ 主寄存器名称
			 * @param anc_UA_ 块编码辅助寄存器名称
			 * @param alpha_ 对角元系数 α
			 * @param beta_ 次对角元系数 β
			 * @details 归一化因子取 Frobenius 范数 s = sqrt(N|α|² + 2(N-1)|β|²)
			 *          （N = 2^主寄存器位数），振幅向量
			 *          prep_state = {√|α|/s, √|β|/s, √|β|/s, √(1-(|α|+2|β|)/s)}，
			 *          前三个分量对应 I / +1 移位 / -1 移位分支，第四个为湮灭分支
			 */
			Block_Encoding_Tridiagonal::Block_Encoding_Tridiagonal(
				std::string_view main_reg_,
				std::string_view anc_UA_,
				double alpha_,
				double beta_) :				
				main_reg(main_reg_), anc_UA(anc_UA_), alpha(alpha_), beta(beta_)
			{
				auto n = pow2(System::size_of(main_reg));
				auto sum = n * std::abs(alpha_) * std::abs(alpha_) + 2 * (n - 1) * std::abs(beta_) * std::abs(beta_);
				auto norm_F = std::sqrt(sum);

				//mat = DenseMatrix<complex_t>(4);
				//mat(0, 0) = std::sqrt(std::abs(alpha_)) / std::sqrt(norm_F);
				//mat(1, 0) = std::sqrt(std::abs(beta_)) / std::sqrt(norm_F);
				//mat(2, 0) = std::sqrt(std::abs(beta_)) / std::sqrt(norm_F);
				//mat(3, 0) = std::sqrt(1 - (std::abs(alpha_) + 2 * std::abs(beta_)) / norm_F);
				//mat(1, 1) = 1;
				//mat(2, 2) = 1;
				//mat(3, 3) = 1;
				//gram_schmidt_process(mat);

				prep_state = std::vector<complex_t>{
					std::sqrt(std::abs(alpha_)) / std::sqrt(norm_F),
					std::sqrt(std::abs(beta_)) / std::sqrt(norm_F),
					std::sqrt(std::abs(beta_)) / std::sqrt(norm_F),
					std::sqrt(1 - (std::abs(alpha_) + 2 * std::abs(beta_)) / norm_F)
				};
			};

			


		} // namespace block_encoding_tridiagonal
	} // namespace block_encoding
}