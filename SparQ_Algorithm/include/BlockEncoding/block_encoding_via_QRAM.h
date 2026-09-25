/**
 * @file block_encoding_via_QRAM.h
 * @brief 基于 QRAM 的任意矩阵块编码
 * @details 通过 U_L / U_R 量子游走分解构造矩阵 A 的块编码：U_L|col⟩|0⟩ = |col⟩|a_col⟩
 *          按列索引制备归一化列向量，U_R|0⟩ = |A⟩ = Σ_i ‖a_i‖|i⟩ 制备列范数分布，
 *          二者组合 U_A = SWAP(row, col) · U_R†(col) · U_L(row, col) 满足
 *          ⟨i|_col⟨0|_row U_A |j⟩_col|0⟩_row = A_ij，数据由 QRAM 层级树结点提供。
 *          与 make_qram.h（数据量化与树构建）配合使用
 */

#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include <iostream>
#include <fstream>
#include "matrix.h"
#include <Eigen/Eigen>
#include <filesystem>
#include "make_qram.h"

namespace qram_simulator {
	namespace block_encoding {
		namespace block_encoding_via_QRAM
		{
			/**
			 * @brief 右乘算子 U_R（列范数状态制备）
			 * @details 实现 U_R|0⟩ = |A⟩ = Σ_i ‖a_i‖|i⟩（a_i 为 A 的第 i 列）：
			 *          自高位向低位逐位遍历列地址寄存器，借助 QRAM 加载树的父/子结点值，
			 *          由 Div_Sqrt_Arccos_UInt_UInt 计算旋转角 arccos(√(child/parent))
			 *          并做条件旋转（CondRot_Fixed_Bool），从树根向下制备出归一化的
			 *          列范数叠加态；每步之后逐步逆计算清理辅助寄存器。
			 *          支持条件控制（ClassControllable）
			 */
			struct U_R : BaseOperator
			{
				/** @brief 列索引寄存器名称 */
				std::string column_index;
				/** @brief 列地址寄存器位宽 */
				size_t addr_size;
				/** @brief 数据寄存器位宽（定点量化位数） */
				size_t data_size;
				/** @brief 有理数（旋转角）寄存器位宽 */
				size_t rational_size;
				/** @brief QRAM 电路指针（存储列范数树） */
				qram_qutrit::QRAMCircuit* qram;

				ClassControllable

				/**
				 * @brief 构造函数
				 * @param qram_ QRAM 电路指针
				 * @param column_index_ 列索引寄存器名称
				 * @param dsz 数据寄存器位宽
				 * @param rsz 有理数寄存器位宽
				 */
				U_R(qram_qutrit::QRAMCircuit* qram_,
					std::string_view column_index_,
					size_t dsz,
					size_t rsz) : column_index(column_index_), addr_size(System::size_of(column_index_)), data_size(dsz), rational_size(rsz), qram(qram_)
				{}

