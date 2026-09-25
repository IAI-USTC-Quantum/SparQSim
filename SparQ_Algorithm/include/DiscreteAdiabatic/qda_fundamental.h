/**
 * @file qda_fundamental.h
 * @brief Fundamental components of the quantum discrete adiabatic (QDA) linear-system solver
 * @details Implements the core components of the optimal-scaling quantum linear-systems
 *          solver based on the discrete adiabatic theorem (Optimal scaling quantum
 *          linear-systems solver via discrete adiabatic theorem, PRX Quantum, 2022, 3(4):
 *          040303; construction details in Appendix F of the paper): the block encoding of
 *          the interpolated Hamiltonian H(s) = (1-f(s))H₀ + f(s)H₁ (Block_Encoding_Hs /
 *          Block_Encoding_Hs_PD), the single-step quantum walk operator (Walk_s), the LCU
 *          combination of walk powers together with Dolph-Chebyshev filtering (Filtering),
 *          and fidelity debugging tools (QDADebugger / GetOutput). The interpolation
 *          parameter f(s) is taken from Eq. (69) of the paper, and the algorithm complexity
 *          is O(κ log(κ/ε)); the concrete solving procedure is assembled in
 *          qda_tridiagonal.h / qda_via_QRAM.h
 */

#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include "matrix.h"
#include "block_encoding.h"
#include "state_preparation.h"
#ifdef USE_CUDA
#include "cuda/cuda_utils.cuh"
#endif

/*****************************************************
				Quantum Walk Process
*****************************************************/
namespace qram_simulator {
	namespace QDA {
		/*
		block-encoding of H(s)
		H(s)=(1-f(s))H_0 + f(s)H_1
		reference: Optimal scaling quantum linear-systems solver via discrete adiabatic theorem[J]. PRX quantum, 2022, 3(4): 040303.
		doi: https://journals.aps.org/prxquantum/abstract/10.1103/PRXQuantum.3.040303
		Construction details: Appendix F
		*/
		/**
		 * @brief Block encoding of the interpolated Hamiltonian H(s) (general version)
		 * @details H(s) = (1-f(s))H₀ + f(s)H₁: H₀ is built from the |b⟩ state preparation
		 *          (enc_b) and a reflection on the main register, while H₁ is built from the
		 *          block encoding of A (enc_A) (circuit details in Appendix F of the paper
		 *          above). The interpolation is realized by applying the rotation matrix
		 *          R_s = [[√N(1-f), √N f], [√N f, √N(f-1)]] (√N = 1/√((1-f)²+f²))
		 *          on anc_2; combined with controlled enc_A/enc_b and the reflections at
		 *          each level, the whole forms a (⟨0|⊗I) U (|0|⊗I)-type block encoding of
		 *          H(s). Supports conditional control (ClassControllable)
		 * @tparam Block_Encoding Block encoding type of A
		 * @tparam State_Prep State preparation type of |b⟩
		 */
		template<typename Block_Encoding, typename State_Prep>
		struct Block_Encoding_Hs
		{
			/** @brief Interpolation parameter f(s) ∈ [0,1] */
			double fs;
			/** @brief Interpolation rotation matrix R_s (applied on anc_2) */
			u22_t R_s;
			/** @brief Main data register name */
			std::string main_reg;
			/** @brief Ancilla register name used by the block encoding of A */
			std::string anc_UA;
			/** @brief Ancilla register anc_1 name */
			std::string anc_1;
			/** @brief Ancilla register anc_2 name (target of the interpolation rotation) */
			std::string anc_2;
			/** @brief Ancilla register anc_3 name */
			std::string anc_3;
			/** @brief Ancilla register anc_4 name */
			std::string anc_4;
			// size_t data_size;
			// size_t rational_size;
			/** @brief Block encoding operator of matrix A */
			Block_Encoding enc_A;
			/** @brief State preparation operator of the right-hand side |b⟩ */
			State_Prep enc_b;
			ClassControllable

			/**
			 * @brief Constructor (computes the interpolation rotation matrix R_s)
			 * @param enc_A_ Block encoding operator of A
			 * @param enc_b_ State preparation operator of |b⟩
			 * @param main_reg_ Main data register name
			 * @param anc_UA_ Ancilla register name of the block encoding of A
			 * @param anc_1_ Ancilla register anc_1 name
			 * @param anc_2_ Ancilla register anc_2 name (target of the interpolation rotation)
			 * @param anc_3_ Ancilla register anc_3 name
			 * @param anc_4_ Ancilla register anc_4 name
			 * @param fs_ Interpolation parameter f(s)
			 */
			Block_Encoding_Hs(
				Block_Encoding enc_A_,
				State_Prep enc_b_,
				std::string_view main_reg_,
				std::string_view anc_UA_,
				std::string_view anc_1_,
				std::string_view anc_2_,
				std::string_view anc_3_,
				std::string_view anc_4_,
				double fs_) : enc_A(enc_A_), enc_b(enc_b_),
			main_reg(main_reg_), anc_UA(anc_UA_),
			anc_1(anc_1_), anc_2(anc_2_), anc_3(anc_3_), anc_4(anc_4_), fs(fs_)
			// data_size(dsz), rational_size(rsz)
			{
				double sqrt_N = 1.0 / sqrt((1 - fs) * (1 - fs) + fs * fs);
				double u00 = sqrt_N * (1 - fs),
					u01 = sqrt_N * fs,
					u10 = sqrt_N * fs,
					u11 = sqrt_N * (fs - 1);
				R_s = u22_t{ u00, u01, u10, u11 };
			};

