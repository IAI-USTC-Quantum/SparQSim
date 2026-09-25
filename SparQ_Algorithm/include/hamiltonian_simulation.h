/**
 * @file hamiltonian_simulation.h
 * @brief Algorithm building blocks for the CKS quantum walk and Hamiltonian simulation / linear-system solving
 * @details Targets Childs-Kothari-Somma (CKS) type algorithms: based on the compact QRAM storage of sparse matrices
 *          (a quantized-element data table + a fixed-length-per-row sparse column-index table), it provides
 *          sparse-matrix oracles (SparseMatrixOracle1 for element queries, SparseMatrixOracle2 — the quantum
 *          binary search converting between column indices and sparse slots), the state-preparation operator T,
 *          the single-step quantum walk QuantumWalk, and the multi-step walk manager QuantumWalkNSteps.
 *          Powers of the walk operator W = T† · P0 · T · Swap correspond to Chebyshev polynomials of the matrix;
 *          the LCU container combines Σ_j c_j · W^(2j+1) with Chebyshev coefficients to approximate the target function.
 *          Together with the BlockEncoding module it belongs to the block-encoding / Hamiltonian-simulation
 *          algorithm family, and its QRAM access semantics are consistent with SparQ/include/qram.h.
 */
#pragma once
#include "sparse_state_simulator.h"
#include "matrix.h"

namespace qram_simulator
{	
	namespace CKS {
		/** @brief Quantum-walk rotation-angle function type: generates a 2x2 unitary rotation matrix from
		 *  the quantized matrix element value v and its row/column position (row, col) */
		using walk_angle_function_t = std::function<u22_t(uint64_t, size_t row, size_t col)>;

			/**
			 * @brief Generates the 2x2 rotation matrix of the quantum walk (case where all matrix elements are
			 *          non-negative)
			 * @param mat_data_size Quantization bit width of the matrix element
			 * @param v Quantized matrix element value
			 * @param row Row index of the element (unused in this overload)
			 * @param col Column index of the element (unused in this overload)
			 * @param mat Output buffer; the 2x2 complex matrix is written in the real/imaginary interleaved
			 *          layout of u22_t
			 * @details Let Amax = 2^mat_data_size - 1 and a = v / Amax; generates the rotation matrix
			 *          [[sqrt(a), -sqrt(1-a)], [sqrt(1-a), sqrt(a)]],
			 *          whose rotation angle theta satisfies cos(theta) = sqrt(a).
			 *          It is used in the quantum walk to encode matrix elements through the amplitude ratio sqrt(a).
			 */
			HOST_DEVICE	inline void _get_coef_positive_only(size_t mat_data_size, size_t v, size_t row, size_t col, double* mat)
		{
			uint64_t Amax_real = pow2(mat_data_size) - 1;
			v = v % (Amax_real + 1);
			double x = std::sqrt(v * 1.0 / Amax_real);
			double y = std::sqrt(1 - v * 1.0 / Amax_real);

			// return { x, -y,  y, x };
			// use mat index to represent the above matrix
			mat[0] = x;
			mat[1] = 0;
			mat[2] = -y;
			mat[3] = 0;
			mat[4] = y;
			mat[5] = 0;
			mat[6] = x;
			mat[7] = 0;
		}

		//u22_t _get_coef(const SparseMatrix& mat, size_t v, size_t row, size_t col);
		/**
		 * @brief Generates the 2x2 rotation matrix of the quantum walk (positive-only elements case), returned as u22_t
		 * @details The parameters have the same meaning as in the double* buffer overload; directly returns the
		 *          rotation matrix.
		 */
		HOST_DEVICE	inline u22_t _get_coef_positive_only(size_t mat_data_size, size_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_positive_only(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;
		}

		/**
		 * @brief Generates the 2x2 rotation matrix of the quantum walk (general case allowing negative elements)
		 * @param mat_data_size Quantization bit width of the matrix element
		 * @param v Quantized matrix element value (interpreted in two's complement)
		 * @param row Row index of the element (used to fix the sign convention for negative elements)
		 * @param col Column index of the element (used to fix the sign convention for negative elements)
		 * @param mat Output buffer; the 2x2 complex matrix is written in the real/imaginary interleaved layout of u22_t
		 * @details Let Amax = 2^(mat_data_size-1) - 1. For non-negative elements it coincides with the positive-only
		 *          case, generating [[sqrt(a), -sqrt(1-a)], [sqrt(1-a), sqrt(a)]] (a = v/Amax);
		 *          for negative elements the diagonal entries become ±i·sqrt(|a|) and the anti-diagonal entries
		 *          sqrt(1-|a|), with the sign of the diagonal entries chosen by comparing row and col (+i when
		 *          row > col, -i when row < col), providing a consistent phase convention for the conjugate-symmetric
		 *          elements of a Hermitian matrix.
		 */
		HOST_DEVICE	inline void _get_coef_common(size_t mat_data_size, uint64_t v, size_t row, size_t col, double* mat)
		{
			uint64_t Amax_real = pow2(mat_data_size - 1) - 1;
			v = v % pow2(mat_data_size);
			int64_t v_real = get_complement(v, mat_data_size);
			if (v_real >= 0)
			{
				double x = std::sqrt(v_real * 1.0 / Amax_real);
				double y = std::sqrt(1 - v_real * 1.0 / Amax_real);

				// return { x, -y,  y, x };
				// use mat index to represent the above matrix
				mat[0] = x;
				mat[1] = 0;
				mat[2] = -y;
				mat[3] = 0;
				mat[4] = y;
				mat[5] = 0;
				mat[6] = x;
				mat[7] = 0;
			}
			else
			{
				double x = std::sqrt(-v_real * 1.0 / Amax_real);
				double y = std::sqrt(1 + v_real * 1.0 / Amax_real);

				if (row > col)
				{
					//return { complex_t{0, x}, complex_t{y, 0},
					//		 complex_t{y, 0}, complex_t{0, x} };
					// use mat index to represent the above matrix
					mat[0] = 0;
					mat[1] = x;
					mat[2] = y;
					mat[3] = 0;
					mat[4] = y;
					mat[5] = 0;
					mat[6] = 0;
					mat[7] = x;
				}
				else
				{
					//return { complex_t{0, -x}, complex_t{y, 0},
					//		 complex_t{y, 0}, complex_t{0, -x} };
					// use mat index to represent the above matrix
					mat[0] = 0;
					mat[1] = -x;
					mat[2] = y;
					mat[3] = 0;
					mat[4] = y;
					mat[5] = 0;
					mat[6] = 0;
					mat[7] = -x;
				}
			}
		}

		/**
		 * @brief Generates the 2x2 rotation matrix of the quantum walk (general case allowing negative elements),
		 *          returned as u22_t
		 * @details The parameters have the same meaning as in the double* buffer overload; directly returns the
		 *          rotation matrix.
		 */
		HOST_DEVICE	inline u22_t _get_coef_common(size_t mat_data_size, uint64_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_common(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;			
		}

		/**
		 * @brief Computes the conjugate transpose (dagger) of a 2x2 matrix in place
		 * @param mat 2x2 complex matrix (real/imaginary interleaved layout of u22_t, modified in place)
		 */
		HOST_DEVICE	inline void u22_dagger(double* mat)
		{
			double tmp;
			mat[1] = -mat[1];
			mat[3] = -mat[3];
			mat[5] = -mat[5];
			mat[7] = -mat[7];

			tmp = mat[2];
			mat[2] = mat[4];
			mat[4] = tmp;

			tmp = mat[5];
			mat[3] = mat[5];
			mat[5] = tmp;	
		}

		/**
		 * @brief Generates the inverse of the quantum-walk 2x2 rotation matrix (positive-only elements case)
		 * @details First generates the forward rotation matrix, then takes its conjugate transpose (dagger).
		 *          The parameters have the same meaning as in the forward version.
		 */
		HOST_DEVICE	inline void  _get_coef_positive_only_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col, double* mat)
		{
			_get_coef_positive_only(mat_data_size, v, row, col, mat);
			u22_dagger(mat);
		}

		/**
		 * @brief Generates the inverse of the quantum-walk 2x2 rotation matrix (general case allowing negative
		 *          elements)
		 * @details First generates the forward rotation matrix, then takes its conjugate transpose (dagger).
		 *          The parameters have the same meaning as in the forward version.
		 */
		HOST_DEVICE	inline void _get_coef_common_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col, double* mat)
		{
			_get_coef_common(mat_data_size, v, row, col, mat);
			u22_dagger(mat);
		}

		/**
		 * @brief Generates the inverse of the quantum-walk 2x2 rotation matrix (positive-only elements case),
		 *          returned as u22_t
		 * @details The parameters have the same meaning as in the double* buffer overload; directly returns the
		 *          inverse rotation matrix.
		 */
		HOST_DEVICE	inline u22_t _get_coef_positive_only_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_positive_only_inv(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;
		}

		/**
		 * @brief Generates the inverse of the quantum-walk 2x2 rotation matrix (general case allowing negative
		 *          elements), returned as u22_t
		 * @details The parameters have the same meaning as in the double* buffer overload; directly returns the
		 *          inverse rotation matrix.
		 */
		HOST_DEVICE	inline u22_t _get_coef_common_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_common_inv(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;
		}

