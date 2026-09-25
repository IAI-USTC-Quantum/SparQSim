/**
 * @file block_encoding_tridiagonal.h
 * @brief Quantum block encoding of tridiagonal matrices
 * @details Implements the block encoding of the symmetric tridiagonal matrix A = αI + βT
 *          (T is the shift matrix whose sub- and super-diagonals are all 1). Based on the
 *          LCU (linear combination of unitaries) decomposition A = αI + βU₊ + βU₋: after the
 *          ancilla register prepares the LCU amplitudes, the conditional shift gates
 *          (PlusOneAndOverflow) execute the +1/-1 shift branches, and the final unitary U
 *          satisfies (⟨0|_{anc}⊗I) U (|0|_{anc}⊗I) = (αI + βU₊ + βU₋)/‖A‖_F.
 *          This block encoding is a core submodule of the tridiagonal version
 *          (qda_tridiagonal.h) of the QDA discrete adiabatic solver
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
			 * @brief Modular shift gate that increments by one and records overflow
			 * @details Performs a +1 operation on the main register: when the main register
			 *          reaches the maximum value 2^n - 1 it wraps around to 0 and flips the
			 *          overflow bit. This gate corresponds to the action of the shift matrices
			 *          U₊/U₋ and is the basic building block for constructing the conditional
			 *          shift branches in the tridiagonal block encoding. Supports conditional
			 *          control (ClassControllable)
			 */
			struct PlusOneAndOverflow : BaseOperator
			{
				using BaseOperator::operator();
				using BaseOperator::dag;

				ClassControllable
				/** @brief Name of the main register to be shifted */
				std::string main_reg;
				/** @brief Overflow-bit register name (flipped when wrap-around occurs) */
				std::string overflow;
				/**
				 * @brief Constructor
				 * @param main_reg_ Main register name
				 * @param overflow_ Overflow-bit register name
				 */
				PlusOneAndOverflow(std::string_view main_reg_, std::string_view overflow_) :
					main_reg(main_reg_), overflow(overflow_) {}
				/**
				 * @brief Apply the increment-by-one shift operation
				 * @param state System state vector
				 */
				void operator()(std::vector<System>& state) const;
				/**
				 * @brief Apply the dagger operation (decrement-by-one shift)
				 * @param state System state vector
				 */
				void dag(std::vector<System>& state) const;
#ifdef USE_CUDA
				/**
				 * @brief CUDA apply of the increment-by-one shift operation
				 * @param s CUDA sparse state
				 */
				void operator()(CuSparseState& s) const;
				/**
				 * @brief CUDA apply of the dagger operation (decrement-by-one shift)
				 * @param s CUDA sparse state
				 */
				void dag(CuSparseState& s) const;
#endif
			};

			/**
			 * @brief Block encoding operator for the tridiagonal matrix A = αI + βT
			 * @details Decomposes A = αI + βU₊ + βU₋ as an LCU: on the 4-qubit ancilla register
			 *          anc_UA it prepares the amplitude vector
			 *          prep_state = {√|α|/s, √|β|/s, √|β|/s, √(1-(|α|+2|β|)/s)},
			 *          where s = ‖A‖_F = sqrt(N|α|² + 2(N-1)|β|²) is the Frobenius norm
			 *          (N = 2^n is the main register dimension). The branches respectively perform
			 *          the identity / +1 shift / -1 shift / annihilation operations, so that the
			 *          unitary U satisfies the block encoding definition
			 *          (⟨0|_{anc}⊗I) U (|0|_{anc}⊗I) = (αI + βU₊ + βU₋)/s,
			 *          i.e., the encoding scale factor is s. When β < 0 an additional conditional
			 *          reflection is inserted to correct the sign of the shift branches.
			 *          Supports conditional control (ClassControllable)
			 */
			struct Block_Encoding_Tridiagonal : BaseOperator
			{
				/** @brief Diagonal-entry coefficient α */
				double alpha;
				/** @brief Off-diagonal-entry coefficient β */
				double beta;
				/** @brief Main register name */
				std::string main_reg;
				/** @brief Block encoding ancilla register name (4 qubits) */
				std::string anc_UA;
				// std::vector<complex_t> matrix_elements;
				//DenseMatrix<complex_t> mat;
				/** @brief LCU state preparation amplitude vector (square roots of the branch coefficients) */
				std::vector<complex_t> prep_state;
				ClassControllable

				/**
				 * @brief Constructor (computes the LCU state preparation amplitudes)
				 * @param main_reg_ Main register name
				 * @param anc_UA_ Block encoding ancilla register name (4 qubits)
				 * @param alpha_ Diagonal-entry coefficient α
				 * @param beta_ Off-diagonal-entry coefficient β
				 * @note The concrete implementation of the amplitude computation is in block_encoding_tridiagonal.cpp
				 */
				Block_Encoding_Tridiagonal(
					std::string_view main_reg_,
					std::string_view anc_UA_,
					double alpha_,
					double beta_);

				/**
				 * @brief Block encoding circuit implementation (forward)
				 * @param state System state vector
				 * @details Flow: split the ancilla register → LCU state preparation → conditional ±1
				 *          shift (with an additional reflection to correct the sign when β < 0) →
				 *          annihilation branch → inverse state preparation and register merging
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
				 * @brief Block encoding circuit implementation (dagger, reverse)
				 * @param state System state vector
				 * @details Same circuit composition as impl, but with the execution order of the
				 *          shift branches and the order of the reflections reversed
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
			 * @brief Extract the encoded block matrix of the tridiagonal block encoding (numerical
			 *        verification helper)
			 * @param qubit_num Number of qubits n of the main register
			 * @param alpha Diagonal-entry coefficient α
			 * @param beta Off-diagonal-entry coefficient β
			 * @return Real matrix of the encoded block (⟨0|_{anc}⊗I) U (|0|_{anc}⊗I) with dimension
			 *         2^n × 2^n; its theoretical value is (αI + βT)/‖αI + βT‖_F
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
			 * @brief Construct the classical tridiagonal matrix αI + βT
			 * @param alpha Diagonal-entry coefficient α
			 * @param beta Off-diagonal-entry coefficient β
			 * @param dim Matrix dimension
			 * @return dim × dim tridiagonal matrix with α on the main diagonal and β on the
			 *         off-diagonals
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
			 * @brief Construct the down-shift matrix U₊ (U₊[i, i-1] = 1, i.e., the sub-diagonal is 1)
			 * @tparam Ty Matrix element type
			 * @param size Matrix dimension
			 * @return size × size down-shift matrix
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
			 * @brief Construct the up-shift matrix U₋ (U₋[i, i+1] = 1, i.e., the super-diagonal is 1)
			 * @tparam Ty Matrix element type
			 * @param size Matrix dimension
			 * @return size × size up-shift matrix
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