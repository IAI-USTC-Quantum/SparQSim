/**
 * @file state_preparation.h
 * @brief 基于 QRAM 的任意稀疏态制备
 * @details 通过二叉树逐层振幅分裂制备目标分布：每层从工作寄存器切出 1 位
 *          旋转比特，用 QRAM 读取父/子节点振幅并计算比例角，施加条件旋转
 *          （CondRot_Fixed_Bool）后卸载辅助寄存器。含算子版（State_Prep_via_QRAM）
 *          与演示驱动（state_preparation_demo 命名空间）。
 *          对应的 Python 实现见 pysparq.algorithms.state_preparation，
 *          C++ 实验入口见 Experiments/StatePreparation
 */

#pragma once
#include "state_manipulator.h"
#include "simple_quantum_simulator.h"
#include "sparse_state_simulator.h"
#include "global_macros.h"

namespace qram_simulator {
	/**
	 * @namespace qram_simulator::state_prep
	 * @brief 态制备算子
	 */
	namespace state_prep {
		/**
		 * @brief 基于 QRAM 的态制备算子（复合算子）
		 * @details 把存于 QRAM 的目标分布逐位制备到工作寄存器：第 k 层切出
		 *          旋转比特，构造父/子地址（addr_parent/addr_child）与父/子数据
		 *          （data_parent/data_child），QRAM 加载后用
		 *          Div_Sqrt_Arccos_UInt_UInt / GetRotateAngle_Int_Int 计算旋转角，
		 *          条件旋转写入振幅比例，再以 dagger 序列卸载全部辅助量。
		 *          支持条件控制（ClassControllable）
		 */
		struct State_Prep_via_QRAM : BaseOperator
		{
			/** @brief 工作寄存器名称（制备结果所在，同时兼任地址前缀） */
			std::string work_qubit;
			/** @brief 地址位宽（= 工作寄存器位宽，二叉树层数） */
			size_t addr_size;
			/** @brief 数据寄存器位宽（存放振幅分子/分母） */
			size_t data_size;
			/** @brief 有理数寄存器位宽（存放除法与反余弦中间结果） */
			size_t rational_size;
			/** @brief 存放目标分布的 QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;

			ClassControllable

			/**
			 * @brief 构造函数
			 * @param qram_ 存放目标分布的 QRAM 电路指针
			 * @param work_qubit_ 工作寄存器名称
			 * @param dsz 数据寄存器位宽
			 * @param rsz 有理数寄存器位宽
			 */
			State_Prep_via_QRAM(qram_qutrit::QRAMCircuit* qram_,
				std::string_view work_qubit_,
				size_t dsz,
				size_t rsz) : work_qubit(work_qubit_), addr_size(System::size_of(work_qubit)), data_size(dsz), rational_size(rsz), qram(qram_)
			{
				//QRAMLoad::version = qram_version;
			};

