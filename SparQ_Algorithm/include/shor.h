/**
 * @file shor.h
 * @brief Shor 量子因数分解算法（标准版 + 半经典版）
 * @details 基于寄存器级编程实现 Shor 算法的量子部分：
 *          模幂算子 ExpMod（|x⟩|z⟩ → |x⟩|z·a^x mod N⟩）、相位估计式完整流程
 *          （Shor）与半经典（测量反馈式）变体 SemiClassicalShor，
 *          以及连分数收尾等经典后处理辅助函数。
 *          对应的 Python 实现见 pysparq.algorithms.shor，
 *          C++ 实验入口见 Experiments/Shor
 */

#pragma once

#include "sparse_state_simulator.h"

namespace qram_simulator {
	/**
	 * @namespace qram_simulator::shor
	 * @brief Shor 因数分解算法组件
	 */
	namespace shor {
		/** @brief 模幂函数类型：x ↦ a^x mod N（由经典预计算封装） */
		using ExpModFunc = std::function<size_t(size_t)>;

		/**
		 * @brief 计算大指数模幂 a^x mod N
		 * @param a 底数
		 * @param x 指数（任意大整数）
		 * @param N 模数（待分解的奇合数）
		 * @return a^x mod N
		 */
		/* compute a^x mod N for any large x */
		size_t general_expmod(size_t a, size_t x, size_t N);

		/**
		 * @brief Shor 算法执行失败异常
		 * @details 在测量结果无法导出有效周期（后处理失败）等场景抛出
		 */
		class ShorExecutionFailed : public std::runtime_error
		{
		public:
			/**
			 * @brief 构造函数
			 * @param message 异常描述信息
			 */
			ShorExecutionFailed(const std::string& message) : std::runtime_error(message) {}
		};

		/**
		 * @brief 抛出 Shor 执行失败异常
		 * @param message 异常描述信息
		 */
		inline void throw_bad_shor_result(const std::string& message)
		{
			throw ShorExecutionFailed(message);
		}

		/**
		 * @brief 模幂量子算子（自伴）
		 * @details 实现 |x⟩|z⟩ → |x⟩|z · (a^x mod N)⟩；
		 *          模幂函数由经典预计算的 ExpModFunc 提供
		 *          （周期 r 内的 a^x mod N 查表），量子侧只做函数表查询式变换
		 */
		/* compute |x>|z> -> |x>|z ^ (a^x mod N)> */
		struct ExpMod : SelfAdjointOperator
		{
			/** @brief 输入（指数）寄存器 ID */
			size_t reg_input;
			/** @brief 输出（幂值）寄存器 ID */
			size_t reg_output;
			/** @brief 经典预计算的模幂函数 */
			ExpModFunc anc_func;

			/**
			 * @brief 构造函数
			 * @param reg_input_ 输入寄存器 ID
			 * @param reg_output_ 输出寄存器 ID
			 * @param func 模幂函数
			 */
			ExpMod(size_t reg_input_, size_t reg_output_, ExpModFunc func)
				:reg_input(reg_input_), reg_output(reg_output_), anc_func(func)
			{}

			/**
			 * @brief 应用模幂操作
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief 半经典 Shor 分解器（测量反馈式量子相位估计）
		 * @details 以逐位测量 + 反馈旋转代替完整的逆 QFT：
		 *          每测一位就根据已测结果对剩余叠加态施加条件相位旋转，
		 *          显著减少所需量子比特。run() 执行量子部分并做部分迹读出，
		 *          postprocess() 用连分数法恢复周期并给出分解结果
		 */
		/* Seems good */
		struct SemiClassicalShor
		{
			/** @brief 随机底数 a（与 N 互素） */
			size_t a;
			/** @brief N 的二进制位数 */
			size_t n;
			/** @brief 待分解的奇合数 N */
			size_t N;
			/** @brief 工作寄存器位宽（2n） */
			size_t size;
			/** @brief 最终测量结果（run() 填充） */
			size_t meas_result = 0;
			/** @brief 恢复出的周期 r（postprocess() 填充，0 表示失败） */
			size_t period = 0;
			/** @brief 分解出的因子 p（postprocess() 填充） */
			size_t p = 0;
			/** @brief 分解出的因子 q（postprocess() 填充） */
			size_t q = 0;

			/**
			 * @brief 构造函数
			 * @param a_ 随机底数（与 N 互素）
			 * @param N_ 待分解的奇合数
			 * @param n_ N 的二进制位数
			 */
			SemiClassicalShor(size_t a_, size_t N_, size_t n_)
				: a(a_), N(N_), n(n_), size(n_ * 2)
			{
			}

			/**
			 * @brief 执行量子部分（半经典相位估计 + 部分迹读出）
			 * @return 测量结果整数值
			 */
			size_t run();

			/** @brief 经典后处理：连分数恢复周期并计算因子 p、q */
			void postprocess();
		};

		/**
		 * @brief 标准 Shor 分解算子（相位估计式）
		 * @details 工作寄存器制备叠加态后经 ExpMod 做模幂，
		 *          再对工作寄存器做部分迹（等价逆 QFT 采样）读出相位信息，
		 *          由经典后处理恢复周期
		 */
		/* Seems good */
		struct Shor
		{
			/** @brief 工作寄存器 ID（存放叠加指数 x） */
			size_t work_reg;
			/** @brief 辅助寄存器 ID（存放 a^x mod N） */
			size_t ancilla_reg;
			/** @brief 经典预计算的模幂函数 */
			ExpModFunc anc_func;

