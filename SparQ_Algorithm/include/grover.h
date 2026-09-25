/**
 * @file grover.h
 * @brief Grover 量子搜索算法（稀疏态/稠密态两套接口）
 * @details 提供基于寄存器级编程的 Grover 组件：QRAM 相位预言机（GroverOracle）、
 *          HPH 扩散算子、完整迭代（GroverOperator）、多次振幅放大（GroverAmplify）
 *          与量子计数（GroverCount），在 N 个条目中以 O(√N) 复杂度搜索标记项。
 *          另含 grover_dense 命名空间下的稠密态旧接口（直接操作
 *          std::vector<complex_t> 状态向量），与 grover 命名空间的稀疏态新接口相区别
 */

#pragma once

#include "state_manipulator.h"
#include "simple_quantum_simulator.h"
#include "sparse_state_simulator.h"
#include "hamiltonian_simulation.h"

namespace qram_simulator {
	/**
	 * @namespace qram_simulator::grover_dense
	 * @brief Grover 算法的稠密态旧接口
	 * @details 直接操作 std::vector<complex_t> 全态向量，配合 QRAM 电路与噪声模型，
	 *          用于与 QRAM-Simulator 基座的稠密态模拟器对接；
	 *          新代码建议使用 grover 命名空间下的稀疏态接口
	 */
	namespace grover_dense {
		/**
		 * @brief 对稠密态应用 QRAM 预言机
		 * @details 将 QRAM 应用于地址位 [0, n)、数据位 n，其余位为 otherqubit；
		 *          执行后校验态归一性，失败时打印状态并抛出异常
		 * @tparam QRAM QRAM 电路类型（qutrit/qubit 实现）
		 * @param state 输入输出稠密态向量
		 * @param n 地址位宽度
		 * @param qram QRAM 电路指针
		 * @param version QRAM 电路版本字符串
		 */
		template<typename QRAM>
		void oracle(std::vector<complex_t>& state, size_t n, QRAM* qram, std::string version)
		{
			profiler _("Grover_shots: oracle");

			std::vector<size_t> addrqubit(n);
			iota(addrqubit.begin(), addrqubit.end(), 0);
			std::vector<size_t> dataqubit(1, n);
			std::vector<size_t> otherqubit;

			state = qram->apply(state, addrqubit, dataqubit, otherqubit, version);
			if (std::abs(amp_sum(state) - 1.0) > epsilon)
			{
				double ampsum = amp_sum(state);
				quantum_simulator::print_state(state, false);
				fmt::print("amp_sum={}\n", ampsum);
				fmt::print("{}\n", (*qram)->to_string_full_info());

				throw_bad_result();
			}
		}

		/**
		 * @brief Grover 扩散算子（关于平均值的反射）
		 * @param state 输入输出稠密态向量
		 * @param n 地址位宽度
		 */
		void diffusion(std::vector<complex_t>& state, size_t n);

		/**
		 * @brief 执行完整 Grover 迭代（预言机 + 扩散）
		 * @param state 输入输出稠密态向量
		 * @param n 地址位宽度
		 * @param pos 标记项在内存中的位置
		 * @param repeat 迭代次数
		 * @param oracle 预言机回调（接受稠密态引用）
		 */
		void grover(std::vector<complex_t>& state, size_t n, size_t pos, size_t repeat,
			std::function<void(decltype(state))> oracle);

		/**
		 * @brief 多次采样运行 Grover 搜索并统计测量结果
		 * @details 每次采样重新制备初态、执行指定轮数的 Grover 迭代后测量，
		 *          只保留最低 n 位（地址位）的测量结果
		 * @tparam QRAM QRAM 电路类型
		 * @param n 地址位宽度
		 * @param pos 标记项在内存中的位置
		 * @param shots 采样次数
		 * @param repeat 每次采样的 Grover 迭代次数
		 * @param noise 各操作类型的噪声模型参数
		 * @param version QRAM 电路版本字符串
		 * @return 长度为 2^n 的向量，第 i 项为测得地址 i 的次数
		 */
		template<typename QRAM>
		std::vector<size_t> grover_shots(size_t n, size_t pos, size_t shots, size_t repeat,
			const std::map<OperationType, double>& noise, std::string version)
		{
			std::vector<size_t> measurements(pow2(n), 0);
			std::vector<complex_t> state;
			memory_t memory(pow2(n), 0);
			memory[pos] = 1;
			QRAM qram(n, 1, memory);
			qram->set_noise_models(noise);

			for (size_t i = 0; i < shots; ++i) {
				profiler _("Grover_shots: Mainloop");
				quantum_simulator::init_n_state(state, n + 1);
				auto oracle_ = std::bind(oracle<QRAM>, std::placeholders::_1, n, &qram, version);
				grover(state, n, pos, repeat, oracle_);
				size_t m = quantum_simulator::measure(state);
				if (m == state.size())
					throw_bad_result();
				// only extract the lowest n
				m -= ((m >> n) << n);
				++measurements[m];
			}
			return measurements;
		}
	}

