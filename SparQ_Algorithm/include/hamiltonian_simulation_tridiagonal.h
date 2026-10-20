/**
 * @file hamiltonian_simulation_tridiagonal.h
 * @brief QRAM-free CKS walk for constant-diagonal tridiagonal matrices
 * @details For A = tridiag(beta, alpha, beta) with constant diagonals every matrix
 *          element is computable arithmetically: A[j,k] = alpha if k == j,
 *          beta if |k-j| == 1, 0 otherwise. This header replaces the two QRAM
 *          table oracles of the CKS walk (SparQ_Algorithm/include/
 *          hamiltonian_simulation.h, struct T) with direct arithmetic oracles,
 *          eliminating the QRAM circuit entirely.
 *
 *          The arithmetic oracles are bit-exact mirrors of the QRAM circuit's
 *          composite register maps (verified against the QRAM path state dumps
 *          2026-10-20, n = 8): the QRAM memory is the concatenation
 *          [elements | sparsity] of 8n words, QuantumBinarySearch_Fast over a
 *          row's 4-slot window is the range test k in [base, base+4), QRAMLoad
 *          is an XOR of memory[addr & (8n-1)], and the slot<->column conversion
 *          adds/subtracts the row address. Out-of-window (garbage) branches are
 *          tagged into the binary-search result register k_comp exactly like
 *          the QRAM path (k_comp = k ^ mem[0] etc.), which keeps every garbage
 *          branch in a singleton Hadamard group; without this the final
 *          Hadamard_Int of T.dag mixes high bits inside one group and writes
 *          amplitudes out of bounds (state norm corruption).
 *
 *          Sign convention: the walk rotation uses |v| (positive_only encoding),
 *          i.e. the walk implements the matrix |A|. For the Poisson case
 *          (all off-diagonals negative) |A| = D*A*D with D = diag((-1)^i), hence
 *          A^{-1}|b> = D*|A|^{-1}*D|b>: prepare the input D|b> (uniform Hadamard
 *          plus TridiagParityPhase) and multiply the extracted amplitudes by
 *          (-1)^i classically. TridiagParityPhase applies that conditional phase.
 */

#pragma once

#include "hamiltonian_simulation.h"
#include <Eigen/Dense>

namespace qram_simulator {

	namespace CKS {

		/**
		 * @brief True Chebyshev-expansion coefficients of 1/x for the odd-walk LCU basis
		 * @details The stock ChebyshevPolynomialCoefficient (binomial-tail / erfc
		 *          asymptotics) approximates 1/x on the walk spectrum only to ~20-40%
		 *          relative error at the small-eigenvalue end (the achieved LCU residual
		 *          floors around 0.2 regardless of j0; verified classically against the
		 *          T_{2j+1} recursion 2026-10-20). This provider solves the collocation
		 *          least-squares problem for the actual expansion
		 *          1/x ~ sum_j d_j T_{2j+1}(x) on the exact (quantized) spectral interval
		 *          [m, M], reaching ~1e-7 relative error at J ~ 512 for kappa = 16.
		 *          Coefficients carry arbitrary signs; the LCU container pairs each
		 *          |d_j| with a sign flag and accumulates a = sum |d_j|.
		 */
		struct ChebyshevInverseCoefficients
		{
			/** @brief Signed coefficients d_j, j = 0..J (term j multiplies T_{2j+1}) */
			std::vector<double> coef;
			/** @brief Truncation index (terms j = 0..J) */
			size_t J = 0;
			/** @brief LCU normalization factor sum |d_j| */
			double a = 0;