			/**
			 * @brief Block encoding circuit implementation (forward)
			 * @param state System state vector
			 * @details Circuit sequence: H(anc_3) → enc_b† → main-register reflection
			 *          (controlled) → enc_b → R_s interpolation rotation (controlled on
			 *          anc_4) → H(anc_2) → controlled enc_A and reflection → inverse
			 *          interpolation rotation → second round of the enc_b reflection
			 *          sequence → H(anc_3)
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				profiler _("Block_Encoding_Hs");
				SPLIT_BY_CONDITIONS {
					Block_Encoding enc_A_copy = enc_A;

					(Hadamard_Bool(anc_3))(state);
					enc_b.dag(state);
					X_Bool(anc_1, 0)(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_1, anc_3, anc_4 })(state);
					X_Bool(anc_1, 0)(state);
					enc_b(state);
					X_Bool(anc_4, 0)(state);
					Rot_Bool(anc_2, R_s).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					Hadamard_Bool(anc_2).conditioned_by_all_ones(anc_4)(state);
					enc_A_copy.conditioned_by_all_ones({ anc_1, anc_2 })(state);
					X_Bool(anc_1, 0).conditioned_by_all_ones(anc_2)(state);
					Reflection_Bool(anc_2, true).conditioned_by_all_ones(anc_1)(state);
					enc_A_copy.conditioned_by_all_ones({ anc_1, anc_2 }).dag(state);
					X_Bool(anc_4, 0)(state);
					Hadamard_Bool(anc_2).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					Rot_Bool(anc_2, R_s).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					enc_b.dag(state);
					X_Bool(anc_1, 0)(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_1, anc_3, anc_4 })(state);
					X_Bool(anc_1, 0)(state);
					enc_b(state);
					(Hadamard_Bool(anc_3))(state);
				}
				MERGE_BY_CONDITIONS
			}

			/**
			 * @brief Block encoding circuit implementation (dagger, inverse)
			 * @param state System state vector
			 * @note The current implementation throws a runtime error right at the entry
			 *       (not yet completed); calling it fails immediately
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				throw_general_runtime_error();
				profiler _("Block_Encoding_Hs::dag");
				
				SPLIT_BY_CONDITIONS {
					Block_Encoding enc_A_copy = enc_A;

					(Hadamard_Bool(anc_3))(state);
					enc_b.dag(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_1, anc_3, anc_4 })(state);
					enc_b(state);
					X_Bool(anc_4, 0)(state);
					Rot_Bool(anc_2, R_s).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					Hadamard_Bool(anc_2).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					enc_A_copy.conditioned_by_all_ones({ anc_1, anc_2 })(state);
					Reflection_Bool(anc_2, true).conditioned_by_all_ones(anc_1)(state);
					X_Bool(anc_1, 0).conditioned_by_all_ones(anc_2)(state);
					enc_A_copy.conditioned_by_all_ones({ anc_1, anc_2 }).dag(state);
					Hadamard_Bool(anc_2).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					Rot_Bool(anc_2, R_s).conditioned_by_all_ones(anc_4)(state);
					X_Bool(anc_4, 0)(state);
					enc_b.dag(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_1, anc_3, anc_4 })(state);
					enc_b(state);
					(Hadamard_Bool(anc_3))(state);
				}
				MERGE_BY_CONDITIONS
			}

			COMPOSITE_OPERATION
		};

		/*
		block-encoding of H(s): positive-definite version
		H(s)=(1-f(s))H_0 + f(s)H_1
		reference: Optimal scaling quantum linear-systems solver via discrete adiabatic theorem[J]. PRX quantum, 2022, 3(4): 040303.
		doi: https://journals.aps.org/prxquantum/abstract/10.1103/PRXQuantum.3.040303
		Construction details: Appendix F
		*/
		/**
		 * @brief Block encoding of the interpolated Hamiltonian H(s) (positive-definite version)
		 * @details Applicable when A is a positive-definite matrix; the circuit is leaner than
		 *          the general version (some controlled layers are omitted, the interpolation
		 *          rotation R_s acts on anc_1 instead, and enc_A is controlled by
		 *          {anc_1, anc_3}); the construction of H(s) = (1-f(s))H₀ + f(s)H₁ follows
		 *          the same idea as the general version. Supports conditional control
		 *          (ClassControllable)
		 * @tparam Block_Encoding Block encoding type of A
		 * @tparam State_Prep State preparation type of |b⟩
		 */
		template<typename Block_Encoding, typename State_Prep>
		struct Block_Encoding_Hs_PD
		{
			/** @brief Interpolation parameter f(s) ∈ [0,1] */
			double fs;
			/** @brief Interpolation rotation matrix R_s (applied on anc_1) */
			u22_t R_s;
			/** @brief Main data register name */
			std::string main_reg;
			/** @brief Ancilla register name used by the block encoding of A */
			std::string anc_UA;
			/** @brief Ancilla register anc_1 name (target of the interpolation rotation) */
			std::string anc_1;
			/** @brief Ancilla register anc_2 name */
			std::string anc_2;
			/** @brief Ancilla register anc_3 name */
			std::string anc_3;
			/** @brief Ancilla register anc_4 name */
			std::string anc_4;
			// size_t data_size;
			// size_t rational_size;
			/** @brief Block encoding operator of matrix A */
			Block_Encoding enc_A;
			/** @brief State preparation operator of the right-hand side |b⟩ */
			State_Prep enc_b;
			ClassControllable

			/**
			 * @brief Constructor (computes the interpolation rotation matrix R_s)
			 * @param enc_A_ Block encoding operator of A
			 * @param enc_b_ State preparation operator of |b⟩
			 * @param main_reg_ Main data register name
			 * @param anc_UA_ Ancilla register name of the block encoding of A
			 * @param anc_1_ Ancilla register anc_1 name (target of the interpolation rotation)
			 * @param anc_2_ Ancilla register anc_2 name
			 * @param anc_3_ Ancilla register anc_3 name
			 * @param anc_4_ Ancilla register anc_4 name (unused in the positive-definite version)
			 * @param fs_ Interpolation parameter f(s)
			 */
			Block_Encoding_Hs_PD(
				Block_Encoding enc_A_,
				State_Prep enc_b_,
				std::string_view main_reg_,
				std::string_view anc_UA_,
				std::string_view anc_1_,
				std::string_view anc_2_,
				std::string_view anc_3_,//a_h
				std::string_view anc_4_,//none
				double fs_) : enc_A(enc_A_), enc_b(enc_b_),
			main_reg(main_reg_), anc_UA(anc_UA_),
			anc_1(anc_1_), anc_2(anc_2_), anc_3(anc_3_), anc_4(anc_4_), fs(fs_)
				// data_size(dsz), rational_size(rsz)
			{
				double sqrt_N = 1.0 / sqrt((1 - fs) * (1 - fs) + fs * fs);
				double u00 = sqrt_N * (1 - fs),
					u01 = sqrt_N * fs,
					u10 = sqrt_N * fs,
					u11 = sqrt_N * (fs - 1);
				R_s = u22_t{ u00, u01, u10, u11 };
			};