			/**
			 * @brief 正向制备实现
			 * @details 逐层（k = 0 … addr_size-1）执行：切旋转比特 → 构造父/子
			 *          地址与数据 → QRAM 加载 → 计算旋转角 → 条件旋转 → 卸载；
			 *          最后一层用 GetRotateAngle_Int_Int 处理叶子节点角度
			 * @tparam Ty 状态类型（std::vector<System> 或 SparseState）
			 * @param state 系统状态
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				profiler _("State_Prep_via_QRAM");

				AddRegister("addr_parent", UnsignedInteger, addr_size + 1)(state);
				AddRegister("addr_child", UnsignedInteger, addr_size + 1)(state);
				AddRegister("data_parent", SignedInteger, data_size)(state);
				AddRegister("data_child", SignedInteger, data_size)(state);
				AddRegister("div_result", Rational, rational_size)(state);

				for (size_t k = 0; k < addr_size; ++k) {
					auto target = SplitRegister(work_qubit, "rotation", 1)(state);
					std::get<1>(System::name_register_map[System::get("rotation")]) = Boolean;
					Add_ConstUInt_InPlace("addr_parent", pow2(k) - 1)(state);
					Add_UInt_UInt_InPlace(work_qubit, "addr_parent")(state);
					Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
					X_Bool("addr_child", 0)(state);
					if (k != addr_size - 1)
					{
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						{
							profiler _("StatePrep::CondRot");
							CondRot_Fixed_Bool("div_result", "rotation")(state);
						}
						ClearZero()(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
					}
					else
					{
						ShiftLeft_InPlace("addr_parent", 1)(state);
						X_Bool("addr_parent", 0)(state);
						Add_ConstUInt_InPlace("addr_child", 1)(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
						{
							profiler _("StatePrep::CondRot");
							CondRot_Fixed_Bool("div_result", "rotation")(state);
						}
						ClearZero()(state);
						GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Add_ConstUInt_InPlace("addr_child", 1).dag(state);
						X_Bool("addr_parent", 0)(state);
						ShiftRight_InPlace("addr_parent", 1)(state);
					}
					X_Bool("addr_child", 0)(state);
					Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
					Add_UInt_UInt_InPlace(work_qubit, "addr_parent").dag(state);
					Add_ConstUInt_InPlace("addr_parent", pow2(k) - 1).dag(state);
					//StatePrint(0, 10)(state);
					CombineRegister(work_qubit, "rotation")(state);
					ShiftLeft_InPlace(work_qubit, 1)(state);
				}
				ShiftRight_InPlace(work_qubit, 1)(state);
				RemoveRegister("addr_parent")(state);
				RemoveRegister("data_parent")(state);
				RemoveRegister("addr_child")(state);
				RemoveRegister("data_child")(state);
				RemoveRegister("div_result")(state);

				ClearZero()(state);
			}

			/**
			 * @brief 逆向（dagger）制备实现
			 * @details impl 的严格逆序：层序倒转（k = addr_size-1 … 0），
			 *          每层内部操作按相反顺序以 .dag() 执行，
			 *          用于把制备好的态还原回初态
			 * @tparam Ty 状态类型
			 * @param state 系统状态
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				profiler _("State_Prep_via_QRAM::dag");

				AddRegister("addr_parent", UnsignedInteger, addr_size + 1)(state);
				AddRegister("addr_child", UnsignedInteger, addr_size + 1)(state);
				AddRegister("data_parent", SignedInteger, data_size)(state);
				AddRegister("data_child", SignedInteger, data_size)(state);
				AddRegister("div_result", Rational, rational_size)(state);

				ShiftLeft_InPlace(work_qubit, 1)(state);
				for (size_t k = 0; k <= addr_size - 1; ++k) {
					ShiftRight_InPlace(work_qubit, 1)(state);
					auto target = SplitRegister(work_qubit, "rotation", 1)(state);
					std::get<1>(System::name_register_map[System::get("rotation")]) = Boolean;
					Add_ConstUInt_InPlace("addr_parent", pow2(addr_size - 1 - k) - 1)(state);
					Add_UInt_UInt_InPlace(work_qubit, "addr_parent")(state);
					Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
					X_Bool("addr_child", 0)(state);
					if (k != 0)
					{
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						{
							profiler _("StatePrep::CondRot::dag");
							// size_t original_size = state.size();
							// fmt::print("stateprep original_size {}\n", original_size);
							CondRot_Fixed_Bool("div_result", "rotation").dag(state);
						}
						ClearZero()(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
					}
					else
					{
						ShiftLeft_InPlace("addr_parent", 1)(state);
						X_Bool("addr_parent", 0)(state);
						Add_ConstUInt_InPlace("addr_child", 1)(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
						{
							profiler _("StatePrep::CondRot::dag");
							// size_t original_size = state.size();
							// fmt::print("stateprep original_size {}\n", original_size);
							CondRot_Fixed_Bool("div_result", "rotation").dag(state);
						}
						ClearZero()(state);
						GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Add_ConstUInt_InPlace("addr_child", 1).dag(state);
						X_Bool("addr_parent", 0)(state);
						ShiftRight_InPlace("addr_parent", 1)(state);
					}
					X_Bool("addr_child", 0)(state);
					Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
					Add_UInt_UInt_InPlace(work_qubit, "addr_parent").dag(state);
					Add_ConstUInt_InPlace("addr_parent", pow2(addr_size - 1 - k) - 1).dag(state);
					CombineRegister(work_qubit, "rotation")(state);

				}

				RemoveRegister("addr_parent")(state);
				RemoveRegister("data_parent")(state);
				RemoveRegister("addr_child")(state);
				RemoveRegister("data_child")(state);
				RemoveRegister("div_result")(state);

				ClearZero()(state);
			}

			COMPOSITE_OPERATION
		};

	}

	/**
	 * @namespace qram_simulator::state_preparation_demo
	 * @brief 态制备演示驱动（实验用）
	 */
	namespace state_preparation_demo {