			/**
			 * @brief Generates the quantum-walk rotation matrix according to the sparse matrix's sign convention
			 * @param mat Sparse matrix (its positive_only and data_size metadata are used)
			 * @param v Quantized matrix element value
			 * @param row Row index of the element
			 * @param col Column index of the element
			 * @return 2x2 unitary rotation matrix (forward)
			 * @details If the matrix contains only non-negative elements, the _get_coef_positive_only path is taken;
			 *          otherwise the _get_coef_common path, which allows negative elements, is taken.
			 */
			inline u22_t make_qw_rotation_matrix(const SparseMatrix& mat, uint64_t v, size_t row, size_t col)
			{
				if (mat.positive_only)
					return _get_coef_positive_only(mat.data_size, v, row, col);
				return _get_coef_common(mat.data_size, v, row, col);
			}

			/**
			 * @brief Generates the inverse (dagger) of the quantum-walk rotation matrix according to the sparse
			 *          matrix's sign convention
			 * @param mat Sparse matrix (its positive_only and data_size metadata are used)
			 * @param v Quantized matrix element value
			 * @param row Row index of the element
			 * @param col Column index of the element
			 * @return Inverse of the 2x2 unitary rotation matrix
			 */
			inline u22_t make_qw_rotation_matrix_inv(const SparseMatrix& mat, uint64_t v, size_t row, size_t col)
			{
				if (mat.positive_only)
					return _get_coef_positive_only_inv(mat.data_size, v, row, col);
				return _get_coef_common_inv(mat.data_size, v, row, col);
			}

			/**
			 * @brief Constructs a lazily evaluated walk rotation-angle function (forward)
			 * @param mat Sparse matrix (captures its positive_only and data_size)
			 * @return A function object taking (v, row, col) as input and returning the 2x2 rotation matrix
			 */
			inline walk_angle_function_t make_func(const SparseMatrix& mat)
			{
				size_t mat_data_size = mat.data_size;
				if (mat.positive_only)
				{
					auto func = [mat_data_size] HOST_DEVICE(uint64_t v, size_t row, size_t col)
						{
							return _get_coef_positive_only(mat_data_size, v, row, col);
						};
					return func;
				}
				auto func = [mat_data_size] HOST_DEVICE(uint64_t v, size_t row, size_t col)
					{
						return _get_coef_common(mat_data_size, v, row, col);
					};
				return func;
			}

			/**
			 * @brief Constructs a lazily evaluated walk rotation-angle function (inverse / dagger)
			 * @param mat Sparse matrix (captures its positive_only and data_size)
			 * @return A function object taking (v, row, col) as input and returning the 2x2 inverse rotation matrix
			 */
			inline walk_angle_function_t make_func_inv(const SparseMatrix& mat)
			{
				size_t mat_data_size = mat.data_size;
				if (mat.positive_only)
				{
					auto func = [mat_data_size] HOST_DEVICE(uint64_t v, size_t row, size_t col)
						{
							return _get_coef_positive_only_inv(mat_data_size, v, row, col);
						};
					return func;
				}
				auto func = [mat_data_size] HOST_DEVICE(uint64_t v, size_t row, size_t col)
					{
						return _get_coef_common_inv(mat_data_size, v, row, col);
					};
				return func;
			}

			// =============================================================================
			// Primitive + Composite Operators
			// These classes inherit BaseOperator or SelfAdjointOperator and directly
			// manipulate quantum state. They are composed by the flow-control classes below.
			// =============================================================================

			/**
			 * @brief Quantum-walk rotation-angle computation operator (self-adjoint)
			 * @details Computes the ratio ratio = |a_jk| / Amax from the quantized matrix element v (for positive-only
			 *          elements Amax = 2^data_size - 1; in the general case Amax = 2^(data_size-1) - 1, with v
			 *          interpreted in two's complement and taken in absolute value), then quantizes the rotation angle
			 *          theta = arccos(sqrt(ratio)) / (2*pi) into a Rational fixed-point value
			 *          and XORs it into the output register. Usually combined with CondRot_Fixed_Bool to form a
			 *          two-step equivalent implementation of the general conditional rotation
			 *          CondRot_General_Bool_QW: first compute the angle, then apply a fixed-angle rotation, and
			 *          finally uncompute the angle. Supports conditional control (ClassControllable).
			 */
			struct GetQWRotateAngle_Int_Int_Int : SelfAdjointOperator
			{
				using SelfAdjointOperator::operator();
				using SelfAdjointOperator::dag;

				/** @brief Register ID of the quantized matrix element */
				size_t data_id;
				/** @brief Register ID of the row index */
				size_t row_id;
				/** @brief Register ID of the column index (sparse slot) */
				size_t col_id;
				/** @brief Register ID of the rotation-angle output (Rational fixed-point) */
				size_t out_id;
				/** @brief Pointer to the sparse matrix (provides quantization and sign-convention metadata) */
				const SparseMatrix* mat;
				ClassControllable

				/**
				 * @brief Constructor (register-name version)
				 * @param data_ Name of the quantized matrix element register
				 * @param row_ Name of the row-index register
				 * @param col_ Name of the column-index (sparse slot) register
				 * @param out_ Name of the rotation-angle output register
				 * @param mat_ Pointer to the sparse matrix
				 */
				GetQWRotateAngle_Int_Int_Int(
					std::string_view data_, std::string_view row_, std::string_view col_,
					std::string_view out_, const SparseMatrix* mat_)
					: data_id(System::get(data_)), row_id(System::get(row_)),
					col_id(System::get(col_)), out_id(System::get(out_)), mat(mat_)
				{
				}

				/**
				 * @brief Constructor (register-ID version)
				 * @param data_ Register ID of the quantized matrix element
				 * @param row_ Register ID of the row index
				 * @param col_ Register ID of the column index (sparse slot)
				 * @param out_ Register ID of the rotation-angle output
				 * @param mat_ Pointer to the sparse matrix
				 */
				GetQWRotateAngle_Int_Int_Int(
					size_t data_, size_t row_, size_t col_, size_t out_, const SparseMatrix* mat_)
					: data_id(data_), row_id(row_), col_id(col_), out_id(out_), mat(mat_)
				{
				}

				/**
				 * @brief Computes the walk rotation angle and writes it to the output register
				 * @param state System state vector
				 * @note The operator is self-adjoint: two consecutive invocations cancel each other.
				 */
				void operator()(std::vector<System>& state) const;
			};

		// Chebyshev approach
		/**
		 * @brief Chebyshev polynomial expansion coefficients for the CKS algorithm
		 * @details Provides the coefficients c_j and their signs for the LCU combination Σ_j c_j · W^(2j+1):
		 *          expansion order b = kappa^2 · log(kappa/eps), truncation point j0 = sqrt(b·log(4b/eps)).
		 *          For large b, c_j is computed with the erfc asymptotic formula; for small b,
		 *          the binomial-distribution tail probability is summed exactly; odd-j terms take a negative sign.
		 */
		struct ChebyshevPolynomialCoefficient
		{
			/** @brief Expansion-order parameter b = kappa^2 · log(kappa/eps) */
			size_t b;

			/**
			 * @brief Constructor
			 * @param b_ Chebyshev expansion-order parameter
			 */
			ChebyshevPolynomialCoefficient(size_t b_)
				:b(b_)
			{ }

			// C(Big, Small) (pick Small from Big)
			// Big*...(Big-Small+1)/(Small*...1)
			/**
			 * @brief Computes the binomial coefficient C(Big, Small) / 4^b, scaled by 4^b
			 * @param Big Upper parameter of the binomial coefficient
			 * @param Small Lower parameter of the binomial coefficient
			 * @return C(Big, Small) / 4^b
			 * @note During the recursion, as soon as the intermediate value exceeds 2^b it is divided by 2^b to
			 *          avoid overflow.
			 */
			double C(size_t Big, size_t Small);

			// Given b, provide j from 0 to b-1
			/**
			 * @brief Computes the j-th coefficient c_j of the Chebyshev expansion
			 * @param j Term index (0 to b-1)
			 * @return Coefficient c_j
			 * @details For b > 100, uses the erfc asymptotic formula c_j = 2·erfc((j+0.5)/sqrt(b));
			 *          otherwise sums the binomial-distribution tail exactly: c_j = 4 · Σ_{i=j+1}^{b} C(2b, b+i).
			 */
			double coef(size_t j);

			// return true if - (odd)
			// return false if + (even)
			/**
			 * @brief Sign of the j-th term
			 * @param j Term index
			 * @return Returns true for odd j (negative sign), false for even j (positive sign)
			 */
			bool sign(size_t j);

			/**
			 * @brief Number of quantum-walk steps for the j-th term
			 * @param j Term index
			 * @return Number of walk steps 2j + 1
			 */
			size_t step(size_t j);
		};

		/* For quantum walk */
		// Note: CondRot_General_Bool_QW is kept for future specialized use but not exported to Python.
		// Current code uses GetQWRotateAngle + CondRot_Fixed_Bool instead.
		/**
		 * @brief General conditional-rotation operator for the quantum walk
		 * @details Generates a 2x2 unitary matrix from the quantized matrix element (v, j, k) via the walk
		 *          rotation-angle function and applies it to the Boolean output register: state branches are first
		 *          sorted and grouped by the output register, then dispatched to the matching implementation by
		 *          matrix shape (diagonal / anti-diagonal / general); dag uses the inverse rotation-angle function.
		 *          Kept for future specialized paths; the current main path replaces it with the two-step
		 *          combination GetQWRotateAngle + CondRot_Fixed_Bool.
		 */
		struct CondRot_General_Bool_QW : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief Name of the row-index register */
			std::string j;
			/** @brief Name of the column-index (sparse slot) register */
			std::string k;
			/** @brief Name of the input (matrix element) register */
			std::string in_name;
			/** @brief Name of the output Boolean register */
			std::string out_name;
			/** @brief Register ID of the row index */
			size_t j_id;
			/** @brief Register ID of the column index (sparse slot) */
			size_t k_id;
			/** @brief Register ID of the input (matrix element) */
			size_t in_id;
			/** @brief Register ID of the output Boolean register */
			size_t out_id;
			/** @brief Pointer to the sparse matrix (provides quantization and sign-convention metadata) */
			const SparseMatrix* mat;