	/**
	 * @namespace qram_simulator::grover
	 * @brief Grover 算法的稀疏态接口（寄存器级编程）
	 */
	namespace grover
	{
		/**
		 * @brief 基于 QRAM 的 Grover 相位预言机
		 * @details 通过 QRAM 加载把内存数据读入数据寄存器，对等于搜索目标的分支
		 *          施加相位翻转，再卸载（uncompute）数据寄存器，
		 *          实现相位反冲式标记：|x⟩|0⟩ → (-1)^{f(x)} |x⟩|0⟩。
		 *          支持条件控制（ClassControllable）
		 */
		struct GroverOracle
		{
			/** @brief QRAM 地址寄存器 ID */
			size_t qram_address_id;
			/** @brief QRAM 数据寄存器 ID */
			size_t qram_data_id;
			/** @brief 搜索目标寄存器 ID（存放待匹配值） */
			size_t search_data_id;
			/** @brief QRAM 电路（qutrit/qubit 实现）指针 */
			qram_qutrit::QRAMCircuit* qram;

			ClassControllable

			/**
			 * @brief 构造函数（寄存器 ID 版）
			 * @param qram_ QRAM 电路指针
			 * @param qram_address_id_ 地址寄存器 ID
			 * @param qram_data_id_ 数据寄存器 ID
			 * @param search_data_id_ 搜索目标寄存器 ID
			 */
			GroverOracle(qram_qutrit::QRAMCircuit* qram_, size_t qram_address_id_,
				size_t qram_data_id_, size_t search_data_id_)
				:
				qram(qram_),
				qram_address_id(qram_address_id_),
				qram_data_id(qram_data_id_),
				search_data_id(search_data_id_)
			{
			}

			/**
			 * @brief 构造函数（寄存器名称版）
			 * @param qram_ QRAM 电路指针
			 * @param qram_address_ 地址寄存器名称
			 * @param qram_data_ 数据寄存器名称
			 * @param search_data_ 搜索目标寄存器名称
			 */
			GroverOracle(qram_qutrit::QRAMCircuit* qram_, std::string_view qram_address_,
				std::string_view qram_data_, std::string_view search_data_)
				:
				qram(qram_),
				qram_address_id(System::get(qram_address_)),
				qram_data_id(System::get(qram_data_)),
				search_data_id(System::get(search_data_))
			{
			}

			/**
			 * @brief 应用预言机操作
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;

		};

		/**
		 * @brief H-P-H 扩散算子
		 * @details Grover 扩散算子的 H⊗n · 相位翻转 · H⊗n 形式：
		 *          对地址寄存器先施加 Hadamard、再对 |0...0⟩ 分支翻转相位、
		 *          最后再施加 Hadamard，实现关于均匀叠加态的反射。
		 *          支持条件控制（ClassControllable）
		 */
		struct HPH
		{
			/** @brief 地址寄存器 ID */
			size_t qram_address_id;
			/** @brief 寄存器位宽（构造时自全局寄存器表缓存） */
			size_t size;

			ClassControllable

			/**
			 * @brief 构造函数（寄存器 ID 版）
			 * @param qram_address_id_ 地址寄存器 ID
			 */
			HPH(size_t qram_address_id_)
				: qram_address_id(qram_address_id_),
				size(System::size_of(qram_address_id_))
			{ }

			/**
			 * @brief 构造函数（寄存器名称版）
			 * @param qram_address_name 地址寄存器名称
			 * @param size_ 寄存器位宽
			 */
			HPH(std::string qram_address_name, size_t size_)
				: qram_address_id(System::get(qram_address_name)), size(size_)
			{ }