			/**
			 * @brief Positive-definite block encoding circuit implementation (forward)
			 * @param state System state vector
			 * @details Circuit sequence: H(anc_2) → enc_b† → main-register reflection
			 *          (controlled on {anc_2, anc_3}) → enc_b → R_s interpolation rotation
			 *          (controlled on anc_3) → H(anc_1) → controlled enc_A and enc_A† →
			 *          inverse interpolation rotation → second round of the enc_b reflection
			 *          sequence → H(anc_2)
			 */
			template<typename Ty>
			void operator()(Ty& state) const
			{
				profiler _("Block_Encoding_Hs_PD");
					
				SPLIT_BY_CONDITIONS
				{
					Block_Encoding enc_A_copy = enc_A;
					(Hadamard_Bool(anc_2))(state);
					enc_b.dag(state);					
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_2, anc_3 })(state);
					enc_b(state);
					X_Bool(anc_3, 0)(state);
					Rot_Bool(anc_1, R_s).conditioned_by_all_ones(anc_3)(state);
					X_Bool(anc_3, 0)(state);
					Hadamard_Bool(anc_1).conditioned_by_all_ones(anc_3)(state);
					enc_A_copy.conditioned_by_all_ones(std::vector<std::string>{ anc_1, anc_3 })(state);
					X_Bool(anc_3, 0)(state);
					enc_A_copy.conditioned_by_all_ones(std::vector<std::string>{ anc_1, anc_3 }).dag(state);
					Hadamard_Bool(anc_1).conditioned_by_all_ones(anc_3)(state);
					X_Bool(anc_3, 0)(state);
					Rot_Bool(anc_1, R_s).conditioned_by_all_ones(anc_3)(state);
					X_Bool(anc_3, 0)(state);
					enc_b.dag(state);
					Reflection_Bool(main_reg, true).conditioned_by_all_ones({ anc_2, anc_3 })(state);
					enc_b(state);
					(Hadamard_Bool(anc_2))(state);
				}
				MERGE_BY_CONDITIONS
			}

		};
		//bool PD = false;
		// template<typename Block_Encoding, typename State_Prep>
		/**
		 * @brief Single-step quantum walk operator W(s) at parameter s
		 * @details Single-step implementation of the discrete adiabatic evolution:
		 *          W(s) = i · R · U_H(s), where U_H(s) is the block encoding of the
		 *          interpolated Hamiltonian H(s) = (1-f(s))H₀ + f(s)H₁ and R is the
		 *          reflection over the block-encoding ancilla registers (acting on
		 *          {anc_UA, anc_1, anc_2} when PD = true, otherwise on
		 *          {anc_UA, anc_2, anc_3}); the whole is then multiplied by the global
		 *          phase i. The interpolation parameter is computed according to Eq. (69)
		 *          of the paper: fs = κ/(κ-1) · (1 - (1 + s(κ^(p-1) - 1))^(1/(1-p))).
		 *          Supports conditional control (ClassControllable)
		 * @tparam Block_Encoding Block encoding type of A
		 * @tparam State_Prep State preparation type of |b⟩
		 * @tparam PD Whether to use the reflection register set of the positive-definite
		 *            variant
		 * @note PD only switches the registers used by the reflection; enc_Hs always uses
		 *       the general block encoding Block_Encoding_Hs (see the commented-out
		 *       conditional type alias EncHs for switching to the positive-definite block
		 *       encoding)
		 */
		template<typename Block_Encoding, typename State_Prep, bool PD = false>
		struct Walk_s
		{
			/** @brief Adiabatic evolution discretization parameter s ∈ [0,1] */
			double s;
			/** @brief Condition number κ of the linear system */
			double kappa;
			/** @brief Adiabatic schedule parameter p */
			double p;
			/** @brief Interpolation parameter f(s) (computed from s, κ, p via Eq. (69) of the paper) */
			double fs;
			// double temp;
			/** @brief Global phase factor (default i) */
			complex_t phase = complex_t(0, 1.0);
			/** @brief Main data register name */
			std::string main_reg;
			/** @brief Ancilla register name used by the block encoding of A */
			std::string anc_UA;
			/** @brief Ancilla register anc_1 name */
			std::string anc_1;
			/** @brief Ancilla register anc_2 name */
			std::string anc_2;
			/** @brief Ancilla register anc_3 name */
			std::string anc_3;
			/** @brief Ancilla register anc_4 name */
			std::string anc_4;
			/** @brief Block encoding operator of matrix A */
			Block_Encoding enc_A;
			/** @brief State preparation operator of the right-hand side |b⟩ */
			State_Prep enc_b;
			/** @brief Positive-definite variant flag (compile-time constant from template parameter PD) */
			constexpr static bool is_positive_definite = PD;
			/** @brief Type alias of the H(s) block encoding */
			using EncHs = Block_Encoding_Hs<Block_Encoding, State_Prep>;
			//using EncHs = std::conditional<PD, Block_Encoding_Hs_PD<Block_Encoding, State_Prep>, Block_Encoding_Hs<Block_Encoding, State_Prep>>;
			/** @brief Block encoding operator instance of H(s) */
			EncHs enc_Hs;

			ClassControllable

			/**
			 * @brief Constructor (internally derives f(s) and assembles the H(s) block encoding)
			 * @param enc_A_ Block encoding operator of A
			 * @param enc_b_ State preparation operator of |b⟩
			 * @param main_reg_ Main data register name
			 * @param anc_UA_ Ancilla register name of the block encoding of A
			 * @param anc_1_ Ancilla register anc_1 name
			 * @param anc_2_ Ancilla register anc_2 name
			 * @param anc_3_ Ancilla register anc_3 name
			 * @param anc_4_ Ancilla register anc_4 name
			 * @param s_ Adiabatic evolution discretization parameter s ∈ [0,1]
			 * @param kappa_ Condition number κ
			 * @param p_ Adiabatic schedule parameter p
			 */
			Walk_s(Block_Encoding enc_A_,
				State_Prep enc_b_,
				std::string_view main_reg_,
				std::string_view anc_UA_,
				std::string_view anc_1_,
				std::string_view anc_2_,
				std::string_view anc_3_,
				std::string_view anc_4_,
				double s_,
				double kappa_,
				double p_) :
			main_reg(main_reg_), anc_UA(anc_UA_),
			anc_1(anc_1_), anc_2(anc_2_), anc_3(anc_3_), anc_4(anc_4_),
			s(s_), kappa(kappa_), p(p_), 
			enc_A(enc_A_), enc_b(enc_b_),
			/* fs (Eq. (69)) page 11 */
			fs(kappa_ / (kappa_ - 1) * (1 - pow(1 + s_ * (pow(kappa_, p_ - 1) - 1), 1 / (1 - p_)))),
			enc_Hs(enc_A_, enc_b_, main_reg, anc_UA, anc_1, anc_2, anc_3, anc_4, fs)
			{
			};

			/**
			 * @brief Single-step walk circuit implementation (forward): H(s) block encoding →
			 *        reflection → global phase
			 * @param state System state vector
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				profiler _("Walk_s");

				SPLIT_BY_CONDITIONS
				{
					(EncHs(enc_Hs))(state);

					if constexpr (!is_positive_definite)
						Reflection_Bool({ anc_UA, anc_2, anc_3 }, false)(state);
					else {
						Reflection_Bool({ anc_UA, anc_1, anc_2 }, false)(state);
					}
				}
				MERGE_BY_CONDITIONS

				(GlobalPhase(phase))(state);
			}

			/**
			 * @brief Single-step walk circuit implementation (dagger, inverse): global phase⁻¹ →
			 *        reflection → H(s) block encoding†
			 * @param state System state vector
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				profiler _("Walk_s::dag");

				(GlobalPhase(-phase))(state);

				SPLIT_BY_CONDITIONS
				{
					if constexpr (!is_positive_definite) {
						Reflection_Bool({ anc_UA, anc_2, anc_3 }, false)(state);
					}
					else {
						Reflection_Bool({ anc_UA, anc_1, anc_2 }, false)(state);
					}

					(EncHs(enc_Hs)).dag(state);
				}
				MERGE_BY_CONDITIONS
			}

			COMPOSITE_OPERATION
		};


		/**
		 * @brief QDA classical reference-solution debugger
		 * @details Stores the original matrix A and the right-hand side b, and computes the
		 *          ideal results of each stage of the discrete adiabatic evolution on the
		 *          classical side: the Hermitian extended interpolation matrix A_f, the
		 *          ideal initial states |0⟩⊗|b⟩ and |1⟩⊗|b⟩, and the ideal eigenstate at
		 *          intermediate times (obtained via a classical linear solver); used for
		 *          fidelity comparison against the quantum simulation results
		 */
		struct QDADebugger
		{
			/** @brief Original matrix A */
			DenseMatrix<double> matrix_A;
			/** @brief Original right-hand side vector b */
			DenseVector<double> vector_b;
			/** @brief Interpolation parameter f(s) */
			double fs;
			/** @brief Dimension of b */
			size_t row_size;

