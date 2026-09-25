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
			 * @brief Generates the 2x2 rotation matrix of the quantum walk (case where all matrix elements are non-negative)
			 * @param mat_data_size Quantization bit width of the matrix element
			 * @param v Quantized matrix element value
			 * @param row Row index of the element (unused in this overload)
			 * @param col Column index of the element (unused in this overload)
			 * @param mat Output buffer; the 2x2 complex matrix is written in the real/imaginary interleaved layout of u22_t
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
		 * @details The parameters have the same meaning as in the double* buffer overload; directly returns the rotation matrix.
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
		 * @brief Generates the 2x2 rotation matrix of the quantum walk (general case allowing negative elements), returned as u22_t
		 * @details The parameters have the same meaning as in the double* buffer overload; directly returns the rotation matrix.
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
		 * @brief Generates the inverse of the quantum-walk 2x2 rotation matrix (general case allowing negative elements)
		 * @details First generates the forward rotation matrix, then takes its conjugate transpose (dagger).
		 *          The parameters have the same meaning as in the forward version.
		 */
		HOST_DEVICE	inline void _get_coef_common_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col, double* mat)
		{
			_get_coef_common(mat_data_size, v, row, col, mat);
			u22_dagger(mat);
		}

		/**
		 * @brief Generates the inverse of the quantum-walk 2x2 rotation matrix (positive-only elements case), returned as u22_t
		 * @details The parameters have the same meaning as in the double* buffer overload; directly returns the inverse rotation matrix.
		 */
		HOST_DEVICE	inline u22_t _get_coef_positive_only_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_positive_only_inv(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;
		}

		/**
		 * @brief Generates the inverse of the quantum-walk 2x2 rotation matrix (general case allowing negative elements),
		 *          returned as u22_t
		 * @details The parameters have the same meaning as in the double* buffer overload; directly returns the inverse rotation matrix.
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
			 * @brief Generates the inverse (dagger) of the quantum-walk rotation matrix according to the sparse matrix's
			 *          sign convention
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
		 *          For large b, c_j is computed with the erfc asymptotic formula; for small b, the binomial-distribution
		 *          tail probability is summed exactly; odd-j terms take a negative sign.
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
			 * @note During the recursion, as soon as the intermediate value exceeds 2^b it is divided by 2^b to avoid overflow.
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
			 * @brief CUDA 应用快速二分查找
			 * @param state CUDA 稀疏状态
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/**
		 * @brief 行首地址计算算子（自伴）
		 * @details 执行 row_offset ^= offset + row_sz * row，即行 row 在 QRAM
		 *          行式定长存储中对应段的起始地址（每行固定 row_sz 个槽位）；
		 *          两次调用相互抵消。
		 */
		struct GetRowAddr : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief 段起始偏移寄存器 ID */
			size_t offset_id;
			/** @brief 行号寄存器 ID */
			size_t row_id;
			/** @brief 每行的槽位数 */
			size_t row_sz;
			/** @brief 行首地址输出寄存器 ID（以 XOR 方式写入） */
			size_t row_offset_id;

			/**
			 * @brief 构造函数（寄存器名称版本）
			 * @param reg_offset 段起始偏移寄存器名称
			 * @param reg_row 行号寄存器名称
			 * @param row_sz_ 每行的槽位数
			 * @param reg_row_offset 行首地址输出寄存器名称
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
			 * @brief 构造函数（寄存器 ID 版本）
			 * @param reg_offset 段起始偏移寄存器 ID
			 * @param reg_row 行号寄存器 ID
			 * @param row_sz_ 每行的槽位数
			 * @param reg_row_offset 行首地址输出寄存器 ID
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
			 * @brief 计算行首地址
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
#ifdef USE_CUDA
			/**
			 * @brief CUDA 计算行首地址
			 * @param state CUDA 稀疏状态
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/**
		 * @brief 矩阵元素存储地址计算算子（自伴）
		 * @details 执行 data_offset ^= offset + row_sz * row + col_sparse，
		 *          即元素（行 row 的第 col_sparse 个稀疏槽位）在 QRAM 数据表中的
		 *          地址；两次调用相互抵消。
		 */
		struct GetDataAddr : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief 数据表起始偏移寄存器 ID */
			size_t offset_id;
			/** @brief 行号寄存器 ID */
			size_t row_id;
			/** @brief 每行的槽位数 */
			size_t row_sz;
			/** @brief 稀疏槽位寄存器 ID */
			size_t col_sparse_id;
			/** @brief 元素地址输出寄存器 ID（以 XOR 方式写入） */
			size_t row_data_id;

			/**
			 * @brief 构造函数（寄存器名称版本）
			 * @param reg_offset 数据表起始偏移寄存器名称
			 * @param reg_row 行号寄存器名称
			 * @param reg_col_sparse 稀疏槽位寄存器名称
			 * @param row_sz_ 每行的槽位数
			 * @param reg_data_offset 元素地址输出寄存器名称
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
			 * @brief 构造函数（寄存器 ID 版本）
			 * @param reg_offset 数据表起始偏移寄存器 ID
			 * @param reg_row 行号寄存器 ID
			 * @param reg_col_sparse 稀疏槽位寄存器 ID
			 * @param row_sz_ 每行的槽位数
			 * @param reg_data_offset 元素地址输出寄存器 ID
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
			 * @brief 计算矩阵元素存储地址
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
			/**
			 * @brief CUDA 计算矩阵元素存储地址
			 * @param state CUDA 稀疏状态
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/*
		|offset>|i>|s_j>|0>			->
		|offset>|i>|s_j>|a_{ij}>
		*/
		/**
		 * @brief 稀疏矩阵 oracle 一号：矩阵元素查询（自伴）
		 * @details 按（行 i，行内稀疏槽位 s_j）查询 QRAM 数据表，加载对应的量化
		 *          矩阵元素 a_{ij}，即 |offset>|i>|s_j>|0> -> |offset>|i>|s_j>|a_{ij}>。
		 *          元素地址由 GetDataAddr 计算（offset + row_size*i + s_j），
		 *          加载后再反计算地址寄存器，保持整体自伴。
		 */
		struct SparseMatrixOracle1 : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 数据表偏移寄存器名称 */
			std::string reg_offset;
			/** @brief 行号寄存器名称 */
			std::string reg_row;
			/** @brief 稀疏槽位寄存器名称（元素在行紧凑存储中的位置） */
			std::string reg_col_id; // position in the sparse-compact storage
			/** @brief 查询结果（量化元素）输出寄存器名称 */
			std::string reg_output;
			/** @brief 每行的槽位数 */
			size_t row_size;

			/**
			 * @brief 构造函数
			 * @param qram QRAM 电路指针
			 * @param reg_offset 数据表偏移寄存器名称
			 * @param reg_row 行号寄存器名称
			 * @param reg_col_id 稀疏槽位寄存器名称
			 * @param reg_output 查询结果输出寄存器名称
			 * @param row_size_ 每行的槽位数
			 */
			SparseMatrixOracle1(qram_qutrit::QRAMCircuit* qram,
				std::string_view reg_offset,
				std::string_view reg_row,
				std::string_view reg_col_id,
				std::string_view reg_output,
				size_t row_size_);

			/**
			 * @brief 元素查询的正向实现（同时作为 dagger 实现）
			 * @param state 系统状态向量
			 * @details 计算 data_addr = offset + row_size*i + s_j，
			 *          QRAM 加载元素到输出寄存器后再次调用 GetDataAddr
			 *          反计算地址寄存器。
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
			 * @brief 元素查询的 dagger 实现
			 * @param state 系统状态向量
			 * @details 算子自伴，直接复用正向实现。
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
		 * @brief 稀疏矩阵 oracle 二号：列下标到稀疏槽位的转换
		 * @details 通过量子二分查找，把矩阵列下标 j 转换为该元素在行紧凑存储中的
		 *          稀疏槽位 s_j，即 |offset>|i>|j> -> |offset>|i>|s_j>。
		 *          行的稀疏表首地址由 GetRowAddr 计算；查找得到的是稀疏表内的
		 *          绝对地址，需经 QRAM 加载列下标并减去行首地址还原为槽位编号。
		 */
		struct SparseMatrixOracle2 : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 稀疏表偏移寄存器名称 */
			std::string reg_sparse_offset;
			/** @brief 行号寄存器名称 */
			std::string reg_row;
			/** @brief 列号寄存器名称（矩阵中的实际列下标） */
			std::string reg_col; // the column in the matrix
			/** @brief 二分查找结果寄存器名称 */
			std::string reg_search_result;
			/** @brief 每行的槽位数 */
			size_t row_size;

			/**
			 * @brief 构造函数
			 * @param qram QRAM 电路指针
			 * @param reg_sparse_offset 稀疏表偏移寄存器名称
			 * @param reg_row_ 行号寄存器名称
			 * @param reg_col_ 列号寄存器名称
			 * @param reg_search_result_ 二分查找结果寄存器名称
			 * @param row_size 每行的槽位数
			 */
			SparseMatrixOracle2(qram_qutrit::QRAMCircuit* qram,
				std::string_view reg_sparse_offset,
				std::string_view reg_row_,
				std::string_view reg_col_,
				std::string_view reg_search_result_,
				size_t row_size);

			/**
			 * @brief 列下标到稀疏槽位转换的正向实现
			 * @param state 系统状态向量
			 * @details 依次：GetRowAddr 计算行稀疏表首地址 -> 量子二分查找
			 *          列下标所在槽位 -> QRAM 加载还原 -> 交换与减去行首地址，
			 *          使列号寄存器最终持有槽位编号 s_j；实现自带详细步骤注释。
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
			 * @brief 列下标到稀疏槽位转换的 dagger 实现
			 * @param state 系统状态向量
			 * @details 按正向实现的逆序执行，把稀疏槽位 s_j 还原为列下标 j。
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
		 * @brief 由稀疏槽位计算列下标（非原地，QRAM 查询实现）
		 * @details |offset>|l>|z> -> |offset>|l>|z + k>：
		 *          以稀疏表偏移加槽位 l 为地址查询 QRAM，得到对应的列下标 k
		 *          并累加（XOR）到目标寄存器，查询后反计算地址寄存器。
		 */
		struct SparseMatrixOracle2_ComputeCol : BaseOperator
		{
			using BaseOperator::operator();

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 稀疏表偏移寄存器名称 */
			std::string sparse_offset;
			/** @brief 列下标寄存器名称（查询输出） */
			std::string k; // j
			/** @brief 稀疏槽位寄存器名称 */
			std::string l; // s_j
			/** @brief 临时地址寄存器名称 */
			std::string addr_offset;
			/** @brief 每行的槽位数 */
			size_t row_size;

			/**
			 * @brief 构造函数
			 * @param qram QRAM 电路指针
			 * @param sparse_offset 稀疏表偏移寄存器名称
			 * @param k 列下标寄存器名称
			 * @param l 稀疏槽位寄存器名称
			 * @param addr_offset 临时地址寄存器名称
			 * @param row_size 每行的槽位数
			 */
			SparseMatrixOracle2_ComputeCol(qram_qutrit::QRAMCircuit* qram,
				std::string_view sparse_offset,
				std::string_view k,
				std::string_view l,
				std::string_view addr_offset,
				size_t row_size);

			/**
			 * @brief 应用列下标计算
			 * @param state 系统状态向量
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
		 * @brief 由列下标计算稀疏槽位（非原地，量子二分查找实现）
		 * @details |offset>|k>|z> -> |offset>|k>|z + l>：
		 *          在行稀疏表区间内以列下标 k 为目标做量子二分查找，
		 *          将命中的槽位地址（含表偏移）累加到 l 后再减去偏移，
		 *          还原为相对槽位编号。
		 */
		struct SparseMatrixOracle2_ComputeSparsity : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 稀疏表偏移寄存器名称 */
			std::string sparse_offset;
			/** @brief 列下标寄存器名称 */
			std::string k; // j
			/** @brief 稀疏槽位寄存器名称 */
			std::string l; // s_j
			/** @brief 每行的槽位数 */
			size_t row_size;

			/**
			 * @brief 构造函数
			 * @param qram QRAM 电路指针
			 * @param sparse_offset 稀疏表偏移寄存器名称
			 * @param k 列下标寄存器名称
			 * @param l 稀疏槽位寄存器名称
			 * @param row_size 每行的槽位数
			 */
			SparseMatrixOracle2_ComputeSparsity(
				qram_qutrit::QRAMCircuit* qram,
				std::string_view sparse_offset,
				std::string_view k,
				std::string_view l,
				size_t row_size);

			/**
			 * @brief 应用稀疏槽位计算
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		// prepare from |j> to |\psi_j>
		/**
		 * @brief CKS 量子行走的状态准备算子 T
		 * @details 对行下标 j 制备该行的"平方根振幅"叠加态：
		 *          |j>|0> -> Σ_k sqrt(A_{j,s_k}) |j>|k>（k 为行内稀疏槽位）。
		 *          流程：对槽位寄存器 k 做 Hadamard 均匀叠加 -> Oracle1 加载量化
		 *          元素 d[j,k] -> Oracle2 的 dagger 把 k 由槽位映射为实际列下标 ->
		 *          GetQWRotateAngle + CondRot_Fixed_Bool 完成以 sqrt(A_{j,k}) 为
		 *          比例的条件旋转 -> 依次反计算各 oracle 恢复寄存器。
		 */
		struct T : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 数据表偏移寄存器名称 */
			std::string reg_data_offset;
			/** @brief 稀疏表偏移寄存器名称 */
			std::string reg_sparse_offset;
			/** @brief 行号寄存器名称 */
			std::string reg_j;
			/** @brief 布尔旗标 b1 寄存器名称 */
			std::string reg_b1;
			/** @brief 槽位 / 列号寄存器名称 */
			std::string reg_k;
			/** @brief 布尔旗标 b2 寄存器名称（条件旋转目标） */
			std::string reg_b2;
			/** @brief 二分查找结果寄存器名称 */
			std::string reg_search_result;
			/** @brief 每行的非零元（槽位）数 */
			size_t nnz_col;
			/** @brief 临时数据寄存器的位宽（取地址位宽与元素位宽的较大值） */
			size_t data_size;
			/** @brief 指向稀疏矩阵 */
			const SparseMatrix* mat;

			/**
			 * @brief 构造函数
			 * @param qram_ QRAM 电路指针
			 * @param reg_data_offset_ 数据表偏移寄存器名称
			 * @param reg_sparse_offset_ 稀疏表偏移寄存器名称
			 * @param reg_j_ 行号寄存器名称
			 * @param reg_b1_ 布尔旗标 b1 寄存器名称
			 * @param reg_k_ 槽位 / 列号寄存器名称
			 * @param reg_b2_ 布尔旗标 b2 寄存器名称
			 * @param reg_search_result_ 二分查找结果寄存器名称
			 * @param nnz_col_ 每行的非零元（槽位）数
			 * @param data_size_ 临时数据寄存器的位宽
			 * @param mat_ 稀疏矩阵指针
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
			 * @brief 状态准备 T 的正向实现
			 * @param system_states 系统状态向量
			 * @details 函数体内以逐行态注释标注了每一步寄存器变换
			 *          （Hadamard 叠加、oracle 加载 / 映射、条件旋转与反计算）。
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
			 * @brief 状态准备 T 的 dagger 实现
			 * @param system_states 系统状态向量
			 * @details 按正向流程的逆序反计算（含逆条件旋转），
			 *          并插入 CheckNan / ClearZero / CheckNormalization 校验。
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
		 * @brief 单步量子行走算子（CKS 行走）
		 * @details 行走算符 W = T† · P0 · T · Swap 的电路实现：P0 为对行走辅助
		 *          寄存器 (b1, k, b2, k_comp) 全零态的相位翻转，
		 *          Swap 交换 (j, b1, j_comp) 与 (k, b2, k_comp) 的行列角色。
		 *          行走的谱由矩阵的本征值决定，其幂次 W^(2j+1) 的矩阵元对应
		 *          矩阵的 Chebyshev 多项式，供 LCU 容器组合逼近目标函数。
		 */
		struct QuantumWalk : BaseOperator
		{
			/** @brief 行走相关的寄存器名称（j/b1/k/b2/j_comp/k_comp 及数据、稀疏偏移） */
			std::string j, b1, k, b2, j_comp, k_comp, data_offset, sparse_offset;
			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 稀疏矩阵副本 */
			SparseMatrix mat;

			/**
			 * @brief 构造函数
			 * @param qram_ QRAM 电路指针
			 * @param j_ 行号寄存器名称
			 * @param b1_ 布尔旗标 b1 寄存器名称
			 * @param k_ 列号寄存器名称
			 * @param b2_ 布尔旗标 b2 寄存器名称
			 * @param j_comp_ j 侧辅助寄存器名称
			 * @param k_comp_ k 侧辅助寄存器名称
			 * @param data_offset_ 数据表偏移寄存器名称
			 * @param sparse_offset_ 稀疏表偏移寄存器名称
			 * @param mat_ 稀疏矩阵
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
			 * @brief 应用单步量子行走
			 * @param system_states 系统状态向量
			 * @details 依次执行 T† -> 相位翻转 P0 -> T -> 行列交换（Swap）。
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
			 * @brief 单步量子行走的 dagger 实现
			 * @param system_states 系统状态向量
			 * @note 未实现，调用时抛出异常。
			 */
			template<typename Ty>
			void impl_dag(Ty& system_states) const
			{
				throw_not_implemented("QuantumWalk::impl_dag is not implemented");
			}

			COMPOSITE_OPERATION
		};

		/**
		 * @brief 多步量子行走管理器
		 * @details 负责行走所需寄存器环境的创建与 n 步行走态的制备：
		 *          初始化时按稀疏矩阵布局创建（或接入）QRAM 电路并登记行走寄存器
		 *          （j/b1/k/b2/j_comp/k_comp 及数据、稀疏偏移寄存器）；
		 *          MakeNStepState 先做 Hadamard 均匀输入与首步行走（T · Swap · T†），
		 *          随后迭代单步行走（相位翻转 + T + Swap + T†），制备与行走幂次
		 *          对应的量子态，供 LCU 容器按 Chebyshev 系数组合。
		 */
		template<typename Ty = SparseState>
		class QuantumWalkNSteps
		{
		public:
			/** @brief 数据表偏移寄存器名称 */
			std::string data_offset = "data_offset";
			/** @brief 稀疏表偏移寄存器名称 */
			std::string sparse_offset = "sparse_offset";
			/** @brief 行号（输入向量）寄存器名称 */
			std::string j = "row_id";
			/** @brief 布尔旗标 b1 寄存器名称 */
			std::string b1 = "reg_b1";
			/** @brief 列号寄存器名称 */
			std::string k = "col_id";
			/** @brief 布尔旗标 b2 寄存器名称 */
			std::string b2 = "reg_b2";
			/** @brief j 侧辅助寄存器名称 */
			std::string j_comp = "j_comp";
			/** @brief k 侧辅助寄存器名称 */
			std::string k_comp = "k_comp";
			/** @brief 稀疏矩阵副本 */
			SparseMatrix mat;
			/** @brief QRAM 地址位宽、元素量化位宽、稀疏表偏移、矩阵阶数与每行非零元数 */
			size_t addr_size, data_size, offset, n_row, nnz_col;
			/** @brief 默认寄存器位宽（地址位宽与元素位宽的较大值） */
			size_t default_register_size;
			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 建议的状态规模预留常量 */
			constexpr static int suggest_reserve = 1024000;

			/**
			 * @brief 构造函数（接入外部 QRAM 电路）
			 * @param mat_ 稀疏矩阵
			 * @param qram_ 外部 QRAM 电路指针（生命周期由调用方管理）
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
			 * @brief 构造函数（内部创建 QRAM 电路）
			 * @param mat_ 稀疏矩阵（按其紧凑布局构建 QRAM 内存）
			 * @note 由本对象持有 QRAM 电路并在析构时释放。
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
			 * @brief 析构函数
			 * @note QRAM 电路由本对象删除，接入外部电路的场景需自行保证所有权约定。
			 */
			~QuantumWalkNSteps()
			{
				delete qram;
			}

			/**
			 * @brief 获取输入向量所在的寄存器名称
			 * @return 行号寄存器 j 的名称
			 */
			std::string GetVecInputReg() const
			{
				return j;
			}

			/**
			 * @brief 获取输入寄存器的初始化位宽
			 * @return log2(n_row)，即表示行号所需的位数
			 */
			size_t get_init_size() const
			{
				return log2(mat.n_row);
			}

			/**
			 * @brief 登记行走所需的全部寄存器
			 * @details 在 System 中注册数据 / 稀疏偏移寄存器与
			 *          j/b1/k/b2/j_comp/k_comp 行走寄存器。
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
			 * @brief 创建初始系统状态
			 * @return 初始化了稀疏偏移寄存器（置为矩阵稀疏表偏移）的系统状态
			 */
			Ty CreateSys()
			{
				Ty system_states(1);
				Init_Unsafe(sparse_offset, offset)(system_states);
				return system_states;
			}

			/**
			 * @brief 制备 n 步量子行走的系统状态
			 * @param n_steps 行走步数（0 表示仅做 Hadamard 均匀叠加）
			 * @return 行走 n 步后的系统状态
			 * @details 流程：创建状态 -> 对 j 寄存器做 Hadamard 均匀输入 ->
			 *          首步行走（T · Swap · T†）-> 迭代 n_steps-1 次单步行走。
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
			 * @brief 行走的首步（不含相位翻转）
			 * @param system_states 系统状态向量
			 * @details 执行 T -> 行列交换（Swap）-> T†（不含相位翻转）。
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
			 * @brief 单步行走的实现
			 * @param system_states 系统状态向量
			 * @details 执行相位翻转 P0 -> T -> 行列交换（Swap）-> T†，
			 *          即行走算符的单个幂次。
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
			 * @brief 推进两步行走
			 * @param system_states 系统状态向量
			 * @details 连续执行两次单步行走，对应 LCU 展开中步数每次增加 2
			 *          （2j+1 -> 2(j+1)+1）。
			 */
			void Step(Ty& system_states)
			{
				profiler _("Step");
				StepImplOneStep(system_states);
				StepImplOneStep(system_states);
			}
		};

		/**
		 * @brief CKS 线性系统求解的 LCU 容器（通用版本）
		 * @details 以线性组合 Σ_j c_j · W^(2j+1)（j = 0..j0）逼近矩阵 Chebyshev
		 *          级数对应的目标算子：每一项由 QuantumWalkNSteps 独立制备对应
		 *          步数的行走态，按系数与符号累加进当前态并归并去重。
		 *          展开阶数 b = kappa^2 · log(kappa/eps)，
		 *          截断点 j0 = sqrt(b · log(4b/eps))。
		 */
		struct LCU_Container
		{
			/** @brief 已累加的 LCU 组合态 */
			std::vector<System> current_state;
			/** @brief 条件数 kappa */
			double kappa;
			/** @brief 目标精度 eps */
			double eps;
			/** @brief Chebyshev 展开阶数参数 b */
			size_t b;
			/** @brief LCU 求和截断点 */
			size_t j0;
			/** @brief 多步量子行走管理器 */
			QuantumWalkNSteps<std::vector<System>> quantum_walk_obj;
			/** @brief Chebyshev 系数计算器 */
			ChebyshevPolynomialCoefficient chebyshev_obj;

			/**
			 * @brief 构造函数
			 * @param mat 稀疏矩阵
			 * @param kappa_ 条件数
			 * @param eps_ 目标精度
			 * @details 计算 b 与 j0 并初始化行走寄存器环境。
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
			 * @brief 获取输入向量所在的寄存器名称
			 * @return 行走管理器的输入寄存器名称
			 */
			auto GetInputVecReg()
			{
				return quantum_walk_obj.GetVecInputReg();
			}

			/**
			 * @brief 制备第 j 项对应的行走态
			 * @param j 项下标
			 * @return 行走 2j+1 步后的系统状态
			 */
			std::vector<System> state_of_j(size_t j);

			/**
			 * @brief 将新状态按系数累加进 LCU 组合态
			 * @param new_state 待累加的状态
			 * @param coef Chebyshev 系数
			 * @param sign 是否取负号（奇数 j 项）
			 */
			void add(std::vector<System> new_state, double coef, bool sign);

			/**
			 * @brief 执行完整的 LCU 迭代
			 * @details 遍历 j = 0..j0：制备行走 2j+1 步的状态、按系数与符号
			 *          累加，并在每轮做排序归并以控制状态规模。
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
		 * @brief CKS 线性系统求解的 LCU 容器（无噪声优化版本）
		 * @details 在同一份行走态上原地迭代而不为每个 LCU 项复制状态，
		 *          因此仅适用于无噪声模拟。ExternalInput 注入输入并完成首步
		 *          行走；Step 逐项推进 LCU 迭代（j 从 0 到 j0，系数之和 a 作为
		 *          LCU 归一化因子）；PartialTrace 对行走辅助寄存器做后选择，
		 *          给出成功概率。
		 */
		template<typename StateTy = SparseState>
		struct LCU_Container_NoiseFree
		{
			/** @brief 已累加的 LCU 组合态 */
			StateTy current_state;
			/** @brief 当前行走态（在各 LCU 项间原地推进） */
			StateTy step_state;
			/** @brief 多步量子行走管理器 */
			QuantumWalkNSteps<StateTy> quantum_walk_obj;
			/** @brief 条件数 kappa */
			double kappa;
			/** @brief 目标精度 eps */
			double eps;
			/** @brief Chebyshev 展开阶数参数 b */
			size_t b;
			/** @brief LCU 求和截断点 */
			size_t j0;
			size_t j = 0; // iteration variable
			/** @brief 已累加的 Chebyshev 系数之和（LCU 归一化因子） */
			double a = 0;
			/** @brief Chebyshev 系数计算器 */
			ChebyshevPolynomialCoefficient chebyshev_obj;

			/**
			 * @brief 构造函数
			 * @param mat 稀疏矩阵
			 * @param kappa 条件数
			 * @param eps 目标精度
			 * @details 计算 b 与 j0、初始化行走寄存器环境并创建行走态。
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
			 * @brief 获取输入向量所在的寄存器名称
			 * @return 行走管理器的输入寄存器名称
			 */
			auto GetInputVecReg() const
			{
				return quantum_walk_obj.GetVecInputReg();
			}

			/**
			 * @brief 获取 QRAM 地址位宽
			 * @return 行走管理器的 addr_size
			 */
			size_t get_addr_size() const
			{
				return quantum_walk_obj.addr_size;
			}

			/**
			 * @brief 注入外部输入（默认构造参数版本）
			 * @details 在输入寄存器上应用输入算子 Ty，清理零振幅后执行首步行走。
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
			 * @brief 注入外部输入（带附加参数版本）
			 * @param args 转发给输入算子 Ty 构造函数的附加参数
			 * @details 在输入寄存器上应用输入算子 Ty，清理零振幅后执行首步行走。
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
			 * @brief 注入外部输入（算子实例版本）
			 * @param op 作用于输入寄存器的输入算子
			 * @details 应用输入算子，清理零振幅后执行首步行走。
			 */
			void ExternalInput_V2(const BaseOperator& op)
			{
				op(step_state);
				ClearZero()(step_state);
				quantum_walk_obj.FirstStep(step_state);
				fmt::print("State size = {}\n", step_state.size());
			}

			/**
			 * @brief 推进一个 LCU 项
			 * @return 尚未到达截断点时返回 true，迭代结束返回 false
			 * @details j 非 0 时先把行走态推进两步（步数 2j+1），
			 *          再累加系数到 a 并把当前行走态按系数与符号加入组合态。
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
			 * @brief 将状态按系数累加进 LCU 组合态
			 * @param new_state 待累加的状态
			 * @param coef Chebyshev 系数
			 * @param sign 是否取负号（奇数 j 项）
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
			 * @brief 计算后选择成功概率（内部实现）
			 * @param state 系统状态向量（会被部分迹选择修改）
			 * @return LCU 后选择的成功概率
			 * @details 对行走辅助寄存器（b1、k、b2、j_comp、k_comp、偏移寄存器）
			 *          全零且 j 落在 [0, n_row) 的分支做部分迹选择，
			 *          结合 LCU 归一化因子 a 得到成功概率
			 *          （PartialTraceSelect 返回 1/sqrt(p)）。
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
			 * @brief 计算后选择成功概率（破坏性版本）
			 * @return 成功概率
			 * @note 会修改 current_state。
			 */
			double PartialTrace()
			{
				return _impl_partial_trace(current_state);
			}

			/**
			 * @brief 计算后选择成功概率（非破坏性版本）
			 * @return (后选择后的状态副本, 成功概率)
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
		 * @brief CKS 线性系统求解的经典理论验证容器
		 * @details 直接用稠密矩阵按 Chebyshev 三项递推
		 *          T_{n+1} = 2·A'·T_n - T_{n-1} 计算行走幂次对应的向量
		 *          （A' 为按量化幅度与 nnz_col 归一化后的稠密矩阵），
		 *          与量子实现的 LCU 组合对照，用于经典层面验证算法正确性。
		 */
		struct LCU_Container_Theory
		{
			/** @brief 稀疏矩阵 */
			SparseMatrix mat;
			/** @brief 归一化后的稠密矩阵 */
			DenseMatrix<complex_t> densemat;
			/** @brief 条件数 kappa */
			double kappa;
			/** @brief 目标精度 eps */
			double eps;
			/** @brief Chebyshev 展开阶数参数 b */
			size_t b;
			/** @brief LCU 求和截断点 */
			size_t j0;
			size_t j = 0; // iteration variable
			/** @brief 已累加的 Chebyshev 系数之和（LCU 归一化因子） */
			double a = 0;
			/** @brief Chebyshev 系数计算器 */
			ChebyshevPolynomialCoefficient chebyshev_obj;
			/** @brief 已累加的 LCU 组合向量 */
			DenseVector<complex_t> current_state;
			/** @brief 当前行走幂次对应的向量 */
			DenseVector<complex_t> step_state;
			DenseVector<complex_t> vec0; // for chebyshev iteration
			DenseVector<complex_t> vec1; // for chebyshev iteration

			/**
			 * @brief 构造函数
			 * @param mat_ 稀疏矩阵
			 * @param kappa_ 条件数
			 * @param eps_ 目标精度
			 * @details 构建归一化稠密矩阵，并以均匀归一化向量初始化
			 *          Chebyshev 递推的前两项（vec0、vec1）。
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
			 * @brief 将向量按系数累加进 LCU 组合向量
			 * @param new_state 待累加的向量
			 * @param coef Chebyshev 系数
			 * @param sign 是否取负号（奇数 j 项）
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
			 * @brief 构造当前 LCU 项对应的行走幂次向量
			 * @return Chebyshev 递推得到的 T(2j+1) 向量
			 * @details j = 0 时返回初始的 vec1；否则按三项递推推进两步，
			 *          对应量子行走每次调用 Step 前进两步。
			 */
			DenseVector<complex_t> MakeStepState();

			/**
			 * @brief 推进一个 LCU 项
			 * @return 尚未到达截断点时返回 true，迭代结束返回 false
			 */
			bool Step();

			/**
			 * @brief 获取最终解向量与成功概率
			 * @return (归一化解向量, 成功概率 = ||current||^2 / a^2)
			 */
			std::pair<DenseVector<complex_t>, double> GetOutput() const;
		};


		/**
		 * @brief 线性系统求解的经典参考实现（全 1 右端项）
		 * @param mat 稀疏矩阵
		 * @return 归一化的解向量（复数）
		 * @details 以全 1 向量为右端项调用带右端项版本。
		 */
		std::vector<complex_t> my_linear_solver_reference(const SparseMatrix& mat);

		/**
		 * @brief 线性系统求解的经典参考实现（指定右端项）
		 * @param mat 稀疏矩阵
		 * @param vec 右端项向量
		 * @return 归一化的解向量（复数）
		 * @details 用 Eigen 稀疏线性求解器求 A x = vec 后按 2 范数归一化，
		 *          作为量子算法结果的经典对照。
		 */
		std::vector<complex_t> my_linear_solver_reference(const SparseMatrix& mat, const DenseVector<double>& vec);

		// Deprecated alias; see docs/naming_conventions.md. Remove in the next major version.
		using QuantumBinarySearchFast [[deprecated("use QuantumBinarySearch_Fast")]] = QuantumBinarySearch_Fast;
	} // namespace CKS
}