			/**
			 * @brief Constructor (collocation least squares on a Lobatto grid)
			 * @param m Spectral interval lower end (smallest walk-matrix eigenvalue)
			 * @param M Spectral interval upper end (largest walk-matrix eigenvalue)
			 * @param J_ Truncation index (highest Chebyshev degree 2J+1)
			 * @param ngrid Number of grid intervals (grid points ngrid+1)
			 */
			ChebyshevInverseCoefficients(double m, double M, size_t J_, size_t ngrid = 4096)
			{
				J = J_;
				Eigen::MatrixXd V(ngrid + 1, J_ + 1);
				Eigen::VectorXd y(ngrid + 1);
				std::vector<double> T(2 * J_ + 2);
				for (size_t i = 0; i <= ngrid; ++i)
				{
					double th = pi * double(i) / double(ngrid);
					double x = 0.5 * (M + m) + 0.5 * (M - m) * std::cos(th);
					T[0] = 1.0;
					T[1] = x;
					for (size_t k = 2; k <= 2 * J_ + 1; ++k)
						T[k] = 2.0 * x * T[k - 1] - T[k - 2];
					y(i) = 1.0 / x;
					for (size_t j = 0; j <= J_; ++j)
						V(i, j) = T[2 * j + 1];
				}
				Eigen::VectorXd d = V.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(y);
				coef.resize(J_ + 1);
				for (size_t j = 0; j <= J_; ++j)
				{
					coef[j] = d(j);
					a += std::fabs(d(j));
				}
			}
		};

		/**
		 * @brief Arithmetic description of a constant-diagonal tridiagonal matrix
		 * @details Carries the quantized (positive_only) element values, the
		 *          clamped-window slot<->column mapping and the combined-table
		 *          addressing constants used by the direct oracles.
		 */
		struct TridiagonalSpec
		{
			/** @brief Matrix order */
			size_t n_row = 0;
			/** @brief Quantization width of the elements (bits) */
			size_t data_size = 0;
			/** @brief Quantized diagonal element (unsigned fixed point) */
			uint64_t alpha_enc = 0;
			/** @brief Quantized |off-diagonal| element (unsigned fixed point) */
			uint64_t beta_enc = 0;
			/** @brief Start of the sparsity (column-index) half in the combined layout (= 4n) */
			size_t sparse_offset = 0;
			/** @brief QRAM address mask: the combined table holds 8n = 2^addr_size words */
			uint64_t addr_mask = 0;
			/** @brief Width of the walk index registers (add/sub wrap-around width) */
			size_t k_width = 0;
			/** @brief Slots per row (l = 2 band rounded up to a power of two) */
			constexpr static size_t slot_count = 4;

			/**
			 * @brief Start column of row j's four-slot window
			 * @param j Row index
			 * @return clamp(j-1, 0, n-4): interior rows span {j-1..j+2}, first rows
			 *         clamp to {0..3}, last rows clamp to {n-4..n-1}
			 */
			HOST_DEVICE size_t window_base(size_t j) const
			{
				size_t b = (j > 0) ? (j - 1) : 0;
				size_t bmax = n_row - slot_count;
				return b < bmax ? b : bmax;
			}

			/**
			 * @brief Column index of (row j, slot s)
			 * @param j Row index
			 * @param s Slot index in [0, 4)
			 */
			HOST_DEVICE size_t col_of(size_t j, size_t s) const
			{
				return window_base(j) + s;
			}

			/**
			 * @brief Quantized element value of (row j, slot s)
			 * @param j Row index
			 * @param s Slot index in [0, 4)
			 * @return alpha_enc on the diagonal, beta_enc on the first off-diagonals,
			 *         0 on the pad slot (and on out-of-band window entries)
			 */
			HOST_DEVICE uint64_t value_of(size_t j, size_t s) const
			{
				size_t c = col_of(j, s);
				if (c == j) return alpha_enc;
				if (c + 1 == j || c == j + 1) return beta_enc;
				return 0;
			}

			/**
			 * @brief Combined QRAM memory word, computed arithmetically
			 * @param idx Raw (unmasked) QRAM address
			 * @return The word the QRAM path stores at address idx & (8n-1):
			 *         the first 4n words are the quantized elements
			 *         (row-major, 4 slots per row), the last 4n words are the
			 *         window column indices
			 */
			HOST_DEVICE uint64_t combined_mem(uint64_t idx) const
			{
				idx &= addr_mask;
				uint64_t half = uint64_t(n_row) * slot_count;
				if (idx < half)
					return value_of(size_t(idx >> 2), size_t(idx & 3));
				uint64_t t = idx - half;
				return uint64_t(col_of(size_t(t >> 2), size_t(t & 3)));
			}
		};