			/**
			 * @brief Constructor (computes f(s) from s, κ, p via Eq. (69))
			 * @param matrix_A_ Original matrix A
			 * @param vector_b_ Original right-hand side vector b
			 * @param s_ Adiabatic evolution discretization parameter s
			 * @param kappa_ Condition number κ
			 * @param p_ Adiabatic schedule parameter p
			 */
			QDADebugger(
				const DenseMatrix<double>& matrix_A_,
				const DenseVector<double>& vector_b_,
				double s_,
				double kappa_,
				double p_
			) :
				matrix_A(matrix_A_),
				vector_b(vector_b_),
				fs(kappa_ / (kappa_ - 1) * (1 - pow(1 + s_ * (pow(kappa_, p_ - 1) - 1), 1 / (1 - p_)))),
				row_size(vector_b_.size)
			{}

			//void init_eigenstate(std::vector<System>& state);

			/**
			 * @brief Compute the Hermitian extended interpolation matrix A_f
			 * @return 2n×2n matrix [[(1-f)I, fA], [fA†, -(1-f)I]]
			 */
			DenseMatrix<double> get_matrix_Af();
			/**
			 * @brief Ideal initial state vector |0⟩⊗|b⟩ (extended space)
			 * @return 2n-dimensional vector whose first n components are b and last n are 0
			 */
			DenseVector<double> get_vector_0b();
			/**
			 * @brief Ideal vector |1⟩⊗|b⟩ (extended space)
			 * @return 2n-dimensional vector whose first n components are 0 and last n are b
			 */
			DenseVector<double> get_vector_1b();
			/**
			 * @brief Compute the ideal eigenstate at intermediate time s (fidelity reference
			 *        state)
			 * @param is_PD Whether this is the positive-definite case (unused in the current
			 *        implementation)
			 * @return Real vector of length 4n (zero-padded according to the main register +
			 *         ancilla layout, for direct comparison with the quantum state)
			 * @details Returns the initial state |0⟩⊗|b⟩ when f(s) ≈ 0; returns the
			 *          normalized solution of A x = b (placed in the |1⟩ branch) when
			 *          f(s) ≈ 1; otherwise solves A_f y = (|0⟩⊗|b⟩) and returns its
			 *          normalized solution as the intermediate eigenstate
			 */
			std::vector<double> get_mid_eigenstate(bool is_PD=false);
			//std::vector<double> get_matrix_dag();
		};

		/**
		 * @brief Post-selection readout operator
		 * @details Filters out, from the final evolved state, the branches in which every
		 *          specified ancilla register (anc_registers) takes value 0, and returns the
		 *          normalized amplitude vector (main register + anc_1 + anc_4 layout)
		 *          together with the success probability (sum of the weights of the matching
		 *          branches); used to read out the solution of the discrete adiabatic
		 *          evolution and verify its fidelity
		 */
		struct GetOutput {
			/** @brief Main register ID */
			size_t main_reg;
			/** @brief Ancilla register ID of the block encoding of A */
			size_t anc_UA;
			/** @brief Ancilla register anc_4 ID */
			size_t anc_4;
			/** @brief Ancilla register anc_3 ID */
			size_t anc_3;
			/** @brief Ancilla register anc_2 ID */
			size_t anc_2;
			/** @brief Ancilla register anc_1 ID */
			size_t anc_1;
			/** @brief LCU index register ID (used by the filtering procedure) */
			size_t index;
			/** @brief Filtering ancilla register anc_h ID */
			size_t anc_h;
			/** @brief List of ancilla register IDs participating in the post-selection */
			std::vector<size_t> anc_registers;
			//std::vector<double> weights;
			/**
			 * @brief Constructor (basic version, post-selects {anc_UA, anc_3, anc_2})
			 * @param main_reg Main register name
			 * @param anc_UA Ancilla register name of the block encoding of A
			 * @param anc_4 Ancilla register anc_4 name
			 * @param anc_3 Ancilla register anc_3 name
			 * @param anc_2 Ancilla register anc_2 name
			 * @param anc_1 Ancilla register anc_1 name
			 * @param is_PD Positive-definite mode flag (unused in the current implementation;
			 *        both modes post-select the same set of registers)
			 */
			GetOutput(
				std::string main_reg,
				std::string anc_UA,
				std::string anc_4,
				std::string anc_3,
				std::string anc_2,
				std::string anc_1,
				bool is_PD=false) :
				main_reg(System::get(main_reg)), anc_UA(System::get(anc_UA)),
				anc_4(System::get(anc_4)), anc_3(System::get(anc_3)), anc_2(System::get(anc_2)), anc_1(System::get(anc_1))
				//,index(index), anc_h(anc_h)
			{
				/* Mode 1: 5 ancillas are included to post-select */
				//if (!is_PD)
				//{
				//	anc_registers = { anc_UA, anc_3, anc_2, index, anc_h };
				//}
				//else {
				//	anc_registers = { anc_UA, anc_3, anc_2 };
				//}
				anc_registers = { System::get(anc_UA), System::get(anc_3), System::get(anc_2) };
			};
			/**
			 * @brief Constructor (filtering version, post-selects {anc_UA, anc_3, anc_2, index, anc_h})
			 * @param main_reg Main register name
			 * @param anc_UA Ancilla register name of the block encoding of A
			 * @param anc_4 Ancilla register anc_4 name
			 * @param anc_3 Ancilla register anc_3 name
			 * @param anc_2 Ancilla register anc_2 name
			 * @param anc_1 Ancilla register anc_1 name
			 * @param index LCU index register name
			 * @param anc_h Filtering ancilla register name
			 */
			GetOutput(
				std::string main_reg,
				std::string anc_UA,
				std::string anc_4,
				std::string anc_3,
				std::string anc_2,
				std::string anc_1,
				std::string index,
				std::string anc_h) :
				main_reg(System::get(main_reg)), anc_UA(System::get(anc_UA)),
				anc_4(System::get(anc_4)), anc_3(System::get(anc_3)), anc_2(System::get(anc_2)), anc_1(System::get(anc_1)),
				index(System::get(index)), anc_h(System::get(anc_h))
			{
				anc_registers = { System::get(anc_UA), System::get(anc_3), System::get(anc_2), 
					System::get(index), System::get(anc_h) };
			}

			/**
			 * @brief Extract the post-selected subspace from a system state vector
			 *        (concrete implementation in qda_fundamental.cpp)
			 * @param state System state vector
			 * @return {Normalized amplitude vector (index = main_reg value + anc_1·2^n +
			 *          anc_4·2^(n+1)), success probability}
			 */
			std::pair<std::vector<complex_t>, double> operator()(const std::vector<System>& state) const;
			
