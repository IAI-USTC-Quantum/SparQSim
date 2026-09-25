/**
 * @file hamiltonian_simulation.cpp
 * @brief Implementation of Hamiltonian simulation
 * @details Implements the quantum walk (QuantumWalk), sparse-matrix oracle, quantum binary search, and LCU containers
 *          declared in hamiltonian_simulation.h
 *          (Chebyshev coefficient computation, QuantumBinarySearch(_Fast) quantum/fast binary search,
 *          GetRowAddr/GetDataAddr address computation, GetQWRotateAngle walk rotation angle,
 *          CondRot_General_Bool_QW general conditional rotation, and the LCU combination and
 *          classical-reference verification of the LCU_Container family of containers)
 */
#include "hamiltonian_simulation.h"
#include "matrix.h"

namespace qram_simulator {
	namespace CKS {
		//u22_t _get_coef(const SparseMatrix& mat, size_t v, size_t row, size_t col)
		//{
		//	/*
		//	A matrix that

		//	\sqrt(A_jk*), ?
		//	\sqrt(1-|Ajk|), ?

		//	\sqrt(A_jk*) = \sqrt(Ajk) for Ajk >= 0
		//	\sqrt(A_jk) = \sqrt(|Ajk|)* sgn(j-k) * 1i

		//	*/

		//	// double coef;
		//	if (mat.positive_only)
		//	{
		//		_get_coef_positive_only(mat, v, row, col);
		//	}
		//	else
		//	{
		//		_get_coef_common(mat, v, row, col);
		//	}
		//}

		/**
		 * @brief Computes the binomial coefficient C(Big, Small) / 4^b, scaled by 4^b
		 * @details Computes the binomial coefficient recursively; whenever the intermediate value exceeds 2^b, it is
		 *          divided by 2^b early to prevent overflow, and the final result is divided by 4^b as a whole. */
		double ChebyshevPolynomialCoefficient::C(size_t Big, size_t Small)
		{
			double ret = 1;
			// size_t D = Big - Small;
			bool flag = true;
			double pow2_b = std::pow(2, b);
			for (size_t i = 0; i < Small; ++i)
			{
				ret /= (Small - i);
				ret *= (Big - i);
				if (flag && ret > pow2_b) {
					flag = false;
					ret /= pow2_b;
				}
			}
			if (flag)
				ret /= pow2_b;

			ret /= pow2_b;

			return ret;
		}

		/**
		 * @brief Computes the j-th Chebyshev coefficient c_j
		 * @details For b > 100, uses the asymptotic formula c_j = 2·erfc((j+0.5)/sqrt(b));
		 *          otherwise, sums the binomial-distribution tail exactly: c_j = 4·Σ_{i=j+1}^{b} C(2b, b+i)/4^b. */
		double ChebyshevPolynomialCoefficient::coef(size_t j)
		{
			if (b > 100)
			{
				return std::erfc((j + 0.5) / std::sqrt(b)) * 2;
			}
			else {
				/* Sum:i {from j+1 to b} C(b+i, 2b) */
				double ret = 0;
				for (size_t i = j + 1; i <= b; ++i)
				{
					ret += C(2 * b, b + i);
				}

				return ret * 4;
			}
		}

		/** @brief Sign of the j-th term: negative sign for odd j (returns true), positive sign for even j */
		bool ChebyshevPolynomialCoefficient::sign(size_t j)
		{
			return j & 1;
		}

		/** @brief Number of walk steps for the j-th term: 2j + 1 */
		size_t ChebyshevPolynomialCoefficient::step(size_t j)
		{
			return 2 * j + 1;
		}

