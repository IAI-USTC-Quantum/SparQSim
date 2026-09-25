/**
 * @file qda_via_QRAM.h
 * @brief QRAM-based general-matrix QDA linear-system solver
 * @details Instantiates the generic single-step discrete adiabatic walk Walk_s from
 *          qda_fundamental.h for the QRAM setting: the block encoding of the matrix A
 *          uses Block_Encoding_via_QRAM (data from the qram_A hierarchy tree), and the
 *          encoding of the right-hand side b uses State_Prep_via_QRAM (data from the
 *          qram_b hierarchy tree). Includes the template version with customizable b
 *          encoding (Walk_s_via_QRAM_A), the standard version (Walk_s_via_QRAM), the
 *          debug version (Walk_s_via_QRAM_Debug), and the complete multi-step solving
 *          sequence (WalkSequence_via_QRAM_Debug, with per-step fidelity statistics).
 *          The corresponding Python implementation is pysparq.algorithms.qda_solver;
 *          the C++ experiment entry point is Experiments/QDA
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
		 * @brief QRAM-based QDA solver
		 */
		namespace QDA_via_QRAM {
			using namespace block_encoding::block_encoding_via_QRAM;
			using namespace state_prep;

			/**
			 * @brief Single-step walk for the QRAM setting (template version with
			 *        customizable b encoding)
			 * @details The matrix block encoding is fixed to Block_Encoding_via_QRAM (data
			 *          from qram_A); the right-hand side encoding type is specified by the
			 *          template parameter Encb_type
			 * @tparam Encb_type Encoding operator type of the right-hand side b
			 */
			template<typename Encb_type>
			struct Walk_s_via_QRAM_A : Walk_s<Block_Encoding_via_QRAM, Encb_type>
			{
				/** @brief Pointer to the QRAM circuit of matrix A (hierarchy tree data) */
				qram_qutrit::QRAMCircuit* qram_A;
				/** @brief Data register bit width */
				size_t data_size;
				/** @brief Rational register bit width */
				size_t rational_size;
				/** @brief Matrix block encoding type */
				using EncA = Block_Encoding_via_QRAM;
				/** @brief Right-hand side encoding type */
				using Encb = Encb_type;

				/**
				 * @brief Constructor
				 * @param qram_A_ Pointer to the QRAM circuit of matrix A
				 * @param encb_ Right-hand side encoding operator instance
				 * @param main_reg_ Main register name
				 * @param anc_UA_ Block encoding ancilla register name
				 * @param anc_1_ Ancilla register 1 name
				 * @param anc_2_ Ancilla register 2 name
				 * @param anc_3_ Ancilla register 3 name
				 * @param anc_4_ Ancilla register 4 name
				 * @param s_ Interpolation parameter s ∈ [0, 1]
				 * @param kappa_ Condition number κ
				 * @param p_ Success probability parameter
				 * @param dsz Data register bit width
				 * @param rsz Rational register bit width
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
			 * @brief Standard single-step walk for the QRAM setting
			 * @details The matrix block encoding uses Block_Encoding_via_QRAM (qram_A), and
			 *          the right-hand side encoding uses State_Prep_via_QRAM (qram_b, QRAM
			 *          state preparation of a classical distribution)
			 */
			struct Walk_s_via_QRAM : Walk_s<Block_Encoding_via_QRAM, State_Prep_via_QRAM>
			{
				/** @brief Pointer to the QRAM circuit of matrix A */
				qram_qutrit::QRAMCircuit* qram_A;
				/** @brief Pointer to the QRAM circuit of the right-hand side b */
				qram_qutrit::QRAMCircuit* qram_b;
				/** @brief Data register bit width */
				size_t data_size;
				/** @brief Rational register bit width */
				size_t rational_size;

				/** @brief Matrix block encoding type */
				using EncA = Block_Encoding_via_QRAM;
				/** @brief Right-hand side encoding type */
				using Encb = State_Prep_via_QRAM;

				/**
				 * @brief Constructor
				 * @param qram_A_ Pointer to the QRAM circuit of matrix A
				 * @param qram_b_ Pointer to the QRAM circuit of the right-hand side b
				 * @param main_reg_ Main register name
				 * @param anc_UA_ Block encoding ancilla register name
				 * @param anc_1_ Ancilla register 1 name
				 * @param anc_2_ Ancilla register 2 name
				 * @param anc_3_ Ancilla register 3 name
				 * @param anc_4_ Ancilla register 4 name
				 * @param s_ Interpolation parameter s ∈ [0, 1]
				 * @param kappa_ Condition number κ
				 * @param p_ Success probability parameter
				 * @param dsz Data register bit width
				 * @param rsz Rational register bit width
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
			 * @brief Debug version of the QDA walk for the QRAM setting
			 * @details Extends Walk_s_via_QRAM with a QDADebugger: holds classical copies of
			 *          the matrix/vector for fidelity comparison against the ideal
			 *          intermediate eigenstate
			 */
			struct Walk_s_via_QRAM_Debug : public Walk_s_via_QRAM, QDADebugger
			{
				/**
				 * @brief Constructor
				 * @param qram_A_ Pointer to the QRAM circuit of matrix A
				 * @param qram_b_ Pointer to the QRAM circuit of the right-hand side b
				 * @param matrix_A_ Classical matrix copy (for fidelity comparison)
				 * @param vector_b_ Classical right-hand side copy (for fidelity comparison)
				 * @param main_reg_ Main register name
				 * @param anc_UA_ Block encoding ancilla register name
				 * @param anc_1_ Ancilla register 1 name
				 * @param anc_2_ Ancilla register 2 name
				 * @param anc_3_ Ancilla register 3 name
				 * @param anc_4_ Ancilla register 4 name
				 * @param s_ Interpolation parameter s ∈ [0, 1]
				 * @param kappa_ Condition number κ
				 * @param p_ Success probability parameter
				 * @param is_PD Whether the matrix is positive definite (selects the H(s)
				 *        construction path)
				 * @param dsz Data register bit width
				 * @param rsz Rational register bit width
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
			 * @brief Complete solving sequence of QDA for the QRAM setting (debug driver)
			 * @details Executes the single-step walk with s = n/steps step by step and
			 *          clears zero-amplitude branches; every few steps, reads out the
			 *          intermediate state with GetOutput and compares it against the ideal
			 *          eigenstate (QDADebugger::get_mid_eigenstate) for fidelity, appending
			 *          statistics such as progress/fidelity/maximum register size to the two
			 *          files stdout_filename and fidelity_filename (the latter generated
			 *          from the stdout filename by replacing "stdout" with "fidelity")
			 */
			struct WalkSequence_via_QRAM_Debug
			{
				/** @brief Total number of discrete adiabatic steps */
				size_t steps;
				/** @brief Condition number κ */
				double kappa;
				/** @brief Success probability parameter */
				double p;
				/** @brief Main register name */
				std::string main_reg;
				/** @brief Block encoding ancilla register name */
				std::string anc_UA;
				/** @brief Names of ancilla registers 1-4 */
				std::string anc_1;
				std::string anc_2;
				std::string anc_3;
				std::string anc_4;
				/** @brief Pointer to the QRAM circuit of matrix A */
				qram_qutrit::QRAMCircuit* qram_A;
				/** @brief Pointer to the QRAM circuit of the right-hand side b */
				qram_qutrit::QRAMCircuit* qram_b;
				/** @brief Classical matrix copy (for fidelity comparison) */
				DenseMatrix<double> matrix_A;
				/** @brief Classical right-hand side copy (for fidelity comparison) */
				DenseVector<double> vector_b;
				/** @brief Data register bit width */
				size_t data_size;
				/** @brief Rational register bit width */
				size_t rational_size;
				/** @brief Run-statistics output filename (the fidelity filename is derived from it) */
				std::string stdout_filename;

				/**
				 * @brief Constructor
				 * @param qram_A_ Pointer to the QRAM circuit of matrix A
				 * @param qram_b_ Pointer to the QRAM circuit of the right-hand side b
				 * @param matrix_A Classical matrix copy
				 * @param vector_b Classical right-hand side copy
				 * @param main_reg_ Main register name
				 * @param anc_UA_ Block encoding ancilla register name
				 * @param anc_1_ Ancilla register 1 name
				 * @param anc_2_ Ancilla register 2 name
				 * @param anc_3_ Ancilla register 3 name
				 * @param anc_4_ Ancilla register 4 name
				 * @param steps_ Total number of discrete adiabatic steps
				 * @param kappa_ Condition number κ
				 * @param p_ Success probability parameter
				 * @param dsz Data register bit width
				 * @param rsz Rational register bit width
				 * @param stdout_filename_ Run-statistics output filename
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
				 * @brief Execute the complete discrete adiabatic sequence (forward)
				 * @tparam Ty State type
				 * @param state System state
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
				 * @brief Execute the inverse (dagger) of the discrete adiabatic sequence
				 * @details Applies the dagger of each step's walk in reverse order (s goes
				 *          from 1 back to 0)
				 * @tparam Ty State type
				 * @param state System state
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