			/**
			 * @brief Extract the post-selected subspace from a sparse state (delegates to the
			 *        basis-state-list version)
			 * @param state Sparse state
			 * @return {Normalized amplitude vector, success probability}
			 */
			std::pair<std::vector<complex_t>, double> operator()(const SparseState& state) const
			{
				return (*this)(state.basis_states);
			}
#ifdef USE_CUDA
			/**
			 * @brief Extract the post-selected subspace from a CUDA sparse state
			 * @param state CUDA sparse state
			 * @return {Normalized amplitude vector, success probability}
			 */
			std::pair<std::vector<complex_t>, double> operator()(const CuSparseState& state) const;
#endif

			/**
			 * @brief Check whether the specified ancilla registers are zero in all branches
			 * @param state System state vector
			 * @return Returns true when {anc_UA, anc_3, anc_2, index, anc_h} are all 0
			 * @note Uses the index/anc_h members; it is only meaningful when constructed via
			 *       the constructor that includes them
			 */
			template<typename Ty>
			bool check_removable(Ty& state)
			{
				std::vector<size_t> anc_registers = { anc_UA, anc_3, anc_2, index, anc_h };
				for (size_t i = 0; i < state.size(); ++i)
				{
					System& s = state[i];
					for (auto &reg_id : anc_registers)
					{
						if (s.GetAs(reg_id, size_t) != 0)
							return false;
					}
				}
				return true;
			}

			/**
			 * @brief Extract the subspace where all ancilla registers (anc_registers) are zero
			 *        and normalize it
			 * @param state System state vector
			 * @return {Subspace state list (normalized when the weight sum is nonzero),
			 *          subspace probability sum}
			 */
			template<typename Ty>
			std::pair<Ty, double> get_subspace(Ty& state)
			{
				size_t size_mreg = System::size_of(main_reg);
				// The size of anc_4/anc_1 is 1. The length of state_ps is pow2(size_mreg + 1 + 1).
				std::vector<System> state_ps;
				double sum = 0.0;
				for (int i = 0; i < state.size(); ++i)
				{
					System& s = state[i];

					bool is_zero = std::all_of(anc_registers.begin(), anc_registers.end(),
						[&](const size_t& reg)
						{
							size_t v = s.GetAs(reg, size_t);
							return v == 0;
						});
					if (!is_zero)
						continue;
					else {
						state_ps.push_back(s);
						sum += abs_sqr(s.amplitude);
					}
				}
				if (std::abs(sum - 0.0) < epsilon) {
					return { state_ps, sum };
				}
				else {
					double _sqr = sum != 0 ? 1.0 / std::sqrt(sum) : 1.0;
					for (int i = 0; i < state_ps.size(); i++) {
						state_ps[i].amplitude *= _sqr;
					}
					return { state_ps, sum };
				}
			}
		};

		/**
		 * @brief Build the projector-complement operator matrix Q_b = I - (|0⟩⟨0|)⊗(|b⟩⟨b|)
		 * @param b Normalized right-hand side vector
		 * @return Complex matrix corresponding to Q_b (extended space)
		 * @tparam Ty Eigen vector expression type
		 */
		template<typename Ty>
		auto GetQb(const Eigen::MatrixBase<Ty>& b) -> EigenMat<complex_t>
		{
			//auto Vec0 = GetVec0();
			auto Vec = GetVec0<complex_t>();
			//fmt::print("Vec0={}\n", eigenmat2str(Vec0));
			auto Vec0b = kroneckerProduct(Vec, b);
			//fmt::print("Vec0b={}\n", eigenmat2str(Vec0b));
			auto Mat0b = Vec0b * Vec0b.adjoint();
			//fmt::print("Mat0b={}\n", eigenmat2str(Mat0b));
			auto MatI = eyes_like(Mat0b);
			//fmt::print("MatI={}\n", eigenmat2str(MatI));
			auto MatQb = MatI - Mat0b;
			//fmt::print("MatQb={}\n", eigenmat2str(MatQb));
			return MatQb;
		}

		/**
		 * @brief Build the Hermitian extended interpolation operator A_f
		 * @param A Original matrix
		 * @param fs Interpolation parameter f(s)
		 * @return Complex matrix corresponding to A_f = (1-f)·σz⊗I + f·[[0, A], [A†, 0]]
		 * @tparam Ty Eigen matrix expression type
		 */
		template<typename Ty>
		EigenMat<complex_t> GetAf(const Eigen::MatrixBase<Ty>& A, double fs)
		{
			return (1 - fs) * kroneckerProduct(GetSigmaZ(), eyes_like(A))
				+ fs * HermitianA(A);
		}

		/**
		 * @brief Build the interpolated Hamiltonian matrix H(s) (off-diagonal block form)
		 * @param A Original matrix
		 * @param fs Interpolation parameter f(s)
		 * @param b Normalized right-hand side vector
		 * @return H(s) = c·[[0, A_f·Q_b], [Q_b·A_f, 0]], where c = 1/√(2f² + 2(1-f)²)
		 * @tparam Ty1 Eigen matrix expression type
		 * @tparam Ty2 Eigen vector expression type
		 */
		template<typename Ty1, typename Ty2>
		EigenMat<complex_t> GetHs(const Eigen::MatrixBase<Ty1>& A, double fs, const Eigen::MatrixBase<Ty2>& b)
		{
			double scalar = 1.0 / std::sqrt(2.0 * fs * fs + 2.0 * (1 - fs) * (1 - fs));
			auto Af = GetAf(A, fs);
			auto Qb = GetQb(b);
			auto AfQb = scalar * Af * Qb;
			auto QbAf = scalar * Qb * Af;
			auto AfQb01 = kroneckerProduct(GetMat01<complex_t>(), AfQb);
			auto QbAf10 = kroneckerProduct(GetMat10<complex_t>(), QbAf);

			return AfQb01 + QbAf10;
		}
	}
}

/*****************************************************
				Filtering Process
*****************************************************/
namespace qram_simulator{
	namespace QDA
	{
		/**
		 * @brief LCU (linear combination of unitaries) container of powers of the walk operator
		 * @details For the i-th bit of the index register, builds a walk operator controlled
		 *          by that bit and applies it 2^(i+1) times; combined with the coefficient
		 *          state preparation on the index register (State_Prep_via_QRAM), realizes
		 *          the Σ_k c_k W^k-type expansion in walk powers (LCU). The progress of each
		 *          step is both printed and written to the log file. Supports conditional
		 *          control (ClassControllable)
		 * @tparam Walk_s Walk operator type
		 */
		template<typename Walk_s>
		struct LCU
		{
			/** @brief LCU index register ID */
			size_t index;
			/** @brief Walk operator instance */
			Walk_s Walk;
			/** @brief Index register bit width */
			size_t index_size;
			/** @brief Log file path */
			std::string filename;
			ClassControllable

			/**
			 * @brief Constructor (index given as a register ID)
			 * @param Walk Walk operator instance
			 * @param index Index register ID
			 * @param filename_ Log file path
			 */
			LCU(Walk_s Walk, size_t index, std::string filename_) :
				Walk(Walk), index(index), filename(filename_)
			{
				index_size = System::size_of(index);
			};