		/**
		 * @brief Build the positive-only SparseMatrix and matching arithmetic spec
		 * @param alpha Diagonal coefficient
		 * @param beta Off-diagonal coefficient (magnitude is encoded; see file docs)
		 * @param n Matrix order (power of two, >= 8)
		 * @param ds Element quantization width in bits
		 * @return (SparseMatrix, TridiagonalSpec) pair with identical encodings: the
		 *         matrix feeds GetQWRotateAngle_Int_Int_Int metadata and QRAM-path
		 *         reference runs; the spec feeds the direct oracles
		 * @note n must be a power of two so the combined 8n-word QRAM table (and
		 *       its address mask) is well defined
		 */
		inline std::pair<SparseMatrix, TridiagonalSpec> make_tridiagonal_matrix_spec(
			double alpha, double beta, size_t n, size_t ds)
		{
			double lam_max = std::fabs(alpha) + 2.0 * std::fabs(beta);
			double scale = 1.0 / lam_max;
			uint64_t alpha_enc = static_cast<uint64_t>(std::floor(
				std::fabs(alpha) * scale * double(pow2(ds))));
			uint64_t beta_enc = static_cast<uint64_t>(std::floor(
				std::fabs(beta) * scale * double(pow2(ds))));

			TridiagonalSpec spec;
			spec.n_row = n;
			spec.data_size = ds;
			spec.alpha_enc = alpha_enc;
			spec.beta_enc = beta_enc;
			spec.sparse_offset = n * TridiagonalSpec::slot_count;
			spec.addr_mask = 2 * spec.sparse_offset - 1;
			spec.k_width = std::max(size_t(log2(n)) + 1, ds);

			std::vector<uint64_t> elements;
			std::vector<size_t> sparsity;
			elements.reserve(n * TridiagonalSpec::slot_count);
			sparsity.reserve(n * TridiagonalSpec::slot_count);
			for (size_t i = 0; i < n; ++i)
				for (size_t s = 0; s < TridiagonalSpec::slot_count; ++s)
				{
					elements.push_back(spec.value_of(i, s));
					sparsity.push_back(spec.col_of(i, s));
				}

			return { SparseMatrix(elements, sparsity, ds,
							TridiagonalSpec::slot_count, n),
				spec };
		}

		/**
		 * @brief Direct element-value oracle for the tridiagonal walk (self-adjoint)
		 * @details Bit-exact arithmetic mirror of SparseMatrixOracle1: the QRAM path
		 *          computes data_addr = data_offset + 4*j + k and XORs the combined
		 *          memory word at that (masked) address into the data register; this
		 *          oracle computes the same word arithmetically (no QRAM). Two
		 *          invocations cancel.
		 */
		struct TridiagOracle1 : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief Register ID of the data-table offset register */
			size_t offset_id;
			/** @brief Register ID of the row index j */
			size_t row_id;
			/** @brief Register ID of the sparse slot s */
			size_t slot_id;
			/** @brief Register ID of the value output */
			size_t out_id;
			/** @brief Arithmetic matrix description */
			TridiagonalSpec spec;

			/**
			 * @brief Constructor (register-name version)
			 */
			TridiagOracle1(std::string_view reg_offset, std::string_view reg_row,
				std::string_view reg_slot, std::string_view reg_out,
				const TridiagonalSpec& spec_)
				: offset_id(System::get(reg_offset)), row_id(System::get(reg_row)),
				slot_id(System::get(reg_slot)), out_id(System::get(reg_out)), spec(spec_)
			{
			}

			/**
			 * @brief Constructor (register-ID version)
			 */
			TridiagOracle1(size_t reg_offset, size_t reg_row, size_t reg_slot,
				size_t reg_out, const TridiagonalSpec& spec_)
				: offset_id(reg_offset), row_id(reg_row), slot_id(reg_slot),
				out_id(reg_out), spec(spec_)
			{
			}

			/// @brief Apply to a CPU state vector
			void operator()(std::vector<System>& state) const;
#ifdef USE_CUDA
			/// @brief Apply to a CUDA sparse state
			void operator()(CuSparseState& state) const;
#endif
		};