		/**
		 * @brief 稀疏态演示载体
		 * @details 构造时注册父/子地址、父/子数据、临时位与有理数寄存器，
		 *          持有初始 |0⟩ 态；提供清空、排序、打印与运行入口
		 */
		struct SparseStateDemo
		{
			/** @brief 地址位宽 */
			size_t addr_size;
			/** @brief 数据位宽 */
			size_t data_size;
			/** @brief 有理数位宽 */
			size_t rational_size;
			/** @brief 稀疏态（基态向量） */
			std::vector<System> system_states;
			/** @brief QRAM 电路指针（make_qram 后由 set_qram 填充） */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief QRAM 电路版本字符串 */
			std::string qram_version;

			/**
			 * @brief 构造函数：注册全部寄存器并制备 |0⟩ 初态
			 * @param asz 地址位宽
			 * @param dsz 数据位宽
			 * @param rsz 有理数位宽
			 * @param qram_version_ QRAM 电路版本
			 */
			SparseStateDemo(size_t asz, size_t dsz, size_t rsz, std::string qram_version_)
				: addr_size(asz), data_size(dsz), rational_size(rsz), qram_version(qram_version_)
			{
				System::add_register("addr_parent", UnsignedInteger, asz + 1);
				System::add_register("addr_child", UnsignedInteger, asz + 1);
				System::add_register("data_parent", SignedInteger, dsz);
				System::add_register("data_child", SignedInteger, dsz);
				System::add_register("temp_bit", Boolean, 1);
				System::add_register("div_result", Rational, rsz);
				system_states.emplace_back();

				QRAMLoad::version = qram_version;
			}

			/** @brief 清空稀疏态（回到单分支 |0⟩） */
			void clear_state();

			/** @brief 按基态键排序稀疏态 */
			void sort_state();

			/** @brief 打印稀疏态为字符串 */
			std::string to_string() const;

			/** @brief 执行态制备流程 */
			void run();
		};

		/**
		 * @brief 态制备完整演示驱动
		 * @details 组合经典侧（随机分布生成、二叉树构建、QRAM 构造）与量子侧
		 *          （SparseStateDemo 载体上的制备执行），并支持噪声注入与
		 *          保真度统计
		 */
		class StatePreparation
		{
		public:
			/** @brief 量子侧稀疏态载体 */
			SparseStateDemo sparse_state;
			/** @brief 工作寄存器比特数（= 二叉树层数） */
			size_t qubit_number;
			/** @brief 数据位宽 */
			size_t data_size;
			/** @brief 数据值域上限 */
			size_t data_range;
			/** @brief 目标分布（经典侧，按地址索引） */
			std::vector<size_t> dist;
			/** @brief 振幅二叉树（经典侧） */
			std::vector<size_t> tree;
			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief QRAM 电路版本字符串 */
			std::string qram_version;

			/**
			 * @brief 构造函数
			 * @param qn 工作寄存器比特数
			 * @param data_sz 数据位宽
			 * @param data_range_ 数据值域上限
			 * @param qram_version_ QRAM 电路版本
			 */
			StatePreparation(size_t qn, size_t data_sz, size_t data_range_, std::string qram_version_)
				: qubit_number(qn), data_size(data_sz), data_range(data_range_),
				sparse_state(qn, data_sz, std::min(size_t(50), data_sz * 2), qram_version_),
				qram_version(qram_version_)
			{
			}

			/** @brief 生成随机目标分布 */
			void random_distribution();

			/** @brief 打印目标分布 */
			void show_distribution();

			/** @brief 获取归一化后的实数目标分布 */
			std::vector<double> get_real_dist();

			/** @brief 由目标分布构建振幅二叉树 */
			void make_tree();

			/** @brief 打印振幅二叉树 */
			void show_tree();

			/** @brief 由二叉树构造 QRAM 内存 */
			void make_qram();

			/** @brief 将构造好的 QRAM 电路注入载体 */
			void set_qram();

			/**
			 * @brief 设置 QRAM 噪声模型
			 * @param noise 噪声参数（各操作类型的错误率）
			 */
			void set_noise(const noise_t& noise);

			/** @brief 计算制备态与目标分布的保真度 */
			double get_fidelity() const;

			/** @brief 计算并打印保真度 */
			double get_fidelity_show() const;

			/** @brief 打印当前稀疏态（前 10 行，含细节） */
			inline void print_state() {
				StatePrint(0 | Detail)(sparse_state.system_states);
			}

			/** @brief 执行完整制备流程 */
			void run();

			/** @brief 清空载体稀疏态 */
			inline void clear_state() { sparse_state.clear_state(); }
		};

	} // namespace state_preparation_demo
} // namespace qram_simulator