			/**
			 * @brief Constructor (index given as a register name)
			 * @param Walk Walk operator instance
			 * @param index Index register name
			 * @param filename_ Log file path
			 */
			LCU(Walk_s Walk, std::string index, std::string filename_) :
				Walk(Walk), index(System::get(index)), filename(filename_)
			{
				index_size = System::size_of(index);

			}

			/**
			 * @brief Execute the LCU combination (forward)
			 * @param state System state vector
			 */
			template<typename Ty>
			void operator()(Ty& state)
			{
				for (size_t i = 0; i < index_size; i++)
				{
					fmt::print("LCU step {} / {}\n", i, index_size);
					{
						std::ofstream f_stdout(filename, std::ios::app);
						if (!f_stdout.is_open()) {
							throw std::runtime_error("Failed to open file.");
						}
						f_stdout << fmt::format("LCU step {} / {}\n", i, index_size);
						f_stdout.close();
					}
					Walk.clear_control_by_bit();
					auto _walk = Walk.conditioned_by_bit(index, i).conditioned_by_all_ones(condition_variable_all_ones);
					for (int j = 0; j < pow2(i + 1); j++)
					{
						_walk(state);
					}
				}
			}

			/**
			 * @brief Execute the LCU combination (dagger, inverse)
			 * @param state System state vector
			 */
			template<typename Ty>
			void dag(Ty& state)
			{
				for (size_t i = 0; i < index_size; i++)
				{
					fmt::print("LCUdag step {} / {}\n", i, index_size);
					{
						std::ofstream f_stdout(filename, std::ios::app);
						if (!f_stdout.is_open()) {
							throw std::runtime_error("Failed to open file.");
						}
						f_stdout << fmt::format("LCUdag step {} / {}\n", i, index_size);
						f_stdout.close();
					}
					Walk.clear_control_by_bit();
					auto _walk = Walk.conditioned_by_bit(index, i).conditioned_by_all_ones(condition_variable_all_ones);
					for (int j = 0; j < pow2(i + 1); j++)
					{
						_walk.dag(state);
					}
				}
			}
		};


		/**
		 * @brief Compute the rotation-angle sequence for sequential state preparation from a
		 *        list of nonnegative coefficients
		 * @param coeffs List of nonnegative coefficients
		 * @return List of rotation angles θ_i = 2·arccos(√(c_i / Σ_{j≥i} c_j))
		 * @throws Throws an exception when a coefficient is negative
		 */
		inline std::vector<double> CalculateAngles(std::vector<double>& coeffs) {
			auto l = coeffs.size();
			std::vector<double> angles(l);
			double sum = std::accumulate(coeffs.begin(), coeffs.end(), 0.0);
			for (size_t i = 0; i < l; ++i)
			{
				double s = coeffs[i];
				if (s < 0) { throw_invalid_input(); }
				else {
					double cos_theta_2 = sqrt(s / std::accumulate(coeffs.begin() + i, coeffs.end(), 0.0));
					angles[i] = 2 * acos(cos_theta_2);
				}
			}
			return angles;
		};

		/**
		 * @brief Compute the Chebyshev polynomial of the first kind T_n(x) (iterative
		 *        implementation)
		 * @param n Polynomial order
		 * @param x Input value
		 * @return T_n(x)
		 */
		inline double chebyshevT(size_t n, double x) {
			// Base cases
			if (n == 0) return 1;
			if (n == 1) return x;
			double T0 = 1;
			double T1 = x;
			double Tn = 0;
			for (size_t k = 2; k <= n; ++k) {
				Tn = 2 * x * T1 - T0;
				T0 = T1;
				T1 = Tn;
			}
			return Tn;
		}

		/**
		 * @brief Compute the Dolph-Chebyshev window function value
		 * @param epsilon_ Error tolerance ε
		 * @param l_ Window length parameter l
		 * @param phi_ Phase angle φ
		 * @return ε·T_l(cosh(acosh(1/ε)/l)·cos φ)
		 */
		inline double DolphChebyshev(double epsilon_, int l_, double phi_) {
			double beta = cosh(acosh(1.0 / epsilon_) / l_);
			double x = epsilon_ * chebyshevT(l_, beta * cos(phi_));
			return x;
		}

		/**
		 * @brief Evaluate an even-function Fourier series
		 * @param weights Coefficient list (w_0 is the constant term)
		 * @param x Evaluation point
		 * @return w_0 + Σ_{i≥1} 2·w_i·cos(i·x)
		 */
		inline double FourierSeries(std::vector<double> weights, double x)
		{
			auto l = weights.size();
			double sum = weights[0];
			for (int i = 1; i < l; i++)
			{
				sum += weights[i] * 2 * cos(i * x);
			}
			return sum;
		}
		// Function to compute the coefficients of the Fourier series
		/**
		 * @brief Numerically compute the Fourier coefficients of the Dolph-Chebyshev filter
		 * @param epsilon_ Error tolerance ε
		 * @param l_ Filter length parameter l
		 * @return List of even-order coefficients (numerical integration of the window
		 *         function, keeping only terms with even j)
		 */
		inline std::vector<double> ComputeFourierCoeffs(double epsilon_, int l_) {
			std::vector<double> coeffs; // Initialize coefficients array
			double P_ = 2 * pi;

			// Calculate each coefficient from w_0 to w_l
			for (int j = 0; j <= l_; ++j) {
				double coeff = 0.0;
				double integral = 0.0;
				double delta_phi = P_ / 10000.0; // Set a small delta_phi for numerical integration

				// Perform numerical integration (e.g., using the trapezoidal rule)
				for (double phi = 0; phi <= P_ / 2; phi += delta_phi) {
					double cos_term = cos(2 * pi * j * phi / P_);
					double func_value = DolphChebyshev(epsilon_, l_, phi);
					double term = func_value * cos_term;

					// Use trapezoidal rule for integration
					integral += term;
				}
				coeff = integral * delta_phi / P_; // Finalize the coefficient
				// Since the function is even, double the coefficient
				//std::cout << "Coefficient a" << j << ": " << 2*coeff << std::endl;
				if ((j % 2) == 0)
					coeffs.push_back(2 * coeff);
			}
			return coeffs;
		}

