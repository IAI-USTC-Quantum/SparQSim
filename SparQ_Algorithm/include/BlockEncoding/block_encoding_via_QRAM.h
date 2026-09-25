/**
 * @file block_encoding_via_QRAM.h
 * @brief QRAM-based block encoding of arbitrary matrices
 * @details Constructs the block encoding of a matrix A via the U_L / U_R quantum walk
 *          decomposition: U_L|col⟩|0⟩ = |col⟩|a_col⟩ prepares the normalized column vector
 *          indexed by the column index, U_R|0⟩ = |A⟩ = Σ_i ‖a_i‖|i⟩ prepares the column-norm
 *          distribution, and their combination U_A = SWAP(row, col) · U_R†(col) · U_L(row, col)
 *          satisfies ⟨i|_col⟨0|_row U_A |j⟩_col|0⟩_row = A_ij, with the data provided by the
 *          QRAM hierarchy tree nodes.
 *          Used together with make_qram.h (data quantization and tree construction)
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
			 * @brief Right-multiplication operator U_R (column-norm state preparation)
			 * @details Implements U_R|0⟩ = |A⟩ = Σ_i ‖a_i‖|i⟩ (a_i is the i-th column of A):
			 *          iterate over the column address register bit by bit from the most
			 *          significant to the least significant bit, use the parent/child node values
			 *          of the QRAM loading tree, compute the rotation angle
			 *          arccos(√(child/parent)) with Div_Sqrt_Arccos_UInt_UInt and perform a
			 *          conditional rotation (CondRot_Fixed_Bool), preparing the normalized
			 *          column-norm superposition state from the tree root downward; after each
			 *          step the ancilla registers are cleaned up by uncomputing step by step.
			 *          Supports conditional control (ClassControllable)
			 */
			struct U_R : BaseOperator
			{
				/** @brief Column index register name */
				std::string column_index;
				/** @brief Column address register width */
				size_t addr_size;
				/** @brief Data register width (fixed-point quantization bit count) */
				size_t data_size;
				/** @brief Rational (rotation angle) register width */
				size_t rational_size;
				/** @brief QRAM circuit pointer (stores the column-norm tree) */
				qram_qutrit::QRAMCircuit* qram;

				ClassControllable

				/**
				 * @brief Constructor
				 * @param qram_ QRAM circuit pointer
				 * @param column_index_ Column index register name
				 * @param dsz Data register width
				 * @param rsz Rational register width
				 */
				U_R(qram_qutrit::QRAMCircuit* qram_,
					std::string_view column_index_,
					size_t dsz,
					size_t rsz) : column_index(column_index_), addr_size(System::size_of(column_index_)), data_size(dsz), rational_size(rsz), qram(qram_)
				{}

				/**
				 * @brief U_R circuit implementation (forward)
				 * @param state System state vector
				 * @details One iteration per address bit: split off the rotation bit → concatenate
				 *          the parent/child addresses → QRAM-load the node values → compute the
				 *          rotation angle and rotate conditionally → uncompute to restore the
				 *          address and data registers
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
				 * @brief U_R circuit implementation (dagger, reverse)
				 * @param state System state vector
				 * @details Executes the inverse conditional rotations and uncomputations in the
				 *          address-bit order opposite to impl
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
			 * @brief Left-multiplication operator U_L (prepares the normalized column vector
			 *        indexed by the column index)
			 * @details Implements U_L|col⟩|0⟩ = |col⟩|a_col⟩ (|a_col⟩ is the normalized quantum
			 *          state corresponding to column col): iterate over the top addr_size bits of
			 *          the row address register; each step concatenates the row/column indices
			 *          into the parent/child addresses of the QRAM tree nodes
			 *          (addr_child = 2·addr_parent + 1), loads the node values, computes the
			 *          rotation angle and rotates conditionally while descending layer by layer;
			 *          the last layer (the leaf layer) instead uses GetRotateAngle_Int_Int with
			 *          an atan2-type angle to handle signs.
			 *          Supports conditional control (ClassControllable)
			 */
			struct U_L : BaseOperator
			{
				/** @brief Row index register name */
				std::string row_index;
				/** @brief Column index register name */
				std::string column_index;
				/** @brief One-sided address register width */
				size_t addr_size;
				/** @brief Data register width (fixed-point quantization bit count) */
				size_t data_size;
				/** @brief Rational (rotation angle) register width */
				size_t rational_size;
				/** @brief QRAM circuit pointer (stores the tree structure of matrix A) */
				qram_qutrit::QRAMCircuit* qram;

				ClassControllable

				/**
				 * @brief Constructor
				 * @param qram_ QRAM circuit pointer
				 * @param row_index_ Row index register name
				 * @param column_index_ Column index register name
				 * @param dsz Data register width
				 * @param rsz Rational register width
				 */
				U_L(qram_qutrit::QRAMCircuit* qram_,
					std::string_view row_index_,
					std::string_view column_index_,
					size_t dsz,
					size_t rsz) : row_index(row_index_), column_index(column_index_), addr_size(System::size_of(row_index_)), data_size(dsz), rational_size(rsz), qram(qram_)
				{}

				/**
				 * @brief U_L circuit implementation (forward)
				 * @param state System state vector
				 * @details Bit-by-bit iteration: split off the rotation bit → concatenate the
				 *          parent/child addresses → QRAM load → conditional rotation → uncompute
				 *          to restore the addresses; non-final layers use Div_Sqrt_Arccos, the
				 *          final layer uses GetRotateAngle
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
				 * @brief U_L circuit implementation (dagger, reverse)
				 * @param state System state vector
				 * @details Executes the inverse conditional rotations and uncomputations in the
				 *          address-bit order opposite to impl
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
			 * @brief QRAM-based matrix block encoding operator U_A
			 * @details Combines U_A = SWAP(row, col) · U_R†(col) · U_L(row, col), which satisfies
			 *          the block encoding definition
			 *          U_A|φ⟩_col|0⟩_row = A|φ⟩_col|0⟩_row + |ψ⊥⟩,
			 *          i.e., ⟨i|_col⟨0|_row U_A |j⟩_col|0⟩_row = A_ij (the encoding scale is
			 *          determined by the normalization factor stored at the QRAM tree root).
			 *          Supports conditional control (ClassControllable)
			 */
			struct Block_Encoding_via_QRAM : BaseOperator
			{
				/** @brief Column index register name */
				std::string column_index;
				/** @brief Row index register name */
				std::string row_index;
				/** @brief One-sided address register width */
				size_t addr_size;
				/** @brief Data register width (fixed-point quantization bit count) */
				size_t data_size;
				/** @brief Rational (rotation angle) register width */
				size_t rational_size;
				/** @brief QRAM circuit pointer (stores the tree structure of matrix A) */
				qram_qutrit::QRAMCircuit* qram;
				ClassControllable
				/**
				 * @brief Constructor
				 * @param qram_ QRAM circuit pointer
				 * @param column_index_ Column index register name
				 * @param row_index_ Row index register name
				 * @param dsz Data register width
				 * @param rsz Rational register width
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
				 * @brief Block encoding circuit implementation (forward): U_L → U_R† → SWAP
				 * @param state System state vector
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
				 * @brief Block encoding circuit implementation (dagger): SWAP → U_R → U_L†
				 * @param state System state vector
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