			/**
			 * @brief Constructor
			 * @param j_ Name of the row-index register
			 * @param k_ Name of the column-index (sparse slot) register
			 * @param reg_in Name of the input (matrix element) register
			 * @param reg_out Name of the output Boolean register
			 * @param mat Pointer to the sparse matrix
			 */
			CondRot_General_Bool_QW(
				std::string_view j_, std::string_view k_, std::string_view reg_in, std::string_view reg_out,
				const SparseMatrix* mat
			)
				: j(j_), k(k_), in_name(reg_in), out_name(reg_out),
				in_id(System::get(reg_in)), out_id(System::get(reg_out)),
				j_id(System::get(j)), k_id(System::get(k)), mat(mat)
			{
			}

			/**
			 * @brief Applies the rotation to the branches within the state interval [l, r)
			 * @param l Left boundary of the interval
			 * @param r Right boundary of the interval
			 * @param state System state vector
			 * @param func Walk rotation-angle function (generates the 2x2 matrix from the matrix element and its
			 *          row/column position)
			 */
			void operate(size_t l, size_t r, std::vector<System>& state, walk_angle_function_t func) const;

			/**
			 * @brief Checks whether the matrix is diagonal
			 * @param data 2x2 matrix
			 * @return Whether the matrix is diagonal
			 */
			static bool _is_diagonal(const u22_t& data);