		/**
		 * @brief Dolph-Chebyshev filtering operator (QDA accuracy enhancement)
		 * @details Applies filtering to the walk sequence to amplify the amplitude of the
		 *          successful branch: first prepares the filter-coefficient state on the
		 *          index register using a QRAM (qram_w), applies a Hadamard on anc_h followed
		 *          by the controlled LCU walk-power expansion, then realizes reflection-style
		 *          filtering through the alternating combination of X(anc_h) and LCU†, and
		 *          finally undoes the preparation and reads the success probability. The
		 *          filter coefficients are given by ComputeFourierCoeffs (Dolph-Chebyshev
		 *          window). Supports conditional control (ClassControllable)
		 * @tparam Walk_type Walk operator type
		 */
		template<typename Walk_type>
		struct Filtering
		{
			/** @brief Pointer to the QRAM circuit storing the filter coefficients */
			qram_qutrit::QRAMCircuit* qram_w;
			/** @brief Main data register name */
			std::string main_reg;
			/** @brief Ancilla register name used by the block encoding of A */
			std::string anc_UA;
			/** @brief Ancilla register anc_4 name */
			std::string anc_4;
			/** @brief Ancilla register anc_3 name */
			std::string anc_3;
			/** @brief Ancilla register anc_2 name */
			std::string anc_2;
			/** @brief Ancilla register anc_1 name */
			std::string anc_1;
			/** @brief LCU index register name */
			std::string index;
			/** @brief Filtering ancilla register name */
			std::string anc_h;
			/** @brief Data register bit width (fixed-point quantization bits) */
			size_t data_size;
			/** @brief Rational (rotation-angle) register bit width */
			size_t rational_size;
			/** @brief Walk operator instance */
			Walk_type Walk;
			/** @brief Index register bit width */
			int index_size;
			/** @brief Run log file path */
			std::string stdout_filename;
			/**
			 * @brief Constructor
			 * @param qram_w Pointer to the QRAM circuit storing the filter coefficients
			 * @param Walk Walk operator instance
			 * @param main_reg Main data register name
			 * @param anc_UA Ancilla register name of the block encoding of A
			 * @param anc_4 Ancilla register anc_4 name
			 * @param anc_3 Ancilla register anc_3 name
			 * @param anc_2 Ancilla register anc_2 name
			 * @param anc_1 Ancilla register anc_1 name
			 * @param index LCU index register name
			 * @param anc_h Filtering ancilla register name
			 * @param ds Data register bit width
			 * @param rs Rational register bit width
			 * @param stdout_filename_ Run log file path
			 */
			Filtering(qram_qutrit::QRAMCircuit* qram_w,
				Walk_type Walk,
				std::string main_reg,
				std::string anc_UA,
				std::string anc_4,
				std::string anc_3,
				std::string anc_2,
				std::string anc_1,
				std::string index,
				std::string anc_h,
				size_t ds,
				size_t rs,
				std::string stdout_filename_) :
				qram_w(qram_w), Walk(Walk), main_reg(main_reg), anc_UA(anc_UA),
				anc_4(anc_4), anc_3(anc_3), anc_2(anc_2), anc_1(anc_1),
				index(index), anc_h(anc_h), data_size(ds), rational_size(rs), stdout_filename(stdout_filename_)
			{
				index_size = System::size_of(index);
			};

			/**
			 * @brief Generate a random initial state (for debugging)
			 * @param state System state vector
			 * @details Applies Hadamards on anc_1 and the main register, then injects a
			 *          random real amplitude into each branch and normalizes
			 */
			template<typename Ty>
			void random_state_generate(Ty& state)
			{
				//Hadamard_Int(anc_4, 1)(state);
				Hadamard_Int(anc_1, 1)(state);
				Hadamard_Int(main_reg, System::size_of(main_reg))(state);

				for (auto& s : state)
				{
					s.amplitude = random_engine::rng() * 2 - 1;
				}
				Normalize()(state);
			}
			//std::string dump_format() const
			//{
			//	return fmt::format("Filtering");
			//}

			/**
			 * @brief Execute the filtering procedure and return the success probability
			 * @param state System state vector
			 * @return Post-selection probability (branches where both anc_h and index are 0),
			 *         obtained by squaring the partial-trace amplitude
			 * @details Procedure: coefficient state preparation → H(anc_h) → LCU
			 *          (controlled) → X(anc_h) → LCU† (controlled) → X(anc_h) → H(anc_h) →
			 *          inverse preparation; afterwards writes the peak resource statistics
			 *          to the log file
			 */
			template<typename Ty>
			double operator()(Ty& state)
			{
				using namespace state_prep;
				State_Prep_via_QRAM(qram_w, index, data_size, rational_size)(state);
				// StatePrint(0, 16)(state);
				(Hadamard_Int_Full(anc_h))(state);
				LCU<Walk_type>(Walk, index, stdout_filename).conditioned_by_all_ones(anc_h)(state);

				X_Bool(anc_h, 0)(state);
				LCU<Walk_type>(Walk, index, stdout_filename).conditioned_by_all_ones(anc_h).dag(state);
				X_Bool(anc_h, 0)(state);

				(Hadamard_Int_Full(anc_h))(state);
				State_Prep_via_QRAM(qram_w, index, data_size, rational_size).dag(state);

				{// stdout writing
					std::ofstream f_stdout(stdout_filename, std::ios::app);
					if (!f_stdout.is_open()) {
						throw std::runtime_error("Failed to open file.");
					}
					f_stdout << fmt::format("\nAfter filtering: \n");
					f_stdout << fmt::format("Maximum Qubit Count = {}\n", System::max_qubit_count);
					f_stdout << fmt::format("Maximum Register Count = {}\n", System::max_register_count);
					f_stdout << fmt::format("Maximum System Size = {}\n\n", System::max_system_size);
					f_stdout.close();
				}

				double prob_inv = PartialTraceSelect({ System::get(anc_h), System::get(index) }, { 0, 0 })(state);
				double prob = (1.0 / prob_inv) * (1.0 / prob_inv);
				return prob;
			}
		};
	} // namespace QDA

	namespace QDA
	{
		/* Extract full unitary of BlockEncoding_Hs */
		/**
		 * @brief Extract the full unitary matrix of Block_Encoding_Hs (for debugging)
		 * @param encHs H(s) block encoding operator
		 * @param main_reg Main data register name
		 * @param anc_UA Ancilla register name of the block encoding of A
		 * @param anc_1 Ancilla register anc_1 name
		 * @param anc_2 Ancilla register anc_2 name
		 * @param anc_3 Ancilla register anc_3 name
		 * @param anc_4 Ancilla register anc_4 name
		 * @return Full unitary matrix of dimension 2^(main register bits + anc_UA bits + 4)
		 * @tparam Block_Encoding Block encoding type of A
		 * @tparam State_Prep State preparation type of |b⟩
		 * @tparam StateType State container type (default SparseState)
		 */
		template <typename Block_Encoding, typename State_Prep, typename StateType = SparseState>
		DenseMatrix<complex_t> _extract_full_unitary(
			Block_Encoding_Hs<Block_Encoding, State_Prep> encHs,
			std::string main_reg, std::string anc_UA,
			std::string anc_1, std::string anc_2, std::string anc_3, std::string anc_4)
		{
			size_t main_reg_num = System::size_of(main_reg);
			size_t anc_UA_num = System::size_of(anc_UA);
			size_t qubit_num = main_reg_num + anc_UA_num + 4;
			fmt::print("main_reg_num = {}, anc_UA_num = {}", main_reg_num, anc_UA_num);

			DenseMatrix<complex_t> ret(pow2(qubit_num));
			int main_reg_pos = System::get(main_reg);
			int anc_UA_pos = System::get(anc_UA);
			int anc_1_pos = System::get(anc_1);
			int anc_2_pos = System::get(anc_2);
			int anc_3_pos = System::get(anc_3);
			int anc_4_pos = System::get(anc_4);

			auto i_range = range(pow2(main_reg_num));
			auto a_A_range = range(pow2(anc_UA_num));
			auto b1_range = range(2);
			auto b2_range = range(2);
			auto b3_range = range(2);
			auto b4_range = range(2);

			for (auto [i, a_A, b1, b2, b3, b4] : product(i_range, a_A_range, b1_range, b2_range, b3_range, b4_range)) {
				/*std::vector<System> state;
				state.emplace_back();*/
				StateType state(1);
				state.back().get(main_reg_pos).value = i;
				state.back().get(anc_UA_pos).value = a_A;
				state.back().get(anc_1_pos).value = b1;
				state.back().get(anc_2_pos).value = b2;
				state.back().get(anc_3_pos).value = b3;
				state.back().get(anc_4_pos).value = b4;

				encHs(state);

				std::vector<complex_t> vec(pow2(qubit_num), 0);

				for (auto& s : state)
				{
					size_t index = concat_value(
						{
							{s.get(main_reg_pos).value, main_reg_num},
							{s.get(anc_UA_pos).value, anc_UA_num},
							{s.get(anc_1_pos).value, 1},
							{s.get(anc_2_pos).value, 1},
							{s.get(anc_3_pos).value, 1},
							{s.get(anc_4_pos).value, 1}
						}
					);
					vec[index] = s.amplitude;
				}
				for (size_t j = 0; j < pow2(qubit_num); ++j)
				{
					size_t index = concat_value(
						{
							{i, main_reg_num},
							{a_A, anc_UA_num},
							{b1, 1},
							{b2, 1},
							{b3, 1},
							{b4, 1}
						}
					);

					ret(j, index) = vec[j];
				}
			}
			return ret;
		}