		/**
		 * @brief Direct slot<->column conversion oracle (QRAM-free SparseMatrixOracle2)
		 * @details Bit-exact arithmetic mirror of the QRAM oracle's composite map over
		 *          (k, k_comp), with separate forward and dagger directions:
		 *
		 *          forward (column -> slot), row_addr = sparse_offset + 4*j,
		 *          base = window_base(j):
		 *            hit  <=> k in [base, base+4)
		 *            sr_1  = k_comp ^ (hit ? row_addr + (k - base) : 0)
		 *            k_1   = k ^ mem(sr_1)
		 *            k_out = (sr_1 - row_addr) mod 2^k_width      (slot, or -row_addr junk)
		 *            k_comp_out = k_1                              (0 on hit, k ^ mem[0] junk)
		 *
		 *          dagger (slot -> column):
		 *            k_1   = (k + row_addr) mod 2^k_width
		 *            k_2   = k_comp ^ mem(k_1)                     (column, or junk)
		 *            hit_2 <=> k_2 in [base, base+4)
		 *            k_out = k_2
		 *            k_comp_out = k_1 ^ (hit_2 ? row_addr + (k_2 - base) : 0)
		 *
		 *          The junk tagging into k_comp is essential: it keeps every garbage
		 *          branch in a singleton Hadamard group, exactly like the QRAM path
		 *          (see the file docs).
		 */
		struct TridiagOracle2 : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief Register ID of the sparse-table offset register */
			size_t offset_id;
			/** @brief Register ID of the row index j */
			size_t row_id;
			/** @brief Register ID of the slot/column register k */
			size_t k_id;
			/** @brief Register ID of the binary-search result register (k_comp) */
			size_t sr_id;
			/** @brief Arithmetic matrix description */
			TridiagonalSpec spec;

			/**
			 * @brief Constructor (register-name version)
			 */
			TridiagOracle2(std::string_view reg_offset, std::string_view reg_row,
				std::string_view reg_k, std::string_view reg_sr,
				const TridiagonalSpec& spec_)
				: offset_id(System::get(reg_offset)), row_id(System::get(reg_row)),
				k_id(System::get(reg_k)), sr_id(System::get(reg_sr)), spec(spec_)
			{
			}

			/**
			 * @brief Constructor (register-ID version)
			 */
			TridiagOracle2(size_t reg_offset, size_t reg_row, size_t reg_k,
				size_t reg_sr, const TridiagonalSpec& spec_)
				: offset_id(reg_offset), row_id(reg_row), k_id(reg_k),
				sr_id(reg_sr), spec(spec_)
			{
			}

			/// @brief Forward (column -> slot) on a CPU state vector
			void operator()(std::vector<System>& state) const override;
			/// @brief Dagger (slot -> column) on a CPU state vector
			void dag(std::vector<System>& state) const override;
#ifdef USE_CUDA
			/// @brief Forward (column -> slot) on a CUDA sparse state
			void operator()(CuSparseState& state) const override;
			/// @brief Dagger (slot -> column) on a CUDA sparse state
			void dag(CuSparseState& state) const override;
#endif
		};

		/**
		 * @brief Parity phase on a register (self-adjoint): amplitude *= (-1)^v
		 * @details Applies phase -1 to branches whose register value is odd. Together
		 *          with a uniform Hadamard it prepares D|b> for the diagonal sign
		 *          transform D = diag((-1)^i) (see file docs).
		 */
		struct TridiagParityPhase : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief Register ID the parity is read from */
			size_t reg_id;

			/**
			 * @brief Constructor (register-name version)
			 */
			TridiagParityPhase(std::string_view reg)
				: reg_id(System::get(reg))
			{
			}

			/**
			 * @brief Constructor (register-ID version)
			 */
			TridiagParityPhase(size_t reg)
				: reg_id(reg)
			{
			}

			/// @brief Apply to a CPU state vector
			void operator()(std::vector<System>& state) const;
#ifdef USE_CUDA
			/// @brief Apply to a CUDA sparse state
			void operator()(CuSparseState& state) const;