			/**
			 * @brief Diagonal-matrix operation implementation (creates no new branches; scales amplitudes in place)
			 * @param l Left boundary of the interval
			 * @param r Right boundary of the interval
			 * @param state System state vector
			 * @param mat 2x2 diagonal matrix
			 */
			void _operate_diagonal(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief Checks whether the matrix is anti-diagonal
			 * @param data 2x2 matrix
			 * @return Whether the matrix is anti-diagonal
			 */
			static bool _is_off_diagonal(const u22_t& data);

			/**
			 * @brief Anti-diagonal matrix operation implementation (creates no new branches; swaps and flips the
			 *          Boolean value in place)
			 * @param l Left boundary of the interval
			 * @param r Right boundary of the interval
			 * @param state System state vector
			 * @param mat 2x2 anti-diagonal matrix
			 */
			void _operate_off_diagonal(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief General 2x2 matrix operation implementation (may create new branches)
			 * @param l Left boundary of the interval
			 * @param r Right boundary of the interval
			 * @param state System state vector
			 * @param mat General 2x2 unitary matrix
			 */
			void _operate_general(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief Applies the general conditional rotation (forward)
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;

			/**
			 * @brief Applies the dagger of the general conditional rotation
			 * @param state System state vector
			 */
			void dag(std::vector<System>& state) const;
#ifdef USE_CUDA
			void operator()(CuSparseState& state) const;
			void dag(CuSparseState& state) const;
#endif
		};

		// quantum binary search
		/**
		 * @brief QRAM-based quantum binary search operator (self-adjoint)
		 * @details Searches the sorted QRAM memory region [offset, offset + total_length) for the address whose
		 *          value equals the target register's value: in each round, flag controls whether subsequent rounds
		 *          remain active; the interval's midpoint address is taken, its value loaded via QRAM and compared
		 *          with the target — on a hit, the midpoint address is XORed into the result register and flag is
		 *          updated to end the active search; otherwise the interval shrinks according to the comparison.
		 *          Each round's temporary registers are saved with Push and uncomputed by Pop in reverse order,
		 *          keeping the whole operation reversible and self-adjoint (impl_dag simply reuses impl).
		 */
		struct QuantumBinarySearch : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief Pointer to the QRAM circuit (provides the sorted memory being searched) */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief Length of the search interval */
			size_t total_length;
			/** @brief Number of binary-search rounds (log2(total_length) + 1) */
			size_t max_step;

			/** @brief Register ID of the search start offset (its value is the interval's left-end address) */
			size_t address_offset_id;
			/** @brief Register ID of the target value */
			size_t target_id;
			/** @brief Register ID of the result (the hit address is written by XOR) */
			size_t result_id;

			/**
			 * @brief Constructor (register-name version)
			 * @param qram Pointer to the QRAM circuit
			 * @param address_offset_register Name of the search start offset register
			 * @param total_length_ Length of the search interval
			 * @param target_register Name of the target value register
			 * @param result_register Name of the result register
			 */
			QuantumBinarySearch(qram_qutrit::QRAMCircuit* qram,
				std::string_view address_offset_register,
				size_t total_length_,
				std::string_view target_register,
				std::string_view result_register);

			/**
			 * @brief Constructor (register-ID version)
			 * @param qram Pointer to the QRAM circuit
			 * @param address_offset_register Register ID of the search start offset
			 * @param total_length_ Length of the search interval
			 * @param target_register Register ID of the target value
			 * @param result_register Register ID of the result
			 */
			QuantumBinarySearch(qram_qutrit::QRAMCircuit* qram,
				size_t address_offset_register,
				size_t total_length_,
				size_t target_register,
				size_t result_register);

			/**
			 * @brief Forward implementation of the binary search (also serves as the dagger implementation)
			 * @param state System state vector
			 * @details After running max_step search rounds forward, all temporary registers are uncomputed in
			 *          reverse order, so the whole is a self-adjoint operation.
			 */
			template<typename Ty>
			void impl(Ty& state) const {
				profiler _("QBS");
				auto flag = AddRegister("flag", Boolean, 1)(state);
				X_Bool(flag, 0)(state);

				auto compare_less = AddRegister("compare_less", Boolean, 1)(state);
				auto compare_equal = AddRegister("compare_equal", Boolean, 1)(state);
				auto left_register = AddRegister("left_register", UnsignedInteger, qram->address_size + 1)(state);
				auto right_register = AddRegister("right_register", UnsignedInteger, qram->address_size + 1)(state);
				auto mid_register = AddRegister("mid_register", UnsignedInteger, qram->address_size + 1)(state);
				auto midval_register = AddRegister("midval_register", UnsignedInteger, qram->address_size)(state);

				// int flag_id = System::get("flag");

				Assign(address_offset_id, left_register)(state);
				Add_UInt_ConstUInt(left_register, total_length, right_register)(state);

				for (size_t iteration_level = 0; iteration_level < max_step; ++iteration_level)
				{
					/* compute the mid */
					GetMid_UInt_UInt(left_register, right_register, mid_register)
						.conditioned_by_nonzeros(flag)(state);
					//(StatePrint(StatePrintDisplay::Detail))(state);

					/* load value */
					QRAMLoad(qram, mid_register, midval_register)
						.conditioned_by_nonzeros(flag)(state);
					//(StatePrint(StatePrintDisplay::Detail))(state);

					/* compare to decide the branch */
					Compare_UInt_UInt(midval_register, target_id, compare_less, compare_equal)
						.conditioned_by_nonzeros(flag)(state);

					/* if found, move the mid register to outside */
					Assign(mid_register, result_id)
						.conditioned_by_nonzeros({ compare_equal, flag })(state);

					if (iteration_level != max_step - 1)
					{
						/* update flag (unnecessary condition) */
						Assign(compare_equal, flag)(state);
						/* update the left/right register with mid register */
						Swap_General_General(left_register, mid_register)
							.conditioned_by_nonzeros({ compare_less, flag })(state);

						X_Bool(compare_less, 0)(state);

						Swap_General_General(right_register, mid_register)
							.conditioned_by_nonzeros({ compare_less, flag })(state);

						Push(mid_register, fmt::format("{}-{}", "mid_register", iteration_level))(state);
						Push(midval_register, fmt::format("{}-{}", "midval_register", iteration_level))(state);
						Push(compare_less, fmt::format("{}-{}", "compare_less", iteration_level))(state);
						Push(compare_equal, fmt::format("{}-{}", "compare_equal", iteration_level))(state);
					}
				}

				// FlipBools(result_register).conditioned_by("flag")(state);

				// todo: auto-uncompute
				// uncompute all garbage variables
				for (size_t iteration_level = max_step; iteration_level --> 0;)
				{
					if (iteration_level != max_step - 1)
					{
						(Pop(compare_equal))(state);
						(Pop(compare_less))(state);
						(Pop(midval_register))(state);
						(Pop(mid_register))(state);

						Swap_General_General(right_register, mid_register)
							.conditioned_by_nonzeros({ compare_less, flag })(state);

						X_Bool(compare_less, 0)(state);
						Swap_General_General(left_register, mid_register)
							.conditioned_by_nonzeros({ compare_less, flag })(state);

						Assign(compare_equal, flag)(state);
					}

					Compare_UInt_UInt(midval_register, target_id, compare_less, compare_equal)
						.conditioned_by_nonzeros(flag)(state);

					QRAMLoad(qram, mid_register, midval_register)
						.conditioned_by_nonzeros(flag)(state);

					GetMid_UInt_UInt(left_register, right_register, mid_register)
						.conditioned_by_nonzeros(flag)(state);
				}

				Add_UInt_ConstUInt(left_register, total_length, right_register)(state);
				Assign(address_offset_id, left_register)(state);
				X_Bool(flag, 0)(state);
				(RemoveRegister(compare_less))(state);
				(RemoveRegister(compare_equal))(state);
				(RemoveRegister(left_register))(state);
				(RemoveRegister(right_register))(state);
				(RemoveRegister(mid_register))(state);
				(RemoveRegister(midval_register))(state);
				(RemoveRegister(flag))(state);
			}
		

			/**
			 * @brief Dagger implementation of the binary search
			 * @param state System state vector
			 * @details The operator is self-adjoint and directly reuses the forward implementation.
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const {
				impl<Ty>(state);
			}

			COMPOSITE_OPERATION
		};

		// quantum binary search
		/**
		 * @brief Fast version of the quantum binary search
		 * @details Directly performs a classical binary search on each state branch at the simulator level
		 *          (avoiding the per-round QRAM load and uncomputation overhead) and XORs the hit address into
		 *          the result register; the search semantics match QuantumBinarySearch.
		 *          Used by SparseMatrixOracle2 for sparse-slot localization.
		 */
		struct QuantumBinarySearch_Fast : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief Pointer to the QRAM circuit (provides the sorted memory being searched) */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief Length of the search interval */
			size_t total_length;
			/** @brief Number of binary-search rounds (log2(total_length) + 1) */
			size_t max_step;

			/** @brief Register ID of the search start offset (its value is the interval's left-end address) */
			size_t address_offset_id;
			/** @brief Register ID of the target value */
			size_t target_id;
			/** @brief Register ID of the result (the hit address is written by XOR) */
			size_t result_id;

			//int iteration_level;

			/**
			 * @brief Constructor (register-name version)
			 * @param qram Pointer to the QRAM circuit
			 * @param address_offset_register Name of the search start offset register
			 * @param total_length_ Length of the search interval
			 * @param target_register Name of the target value register
			 * @param result_register Name of the result register
			 */
			QuantumBinarySearch_Fast(qram_qutrit::QRAMCircuit* qram,
				std::string_view address_offset_register,
				size_t total_length_,
				std::string_view target_register,
				std::string_view result_register);

			/**
			 * @brief Constructor (register-ID version)
			 * @param qram Pointer to the QRAM circuit
			 * @param address_offset_register Register ID of the search start offset
			 * @param total_length_ Length of the search interval
			 * @param target_register Register ID of the target value
			 * @param result_register Register ID of the result
			 */
			QuantumBinarySearch_Fast(qram_qutrit::QRAMCircuit* qram,
				size_t address_offset_register,
				size_t total_length_,
				size_t target_register,
				size_t result_register);

			/**
			 * @brief Performs a classical binary search on a single state branch
			 * @param offset Start address of the search interval
			 * @param target Target value
			 * @return The hit address; returns 0 on a miss
			 */
			size_t binary_search(size_t offset, size_t target) const;

			/**
			 * @brief Applies the fast binary search (classical computation branch by branch)
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
#ifdef USE_CUDA
			/**
			 * @brief CUDA: applies the fast binary search
			 * @param state CUDA sparse state
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/**
		 * @brief Row-start address computation operator (self-adjoint)
		 * @details Performs row_offset ^= offset + row_sz * row, i.e. the start address of the segment corresponding
		 *          to row row in the QRAM row-wise fixed-length storage (each row occupies a fixed row_sz slots);
		 *          two invocations cancel each other.
		 */
		struct GetRowAddr : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief Register ID of the segment start offset */
			size_t offset_id;
			/** @brief Register ID of the row index */
			size_t row_id;
			/** @brief Number of slots per row */
			size_t row_sz;
			/** @brief Register ID of the row-start address output (written by XOR) */
			size_t row_offset_id;

			/**
			 * @brief Constructor (register-name version)
			 * @param reg_offset Name of the segment start offset register
			 * @param reg_row Name of the row-index register
			 * @param row_sz_ Number of slots per row
			 * @param reg_row_offset Name of the row-start address output register
			 */
			GetRowAddr(std::string_view reg_offset,
				std::string_view reg_row,
				size_t row_sz_,
				std::string_view reg_row_offset)
			{
				offset_id = System::get(reg_offset);
				row_id = System::get(reg_row);
				row_offset_id = System::get(reg_row_offset);
				row_sz = row_sz_;
			}

			/**
			 * @brief Constructor (register-ID version)
			 * @param reg_offset Register ID of the segment start offset
			 * @param reg_row Register ID of the row index
			 * @param row_sz_ Number of slots per row
			 * @param reg_row_offset Register ID of the row-start address output
			 */
			GetRowAddr(int reg_offset,
				int reg_row,
				size_t row_sz_,
				int reg_row_offset)
			{
				offset_id = reg_offset;
				row_id = reg_row;
				row_offset_id = reg_row_offset;
				row_sz = row_sz_;
			}

			/**
			 * @brief Computes the row-start address
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
#ifdef USE_CUDA
			/**
			 * @brief CUDA: computes the row-start address
			 * @param state CUDA sparse state
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/**
		 * @brief Matrix-element storage address computation operator (self-adjoint)
		 * @details Performs data_offset ^= offset + row_sz * row + col_sparse, i.e. the address in the QRAM data
		 *          table of the element (the col_sparse-th sparse slot of row row);
		 *          two invocations cancel each other.
		 */
		struct GetDataAddr : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief Register ID of the data-table start offset */
			size_t offset_id;
			/** @brief Register ID of the row index */
			size_t row_id;
			/** @brief Number of slots per row */
			size_t row_sz;
			/** @brief Register ID of the sparse slot */
			size_t col_sparse_id;
			/** @brief Register ID of the element-address output (written by XOR) */
			size_t row_data_id;

			/**
			 * @brief Constructor (register-name version)
			 * @param reg_offset Name of the data-table start offset register
			 * @param reg_row Name of the row-index register
			 * @param reg_col_sparse Name of the sparse-slot register
			 * @param row_sz_ Number of slots per row
			 * @param reg_data_offset Name of the element-address output register
			 */
			GetDataAddr(std::string_view reg_offset, std::string_view reg_row,
				std::string_view reg_col_sparse, size_t row_sz_, std::string_view reg_data_offset)
			{
				offset_id = System::get(reg_offset);
				row_id = System::get(reg_row);
				col_sparse_id = System::get(reg_col_sparse);
				row_data_id = System::get(reg_data_offset);
				row_sz = row_sz_;
			}

			/**
			 * @brief Constructor (register-ID version)
			 * @param reg_offset Register ID of the data-table start offset
			 * @param reg_row Register ID of the row index
			 * @param reg_col_sparse Register ID of the sparse slot
			 * @param row_sz_ Number of slots per row
			 * @param reg_data_offset Register ID of the element-address output
			 */
			GetDataAddr(size_t reg_offset, size_t reg_row,
				size_t reg_col_sparse, size_t row_sz_, size_t reg_data_offset)
			{
				offset_id = reg_offset;
				row_id = reg_row;
				col_sparse_id = reg_col_sparse;
				row_data_id = reg_data_offset;
				row_sz = row_sz_;
			}

			/**
			 * @brief Computes the storage address of a matrix element
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
			/**
			 * @brief CUDA: computes the storage address of a matrix element
			 * @param state CUDA sparse state
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/*
		|offset>|i>|s_j>|0>			->
		|offset>|i>|s_j>|a_{ij}>
		*/
		/**
		 * @brief Sparse-matrix oracle No. 1: matrix-element query (self-adjoint)
		 * @details Queries the QRAM data table by (row i, in-row sparse slot s_j) and loads the corresponding
		 *          quantized matrix element a_{ij}, i.e. |offset>|i>|s_j>|0> -> |offset>|i>|s_j>|a_{ij}>.
		 *          The element address is computed by GetDataAddr (offset + row_size*i + s_j);
		 *          after the load the address register is uncomputed again, keeping the whole self-adjoint.
		 */
		struct SparseMatrixOracle1 : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief Pointer to the QRAM circuit */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief Name of the data-table offset register */
			std::string reg_offset;
			/** @brief Name of the row-index register */
			std::string reg_row;
			/** @brief Name of the sparse-slot register (the element's position in the row's compact storage) */
			std::string reg_col_id; // position in the sparse-compact storage
			/** @brief Name of the query-result (quantized element) output register */
			std::string reg_output;
			/** @brief Number of slots per row */
			size_t row_size;

			/**
			 * @brief Constructor
			 * @param qram Pointer to the QRAM circuit
			 * @param reg_offset Name of the data-table offset register
			 * @param reg_row Name of the row-index register
			 * @param reg_col_id Name of the sparse-slot register
			 * @param reg_output Name of the query-result output register
			 * @param row_size_ Number of slots per row
			 */
			SparseMatrixOracle1(qram_qutrit::QRAMCircuit* qram,
				std::string_view reg_offset,
				std::string_view reg_row,
				std::string_view reg_col_id,
				std::string_view reg_output,
				size_t row_size_);

			/**
			 * @brief Forward implementation of the element query (also serves as the dagger implementation)
			 * @param state System state vector
			 * @details Computes data_addr = offset + row_size*i + s_j,
			 *          loads the element into the output register via QRAM, then calls GetDataAddr again
			 *          to uncompute the address register.
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				AddRegister("data_addr", UnsignedInteger, qram->address_size)(state);
				GetDataAddr(reg_offset, reg_row, reg_col_id, row_size, "data_addr")(state);
				QRAMLoad(qram, "data_addr", reg_output)(state);
				GetDataAddr(reg_offset, reg_row, reg_col_id, row_size, "data_addr")(state);
				RemoveRegister("data_addr")(state);
			}

			/**
			 * @brief Dagger implementation of the element query
			 * @param state System state vector
			 * @details The operator is self-adjoint and directly reuses the forward implementation.
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				impl<Ty>(state);
			}

			COMPOSITE_OPERATION
		};

		/*
		|offset>|i>|j>|0>			->
		|offset>|i>|s_j>
		*/
		/**
		 * @brief Sparse-matrix oracle No. 2: conversion from column index to sparse slot
		 * @details Via quantum binary search, converts the matrix column index j into the sparse slot s_j of that
		 *          element in the row's compact storage, i.e. |offset>|i>|j> -> |offset>|i>|s_j>.
		 *          The start address of the row's sparse table is computed by GetRowAddr; the search yields the
		 *          absolute address within the sparse table, so the column index is restored to a slot number by
		 *          loading it via QRAM and subtracting the row-start address.
		 */
		struct SparseMatrixOracle2 : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief Pointer to the QRAM circuit */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief Name of the sparse-table offset register */
			std::string reg_sparse_offset;
			/** @brief Name of the row-index register */
			std::string reg_row;
			/** @brief Name of the column-index register (the actual column index in the matrix) */
			std::string reg_col; // the column in the matrix
			/** @brief Name of the binary-search result register */
			std::string reg_search_result;
			/** @brief Number of slots per row */
			size_t row_size;

			/**
			 * @brief Constructor
			 * @param qram Pointer to the QRAM circuit
			 * @param reg_sparse_offset Name of the sparse-table offset register
			 * @param reg_row_ Name of the row-index register
			 * @param reg_col_ Name of the column-index register
			 * @param reg_search_result_ Name of the binary-search result register
			 * @param row_size Number of slots per row
			 */
			SparseMatrixOracle2(qram_qutrit::QRAMCircuit* qram,
				std::string_view reg_sparse_offset,
				std::string_view reg_row_,
				std::string_view reg_col_,
				std::string_view reg_search_result_,
				size_t row_size);

			/**
			 * @brief Forward implementation of the column-index to sparse-slot conversion
			 * @param state System state vector
			 * @details Steps in order: GetRowAddr computes the row's sparse-table start address -> quantum binary
			 *          search locates the slot holding the column index -> QRAM load restores it -> swap and subtract
			 *          the row-start address, so the column register finally holds the slot number s_j; the
			 *          implementation carries detailed step-by-step comments.
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				// |i>|j> -> |i>|s_j>

				// |offset>|i>|j>
				AddRegister("row_addr", UnsignedInteger, qram->address_size)(state);

				// |offset>|i>|j>|row_addr = offset + i * row_size>
				GetRowAddr(reg_sparse_offset, reg_row, row_size, "row_addr")(state);

				// |offset>|i>|j>|row_addr>|result = s_j>
				//QuantumBinarySearch(qram, "row_addr", row_size, reg_col, reg_search_result)(state);
				QuantumBinarySearch_Fast(qram, "row_addr", row_size, reg_col, reg_search_result)(state);

				// |offset>|i>|0>|row_addr>|result> 
				QRAMLoad(qram, reg_search_result, reg_col)(state);

				// |offset>|i>|result>|row_addr>|0> 
				Swap_General_General(reg_col, reg_search_result)(state);

				// |offset>|i>|s_j>|row_addr>|0> 
				Add_AnyInt_AnyInt_InPlace(reg_col, "row_addr").dag(state);

				// |offset>|i>|s_j>|0>|0> 
				GetRowAddr(reg_sparse_offset, reg_row, row_size, "row_addr")(state);

				RemoveRegister("row_addr")(state);
			}


			/**
			 * @brief Dagger implementation of the column-index to sparse-slot conversion
			 * @param state System state vector
			 * @details Executes the forward implementation's steps in reverse, restoring the sparse slot s_j back
			 *          into the column index j.
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				AddRegister("row_addr", UnsignedInteger, qram->address_size)(state);

				// |offset>|i>|s_j>|0>|0>|row_addr>
				GetRowAddr(reg_sparse_offset, reg_row, row_size, "row_addr")(state);
				Add_AnyInt_AnyInt_InPlace(reg_col, "row_addr")(state);
				Swap_General_General(reg_col, reg_search_result)(state);
				QRAMLoad(qram, reg_search_result, reg_col)(state);
				//QuantumBinarySearch(qram, "row_addr", row_size, reg_col, reg_search_result)(state);
				QuantumBinarySearch_Fast(qram, "row_addr", row_size, reg_col, reg_search_result)(state);
				GetRowAddr(reg_sparse_offset, reg_row, row_size, "row_addr")(state);

				RemoveRegister("row_addr")(state);
			}

			COMPOSITE_OPERATION
		};

		/*
		* To compute k from j,l, out-of-place
		* Implemented by QRAM query
		*
		* |offset>|l>|z>			->
		* |offset>|l>|z + k>
		*/

		/**
		 * @brief Computes the column index from the sparse slot (out-of-place, QRAM-query implementation)
		 * @details |offset>|l>|z> -> |offset>|l>|z + k>:
		 *          queries the QRAM at the address sparse-table offset plus slot l, obtains the corresponding column
		 *          index k and accumulates (XORs) it into the target register; the address register is uncomputed
		 *          after the query.
		 */
		struct SparseMatrixOracle2_ComputeCol : BaseOperator
		{
			using BaseOperator::operator();

			/** @brief Pointer to the QRAM circuit */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief Name of the sparse-table offset register */
			std::string sparse_offset;
			/** @brief Name of the column-index register (query output) */
			std::string k; // j
			/** @brief Name of the sparse-slot register */
			std::string l; // s_j
			/** @brief Name of the temporary address register */
			std::string addr_offset;
			/** @brief Number of slots per row */
			size_t row_size;

			/**
			 * @brief Constructor
			 * @param qram Pointer to the QRAM circuit
			 * @param sparse_offset Name of the sparse-table offset register
			 * @param k Name of the column-index register
			 * @param l Name of the sparse-slot register
			 * @param addr_offset Name of the temporary address register
			 * @param row_size Number of slots per row
			 */
			SparseMatrixOracle2_ComputeCol(qram_qutrit::QRAMCircuit* qram,
				std::string_view sparse_offset,
				std::string_view k,
				std::string_view l,
				std::string_view addr_offset,
				size_t row_size);

			/**
			 * @brief Applies the column-index computation
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/*
		* To compute l from j,k, out-of-place
		* Implemented by QBS
		*
		|offset>|k>|z>			->
		|offset>|k>|z + l>
		*/
		/**
		 * @brief Computes the sparse slot from the column index (out-of-place, quantum-binary-search implementation)
		 * @details |offset>|k>|z> -> |offset>|k>|z + l>:
		 *          performs a quantum binary search over the row's sparse-table interval with the column index k as
		 *          the target, accumulates the hit slot address (including the table offset) into l, and then
		 *          subtracts the offset to restore the relative slot number.
		 */
		struct SparseMatrixOracle2_ComputeSparsity : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief Pointer to the QRAM circuit */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief Name of the sparse-table offset register */
			std::string sparse_offset;
			/** @brief Name of the column-index register */
			std::string k; // j
			/** @brief Name of the sparse-slot register */
			std::string l; // s_j
			/** @brief Number of slots per row */
			size_t row_size;

			/**
			 * @brief Constructor
			 * @param qram Pointer to the QRAM circuit
			 * @param sparse_offset Name of the sparse-table offset register
			 * @param k Name of the column-index register
			 * @param l Name of the sparse-slot register
			 * @param row_size Number of slots per row
			 */
			SparseMatrixOracle2_ComputeSparsity(
				qram_qutrit::QRAMCircuit* qram,
				std::string_view sparse_offset,
				std::string_view k,
				std::string_view l,
				size_t row_size);

			/**
			 * @brief Applies the sparse-slot computation
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		// prepare from |j> to |\psi_j>
		/**
		 * @brief State-preparation operator T of the CKS quantum walk
		 * @details For the row index j, prepares the row's "square-root amplitude" superposition:
		 *          |j>|0> -> Σ_k sqrt(A_{j,s_k}) |j>|k> (k is the in-row sparse slot).
		 *          Workflow: apply Hadamard to the slot register k to form a uniform superposition -> Oracle1 loads
		 *          the quantized element d[j,k] -> the dagger of Oracle2 maps k from the slot to the actual column
		 *          index -> GetQWRotateAngle + CondRot_Fixed_Bool perform the conditional rotation with ratio
		 *          sqrt(A_{j,k}) -> uncompute each oracle in turn to restore the registers.
		 */
		struct T : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief Pointer to the QRAM circuit */
			qram_qutrit::QRAMCircuit* qram;
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
			/** @brief Name of the Boolean flag b2 register (target of the conditional rotation) */
			std::string reg_b2;
			/** @brief Name of the binary-search result register */
			std::string reg_search_result;
			/** @brief Number of non-zero elements (slots) per row */
			size_t nnz_col;
			/** @brief Bit width of the temporary data register (max of the address width and the element width) */
			size_t data_size;
			/** @brief Pointer to the sparse matrix */
			const SparseMatrix* mat;

			/**
			 * @brief Constructor
			 * @param qram_ Pointer to the QRAM circuit
			 * @param reg_data_offset_ Name of the data-table offset register
			 * @param reg_sparse_offset_ Name of the sparse-table offset register
			 * @param reg_j_ Name of the row-index register
			 * @param reg_b1_ Name of the Boolean flag b1 register
			 * @param reg_k_ Name of the slot / column-index register
			 * @param reg_b2_ Name of the Boolean flag b2 register
			 * @param reg_search_result_ Name of the binary-search result register
			 * @param nnz_col_ Number of non-zero elements (slots) per row
			 * @param data_size_ Bit width of the temporary data register
			 * @param mat_ Pointer to the sparse matrix
			 */
			T(qram_qutrit::QRAMCircuit* qram_,
				std::string_view reg_data_offset_,
				std::string_view reg_sparse_offset_,
				std::string_view reg_j_, std::string_view reg_b1_,
				std::string_view reg_k_, std::string_view reg_b2_,
				std::string_view reg_search_result_,
				size_t nnz_col_, size_t data_size_,
				const SparseMatrix* mat_)
				: qram(qram_), reg_data_offset(reg_data_offset_), reg_sparse_offset(reg_sparse_offset_),
				reg_j(reg_j_), reg_b1(reg_b1_), reg_k(reg_k_), reg_b2(reg_b2_),
				reg_search_result(reg_search_result_), nnz_col(nnz_col_), data_size(data_size_),
				mat(mat_)
			{
			}

			/**
			 * @brief Forward implementation of the state preparation T
			 * @param system_states System state vector
			 * @details Inside the function body, per-line state comments mark each step's register transformation
			 *          (Hadamard superposition, oracle loading / mapping, conditional rotation, and uncomputation).
			 */
			template<typename Ty>
			void impl(Ty& system_states) const
			{
				profiler _("T");
				AddRegister("data", UnsignedInteger, data_size)(system_states);

				// |j> -> |j>��|s_k>
				Hadamard_Int(reg_k, log2(nnz_col))(system_states);

				// |j>��|s_k> -> |j>��|s_k>|d[j,k]>
				SparseMatrixOracle1(qram, reg_data_offset,
					reg_j, reg_k, "data", nnz_col)(system_states);

				// |j>��|s_k>(a|0>+b|1>) -> |j>��|k>(a|0>+b|1>)
				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col).dag(system_states);

				// |j>��|s_k>|d[j,k]> -> |j>��|s_k>|d[j,k]>(a|0>+b|1>)
				// Two-step equivalent of CondRot_General_Bool_QW: compute angle then apply fixed rotation
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				CondRot_Fixed_Bool("rotation", reg_b2)(system_states);
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				RemoveRegister("rotation")(system_states);

				// |j>��|s_k>(a|0>+b|1>) -> |j>��|k>(a|0>+b|1>)
				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col)(system_states);

				// |j>��|s_k>|d[j,k]>(a|0>+b|1>) -> |j>��|s_k>(a|0>+b|1>)
				SparseMatrixOracle1(qram, reg_data_offset,
					reg_j, reg_k, "data", nnz_col)(system_states);
				RemoveRegister("data")(system_states);

				// |j>��|s_k>(a|0>+b|1>) -> |j>��|k>(a|0>+b|1>)
				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col).dag(system_states);

				CheckNan()(system_states);
			}