		/**
		 * @brief Extract the full unitary matrix of Block_Encoding_Hs (convenient
		 *        SparseState wrapper)
		 * @param encHs H(s) block encoding operator
		 * @param main_reg Main data register name
		 * @param anc_UA Ancilla register name of the block encoding of A
		 * @param anc_1 Ancilla register anc_1 name
		 * @param anc_2 Ancilla register anc_2 name
		 * @param anc_3 Ancilla register anc_3 name
		 * @param anc_4 Ancilla register anc_4 name
		 * @return Full unitary matrix
		 * @tparam Block_Encoding Block encoding type of A
		 * @tparam State_Prep State preparation type of |b⟩
		 */
		template <typename Block_Encoding, typename State_Prep>
		DenseMatrix<complex_t> extract_full_unitary(
			Block_Encoding_Hs<Block_Encoding, State_Prep> encHs,
			std::string main_reg, std::string anc_UA,
			std::string anc_1, std::string anc_2, std::string anc_3, std::string anc_4)
		{
			return _extract_full_unitary<Block_Encoding, State_Prep, SparseState>(encHs, main_reg, anc_UA, anc_1, anc_2, anc_3, anc_4);
		}
		

		/* Extract the block encoding part of the Hs */
		/**
		 * @brief Extract the effective encoded block of the H(s) block encoding (for
		 *        debugging)
		 * @param encHs H(s) block encoding operator
		 * @param main_reg Main data register name
		 * @param anc_UA Ancilla register name of the block encoding of A
		 * @param anc_1 Ancilla register anc_1 name
		 * @param anc_2 Ancilla register anc_2 name
		 * @param anc_3 Ancilla register anc_3 name
		 * @param anc_4 Ancilla register anc_4 name
		 * @param qubit_num Number of qubits n of the main register
		 * @return 2^(n+2)-dimensional matrix, post-selected (anc_UA / anc_2 / anc_3 all 0)
		 *         and normalized by the success probability, whose row and column indices
		 *         include the two outer block indicators anc_1 and anc_4
		 * @tparam Block_Encoding Block encoding type of A
		 * @tparam State_Prep State preparation type of |b⟩
		 * @tparam StateType State container type (default SparseState)
		 */
		template <typename Block_Encoding, typename State_Prep, typename StateType = SparseState>
		DenseMatrix<complex_t> _extract_block_encoding_Hs(
			Block_Encoding_Hs<Block_Encoding, State_Prep> encHs,
			std::string main_reg, std::string anc_UA,
			std::string anc_1, std::string anc_2, std::string anc_3, std::string anc_4,
			size_t qubit_num)
		{
			DenseMatrix<complex_t> ret(pow2(qubit_num + 2));
			auto main_reg_pos = System::get(main_reg);
			auto anc_UA_pos = System::get(anc_UA);
			auto anc_1_pos = System::get(anc_1);
			auto anc_2_pos = System::get(anc_2);
			auto anc_3_pos = System::get(anc_3);
			auto anc_4_pos = System::get(anc_4);

			auto i_range = range(pow2(qubit_num));
			auto b1_range = range(2);
			auto b4_range = range(2);

			for (auto [i, b1, b4] : product(i_range, b1_range, b4_range))
			{
				//std::vector<System> state;
				//state.emplace_back();
				StateType state(1);
				state.back().get(main_reg_pos).value = i;
				state.back().get(anc_UA_pos).value = 0;
				state.back().get(anc_1_pos).value = b1;
				state.back().get(anc_2_pos).value = 0;
				state.back().get(anc_3_pos).value = 0;
				state.back().get(anc_4_pos).value = b4;

				encHs(state);
				//StatePrint()(state);
				double prob = PartialTraceSelect({ { anc_UA, 0 },{ anc_2, 0 },{ anc_3, 0 } })(state);

				std::vector<complex_t> vec(pow2(qubit_num + 2), 0);
				for (auto& s : state)
				{
					vec[s.get(main_reg_pos).value
						+ (s.get(anc_1_pos).value << qubit_num)
						+ (s.get(anc_4_pos).value << (qubit_num + 1))
					] = s.amplitude;
				}
				for (size_t j = 0; j < pow2(qubit_num + 2); ++j)
				{
					ret(j, (b4 << (qubit_num + 1)) + (b1 << qubit_num) + i) = vec[j] / prob;
				}
			}
			return ret;
		}


		/**
		 * @brief Extract the effective encoded block of the H(s) block encoding (convenient
		 *        SparseState wrapper)
		 * @param encHs H(s) block encoding operator
		 * @param main_reg Main data register name
		 * @param anc_UA Ancilla register name of the block encoding of A
		 * @param anc_1 Ancilla register anc_1 name
		 * @param anc_2 Ancilla register anc_2 name
		 * @param anc_3 Ancilla register anc_3 name
		 * @param anc_4 Ancilla register anc_4 name
		 * @param qubit_num Number of qubits of the main register
		 * @return Encoded block matrix after post-selection normalization
		 * @tparam Block_Encoding Block encoding type of A
		 * @tparam State_Prep State preparation type of |b⟩
		 */
		template <typename Block_Encoding, typename State_Prep>
		DenseMatrix<complex_t> extract_block_encoding_Hs(
			Block_Encoding_Hs<Block_Encoding, State_Prep> encHs,
			std::string main_reg, std::string anc_UA,
			std::string anc_1, std::string anc_2, std::string anc_3, std::string anc_4,
			size_t qubit_num)
		{
			return _extract_block_encoding_Hs<Block_Encoding, State_Prep, SparseState>(encHs, main_reg, anc_UA, anc_1, anc_2, anc_3, anc_4, qubit_num);
		}
	}
}