			/**
			 * @brief 应用扩散操作
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;

		};

		/**
		 * @brief 单次完整 Grover 迭代算子
		 * @details 组合 GroverOracle（相位标记）与 HPH（扩散反射），
		 *          构成一次标准 Grover 迭代 G = HPH · Oracle。
		 *          支持条件控制（ClassControllable）
		 */
		struct GroverOperator
		{
			/** @brief QRAM 地址寄存器 ID */
			size_t qram_address_id;
			/** @brief QRAM 数据寄存器 ID */
			size_t qram_data_id;
			/** @brief 搜索目标寄存器 ID */
			size_t search_data_id;
			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;

			ClassControllable

			/**
			 * @brief 构造函数
			 * @param qram_ QRAM 电路指针
			 * @param qram_address_id_ 地址寄存器 ID
			 * @param qram_data_id_ 数据寄存器 ID
			 * @param search_data_id_ 搜索目标寄存器 ID
			 */
			GroverOperator(qram_qutrit::QRAMCircuit* qram_, size_t qram_address_id_,
				size_t qram_data_id_, size_t search_data_id_)
				: qram(qram_), qram_address_id(qram_address_id_),
				qram_data_id(qram_data_id_),
				search_data_id(search_data_id_)
			{
			}

			/**
			 * @brief 应用单次 Grover 迭代
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief 多轮振幅放大算子
		 * @details 连续执行 n_repeats 次 GroverOperator 迭代，
		 *          把标记项的测量概率放大到接近 1
		 */
		struct GroverAmplify
		{
			/** @brief 迭代次数 */
			size_t n_repeats;

			/** @brief QRAM 地址寄存器 ID */
			size_t qram_address_id;
			/** @brief QRAM 数据寄存器 ID */
			size_t qram_data_id;
			/** @brief 搜索目标寄存器 ID */
			size_t search_data_id;
			/** @brief 搜索目标值的位宽 */
			size_t data_size;
			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;

			/**
			 * @brief 构造函数
			 * @param qram_ QRAM 电路指针
			 * @param qram_address_id_ 地址寄存器 ID
			 * @param search_data_id_ 搜索目标寄存器 ID
			 * @param data_size_ 搜索目标值位宽
			 * @param n_repeats_ 迭代次数
			 */
			GroverAmplify(qram_qutrit::QRAMCircuit* qram_, size_t qram_address_id_, size_t search_data_id_,
				size_t data_size_, size_t n_repeats_)
				: qram(qram_), qram_address_id(qram_address_id_), n_repeats(n_repeats_),
				search_data_id(search_data_id_), data_size(data_size_)
			{}

			/**
			 * @brief 应用多轮振幅放大
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state);

		};

		/**
		 * @brief 量子计数（quantum counting）算子
		 * @details 电路结构：计数寄存器 Hadamard 制备叠加态 → 以计数寄存器
		 *          控制 Grover 迭代幂（c-U^{2^k}）→ 对计数寄存器做逆 QFT，
		 *          从而估计标记项数目 M（相位估计视角：sin²θ = M/N）
		 */
		struct GroverCount
		{
			/*
			-- H -- (c-U) - iQFT
			*/
			/** @brief 计数寄存器 ID */
			size_t count_reg;
			/** @brief QRAM 地址寄存器 ID */
			size_t addr_reg;
			/** @brief QRAM 数据寄存器 ID */
			size_t data_reg;
			/** @brief 搜索目标寄存器 ID */
			size_t search_data_reg;
			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;

			/**
			 * @brief 构造函数
			 * @param qram_ QRAM 电路指针
			 * @param count_reg_ 计数寄存器 ID
			 * @param addr_reg_ 地址寄存器 ID
			 * @param data_reg_ 数据寄存器 ID
			 * @param search_data_reg_ 搜索目标寄存器 ID
			 */
			GroverCount(qram_qutrit::QRAMCircuit* qram_, size_t count_reg_, size_t addr_reg_,
				size_t data_reg_, size_t search_data_reg_)
				: qram(qram_), count_reg(count_reg_), addr_reg(addr_reg_),
				data_reg(data_reg_), search_data_reg(search_data_reg_)
			{}

			/**
			 * @brief 执行量子计数
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state);
		};
	}

} // namespace qram_simulator