		QuantumBinarySearch::QuantumBinarySearch(
			qram_qutrit::QRAMCircuit* qram_,
			std::string_view address_offset_register_name_,
			size_t total_length_,
			std::string_view target_register_name_,
			std::string_view result_register_name_)
		{
			qram = qram_;
			total_length = total_length_;
			max_step = size_t(std::log2(total_length_)) + 1;
			// iteration_level = max_step;

			// address_offset_register = address_offset_register_name_;
			address_offset_id = System::get(address_offset_register_name_);

			// target_register = target_register_name_;
			target_id = System::get(target_register_name_);

			// result_register = result_register_name_;
			result_id = System::get(result_register_name_);
		}

		QuantumBinarySearch::QuantumBinarySearch(
			qram_qutrit::QRAMCircuit* qram_,
			size_t address_offset_register_name_,
			size_t total_length_,
			size_t target_register_name_,
			size_t result_register_name_)
		{
			qram = qram_;
			total_length = total_length_;
			max_step = size_t(std::log2(total_length_)) + 1;
			// iteration_level = max_step;

			// address_offset_register = address_offset_register_name_;
			address_offset_id = address_offset_register_name_;

			// target_register = target_register_name_;
			target_id = target_register_name_;

			// result_register = result_register_name_;
			result_id = result_register_name_;
		}


		QuantumBinarySearch_Fast::QuantumBinarySearch_Fast(
			qram_qutrit::QRAMCircuit* qram_,
			std::string_view address_offset_register_name_,
			size_t total_length_,
			std::string_view target_register_name_,
			std::string_view result_register_name_)
		{
			qram = qram_;
			total_length = total_length_;
			max_step = size_t(std::log2(total_length_)) + 1;

			address_offset_id = System::get(address_offset_register_name_);
			target_id = System::get(target_register_name_);
			result_id = System::get(result_register_name_);
		}

		QuantumBinarySearch_Fast::QuantumBinarySearch_Fast(
			qram_qutrit::QRAMCircuit* qram_,
			size_t address_offset_register_name_,
			size_t total_length_,
			size_t target_register_name_,
			size_t result_register_name_)
		{
			qram = qram_;
			total_length = total_length_;
			max_step = size_t(std::log2(total_length_)) + 1;

			address_offset_id = address_offset_register_name_;
			target_id = target_register_name_;
			result_id = result_register_name_;
		}

		/**
		 * @brief Classical binary search on a single branch (simulator-level shortcut)
		 * @details Performs binary search directly on the QRAM memory range [offset, offset+total_length);
		 *          returns the address on a hit, and 0 on a miss. */
		size_t QuantumBinarySearch_Fast::binary_search(size_t offset, size_t target) const
		{
			const auto& mem = qram->memory;
			size_t l = offset;
			size_t r = l + total_length;
			size_t mid;
			for (size_t i = 0; i < max_step; ++i)
			{
				mid = (l + r) / 2;
				if (mem[mid] == target)
					return mid;
				else if (mem[mid] < target)
					l = mid;
				else
					r = mid;
			}
			return 0;
		}

