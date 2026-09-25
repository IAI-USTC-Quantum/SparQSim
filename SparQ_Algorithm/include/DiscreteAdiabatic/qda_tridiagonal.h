/**
 * @file qda_tridiagonal.h
 * @brief Tridiagonal-matrix version of the QDA linear-system solver
 * @details Instantiates the generic single-step discrete adiabatic walk Walk_s from
 *          qda_fundamental.h for the tridiagonal setting: the block encoding of the
 *          matrix A = αI + βT uses Block_Encoding_Tridiagonal, and the encoding of the
 *          right-hand side b uses Hadamard_Int_Full (uniform distribution).
 *          Provides Walk_s_Tridiagonal (standard version) and Walk_s_Tridiagonal_Debug
 *          (debug version with fidelity comparison). The corresponding Python
 *          implementation is pysparq.algorithms.qda_solver; the C++ experiment entry
 *          point is Experiments/QDA
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
	 * @brief Discrete adiabatic (QDA) linear-system solver
	 */
	namespace QDA {
		/**
		 * @namespace qram_simulator::QDA::QDA_tridiagonal
		 * @brief Tridiagonal-matrix version of the QDA solver
		 */
		namespace QDA_tridiagonal {

			/**
			 * @brief Single-step discrete adiabatic walk for the tridiagonal setting
			 * @details Combines Block_Encoding_Tridiagonal (block encoding of A = αI + βT)
			 *          with Hadamard_Int_Full (uniform-superposition encoding of b), and
			 *          implements the single-step walk of the interpolated Hamiltonian H(s)
			 *          following the Walk_s template of qda_fundamental.h
			 */
			struct Walk_s_Tridiagonal : Walk_s<Block_Encoding_Tridiagonal, Hadamard_Int_Full>
			{
				/** @brief Matrix block encoding type */
				using EncA = Block_Encoding_Tridiagonal;
				/** @brief Right-hand side encoding type */
				using Encb = Hadamard_Int_Full;

				/**
				 * @brief Constructor
				 * @param main_reg_ Main register name
				 * @param anc_UA_ Block encoding ancilla register name
				 * @param anc_1_ Ancilla register 1 name
				 * @param anc_2_ Ancilla register 2 name
				 * @param anc_3_ Ancilla register 3 name
				 * @param anc_4_ Ancilla register 4 name
				 * @param s_ Interpolation parameter s ∈ [0, 1]
				 * @param kappa_ Condition number κ
				 * @param p_ Success probability parameter
				 * @param alpha_ Tridiagonal diagonal coefficient α
				 * @param beta_ Tridiagonal subdiagonal coefficient β
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
			 * @brief Debug version of the tridiagonal QDA walk
			 * @details Extends Walk_s_Tridiagonal with a QDADebugger: holds classical
			 *          copies of the matrix/vector for fidelity comparison against the
			 *          ideal intermediate eigenstate
			 */
			struct Walk_s_Tridiagonal_Debug : public Walk_s_Tridiagonal, QDADebugger
			{
				// size_t row_size;

				/**
				 * @brief Constructor
				 * @param matrix Classical copy of the tridiagonal matrix (for fidelity
				 *        comparison)
				 * @param vec Classical copy of the right-hand side (for fidelity comparison)
				 * @param main_reg_ Main register name
				 * @param anc_UA_ Block encoding ancilla register name
				 * @param anc_1_ Ancilla register 1 name
				 * @param anc_2_ Ancilla register 2 name
				 * @param anc_3_ Ancilla register 3 name
				 * @param anc_4_ Ancilla register 4 name
				 * @param s_ Interpolation parameter s ∈ [0, 1]
				 * @param kappa_ Condition number κ
				 * @param p_ Success probability parameter
				 * @param alpha_ Tridiagonal diagonal coefficient α
				 * @param beta_ Tridiagonal subdiagonal coefficient β
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