			/**
			 * @brief 构造函数
			 * @param work_register 工作寄存器 ID
			 * @param ancilla_register 辅助寄存器 ID
			 * @param a_ 底数 a（仅作语义记录，实际计算走 func）
			 * @param N_ 模数 N（仅作语义记录）
			 * @param func 模幂函数
			 */
			Shor(size_t work_register, size_t ancilla_register, size_t a_, size_t N_, ExpModFunc func)
				: work_reg(work_register), ancilla_reg(ancilla_register), anc_func(func)
			{}

			/**
			 * @brief 执行 Shor 量子部分
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief 由测量值求最优连分数近似 y/Q 对应的分子分母
		 * @param y 相位估计测量值
		 * @param Q 分母上界（通常为 2^size）
		 * @param N 待分解数（近似结果需分母小于 N）
		 * @return 最优逼近的 (分子, 分母) 对
		 */
		std::pair<size_t, size_t> find_best_fraction(size_t y, size_t Q, size_t N);

		/**
		 * @brief 由测量结果计算周期 r
		 * @param meas_result 相位估计测量值
		 * @param size 工作寄存器位宽
		 * @param N 待分解数
		 * @return 候选周期（0 表示失败）
		 */
		uint64_t compute_period(uint64_t meas_result, size_t size, size_t N);

		/**
		 * @brief 校验周期候选：r 须为偶且 a^{r/2} ≢ -1 (mod N)
		 * @param period 周期候选
		 * @param a 底数
		 * @param N 待分解数
		 * @throws ShorExecutionFailed 校验失败时抛出
		 */
		void check_period(uint64_t period, uint64_t a, uint64_t N);

		/**
		 * @brief Shor 经典后处理：由测量值恢复周期并计算因子
		 * @param meas 相位估计测量值
		 * @param size 工作寄存器位宽
		 * @param a 底数
		 * @param N 待分解数
		 * @return (p, q) 因子对（失败时返回无效值）
		 * @throws ShorExecutionFailed 无法恢复有效周期时抛出
		 */
		std::tuple<uint64_t, uint64_t> shor_postprocess(uint64_t meas, size_t size, uint64_t a, uint64_t N);

		/**
		 * @brief 标准 Shor 分解完整流程（C++ 实验入口）
		 * @details 随机（或指定）底数 a → 预计算模幂表 → 量子相位估计 →
		 *          部分迹读出 → 连分数后处理输出因子
		 * @param N 待分解的奇合数
		 * @param ainput 可选指定底数（缺省随机选取）
		 * @return 0 表示流程完成；1 表示 a 与 N 不互素（此时 gcd(a,N) 即因子）
		 */
		inline int common_shor(size_t N, std::optional<size_t> ainput = std::nullopt)
		{
			size_t n = log2(N) + 1;
			size_t size = n * 2;

			uint64_t a;
			if (ainput.has_value())
				a = ainput.value();
			else
				a = uint64_t(random_engine::rng() * (N - 1) + 1);

			// check whether a and N are coprime
			if (std::gcd(a, N) != 1)
			{
				fmt::print("a = {} and N = {} are not coprime\n", a, N);
				return 1;
			}

			auto work_reg = System::add_register("work_reg", UnsignedInteger, size);
			auto anc_reg = System::add_register("anc_reg", UnsignedInteger, n);

			std::vector<System> state;
			state.emplace_back();

			std::vector<uint64_t> axmodn;
			uint64_t value = 1;
			axmodn.push_back(value);
			for (size_t i = 1; i < N; ++i)
			{
				uint64_t next_val = axmodn.back() * a % N;
				if (next_val == 1)
					break;
				else
					axmodn.push_back(next_val);
			}

			size_t r = axmodn.size();
			fmt::print("r = {} (N = {}, n = {}, a = {})\n", r, N, n, a);

			ExpModFunc func = [r, &axmodn](size_t x) -> size_t
				{
					x = x % r;
					return axmodn[x];
				};

			Shor(work_reg, anc_reg, a, N, func)(state);

			//(RemoveRegister(anc_reg))(state);
			//SortByAmplitude()(state);
			//StatePrint(Detail | Prob)(state);

			auto&& [meas_res, _] = PartialTrace(std::vector{ work_reg })(state);
			auto&& [p, q] = shor_postprocess(meas_res[0], size, a, N);
		}

		/**
		 * @brief 半经典 Shor 分解完整流程（C++ 实验入口）
		 * @details 与 common_shor 相同的经典准备，量子部分改用
		 *          SemiClassicalShor 的逐位测量反馈式相位估计
		 * @param N 待分解的奇合数
		 * @param ainput 可选指定底数（缺省随机选取）
		 * @return 0 表示流程完成；1 表示 a 与 N 不互素（此时 gcd(a,N) 即因子）
		 */
		inline auto semi_classical_shor(size_t N, std::optional<size_t> ainput = std::nullopt)
		{
			size_t n = log2(N) + 1;

			size_t a;
			if (ainput.has_value())
				a = ainput.value();
			else
				a = size_t(random_engine::rng() * (N - 1) + 1);

			// check whether a and N are coprime
			if (std::gcd(a, N) != 1)
			{
				fmt::print("a = {} and N = {} are not coprime\n", a, N);
				return 1;
			}

			std::vector<uint64_t> axmodn;
			uint64_t value = 1;
			axmodn.push_back(value);
			for (size_t i = 1; i < N; ++i)
			{
				uint64_t next_val = axmodn.back() * a % N;
				if (next_val == 1)
					break;
				else
					axmodn.push_back(next_val);
			}

			size_t r = axmodn.size();

			fmt::print("r = {} (N = {}, n = {}, a = {})\n", r, N, n, a);

			SemiClassicalShor obj(a, N, n);
			uint64_t result = obj.run();
			obj.postprocess();

			return 0;
		}
	}
} // namespace qram_simulator