				/**
				 * @brief U_R 电路实现（正向）
				 * @param state 系统状态向量
				 * @details 每个地址位一次迭代：拆出旋转位 → 拼接父/子地址 → QRAM 加载
				 *          结点值 → 计算旋转角并条件旋转 → 逆计算还原地址与数据寄存器
				 */
				template<typename Ty>
				void impl(Ty& state) const
				{
					profiler _("U_R");
					AddRegister("addr_child", UnsignedInteger, addr_size + 1)(state);
					AddRegister("data_parent", SignedInteger, data_size)(state);
					AddRegister("data_child", SignedInteger, data_size)(state);
					AddRegister("temp_bit", Boolean, 1)(state);
					AddRegister("div_result", Rational, rational_size)(state);

					for (size_t k = 0; k < addr_size; ++k) {
						auto target = SplitRegister(column_index, "rotation", 1)(state);
						CombineRegister(column_index, "temp_bit")(state);
						ShiftRight_InPlace(column_index, 1)(state);
						Add_ConstUInt_InPlace(column_index, pow2(k) - 1)(state);
						Mult_UInt_ConstUInt(column_index, 2, "addr_child")(state);
						X_Bool("addr_child", 0)(state);
						QRAMLoad(qram, column_index, "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						std::get<1>(System::name_register_map[System::get("rotation")]) = Boolean;
						{
							profiler _("U_R::CondRot");
							// size_t original_size = state.size();
							// fmt::print("U_R original_size {}\n", original_size);
							CondRot_Fixed_Bool("div_result", "rotation")(state);
						}
						ClearZero()(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						QRAMLoad(qram, column_index, "data_parent")(state);
						X_Bool("addr_child", 0)(state);
						Mult_UInt_ConstUInt(column_index, 2, "addr_child")(state);
						Add_ConstUInt_InPlace(column_index, pow2(addr_size) - pow2(k) + 1)(state);
						ShiftLeft_InPlace(column_index, 1)(state);
						SplitRegister(column_index, "temp_bit", 1)(state);
						CombineRegister(column_index, "rotation")(state);
						ShiftLeft_InPlace(column_index, 1)(state);
					}
					ShiftRight_InPlace(column_index, 1)(state);
					RemoveRegister("data_parent")(state);
					RemoveRegister("addr_child")(state);
					RemoveRegister("data_child")(state);
					RemoveRegister("temp_bit")(state);
					RemoveRegister("div_result")(state);

					ClearZero()(state);
				}

				/**
				 * @brief U_R 电路实现（dagger，逆向）
				 * @param state 系统状态向量
				 * @details 按与 impl 相反的地址位顺序执行逆条件旋转与逆计算
				 */
				template<typename Ty>
				void impl_dag(Ty& state) const
				{
					profiler _("U_R::dag");
					AddRegister("addr_child", UnsignedInteger, addr_size + 1)(state);
					AddRegister("data_parent", SignedInteger, data_size)(state);
					AddRegister("data_child", SignedInteger, data_size)(state);
					AddRegister("temp_bit", Boolean, 1)(state);
					AddRegister("div_result", Rational, rational_size)(state);

					ShiftLeft_InPlace(column_index, 1)(state);
					for (size_t k = 0; k < addr_size; ++k) {
						ShiftRight_InPlace(column_index, 1)(state);
						SplitRegister(column_index, "rotation", 1)(state);
						CombineRegister(column_index, "temp_bit")(state);
						ShiftRight_InPlace(column_index, 1)(state);
						Add_ConstUInt_InPlace(column_index, pow2(addr_size - 1 - k) - 1)(state);
						Mult_UInt_ConstUInt(column_index, 2, "addr_child")(state);
						X_Bool("addr_child", 0)(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						QRAMLoad(qram, column_index, "data_parent")(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						std::get<1>(System::name_register_map[System::get("rotation")]) = Boolean;
						{
							profiler _("U_R::CondRot::dag");
							// size_t original_size = state.size();
							// fmt::print("U_R original_size {}\n", original_size);
							CondRot_Fixed_Bool("div_result", "rotation").dag(state);
						}
						ClearZero()(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						QRAMLoad(qram, column_index, "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						X_Bool("addr_child", 0)(state);
						Mult_UInt_ConstUInt(column_index, 2, "addr_child")(state);
						Add_ConstUInt_InPlace(column_index, pow2(addr_size) - pow2(addr_size - 1 - k) + 1)(state);
						ShiftLeft_InPlace(column_index, 1)(state);
						SplitRegister(column_index, "temp_bit", 1)(state);
						CombineRegister(column_index, "rotation")(state);
					}
					RemoveRegister("data_parent")(state);
					RemoveRegister("addr_child")(state);
					RemoveRegister("data_child")(state);
					RemoveRegister("temp_bit")(state);
					RemoveRegister("div_result")(state);

					ClearZero()(state);
				}

				COMPOSITE_OPERATION
			};

			/**
			 * @brief 左乘算子 U_L（按列索引制备归一化列向量）
			 * @details 实现 U_L|col⟩|0⟩ = |col⟩|a_col⟩（|a_col⟩ 为第 col 列对应的
			 *          归一化量子态）：遍历行地址寄存器的高 addr_size 位，每步由
			 *          行/列索引拼接出 QRAM 树结点的父/子地址（addr_child = 2·addr_parent + 1），
			 *          加载结点值后计算旋转角并条件旋转逐层下行；最后一层（叶子层）
			 *          改用 GetRotateAngle_Int_Int 以 atan2 型角度处理符号。
			 *          支持条件控制（ClassControllable）
			 */
			struct U_L : BaseOperator
			{
				/** @brief 行索引寄存器名称 */
				std::string row_index;
				/** @brief 列索引寄存器名称 */
				std::string column_index;
				/** @brief 单侧地址寄存器位宽 */
				size_t addr_size;
				/** @brief 数据寄存器位宽（定点量化位数） */
				size_t data_size;
				/** @brief 有理数（旋转角）寄存器位宽 */
				size_t rational_size;
				/** @brief QRAM 电路指针（存储矩阵 A 的树结构） */
				qram_qutrit::QRAMCircuit* qram;

				ClassControllable

				/**
				 * @brief 构造函数
				 * @param qram_ QRAM 电路指针
				 * @param row_index_ 行索引寄存器名称
				 * @param column_index_ 列索引寄存器名称
				 * @param dsz 数据寄存器位宽
				 * @param rsz 有理数寄存器位宽
				 */
				U_L(qram_qutrit::QRAMCircuit* qram_,
					std::string_view row_index_,
					std::string_view column_index_,
					size_t dsz,
					size_t rsz) : row_index(row_index_), column_index(column_index_), addr_size(System::size_of(row_index_)), data_size(dsz), rational_size(rsz), qram(qram_)
				{}

				/**
				 * @brief U_L 电路实现（正向）
				 * @param state 系统状态向量
				 * @details 逐位迭代：拆出旋转位 → 拼接父/子地址 → QRAM 加载 → 条件旋转
				 *          → 逆计算还原地址；非末层用 Div_Sqrt_Arccos，末层用 GetRotateAngle
				 */
				template<typename Ty>
				void impl(Ty& state) const
				{
					profiler _("U_L");

					AddRegister("addr_parent", UnsignedInteger, 2 * addr_size + 1)(state);
					AddRegister("addr_child", UnsignedInteger, 2 * addr_size + 1)(state);
					AddRegister("data_parent", SignedInteger, data_size)(state);
					AddRegister("data_child", SignedInteger, data_size)(state);
					AddRegister("div_result", Rational, rational_size)(state);

					for (size_t k = addr_size; k < 2 * addr_size; ++k) {
						auto target = SplitRegister(row_index, "rotation", 1)(state);
						std::get<1>(System::name_register_map[System::get("rotation")]) = Boolean;
						Add_ConstUInt_InPlace("addr_parent", pow2(k) - 1)(state);
						Add_Mult_UInt_ConstUInt_InPlace(column_index, pow2(k - addr_size), "addr_parent")(state);
						Add_UInt_UInt_InPlace(row_index, "addr_parent")(state);
						Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
						X_Bool("addr_child", 0)(state);
						if (k != 2 * addr_size - 1)
						{
							QRAMLoad(qram, "addr_parent", "data_parent")(state);
							QRAMLoad(qram, "addr_child", "data_child")(state);
							Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
							{
								profiler _("U_L::CondRot");
								// size_t original_size = state.size();
								// fmt::print("U_L original_size {}\n", original_size);
								CondRot_Fixed_Bool("div_result", "rotation")(state);
							}
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
								profiler _("U_L::CondRot");
								CondRot_Fixed_Bool("div_result", "rotation")(state);
							}
							GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
							QRAMLoad(qram, "addr_parent", "data_parent")(state);
							QRAMLoad(qram, "addr_child", "data_child")(state);
							Add_ConstUInt_InPlace("addr_child", 1).dag(state);
							X_Bool("addr_parent", 0)(state);
							ShiftRight_InPlace("addr_parent", 1)(state);
						}
						X_Bool("addr_child", 0)(state);
						Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
						Add_UInt_UInt_InPlace(row_index, "addr_parent").dag(state);
						Add_Mult_UInt_ConstUInt_InPlace(column_index, pow2(k - addr_size), "addr_parent").dag(state);
						Add_ConstUInt_InPlace("addr_parent", pow2(k) - 1).dag(state);
						CombineRegister(row_index, "rotation")(state);
						ShiftLeft_InPlace(row_index, 1)(state);
					}
					ShiftRight_InPlace(row_index, 1)(state);
					RemoveRegister("addr_parent")(state);
					RemoveRegister("data_parent")(state);
					RemoveRegister("addr_child")(state);
					RemoveRegister("data_child")(state);
					RemoveRegister("div_result")(state);
					ClearZero()(state);
				}

				/**
				 * @brief U_L 电路实现（dagger，逆向）
				 * @param state 系统状态向量
				 * @details 按与 impl 相反的地址位顺序执行逆条件旋转与逆计算
				 */
				template<typename Ty>
				void impl_dag(Ty& state) const
				{
					profiler _("U_L::dag");
					AddRegister("addr_parent", UnsignedInteger, 2 * addr_size + 1)(state);
					AddRegister("addr_child", UnsignedInteger, 2 * addr_size + 1)(state);
					AddRegister("data_parent", SignedInteger, data_size)(state);
					AddRegister("data_child", SignedInteger, data_size)(state);
					AddRegister("div_result", Rational, rational_size)(state);

					ShiftLeft_InPlace(row_index, 1)(state);
					for (size_t k = 2 * addr_size - 1; k >= addr_size; --k) {
						ShiftRight_InPlace(row_index, 1)(state);
						SplitRegister(row_index, "rotation", 1)(state);
						std::get<1>(System::name_register_map[System::get("rotation")]) = Boolean;
						Add_ConstUInt_InPlace("addr_parent", pow2(k) - 1)(state);
						Add_Mult_UInt_ConstUInt_InPlace(column_index, pow2(k - addr_size), "addr_parent")(state);
						Add_UInt_UInt_InPlace(row_index, "addr_parent")(state);
						Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
						X_Bool("addr_child", 0)(state);
						if (k != 2 * addr_size - 1)
						{
							QRAMLoad(qram, "addr_child", "data_child")(state);
							QRAMLoad(qram, "addr_parent", "data_parent")(state);
							Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
							{
								profiler _("U_L::CondRot::dag");
								CondRot_Fixed_Bool("div_result", "rotation").dag(state);
							}
							ClearZero()(state);
							Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
							QRAMLoad(qram, "addr_child", "data_child")(state);
							QRAMLoad(qram, "addr_parent", "data_parent")(state);
						}
						else
						{
							ShiftLeft_InPlace("addr_parent", 1)(state);
							X_Bool("addr_parent", 0)(state);
							Add_ConstUInt_InPlace("addr_child", 1)(state);
							QRAMLoad(qram, "addr_child", "data_child")(state);
							QRAMLoad(qram, "addr_parent", "data_parent")(state);
							GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
							{
								profiler _("U_L::CondRot::dag");
								// size_t original_size = state.size();
								// fmt::print("U_L original_size {}\n", original_size);
								CondRot_Fixed_Bool("div_result", "rotation").dag(state);
							}
							ClearZero()(state);
							GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
							QRAMLoad(qram, "addr_child", "data_child")(state);
							QRAMLoad(qram, "addr_parent", "data_parent")(state);
							Add_ConstUInt_InPlace("addr_child", 1).dag(state);
							X_Bool("addr_parent", 0)(state);
							ShiftRight_InPlace("addr_parent", 1)(state);

						}
						X_Bool("addr_child", 0)(state);
						Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
						Add_UInt_UInt_InPlace(row_index, "addr_parent").dag(state);
						Add_Mult_UInt_ConstUInt_InPlace(column_index, pow2(k - addr_size), "addr_parent").dag(state);
						Add_ConstUInt_InPlace("addr_parent", pow2(k) - 1).dag(state);
						CombineRegister(row_index, "rotation")(state);

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

			/*
			Block-encoding of A: U_A
			U_L|col>|0>=|col>|a_{col}>
			U_R|0>=|A>=\sum_{i}{\Vert a_i \Vert |i>}
			a_i:i-th column of A
			U_A=SWAP(col,row) * U_R^{dag}(col) * U_L(row,col)

			U_A|\phi>_{col}|0>_{row}=A|\phi>_{col}|0>_{row}+|psi^{\perp}>
			or
			<i|_col<0|_row U_A |j>_col|0>_row=A_{ij}⟩
			*/
			/**
			 * @brief 基于 QRAM 的矩阵块编码算子 U_A
			 * @details 组合 U_A = SWAP(row, col) · U_R†(col) · U_L(row, col)，满足块编码定义
			 *          U_A|φ⟩_col|0⟩_row = A|φ⟩_col|0⟩_row + |ψ⊥⟩，
			 *          即 ⟨i|_col⟨0|_row U_A |j⟩_col|0⟩_row = A_ij（编码尺度由 QRAM 树
			 *          根结点存储的归一化因子决定）。支持条件控制（ClassControllable）
			 */
			struct Block_Encoding_via_QRAM : BaseOperator
			{
				/** @brief 列索引寄存器名称 */
				std::string column_index;
				/** @brief 行索引寄存器名称 */
				std::string row_index;
				/** @brief 单侧地址寄存器位宽 */
				size_t addr_size;
				/** @brief 数据寄存器位宽（定点量化位数） */
				size_t data_size;
				/** @brief 有理数（旋转角）寄存器位宽 */
				size_t rational_size;
				/** @brief QRAM 电路指针（存储矩阵 A 的树结构） */
				qram_qutrit::QRAMCircuit* qram;
				ClassControllable
				/**
				 * @brief 构造函数
				 * @param qram_ QRAM 电路指针
				 * @param column_index_ 列索引寄存器名称
				 * @param row_index_ 行索引寄存器名称
				 * @param dsz 数据寄存器位宽
				 * @param rsz 有理数寄存器位宽
				 */
				Block_Encoding_via_QRAM(qram_qutrit::QRAMCircuit* qram_,
					std::string_view column_index_,
					std::string_view row_index_,
					size_t dsz,
					size_t rsz) : qram(qram_), column_index(column_index_),
				row_index(row_index_), addr_size(System::size_of(row_index_)),
				data_size(dsz), rational_size(rsz)
				{
					//QRAMLoad::version = qram_version;
				};

				/**
				 * @brief 块编码电路实现（正向）：U_L → U_R† → SWAP
				 * @param state 系统状态向量
				 */
				template<typename Ty>
				void impl(Ty& state) const
				{
					profiler _("Block_Encoding_via_QRAM");

					SPLIT_BY_CONDITIONS
					{
						U_L(qram, row_index, column_index, data_size, rational_size)(state);
						U_R(qram, column_index, data_size, rational_size).dag(state);
						Swap_General_General(row_index, column_index)(state);
					}
					MERGE_BY_CONDITIONS
				}

				/**
				 * @brief 块编码电路实现（dagger）：SWAP → U_R → U_L†
				 * @param state 系统状态向量
				 */
				template<typename Ty>
				void impl_dag(Ty& state) const
				{
					profiler _("Block_Encoding_via_QRAM::dag");

					SPLIT_BY_CONDITIONS
					{
						Swap_General_General(row_index, column_index)(state);
						U_R(qram, column_index, data_size, rational_size)(state);
						U_L(qram, row_index, column_index, data_size, rational_size).dag(state);
					}
					MERGE_BY_CONDITIONS
				}

				COMPOSITE_OPERATION
			};

		}
	}
}