			/**
			 * @brief Dagger implementation of the state preparation T
			 * @param system_states System state vector
			 * @details Uncomputes in the reverse order of the forward workflow (including the inverse conditional
			 *          rotation), and inserts CheckNan / ClearZero / CheckNormalization checks.
			 */
			template<typename Ty>
			void impl_dag(Ty& system_states) const
			{
				profiler _("T.dag");
				AddRegister("data", UnsignedInteger, data_size)(system_states);

				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col)(system_states);

				CheckNan()(system_states);

				SparseMatrixOracle1(qram, reg_data_offset,
					reg_j, reg_k, "data", nnz_col)(system_states);

				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col)(system_states);

				// Two-step inverse of CondRot_General_Bool_QW: self-adjoint angle + inverse fixed rotation
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				CondRot_Fixed_Bool("rotation", reg_b2).dag(system_states);
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				RemoveRegister("rotation")(system_states);
				ClearZero()(system_states);

				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col).dag(system_states);

				CheckNormalization()(system_states);

				SparseMatrixOracle1(qram, reg_data_offset,
					reg_j, reg_k, "data", nnz_col)(system_states);

				Hadamard_Int(reg_k, log2(nnz_col))(system_states);
				ClearZero()(system_states);

				RemoveRegister("data")(system_states);
			}

			COMPOSITE_OPERATION
		};

		// =============================================================================
		// Flow-Control / Algorithm Classes
		// These classes orchestrate operators and manage state for algorithm execution.
		// They own registers and coordinate quantum operations for testing/verification.
		// =============================================================================

		/**
		 * @brief Single-step quantum walk operator (CKS walk)
		 * @details Circuit implementation of the walk operator W = T† · P0 · T · Swap: P0 is the phase flip on the
		 *          all-zero state of the walk auxiliary registers (b1, k, b2, k_comp),
		 *          and Swap exchanges the row/column roles of (j, b1, j_comp) and (k, b2, k_comp).
		 *          The walk's spectrum is determined by the matrix's eigenvalues; the matrix elements of its powers
		 *          W^(2j+1) correspond to Chebyshev polynomials of the matrix, which the LCU container combines to
		 *          approximate the target function.
		 */
		struct QuantumWalk : BaseOperator
		{
			/** @brief Names of the walk-related registers (j/b1/k/b2/j_comp/k_comp plus data and sparse offsets) */
			std::string j, b1, k, b2, j_comp, k_comp, data_offset, sparse_offset;
			/** @brief Pointer to the QRAM circuit */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief Copy of the sparse matrix */
			SparseMatrix mat;

			/**
			 * @brief Constructor
			 * @param qram_ Pointer to the QRAM circuit
			 * @param j_ Name of the row-index register
			 * @param b1_ Name of the Boolean flag b1 register
			 * @param k_ Name of the column-index register
			 * @param b2_ Name of the Boolean flag b2 register
			 * @param j_comp_ Name of the j-side auxiliary register
			 * @param k_comp_ Name of the k-side auxiliary register
			 * @param data_offset_ Name of the data-table offset register
			 * @param sparse_offset_ Name of the sparse-table offset register
			 * @param mat_ Sparse matrix
			 */
			QuantumWalk(
				qram_qutrit::QRAMCircuit* qram_,
				std::string_view j_, std::string_view b1_,
				std::string_view k_, std::string_view b2_,
				std::string_view j_comp_,
				std::string_view k_comp_,
				std::string_view data_offset_,
				std::string_view sparse_offset_,
				const SparseMatrix& mat_)
				:qram(qram_), j(j_), b1(b1_), k(k_), b2(b2_),
				j_comp(j_comp_), k_comp(k_comp_),
				data_offset(data_offset_), sparse_offset(sparse_offset_),
				mat(mat_)
			{}

			/**
			 * @brief Applies the single-step quantum walk
			 * @param system_states System state vector
			 * @details Executes in order: T† -> phase flip P0 -> T -> row/column swap (Swap).
			 */
			template<typename Ty>
			void impl(Ty& system_states) const
			{
				profiler _("QuantumWalk");
				size_t addr_size = log2(mat.get_data().size());
				size_t data_size = mat.data_size;
				size_t reg_size = std::max(addr_size, data_size);
				size_t offset = mat.get_sparsity_offset();
				//size_t n_row = mat.n_row;
				size_t nnz_col = mat.nnz_col;

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, reg_size, &mat)
					.dag(system_states);

				// (StatePrint(StatePrintDisplay::Detail))(system_states);

				ZeroConditionalPhaseFlip({ b1, k, b2, k_comp })
					(system_states);

				//RangeConditionalPhaseFlip(j, n_row)
				//	(system_states);

				// (StatePrint(StatePrintDisplay::Detail))(system_states);

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, reg_size, &mat)
					(system_states);

				// (StatePrint(StatePrintDisplay::Detail))(system_states);

				Swap_General_General(j, k)(system_states);
				Swap_General_General(b1, b2)(system_states);
				Swap_General_General(j_comp, k_comp)(system_states);
			}

			/**
			 * @brief Dagger implementation of the single-step quantum walk
			 * @param system_states System state vector
			 * @note Not implemented; throws an exception when called.
			 */
			template<typename Ty>
			void impl_dag(Ty& system_states) const
			{
				throw_not_implemented("QuantumWalk::impl_dag is not implemented");
			}

			COMPOSITE_OPERATION
		};

		/**
		 * @brief Multi-step quantum walk manager
		 * @details Creates the register environment needed by the walk and prepares the n-step walk state:
		 *          at initialization it creates (or attaches to) the QRAM circuit according to the sparse-matrix
		 *          layout and registers the walk registers
		 *          (j/b1/k/b2/j_comp/k_comp plus the data and sparse offset registers);
		 *          MakeNStepState first applies the uniform Hadamard input and the first walk step (T · Swap · T†),
		 *          then iterates single walk steps (phase flip + T + Swap + T†), preparing the quantum state that
		 *          corresponds to the walk's power, for the LCU container to combine with Chebyshev coefficients.
		 */
		template<typename Ty = SparseState>
		class QuantumWalkNSteps
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
			/** @brief Copy of the sparse matrix */
			SparseMatrix mat;
			/** @brief QRAM address width, element quantization width, sparse-table offset, matrix order, and
			 *  non-zeros per row */
			size_t addr_size, data_size, offset, n_row, nnz_col;
			/** @brief Default register width (max of the address width and the element width) */
			size_t default_register_size;
			/** @brief Pointer to the QRAM circuit */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief Suggested state-size reserve constant */
			constexpr static int suggest_reserve = 1024000;

			/**
			 * @brief Constructor (attaching to an external QRAM circuit)
			 * @param mat_ Sparse matrix
			 * @param qram_ Pointer to the external QRAM circuit (lifetime managed by the caller)
			 */
			QuantumWalkNSteps(const SparseMatrix& mat_,
				qram_qutrit::QRAMCircuit* qram_)
			{
				mat = mat_;
				auto&& data = mat.get_data();
				addr_size = log2(data.size());
				data_size = mat.data_size;
				offset = mat.get_sparsity_offset();
				n_row = mat.n_row;
				nnz_col = mat.nnz_col;
				default_register_size = std::max(addr_size, data_size);
				qram = qram_;
			}

			/**
			 * @brief Constructor (internally creates the QRAM circuit)
			 * @param mat_ Sparse matrix (QRAM memory is built from its compact layout)
			 * @note The QRAM circuit is owned by this object and released on destruction.
			 */
			QuantumWalkNSteps(const SparseMatrix& mat_)
			{
				mat = mat_;
				auto&& data = mat.get_data();
				addr_size = log2(data.size());
				data_size = mat.data_size;
				offset = mat.get_sparsity_offset();
				n_row = mat.n_row;
				nnz_col = mat.nnz_col;

				default_register_size = std::max(addr_size, data_size);

				qram = new qram_qutrit::QRAMCircuit(addr_size, data_size, std::move(data));
			}

			/**
			 * @brief Destructor
			 * @note The QRAM circuit is deleted by this object; when attaching an external circuit, ownership
			 *          conventions must be ensured by the caller.
			 */
			~QuantumWalkNSteps()
			{
				delete qram;
			}

			/**
			 * @brief Gets the name of the register holding the input vector
			 * @return The name of the row-index register j
			 */
			std::string GetVecInputReg() const
			{
				return j;
			}

			/**
			 * @brief Gets the initialization width of the input register
			 * @return log2(n_row), i.e. the number of bits needed to represent the row index
			 */
			size_t get_init_size() const
			{
				return log2(mat.n_row);
			}

			/**
			 * @brief Registers all registers needed by the walk
			 * @details Registers the data / sparse offset registers and the
			 *          j/b1/k/b2/j_comp/k_comp walk registers in System.
			 */
			void InitEnvironment()
			{
				/* Register Init */
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
			 * @brief Creates the initial system state
			 * @return A system state with the sparse offset register initialized (set to the matrix's sparse-table
			 *          offset)
			 */
			Ty CreateSys()
			{
				Ty system_states(1);
				Init_Unsafe(sparse_offset, offset)(system_states);
				return system_states;
			}

			/**
			 * @brief Prepares the system state of an n-step quantum walk
			 * @param n_steps Number of walk steps (0 means only the uniform Hadamard superposition is applied)
			 * @return The system state after n walk steps
			 * @details Workflow: create the state -> apply the uniform Hadamard input on the j register ->
			 *          first walk step (T · Swap · T†) -> iterate n_steps-1 single walk steps.
			 */
			Ty MakeNStepState(size_t n_steps)
			{
				profiler _("MakeNStepState");
				auto system_states = CreateSys();

				Hadamard_Int(j, get_init_size())(system_states);
				ClearZero()(system_states);
				if (n_steps == 0)
					return system_states;

				FirstStep(system_states);

				fmt::print("State size = {}\n", system_states.size());
				for (size_t i = 0; i < n_steps - 1; ++i)
				{
					StepImplOneStep(system_states);
				}
				return system_states;
			}

			/**
			 * @brief First step of the walk (without the phase flip)
			 * @param system_states System state vector
			 * @details Executes T -> row/column swap (Swap) -> T† (without the phase flip).
			 */
			void FirstStep(Ty& system_states)
			{
				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, default_register_size, &mat)
					(system_states);

				Swap_General_General(j, k)(system_states);
				Swap_General_General(b1, b2)(system_states);
				Swap_General_General(j_comp, k_comp)(system_states);

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, default_register_size, &mat)
					.dag(system_states);
			}
			/**
			 * @brief Implementation of a single walk step
			 * @param system_states System state vector
			 * @details Executes phase flip P0 -> T -> row/column swap (Swap) -> T†,
			 *          i.e. a single power of the walk operator.
			 */
			void StepImplOneStep(Ty& system_states)
			{
				ZeroConditionalPhaseFlip({ j_comp, k_comp, b1, k, b2 })
					(system_states);

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, default_register_size, &mat)
					(system_states);

				CheckNan()(system_states);

				Swap_General_General(j, k)(system_states);
				Swap_General_General(b1, b2)(system_states);
				Swap_General_General(j_comp, k_comp)(system_states);

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, default_register_size, &mat)
					.dag(system_states);

				CheckNan()(system_states);
			}
			/**
			 * @brief Advances the walk by two steps
			 * @param system_states System state vector
			 * @details Executes two consecutive single walk steps, matching the LCU expansion where the number of
			 *          steps increases by 2 each time (2j+1 -> 2(j+1)+1).
			 */
			void Step(Ty& system_states)
			{
				profiler _("Step");
				StepImplOneStep(system_states);
				StepImplOneStep(system_states);
			}
		};

		/**
		 * @brief LCU container for CKS linear-system solving (general version)
		 * @details Approximates the target operator corresponding to the matrix's Chebyshev series by the linear
		 *          combination Σ_j c_j · W^(2j+1) (j = 0..j0): each term's walk state, with its corresponding number
		 *          of steps, is prepared independently by QuantumWalkNSteps, then accumulated into the current state
		 *          by coefficient and sign, and merged with deduplication.
		 *          Expansion order b = kappa^2 · log(kappa/eps),
		 *          truncation point j0 = sqrt(b · log(4b/eps)).
		 */
		struct LCU_Container
		{
			/** @brief The accumulated LCU combined state */
			std::vector<System> current_state;
			/** @brief Condition number kappa */
			double kappa;
			/** @brief Target precision eps */
			double eps;
			/** @brief Chebyshev expansion-order parameter b */
			size_t b;
			/** @brief LCU summation truncation point */
			size_t j0;
			/** @brief Multi-step quantum walk manager */
			QuantumWalkNSteps<std::vector<System>> quantum_walk_obj;
			/** @brief Chebyshev coefficient calculator */
			ChebyshevPolynomialCoefficient chebyshev_obj;

			/**
			 * @brief Constructor
			 * @param mat Sparse matrix
			 * @param kappa_ Condition number
			 * @param eps_ Target precision
			 * @details Computes b and j0 and initializes the walk register environment.
			 */
			LCU_Container(const SparseMatrix& mat, double kappa_, double eps_) :
				quantum_walk_obj(mat),
				kappa(kappa_), eps(eps_),
				b(static_cast<size_t>(kappa* kappa* (std::log(kappa) - std::log(eps)))),
				chebyshev_obj(b),
				j0(static_cast<size_t>(std::sqrt(b* (std::log(4 * b) - std::log(eps)))))
			{
				quantum_walk_obj.InitEnvironment();
			}

			/**
			 * @brief Gets the name of the register holding the input vector
			 * @return The walk manager's input register name
			 */
			auto GetInputVecReg()
			{
				return quantum_walk_obj.GetVecInputReg();
			}

			/**
			 * @brief Prepares the walk state corresponding to the j-th term
			 * @param j Term index
			 * @return The system state after 2j+1 walk steps
			 */
			std::vector<System> state_of_j(size_t j);

			/**
			 * @brief Accumulates a new state, scaled by its coefficient, into the LCU combined state
			 * @param new_state The state to accumulate
			 * @param coef Chebyshev coefficient
			 * @param sign Whether to take the negative sign (odd-j terms)
			 */
			void add(std::vector<System> new_state, double coef, bool sign);

			/**
			 * @brief Runs the full LCU iteration
			 * @details Iterates j = 0..j0: prepares the state after 2j+1 walk steps, accumulates it by coefficient
			 *          and sign, and sort-merges each round to keep the state size under control.
			 */
			void iterate();
		};

		/* The noisefree version */
		/* It will iterate over the original state,
		   without making extra copies.

		   This optimization cannot be applied to
		   noisy simulation.
		*/

		/**
		 * @brief LCU container for CKS linear-system solving (noise-free optimized version)
		 * @details Iterates in place on a single walk state instead of copying the state for each LCU term,
		 *          hence it only applies to noise-free simulation. ExternalInput injects the input and completes
		 *          the first walk step; Step advances the LCU iteration term by term (j from 0 to j0, with the
		 *          coefficient sum a as the LCU normalization factor); PartialTrace post-selects on the walk
		 *          auxiliary registers and yields the success probability.
		 */
		template<typename StateTy = SparseState>
		struct LCU_Container_NoiseFree
		{
			/** @brief The accumulated LCU combined state */
			StateTy current_state;
			/** @brief The current walk state (advanced in place across LCU terms) */
			StateTy step_state;
			/** @brief Multi-step quantum walk manager */
			QuantumWalkNSteps<StateTy> quantum_walk_obj;
			/** @brief Condition number kappa */
			double kappa;
			/** @brief Target precision eps */
			double eps;
			/** @brief Chebyshev expansion-order parameter b */
			size_t b;
			/** @brief LCU summation truncation point */
			size_t j0;
			size_t j = 0; // iteration variable
			/** @brief The accumulated sum of Chebyshev coefficients (LCU normalization factor) */
			double a = 0;
			/** @brief Chebyshev coefficient calculator */
			ChebyshevPolynomialCoefficient chebyshev_obj;

			/**
			 * @brief Constructor
			 * @param mat Sparse matrix
			 * @param kappa Condition number
			 * @param eps Target precision
			 * @details Computes b and j0, initializes the walk register environment, and creates the walk state.
			 */
			LCU_Container_NoiseFree(const SparseMatrix& mat, double kappa, double eps) :
				quantum_walk_obj(mat),
				kappa(kappa), eps(eps),
				b(static_cast<size_t>(kappa* kappa* (std::log(kappa) - std::log(eps)))),
				chebyshev_obj(b),
				j0(static_cast<size_t>(std::sqrt(b* (std::log(4 * b) - std::log(eps)))))
			{
				quantum_walk_obj.InitEnvironment();
				step_state = quantum_walk_obj.CreateSys();
			}

			/**
			 * @brief Gets the name of the register holding the input vector
			 * @return The walk manager's input register name
			 */
			auto GetInputVecReg() const
			{
				return quantum_walk_obj.GetVecInputReg();
			}

			/**
			 * @brief Gets the QRAM address width
			 * @return The walk manager's addr_size
			 */
			size_t get_addr_size() const
			{
				return quantum_walk_obj.addr_size;
			}

			/**
			 * @brief Injects an external input (default-construction version)
			 * @details Applies the input operator Ty on the input register, clears zero amplitudes, then runs the
			 *          first walk step.
			 */
			template<typename Ty>
			void ExternalInput()
			{
				(Ty(GetInputVecReg()))(step_state);
				ClearZero()(step_state);
				quantum_walk_obj.FirstStep(step_state);
				fmt::print("State size = {}\n", step_state.size());
			}

			/**
			 * @brief Injects an external input (version with extra arguments)
			 * @param args Extra arguments forwarded to the constructor of the input operator Ty
			 * @details Applies the input operator Ty on the input register, clears zero amplitudes, then runs the
			 *          first walk step.
			 */
			template<typename Ty, typename ...Args>
			void ExternalInput(Args &&...args)
			{
				Ty(GetInputVecReg(), std::forward<Args>(args)...)(step_state);
				ClearZero()(step_state);
				quantum_walk_obj.FirstStep(step_state);
				fmt::print("State size = {}\n", step_state.size());
			}

			/**
			 * @brief Injects an external input (operator-instance version)
			 * @param op The input operator applied to the input register
			 * @details Applies the input operator, clears zero amplitudes, then runs the first walk step.
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
			 * @return Returns true while the truncation point has not been reached, false when the iteration is over
			 * @details When j is non-zero, first advances the walk state by two steps (step count 2j+1),
			 *          then accumulates the coefficient into a and adds the current walk state, scaled by the
			 *          coefficient and sign, into the combined state.
			 */
			bool Step() {
				if (j <= j0) {
					if (j != 0)
					{
						quantum_walk_obj.Step(step_state);
					}
				
					a += chebyshev_obj.coef(j);
					Add(step_state, chebyshev_obj.coef(j), chebyshev_obj.sign(j));
					++j;
					return true;
				}
				else
					return false;
			}

			/**
			 * @brief Accumulates the state, scaled by its coefficient, into the LCU combined state
			 * @param new_state The state to accumulate
			 * @param coef Chebyshev coefficient
			 * @param sign Whether to take the negative sign (odd-j terms)
			 */
			void Add(const StateTy& new_state, double coef, bool sign)
			{
				if (sign)
					coef *= -1;

				add_systems(current_state, new_state, coef);
			}

			// inline std::vector<complex_t> _impl_get_output(StateTy& state) const
			// {
			// 	std::vector<complex_t> m(quantum_walk_obj.n_row, 0);
			// 	auto id = System::get(GetInputVecReg());
			// 	for (System& s : state)
			// 	{
			// 		StateStorage& st = s.get(id);
			// 		uint64_t v = st.as<uint64_t>(System::size_of(id));
			// 		m[s.GetAs(id, uint64_t)] = s.amplitude;
			// 	}
			// 	return m;
			// }

			/**
			 * @brief Computes the post-selection success probability (internal implementation)
			 * @param state System state vector (modified by the partial-trace selection)
			 * @return The LCU post-selection success probability
			 * @details Applies the partial-trace selection to branches where the walk auxiliary registers
			 *          (b1, k, b2, j_comp, k_comp, and the offset registers) are all zero and j lies in [0, n_row),
			 *          and combines it with the LCU normalization factor a to obtain the success probability
			 *          (PartialTraceSelect returns 1/sqrt(p)).
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
			 * @brief Computes the post-selection success probability (destructive version)
			 * @return The success probability
			 * @note Modifies current_state.
			 */
			double PartialTrace()
			{
				return _impl_partial_trace(current_state);
			}

			/**
			 * @brief Computes the post-selection success probability (non-destructive version)
			 * @return (A copy of the post-selected state, the success probability)
			 */
			std::tuple<StateTy, double> PartialTrace_Nondestructive()  const
			{
				StateTy state = current_state;
				double ret = _impl_partial_trace(state);
				return { state, ret };
			}

		};

		/* Directly use the theory to validate */
		/**
		 * @brief Classical theory-verification container for CKS linear-system solving
		 * @details Directly computes, with the dense matrix and the Chebyshev three-term recurrence
		 *          T_{n+1} = 2·A'·T_n - T_{n-1}, the vector corresponding to each power of the walk
		 *          (A' is the dense matrix normalized by the quantization scale and nnz_col),
		 *          to be compared against the quantum implementation's LCU combination, verifying the
		 *          algorithm's correctness at the classical level.
		 */
		struct LCU_Container_Theory
		{
			/** @brief Sparse matrix */
			SparseMatrix mat;
			/** @brief The normalized dense matrix */
			DenseMatrix<complex_t> densemat;
			/** @brief Condition number kappa */
			double kappa;
			/** @brief Target precision eps */
			double eps;
			/** @brief Chebyshev expansion-order parameter b */
			size_t b;
			/** @brief LCU summation truncation point */
			size_t j0;
			size_t j = 0; // iteration variable
			/** @brief The accumulated sum of Chebyshev coefficients (LCU normalization factor) */
			double a = 0;
			/** @brief Chebyshev coefficient calculator */
			ChebyshevPolynomialCoefficient chebyshev_obj;
			/** @brief The accumulated LCU combined vector */
			DenseVector<complex_t> current_state;
			/** @brief The vector corresponding to the current walk power */
			DenseVector<complex_t> step_state;
			DenseVector<complex_t> vec0; // for chebyshev iteration
			DenseVector<complex_t> vec1; // for chebyshev iteration

			/**
			 * @brief Constructor
			 * @param mat_ Sparse matrix
			 * @param kappa_ Condition number
			 * @param eps_ Target precision
			 * @details Builds the normalized dense matrix and initializes the first two terms of the Chebyshev
			 *          recurrence (vec0, vec1) with a uniformly normalized vector.
			 */
			LCU_Container_Theory(const SparseMatrix& mat_, double kappa_, double eps_) :
				mat(mat_),
				densemat(sparse2dense<complex_t>(mat)),
				kappa(kappa_),
				eps(eps_),
				b(static_cast<size_t>(kappa* kappa* (std::log(kappa) - std::log(eps)))),
				chebyshev_obj(b),
				j0(static_cast<size_t>(std::sqrt(b* (std::log(4 * b) - std::log(eps))))),
				current_state(mat.n_row),
				step_state(mat.n_row),
				vec0(mat.n_row),
				vec1(mat.n_row)
			{
				densemat = densemat / pow2(mat.data_size);
				densemat = densemat / mat.nnz_col;
				std::fill(step_state.data.begin(), step_state.data.end(), 1.0);
				step_state = step_state / step_state.norm2();
				vec0 = step_state;
				vec1 = densemat * vec0;
			}

			/**
			 * @brief Accumulates the vector, scaled by its coefficient, into the LCU combined vector
			 * @param new_state The vector to accumulate
			 * @param coef Chebyshev coefficient
			 * @param sign Whether to take the negative sign (odd-j terms)
			 */
			inline void Add(const DenseVector<complex_t>& new_state,
				double coef, bool sign)
			{
				if (sign)
					coef *= -1;

				for (size_t i = 0; i < current_state.data.size(); ++i)
				{
					current_state[i] += coef * new_state[i];
				}
			}

			/**
			 * @brief Builds the walk-power vector corresponding to the current LCU term
			 * @return The T(2j+1) vector obtained from the Chebyshev recurrence
			 * @details For j = 0, returns the initial vec1; otherwise advances two steps via the three-term
			 *          recurrence, matching the quantum walk advancing two steps per Step() call.
			 */
			DenseVector<complex_t> MakeStepState();

			/**
			 * @brief Advances the LCU by one term
			 * @return Returns true while the truncation point has not been reached, false when the iteration is over
			 */
			bool Step();

			/**
			 * @brief Gets the final solution vector and the success probability
			 * @return (Normalized solution vector, success probability = ||current||^2 / a^2)
			 */
			std::pair<DenseVector<complex_t>, double> GetOutput() const;
		};


		/**
		 * @brief Classical reference implementation of linear-system solving (all-ones right-hand side)
		 * @param mat Sparse matrix
		 * @return The normalized solution vector (complex)
		 * @details Calls the version with a right-hand side, using the all-ones vector as the right-hand side.
		 */
		std::vector<complex_t> my_linear_solver_reference(const SparseMatrix& mat);

		/**
		 * @brief Classical reference implementation of linear-system solving (with a given right-hand side)
		 * @param mat Sparse matrix
		 * @param vec Right-hand-side vector
		 * @return The normalized solution vector (complex)
		 * @details Solves A x = vec with the Eigen sparse linear solver and normalizes by the 2-norm,
		 *          serving as the classical reference for the quantum algorithm's result.
		 */
		std::vector<complex_t> my_linear_solver_reference(const SparseMatrix& mat, const DenseVector<double>& vec);

		// Deprecated alias; see docs/naming_conventions.md. Remove in the next major version.
		using QuantumBinarySearchFast [[deprecated("use QuantumBinarySearch_Fast")]] = QuantumBinarySearch_Fast;
	} // namespace CKS
}