		void QuantumBinarySearch_Fast::operator()(std::vector<System>& state) const
		{
			profiler _("QBS_Fast");
	#ifdef SINGLE_THREAD
			for (auto& s : state)
			{
	#else
	#pragma omp parallel for
			for (int i = 0; i < state.size(); ++i)
			{
				auto& s = state[i];
	#endif
				auto offset = s.GetAs(address_offset_id, uint64_t);
				auto target = s.GetAs(target_id, uint64_t);
				// Each state branch performs an independent classical binary search;
				// the hit address is XORed into the result register
				size_t result = binary_search(offset, target);
				s.get(result_id).value ^= result;
			}
		}

		void GetRowAddr::operator()(std::vector<System>& state) const
		{
	#ifdef SINGLE_THREAD
			for (auto& s : state)
			{
	#else
	#pragma omp parallel for
			for (int i = 0; i < state.size(); ++i)
			{
				auto& s = state[i];
	#endif
				s.get(row_offset_id).value ^=
					s.GetAs(offset_id, uint64_t) +
					row_sz * s.GetAs(row_id, uint64_t);
			}
		}

			void GetDataAddr::operator()(std::vector<System>& state) const
			{
#ifdef SINGLE_THREAD
			for (auto& s : state)
			{
#else
#pragma omp parallel for
			for (int i = 0; i < state.size(); ++i)
			{
				auto& s = state[i];
#endif
				s.get(row_data_id).value ^=
					s.GetAs(offset_id, uint64_t) +
					row_sz * s.GetAs(row_id, uint64_t) +
					s.GetAs(col_sparse_id, uint64_t);
				}
			}

			void GetQWRotateAngle_Int_Int_Int::operator()(std::vector<System>& state) const
			{
	#ifdef SINGLE_THREAD
				for (auto& s : state)
				{
	#else
	#pragma omp parallel for
				for (int i = 0; i < state.size(); ++i)
				{
					auto& s = state[i];
	#endif
					if (ConditionNotSatisfied(s))
						continue;

					// Read the quantized element and normalize it to a ratio in [0,1] according to the matrix
					// sign convention
					uint64_t v = s.GetAs(data_id, uint64_t);
					double ratio;
					if (mat->positive_only)
					{
						uint64_t Amax_real = pow2(mat->data_size) - 1;
						v = v % (Amax_real + 1);
						ratio = v * 1.0 / Amax_real;
					}
					else
					{
						uint64_t Amax_real = pow2(mat->data_size - 1) - 1;
						v = v % pow2(mat->data_size);
						int64_t v_real = get_complement(v, mat->data_size);
						ratio = std::abs(v_real) * 1.0 / Amax_real;
					}
					ratio = std::max(0.0, std::min(1.0, ratio));
					// Walk rotation angle theta = arccos(sqrt(ratio)), normalized by 1/(2*pi),
					// quantized into a Rational fixed-point value and then XORed into the output register
					double out = std::acos(std::sqrt(ratio)) / pi / 2;
					s.get(out_id).value ^= get_rational(out, System::size_of(out_id));
				}
			}

			SparseMatrixOracle1::SparseMatrixOracle1(
			qram_qutrit::QRAMCircuit* qram_,
			std::string_view reg_offset_,
			std::string_view reg_row_,
			std::string_view reg_col_id_,
			std::string_view reg_output_,
			size_t row_size_
		)
		{
			qram = qram_;
			reg_offset = reg_offset_;
			reg_row = reg_row_;
			reg_col_id = reg_col_id_;
			reg_output = reg_output_;
			row_size = row_size_;
		}
	
		SparseMatrixOracle2::SparseMatrixOracle2(
			qram_qutrit::QRAMCircuit* qram_,
			std::string_view reg_sparse_offset_,
			std::string_view reg_row_,
			std::string_view reg_col_,
			std::string_view reg_search_result_,
			size_t rowsize_)
		{
			qram = qram_;
			reg_sparse_offset = reg_sparse_offset_;
			reg_row = reg_row_;
			reg_col = reg_col_;
			reg_search_result = reg_search_result_;
			row_size = rowsize_;
		}

		SparseMatrixOracle2_ComputeCol::SparseMatrixOracle2_ComputeCol(
			qram_qutrit::QRAMCircuit* qram_,
			std::string_view sparse_offset_,
			std::string_view k_,
			std::string_view l_,
			std::string_view addr_offset_,
			size_t row_size_)
		{
			qram = qram_;
			sparse_offset = sparse_offset_;
			k = k_;
			l = l_;
			row_size = row_size_;
			addr_offset = addr_offset_;
		}

		void SparseMatrixOracle2_ComputeCol::operator()(std::vector<System>& system_states) const
		{
			/*
			* To compute k from j,l, out-of-place
			* Implemented by QRAM query
			*
			* |sparse_offset>|l>|z>|addr_offset = 0>			->
			* |sparse_offset>|l>|z + k>|addr_offset = 0>	
			*/			
			// addr_offset += l
			Add_UInt_UInt(sparse_offset, l, addr_offset)(system_states);

			// a[addr_offset] -> k
			QRAMLoad(qram, addr_offset, k)(system_states);

			// Cancel addr_offset
			Add_UInt_UInt(sparse_offset, l, addr_offset)(system_states);
		}

		SparseMatrixOracle2_ComputeSparsity::SparseMatrixOracle2_ComputeSparsity(
			qram_qutrit::QRAMCircuit* qram_,
			std::string_view sparse_offset_,
			std::string_view k_,
			std::string_view l_,
			size_t row_size_)
		{
			qram = qram_;
			sparse_offset = sparse_offset_;
			k = k_;
			l = l_;
			row_size = row_size_;
		}

		void SparseMatrixOracle2_ComputeSparsity::operator()(std::vector<System>& system_states) const
		{
			/*
			* To compute l from j,k, out-of-place
			* Implemented by QBS
			*
			|sparse_offset>|k>|z>			->
			|sparse_offset>|k>|z + l>
			*/

			// Search from [sparse_offset, sparse_offset + rowsize]
			// |sparse_offset>|k>|l> -> |sparse_offset>|k>|l + SearchResult (l+offset)>
			QuantumBinarySearch(qram, sparse_offset, row_size, k, l)(system_states);

			// Cancel extra offset 
			// |sparse_offset>|k>|l + SearchResult (l+offset)> ->
			// |sparse_offset>|k>|l + SearchResult (l)> ->
			Add_AnyInt_AnyInt_InPlace(l, sparse_offset).dag(system_states);
		}

			void CondRot_General_Bool_QW::operate(size_t l, size_t r, std::vector<System>& state, walk_angle_function_t func) const
		{
			size_t n = r - l;
			constexpr size_t full_size = 2;
			size_t original_size = state.size();

			if (n == 0) return;

			// 1. get the rotation matrix
			size_t v = state[l].GetAs(in_id, uint64_t);

			size_t row_id = state[l].GetAs(j_id, uint64_t);
			size_t col_id = state[l].GetAs(k_id, uint64_t);

			// Generate the 2x2 walk rotation matrix from the quantized element value and the row/column position
			u22_t rot_mat = func(v, row_id, col_id);

				if (_is_diagonal(rot_mat))
				{
					_operate_diagonal(l, r, state, rot_mat);
				}
				else if (_is_off_diagonal(rot_mat))
				{
					_operate_off_diagonal(l, r, state, rot_mat);
				}
				else
				{
					_operate_general(l, r, state, rot_mat);
				}
			}

		bool CondRot_General_Bool_QW::_is_diagonal(const u22_t &data)
		{
			if (abs_sqr(data[1]) < epsilon &&
				abs_sqr(data[2]) < epsilon)
			{
				return true;
			}
			return false;
		}

		void CondRot_General_Bool_QW::_operate_diagonal(size_t l, size_t r,
			std::vector<System>& state, const u22_t& mat) const
		{
			// diagonal means that no new elements will be created
			// any operation can be handled in-place

			std::complex<double> a0 = mat[0];
			std::complex<double> a1 = mat[3];

			for (size_t i = l; i < r; ++i)
			{
				auto& s = state[i];
				if (s.get(out_id).as<bool>(1))
				{
					s.amplitude *= a1;
				}
				else
				{
					s.amplitude *= a0;
				}
			}
		}

		bool CondRot_General_Bool_QW::_is_off_diagonal(const u22_t &data)
		{
			if (abs_sqr(data[0]) < epsilon &&
				abs_sqr(data[3]) < epsilon)
			{
				return true;
			}
			return false;
		}

		void CondRot_General_Bool_QW::_operate_off_diagonal(size_t l, size_t r,
			std::vector<System>& state, const u22_t& mat) const
		{
			// diagonal means that no new elements will be created
			// any operation can be handled in-place
			// with changing of storage (flipping)

			std::complex<double> a0 = mat[2];
			std::complex<double> a1 = mat[1];

			for (size_t i = l; i < r; ++i)
			{
				auto& s = state[i];
				auto& reg = s.get(out_id);
				if (reg.as<bool>(1))
				{
					s.amplitude *= a1;
					reg.value = 0; // flip
				}
				else
				{
					s.amplitude *= a0;
					reg.value = 1; // flip
				}
			}
		}

		void CondRot_General_Bool_QW::_operate_general(size_t l, size_t r,
			std::vector<System>& state, const u22_t &mat) const
		{
			size_t n = r - l;
			if (n == 1) // an extra entry should be added
			{
				size_t new_pos = state.size();
				state.push_back(state[l]);

				bool v = state[l].get(out_id).as<bool>(1);

				// if the original is 0
				if (!v)
				{
					state[new_pos].get(out_id).value = 1;

					state[l].amplitude *= mat[0];		// where |0>
					state[new_pos].amplitude *= mat[2]; // where |1>
				}
				// if the original is 1
				else
				{
					state[new_pos].get(out_id).value = 0;

					state[new_pos].amplitude *= mat[1]; // where |0>
					state[l].amplitude *= mat[3];		// where |1>
				}
			}
			else // everything can be computed in place
			{
				std::vector<std::complex<double>> new_amps(2, 0);
				complex_t a = state[l + 0].amplitude;
				complex_t b = state[l + 1].amplitude;
				state[l + 0].amplitude = a * mat[0] + b * mat[1];
				state[l + 1].amplitude = a * mat[2] + b * mat[3];
			}
		}

		void CondRot_General_Bool_QW::operator()(std::vector<System>& state) const
		{
			profiler _("CondRot_QW");
			(SortExceptKey(out_id))(state);
			size_t current_size = state.size();
			auto iter_l = 0;
			auto iter_r = 1;

			// States are already sorted by the output register; apply the walk rotation group by group. The dag version
			// uses the inverse rotation-angle function instead
			walk_angle_function_t func = make_func(*mat);

			while (true)
			{
				if (iter_r == current_size)
				{
					operate(iter_l, iter_r, state, func);
					break;
				}
				if (!compare_equal(state[iter_l], state[iter_r], out_id))
				{
					operate(iter_l, iter_r, state, func);
					iter_l = iter_r;
					iter_r = iter_l + 1;
				}
				else
				{
					iter_r++;
				}
			}
			ClearZero()(state);
			System::update_max_size(state.size());
		}

		void CondRot_General_Bool_QW::dag(std::vector<System>& state) const
		{
			profiler _("CondRot_QW");
			(SortExceptKey(out_id))(state);
			size_t current_size = state.size();
			auto iter_l = 0;
			auto iter_r = 1;
			walk_angle_function_t func = make_func_inv(*mat);

			while (true)
			{
				if (iter_r == current_size)
				{
					operate(iter_l, iter_r, state, func);
					break;
				}
				if (!compare_equal(state[iter_l], state[iter_r], out_id))
				{
					operate(iter_l, iter_r, state, func);
					iter_l = iter_r;
					iter_r = iter_l + 1;
				}
				else
				{
					iter_r++;
				}
			}
			ClearZero()(state);
			System::update_max_size(state.size());
		}

		std::vector<System> LCU_Container::state_of_j(size_t j)
		{
			size_t step = chebyshev_obj.step(j);
			return quantum_walk_obj.MakeNStepState(step);
		}

		void LCU_Container::add(std::vector<System> new_state, double coef, bool sign)
		{
			if (new_state.size() == 0) return;

			if (current_state.size() != 0)
			{
				if (current_state[0].registers.size() != new_state[0].registers.size())
				{
					throw_general_runtime_error();
				}
			}

			if (sign) coef *= -1;

			for (auto& s : new_state)
			{
				// s.sort_by_name();
				s.amplitude *= coef;
			}
			// The new state, scaled by the Chebyshev coefficient, is merged into the LCU combined state
			current_state.insert(current_state.end(), new_state.begin(), new_state.end());

		}

		void LCU_Container::iterate()
		{
			// LCU iteration: the j-th term = c_j · W^(2j+1). First prepare the walk state with the corresponding
			// number of steps, then accumulate it into the combined state according to the coefficient and sign,
			// and finally sort-merge to keep the state size under control
			for (size_t j = 0; j <= j0; ++j)
			{
				double coef = chebyshev_obj.coef(j);
				bool sign = chebyshev_obj.sign(j);
				auto &&ret = quantum_walk_obj.MakeNStepState(2 * j + 1);
				fmt::print("j={}\n", j);
				add(std::move(ret), coef, sign);

				sort_merge_unique_erase(current_state, std::less<System>(),
					std::equal_to<System>(), merge_system, remove_system);
			}
		}

		DenseVector<complex_t> LCU_Container_Theory::MakeStepState()
		{
			/* To fix: Why is this result different from chebyshev_n ?? */

			// T(2j+1)
			if (j == 0)
				return vec1;

			DenseVector<complex_t> vec2;
			// Chebyshev three-term recurrence T_{n+1} = 2·A'·T_n - T_{n-1} (A' is the normalized dense matrix);
			// each call advances two steps, corresponding to two consecutive single steps of the quantum-walk Step()
			vec2 = (densemat * vec1) * complex_t{ 2.0 } - vec0;
			vec0 = vec1;
			vec1 = vec2;
			vec2 = (densemat * vec1) * complex_t{ 2.0 } - vec0;
			vec0 = vec1;
			vec1 = vec2;

			return vec2;
		}

		bool LCU_Container_Theory::Step() 
		{
			if (j <= j0) {
				DenseVector<complex_t> step_state = MakeStepState();
				a += chebyshev_obj.coef(j);

				//fmt::print("Theory - \n");
				//fmt::print("coef = {}, sign = {}\n",
				//	chebyshev_obj.coef(j), chebyshev_obj.sign(j));
				//step_state = step_state / step_state.norm2();
				//fmt::print("Step = {}\n", complex2str(step_state.data));
				//fmt::print("Normal = {}\n", norm2(step_state.data));
				//fmt::print("Theory Ends\n");

				Add(step_state, chebyshev_obj.coef(j), chebyshev_obj.sign(j));
				++j;
				return true;
			}
			else
				return false;
		}

		std::pair<DenseVector<complex_t>, double> LCU_Container_Theory::GetOutput() const
		{
			// obtain the normalization factor
			double factor = current_state.norm2();
			// Success probability = ||current||^2 / a^2, where a is the accumulated sum of Chebyshev coefficients
			double prob = factor * factor / a / a;

			DenseVector<complex_t> ret = current_state / factor;
			return { ret, prob };
		}

		std::vector<complex_t> my_linear_solver_reference(const SparseMatrix& mat)
		{
			auto vec = ones<double>(mat.n_row);

			return my_linear_solver_reference(mat, vec);
		}

		std::vector<complex_t> my_linear_solver_reference(const SparseMatrix& mat, const DenseVector<double>& vec)
		{
			profiler _("my_linear_solver_reference");
			//auto densemat = sparse2dense<double>(mat);

			//if (!densemat.is_symmetric())
			//{
			//	fmt::print("Not symmetric!");
			//	throw_invalid_input();
			//}

			// auto result = my_linear_solver(densemat, vec);
			// Classical reference: solve with the Eigen sparse linear solver, then normalize by the 2-norm
			auto result = eigen_linear_solver(mat, vec);

			auto normalized_result = result / result.norm2();

			return normalized_result.to_vec<complex_t>();
		}
	}
}