#endif
		};

		/**
		 * @brief QRAM-free state preparation T for the tridiagonal CKS walk
		 * @details Op-for-op bit-exact mirror of CKS::T (hamiltonian_simulation.h)
		 *          with the two QRAM table oracles replaced by TridiagOracle1 /
		 *          TridiagOracle2 (same composite register maps, including the
		 *          k_comp junk tagging and the data/rotation scratch registers).
		 */
		struct T_Tridiagonal : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief Name of the data-table offset register */
			std::string reg_data_offset;
			/** @brief Name of the sparse-table offset register */
			std::string reg_sparse_offset;
			/** @brief Name of the row-index register */
			std::string reg_j;
			/** @brief Name of the Boolean flag b1 register */
			std::string reg_b1;
			/** @brief Name of the slot / column-index register */
			std::string reg_k;
			/** @brief Name of the Boolean flag b2 register (rotation target) */
			std::string reg_b2;
			/** @brief Name of the binary-search result register (k_comp) */
			std::string reg_search_result;
			/** @brief Bit width of the temporary data register */
			size_t data_size;
			/** @brief Sparse-matrix metadata for the rotation-angle operator */
			const SparseMatrix* mat;
			/** @brief Arithmetic matrix description */
			TridiagonalSpec spec;

			/**
			 * @brief Constructor
			 */
			T_Tridiagonal(
				std::string_view reg_data_offset_,
				std::string_view reg_sparse_offset_,
				std::string_view reg_j_, std::string_view reg_b1_,
				std::string_view reg_k_, std::string_view reg_b2_,
				std::string_view reg_search_result_,
				size_t data_size_, const SparseMatrix* mat_, const TridiagonalSpec& spec_)
				: reg_data_offset(reg_data_offset_), reg_sparse_offset(reg_sparse_offset_),
				reg_j(reg_j_), reg_b1(reg_b1_), reg_k(reg_k_), reg_b2(reg_b2_),
				reg_search_result(reg_search_result_), data_size(data_size_),
				mat(mat_), spec(spec_)
			{
			}

			/**
			 * @brief Forward implementation (mirror of T::impl)
			 */
			template<typename Ty>
			void impl(Ty& system_states) const
			{
				profiler _("T_Tridiagonal");
				AddRegister("data", UnsignedInteger, data_size)(system_states);

				// |j> -> |j>|s_k>
				Hadamard_Int(reg_k, log2(TridiagonalSpec::slot_count))(system_states);

				// |j>|s_k> -> |j>|s_k>|d[j,k]>
				TridiagOracle1(reg_data_offset, reg_j, reg_k, "data", spec)(system_states);

				// |j>|s_k> -> |j>|k> (slot -> column)
				TridiagOracle2(reg_sparse_offset, reg_j, reg_k,
					reg_search_result, spec).dag(system_states);

				// conditional rotation on b2 (angle from the data value)
				// 2026-10-20: "rotation" was never registered before use in the upstream
				// T (name-based constructors resolve registers immediately); registered
				// here as a 32-bit Rational angle in [0,1) fixed point.
				AddRegister("rotation", Rational, 32)(system_states);
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				CondRot_Fixed_Bool("rotation", reg_b2)(system_states);
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				RemoveRegister("rotation")(system_states);

				// |j>|k> -> |j>|s_k> (column -> slot)
				TridiagOracle2(reg_sparse_offset, reg_j, reg_k,
					reg_search_result, spec)(system_states);

				// uncompute the data value
				TridiagOracle1(reg_data_offset, reg_j, reg_k, "data", spec)(system_states);
				RemoveRegister("data")(system_states);

				// |j>|s_k> -> |j>|k> (T leaves k holding the column index)
				TridiagOracle2(reg_sparse_offset, reg_j, reg_k,
					reg_search_result, spec).dag(system_states);

				CheckNan()(system_states);
			}

			/**
			 * @brief Dagger implementation (mirror of T::impl_dag)
			 */
			template<typename Ty>
			void impl_dag(Ty& system_states) const
			{
				profiler _("T_Tridiagonal.dag");
				AddRegister("data", UnsignedInteger, data_size)(system_states);

				TridiagOracle2(reg_sparse_offset, reg_j, reg_k,
					reg_search_result, spec)(system_states);

				CheckNan()(system_states);

				TridiagOracle1(reg_data_offset, reg_j, reg_k, "data", spec)(system_states);

				TridiagOracle2(reg_sparse_offset, reg_j, reg_k,
					reg_search_result, spec)(system_states);

				AddRegister("rotation", Rational, 32)(system_states);
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				CondRot_Fixed_Bool("rotation", reg_b2).dag(system_states);
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				RemoveRegister("rotation")(system_states);
				ClearZero()(system_states);

				TridiagOracle2(reg_sparse_offset, reg_j, reg_k,
					reg_search_result, spec).dag(system_states);

				CheckNormalization()(system_states);

				TridiagOracle1(reg_data_offset, reg_j, reg_k, "data", spec)(system_states);

				Hadamard_Int(reg_k, log2(TridiagonalSpec::slot_count))(system_states);
				ClearZero()(system_states);

				RemoveRegister("data")(system_states);
			}

			COMPOSITE_OPERATION
		};

		/**
		 * @brief QRAM-free multi-step walk manager for the tridiagonal CKS walk
		 * @details Mirror of QuantumWalkNSteps with the QRAM circuit removed and
		 *          T replaced by T_Tridiagonal. Register names, environment layout,
		 *          step structure (P0 / T / swap / T.dag) and the suggested reserve
		 *          match the QRAM path so the LCU container and post-selection
		 *          conditions carry over unchanged.
		 */
		template<typename Ty>
		class QuantumWalkNSteps_Tridiagonal
		{
		public:
			/** @brief Name of the data-table offset register */
			std::string data_offset = "data_offset";
			/** @brief Name of the sparse-table offset register */
			std::string sparse_offset = "sparse_offset";
			/** @brief Name of the row-index (input vector) register */
			std::string j = "row_id";
			/** @brief Name of the Boolean flag b1 register */
			std::string b1 = "reg_b1";
			/** @brief Name of the column-index register */
			std::string k = "col_id";
			/** @brief Name of the Boolean flag b2 register */
			std::string b2 = "reg_b2";
			/** @brief Name of the j-side auxiliary register */
			std::string j_comp = "j_comp";
			/** @brief Name of the k-side auxiliary register */
			std::string k_comp = "k_comp";
			/** @brief Sparse-matrix metadata (rotation-angle operator) */
			SparseMatrix mat;
			/** @brief Arithmetic matrix description */
			TridiagonalSpec spec;
			/** @brief Element quantization width, matrix order, slots per row */
			size_t data_size, n_row, nnz_col;
			/** @brief Default register width (max of index and element widths) */
			size_t default_register_size;
			/** @brief Value the sparse-offset register is initialized to (= 4n, the
			 *         combined-table sparsity half start, mirroring the QRAM layout) */
			size_t offset = 0;
			/** @brief Suggested state-size reserve constant */
			constexpr static int suggest_reserve = 1024000;

			/**
			 * @brief Constructor
			 * @param mat_ Positive-only SparseMatrix carrying the quantized elements
			 * @param spec_ Matching arithmetic spec
			 */
			QuantumWalkNSteps_Tridiagonal(const SparseMatrix& mat_, const TridiagonalSpec& spec_)
			{
				mat = mat_;
				spec = spec_;
				data_size = mat.data_size;
				n_row = mat.n_row;
				nnz_col = mat.nnz_col;
				offset = mat.get_sparsity_offset();
				default_register_size = std::max(size_t(log2(mat.n_row)) + 1, data_size);
			}

			/**
			 * @brief Gets the name of the register holding the input vector
			 */
			std::string GetVecInputReg() const
			{
				return j;
			}

			/**
			 * @brief Gets the initialization width of the input register
			 */
			size_t get_init_size() const
			{
				return log2(mat.n_row);
			}

			/**
			 * @brief Registers all registers needed by the walk
			 */
			void InitEnvironment()
			{
				System::add_register(data_offset, UnsignedInteger, default_register_size);
				System::add_register(sparse_offset, UnsignedInteger, default_register_size);
				System::add_register(j, UnsignedInteger, default_register_size);
				System::add_register(b1, Boolean, 1);
				System::add_register(k, UnsignedInteger, default_register_size);
				System::add_register(b2, Boolean, 1);
				System::add_register(j_comp, UnsignedInteger, default_register_size);
				System::add_register(k_comp, UnsignedInteger, default_register_size);
			}

			/**
			 * @brief Creates the initial system state (sparse offset zeroed)
			 */
			Ty CreateSys()
			{
				Ty system_states(1);
				Init_Unsafe(sparse_offset, offset)(system_states);
				return system_states;
			}

			/**
			 * @brief First step of the walk (without the phase flip)
			 */
			void FirstStep(Ty& system_states)
			{
				T_Tridiagonal(data_offset, sparse_offset, j, b1, k, b2, k_comp,
					default_register_size, &mat, spec)
					(system_states);

				Swap_General_General(j, k)(system_states);
				Swap_General_General(b1, b2)(system_states);
				Swap_General_General(j_comp, k_comp)(system_states);

				T_Tridiagonal(data_offset, sparse_offset, j, b1, k, b2, k_comp,
					default_register_size, &mat, spec)
					.dag(system_states);
			}

			/**
			 * @brief Implementation of a single walk step
			 */
			void StepImplOneStep(Ty& system_states)
			{
				ZeroConditionalPhaseFlip({ j_comp, k_comp, b1, k, b2 })
					(system_states);

				T_Tridiagonal(data_offset, sparse_offset, j, b1, k, b2, k_comp,
					default_register_size, &mat, spec)
					(system_states);

				CheckNan()(system_states);

				Swap_General_General(j, k)(system_states);
				Swap_General_General(b1, b2)(system_states);
				Swap_General_General(j_comp, k_comp)(system_states);

				T_Tridiagonal(data_offset, sparse_offset, j, b1, k, b2, k_comp,
					default_register_size, &mat, spec)
					.dag(system_states);

				CheckNan()(system_states);
			}

			/**
			 * @brief Advances the walk by two steps
			 */
			void Step(Ty& system_states)
			{
				profiler _("Step");
				StepImplOneStep(system_states);
				StepImplOneStep(system_states);
			}
		};

		/**
		 * @brief QRAM-free LCU container for the tridiagonal CKS linear-system solve
		 * @details Mirror of LCU_Container_NoiseFree on QuantumWalkNSteps_Tridiagonal;
		 *          expansion order b = kappa^2*log(kappa/eps), truncation
		 *          j0 = sqrt(b*log(4b/eps)), identical post-selection conditions.
		 */
		template<typename StateTy = SparseState>
		struct LCU_Container_NoiseFree_Tridiagonal
		{
			/** @brief The accumulated LCU combined state */
			StateTy current_state;
			/** @brief The current walk state (advanced in place across LCU terms) */
			StateTy step_state;
			/** @brief Multi-step quantum walk manager (QRAM-free) */
			QuantumWalkNSteps_Tridiagonal<StateTy> quantum_walk_obj;
			/** @brief Condition number kappa */
			double kappa;
			/** @brief Target precision eps */
			double eps;
			/** @brief Chebyshev expansion-order parameter b */
			size_t b;
			/** @brief LCU summation truncation point */
			size_t j0;
			size_t j = 0;
			/** @brief Accumulated Chebyshev coefficient sum (LCU normalization) */
			double a = 0;
			/** @brief Chebyshev coefficient calculator */
			ChebyshevPolynomialCoefficient chebyshev_obj;
			/** @brief Optional true 1/x coefficients (overrides chebyshev_obj when set) */
			const ChebyshevInverseCoefficients* inv_coef = nullptr;

			/**
			 * @brief Switches the container to the true 1/x LCU coefficients
			 * @param coef Pointer to a ChebyshevInverseCoefficients built on the exact
			 *             walk-matrix spectral interval (caller keeps it alive)
			 * @details Sets the truncation point to the provider's J. Pass nullptr to
			 *          restore the stock binomial-tail coefficients.
			 */
			void set_inverse_coefficients(const ChebyshevInverseCoefficients* coef)
			{
				inv_coef = coef;
				j0 = coef ? coef->J : static_cast<size_t>(
					std::sqrt(double(b) * (std::log(4.0 * double(b)) - std::log(eps))));
			}

			/**
			 * @brief Constructor
			 * @param mat Positive-only SparseMatrix carrying the quantized elements
			 * @param spec Matching arithmetic spec
			 * @param kappa Condition number
			 * @param eps Target precision
			 */
			LCU_Container_NoiseFree_Tridiagonal(const SparseMatrix& mat,
				const TridiagonalSpec& spec, double kappa, double eps) :
				quantum_walk_obj(mat, spec),
				kappa(kappa), eps(eps),
				b(static_cast<size_t>(kappa* kappa* (std::log(kappa) - std::log(eps)))),
				chebyshev_obj(b),
				j0(static_cast<size_t>(std::sqrt(b* (std::log(4 * b) - std::log(eps)))))
			{
				quantum_walk_obj.InitEnvironment();
				step_state = quantum_walk_obj.CreateSys();
#ifdef USE_CUDA
				// The accumulator must be GPU-resident: a CPU-resident default
				// construction makes every per-term add_systems take the host
				// fallback (D2H copy + single-threaded std::sort of the whole
				// accumulated state, O(minutes) per term at n = 2^20).
				current_state.move_to_gpu();
#endif
			}

			/**
			 * @brief Gets the name of the register holding the input vector
			 */
			auto GetInputVecReg() const
			{
				return quantum_walk_obj.GetVecInputReg();
			}

			/**
			 * @brief Injects an external input operator (operator-instance version)
			 */
			void ExternalInput_V2(const BaseOperator& op)
			{
				op(step_state);
				ClearZero()(step_state);
				quantum_walk_obj.FirstStep(step_state);
				fmt::print("State size = {}\n", step_state.size());
			}

			/**
			 * @brief Advances the LCU by one term
			 */
			bool Step() {
				if (j <= j0) {
					if (j != 0)
					{
						quantum_walk_obj.Step(step_state);
					}

					double c;
					bool sgn;
					if (inv_coef)
					{
						c = std::fabs(inv_coef->coef[j]);
						sgn = inv_coef->coef[j] < 0.0;
					}
					else
					{
						c = chebyshev_obj.coef(j);
						sgn = chebyshev_obj.sign(j);
					}
					a += c;
					Add(step_state, c, sgn);
					++j;
					return true;
				}
				else
					return false;
			}

			/**
			 * @brief Accumulates the state, scaled by its coefficient, into the LCU state
			 */
			void Add(const StateTy& new_state, double coef, bool sign)
			{
				if (sign)
					coef *= -1;

				add_systems(current_state, new_state, coef);
			}

			/**
			 * @brief Computes the post-selection success probability (internal)
			 */
			double _impl_partial_trace(StateTy& state) const
			{
				double ret = PartialTraceSelect({
					  {quantum_walk_obj.b1, 0},
					  {quantum_walk_obj.k, 0},
					  {quantum_walk_obj.b2, 0},
					  {quantum_walk_obj.j_comp, 0},
					  {quantum_walk_obj.k_comp, 0},
					  {quantum_walk_obj.sparse_offset, quantum_walk_obj.offset},
					  {quantum_walk_obj.data_offset, 0}
					})(state);

				ret *= PartialTraceSelectRange(
					quantum_walk_obj.j,
					{ 0, quantum_walk_obj.n_row - 1 })(state);

				return 1.0 / ret / ret / a / a;
			}

			/**
			 * @brief Computes the post-selection success probability (destructive)
			 */
			double PartialTrace()
			{
				return _impl_partial_trace(current_state);
			}

			/**
			 * @brief Computes the post-selection success probability (non-destructive)
			 */
			std::tuple<StateTy, double> PartialTrace_Nondestructive()  const
			{
				StateTy state = current_state;
				double ret = _impl_partial_trace(state);
				return { state, ret };
			}
		};

	}
}
