/**
 * @file debugger.h
 * @brief Debugging utilities definitions
 * @details Provides debugging, validation, and checking tools for quantum states, including normalization checks,
 *          NaN checks, state printing, block encoding extraction, and so on
 */

#pragma once
#include <functional>
#include <sstream>
#include "basic_components.h"
#include "matrix.h"
#include "partial_trace.h"
#include "sort_state.h"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Module inheritance test class
	 * @details Used to test the inheritance mechanism of BaseOperator
	 */
	struct ModuleInheritance_Test : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/**
		 * @brief Apply the test operation
		 * @param state System state vector
		 */
		inline void operator()(std::vector<System>& state) const
		{
			fmt::print("ModuleInheritance_Test::operator()\n");
		}

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the test operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Self-adjoint module inheritance test class
	 * @details Used to test the inheritance mechanism of SelfAdjointOperator
	 */
	struct ModuleInheritance_Test_SelfAdjoint : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/**
		 * @brief Apply the test operation
		 * @param state System state vector
		 */
		inline void operator()(std::vector<System>& state) const
		{
			fmt::print("ModuleInheritance_Test_SelfAdjoint::operator()\n");
		}

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the test operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Normalization check class
	 * @details Checks whether a quantum state is normalized (total probability is 1)
	 */
	struct CheckNormalization : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Normalization check threshold */
		double threshold = 1e-5;

		/**
		 * @brief Default constructor (uses the default threshold)
		 */
		CheckNormalization();

		/**
		 * @brief Constructor (with a specified threshold)
		 * @param threshold_ Check threshold
		 */
		CheckNormalization(double threshold_) : threshold(threshold_) {}

		/**
		 * @brief Apply the normalization check
		 * @param state System state vector
		 * @throws Throws an exception when the state is not normalized
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the normalization check
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Normalization check and renormalization class
	 * @details Checks the normalization of a quantum state and renormalizes it if it is not normalized
	 */
	struct CheckNormalization_Renormalize : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Normalization check threshold */
		double threshold = 1e-5;

		/**
		 * @brief Default constructor
		 */
		CheckNormalization_Renormalize() {}

		/**
		 * @brief Constructor (with a specified threshold)
		 * @param threshold_ Check threshold
		 */
		CheckNormalization_Renormalize(double threshold_) : threshold(threshold_) {}

		/**
		 * @brief Apply the check and renormalization
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the check and renormalization
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief NaN check class
	 * @details Checks whether NaN values exist in a quantum state
	 */
	struct CheckNan : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/**
		 * @brief Default constructor
		 */
		CheckNan();

		/**
		 * @brief Apply the NaN check
		 * @param state System state vector
		 * @throws Throws an exception when a NaN is found
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the NaN check
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Normalization viewing class
	 * @details Prints the normalization information of a quantum state
	 */
	struct ViewNormalization : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/**
		 * @brief Default constructor
		 */
		ViewNormalization();

		/**
		 * @brief Apply the normalization viewing
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the normalization viewing
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief State print display mode enum
	 */
	enum StatePrintDisplay : int32_t
	{
		Default = 0,   ///< Default display mode
		Detail = 1,    ///< Detailed display mode
		Binary = 2,    ///< Binary display mode
		Prob = 4,      ///< Probability display mode
	};

	/**
	 * @brief State printing class
	 * @details Prints quantum state information to standard output
	 */
	struct StatePrint : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Print switch */
		static bool on;

		/** @brief Display mode */
		int32_t display;

		/** @brief Precision */
		int precision;

		/**
		 * @brief Constructor (with a specified display mode)
		 * @param disp Display mode
		 */
		StatePrint(int32_t disp = 0) : display(disp), precision(0) {}

		/**
		 * @brief Constructor (with a specified display mode and precision)
		 * @param disp Display mode
		 * @param precision Precision
		 */
		StatePrint(int32_t disp, int precision) : display(disp), precision(precision) {}

		/**
		 * @brief Constructor (enum display mode)
		 * @param disp Display mode enum
		 */
		StatePrint(StatePrintDisplay disp) : display(static_cast<int32_t>(disp)), precision(0) {}

		/**
		 * @brief Convert the display mode to a string
		 * @return Display mode string
		 */
		std::string disp2str() const;

		/**
		 * @brief Apply state printing and return the formatted string
		 * @param state System state vector
		 * @return Formatted state string
		 */
		std::string to_string(std::vector<System>& state) const;

		/**
		 * @brief Apply state printing (to standard output)
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply state printing and return the formatted string
		 * @param state CUDA sparse state
		 * @return Formatted state string
		 */
		std::string to_string(CuSparseState& state) const;

		/**
		 * @brief CUDA apply state printing (to standard output)
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Removability test class
	 * @details Tests whether the specified register can be safely removed
	 */
	struct TestRemovable : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Register ID */
		size_t register_id;

		/**
		 * @brief Constructor (name version)
		 * @param register_name Register name
		 */
		TestRemovable(std::string_view register_name);

		/**
		 * @brief Constructor (ID version)
		 * @param register_name Register ID
		 */
		TestRemovable(size_t register_name);

		/**
		 * @brief Apply the removability test
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the removability test
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Duplicate key check class
	 * @details Checks whether duplicate system keys exist in a quantum state
	 */
	struct CheckDuplicateKey : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/**
		 * @brief Default constructor
		 */
		CheckDuplicateKey() {}

		/**
		 * @brief Check whether duplicate keys exist
		 * @param system_states System state vector
		 * @return Whether duplicate keys exist
		 */
		bool has_duplicate(std::vector<System>& system_states) const;

		/**
		 * @brief Apply the duplicate key check
		 * @param system_states System state vector
		 * @throws Throws an exception when duplicate keys are found
		 */
		void operator()(std::vector<System>& system_states) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the duplicate key check
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Check the unitarity of an in-place operation (general version)
	 * @details Verifies the unitarity of an in-place operation for an arbitrary register layout:
	 *          1. Iterate over all 2^total_bits input states
	 *          2. Perform the round-trip test: U then U† (or U† then U)
	 *          3. Verify that the state has not split (state.size() == 1)
	 *          4. Verify that the round-trip restores the original value (U*U† = I)
	 *          5. Verify bijectivity (no output collisions)
	 * @tparam Op Operation type (must inherit from BaseOperator)
	 * @param reg_sizes Size of each register (initializer_list); a total of ≤ 16 bits is recommended
	 * @param op_factory Factory function: std::vector<size_t>(reg_ids) → Op
	 * @param dagger When false, executes U then U†; when true, executes U† then U
	 * @return Truth table (input index → output index)
	 * @throws Throws an exception when the operation is non-unitary (state split), non-bijective (output collision),
	 *          or when the round-trip fails
	 */
	template <typename Op>
	std::vector<size_t> check_inplace_unitarity(
		std::initializer_list<size_t> reg_sizes,
		std::function<Op(std::vector<size_t>)> op_factory,
		bool dagger = false)
	{
		System::clear();

		std::vector<size_t> reg_ids;
		std::vector<size_t> sizes_vec;
		std::stringstream sbuf;
		for (size_t i = 0; i < reg_sizes.size(); ++i) {
			sbuf.str(""); sbuf << "_qram_tmp_" << i;
			size_t sz = *(reg_sizes.begin() + i);
			size_t id = System::add_register(sbuf.str(), UnsignedInteger, sz);
			reg_ids.push_back(id);
			sizes_vec.push_back(sz);
		}

		size_t total_bits = 0;
		for (size_t sz : sizes_vec) total_bits += sz;
		if (total_bits > 16)
			fmt::print("WARNING: check_inplace_unitarity with {} total bits\n", total_bits);

		Op op = op_factory(reg_ids);
		const size_t N = pow2(total_bits);

		std::vector<bool>   output_seen(N, false);
		std::vector<size_t> truth_table(N, 0);

		for (size_t input = 0; input < N; ++input) {
			std::vector<System> st;
			st.emplace_back();

			size_t remaining = input;
			for (size_t ri = 0; ri < reg_ids.size(); ++ri) {
				st[0].get(reg_ids[ri]).value = remaining & (pow2(sizes_vec[ri]) - 1);
				remaining >>= sizes_vec[ri];
			}

			if (dagger) {
				op.dag(st);
				op(st);
			} else {
				op(st);
				op.dag(st);
			}

			if (st.size() != 1)
				throw_bad_result("check_inplace_unitarity: state split (non-unitary)");

			size_t roundtrip = 0, shift = 0;
			for (size_t ri = 0; ri < reg_ids.size(); ++ri) {
				roundtrip |= (st[0].get(reg_ids[ri]).value & (pow2(sizes_vec[ri]) - 1)) << shift;
				shift += sizes_vec[ri];
			}
			if (roundtrip != input)
				throw_bad_result("check_inplace_unitarity: U*U† ≠ I");

			size_t output = 0; shift = 0;
			for (size_t ri = 0; ri < reg_ids.size(); ++ri) {
				output |= (st[0].get(reg_ids[ri]).value & (pow2(sizes_vec[ri]) - 1)) << shift;
				shift += sizes_vec[ri];
			}
			if (output_seen[output])
				throw_bad_result("check_inplace_unitarity: non-bijective output collision");
			output_seen[output] = true;
			truth_table[input] = output;
		}

		System::clear();
		return truth_table;
	}

	/**
	 * @brief Extract the block encoding matrix (internal implementation)
	 * @tparam BlockEncoding Block encoding type
	 * @tparam StateType State type
	 * @param encA Block encoding object
	 * @param main_reg Main register name
	 * @param anc_UA Ancilla register name
	 * @param is_full Whether to extract the full matrix
	 * @param is_dag Whether this is the dagger version
	 * @return Dense matrix representation of the block encoding
	 */
	template<typename BlockEncoding, typename StateType = SparseState>
	DenseMatrix<complex_t> _extract_block_encoding(BlockEncoding encA, std::string_view main_reg, std::string_view anc_UA,
		bool is_full = false, bool is_dag = false)
	{
		size_t main_reg_num = System::size_of(main_reg);
		size_t anc_UA_num = System::size_of(anc_UA);
		size_t main_reg_pos = System::get(main_reg);
		size_t anc_UA_pos = System::get(anc_UA);
		if (is_full == true)
		{
			DenseMatrix<complex_t> ret(pow2(main_reg_num + anc_UA_num));
			auto range_a = range(pow2(anc_UA_num));
			auto range_m = range(pow2(main_reg_num));

			for (auto [a, m] : product(range_a, range_m))
			{
				StateType state(1);

				state.back().get(main_reg_pos).value = m;
				state.back().get(anc_UA_pos).value = a;
				if (is_dag)
					encA.dag(state);
				else
					encA(state);
				std::vector<complex_t> vec(pow2(main_reg_num + anc_UA_num), 0);
				for (auto& s : state)
				{
					size_t _index = concat_value(
						{
							{s.get(main_reg_pos).value, main_reg_num},
							{s.get(anc_UA_pos).value, anc_UA_num},
						}
						);
					vec[_index] = s.amplitude;
				}
				size_t index = concat_value(
					{
						{m, main_reg_num},
						{a, anc_UA_num},
					}
					);
				for (size_t j = 0; j < pow2(main_reg_num + anc_UA_num); ++j)
				{
					ret(j, index) = vec[j];
				}
			}
			return ret;
		}
		else
		{
			DenseMatrix<complex_t> ret(pow2(main_reg_num));

			for (auto i : range(pow2(main_reg_num)))
			{
				StateType state(1);

				state.back().get(main_reg_pos).value = i;
				state.back().get(anc_UA_pos).value = 0;

				if (is_dag)
					encA.dag(state);
				else
					encA(state);

				double prob = PartialTraceSelect({ {anc_UA, 0} })(state);

				std::vector<complex_t> vec(pow2(main_reg_num), 0);
				for (auto& s : state)
				{
					vec[s.get(main_reg_pos).value] = s.amplitude;
				}
				for (size_t j = 0; j < pow2(main_reg_num); ++j)
				{
					ret(j, i) = vec[j] / prob;
				}
			}
			return ret;
		}
	}

	/**
	 * @brief Extract the block encoding matrix
	 * @tparam BlockEncoding Block encoding type
	 * @param encA Block encoding object
	 * @param main_reg Main register name
	 * @param anc_UA Ancilla register name
	 * @param is_full Whether to extract the full matrix
	 * @param is_dag Whether this is the dagger version
	 * @return Dense matrix representation of the block encoding
	 */
	template<typename BlockEncoding>
	DenseMatrix<complex_t> extract_block_encoding(BlockEncoding encA, std::string_view main_reg, std::string_view anc_UA,
		bool is_full = false, bool is_dag = false)
	{
		return _extract_block_encoding<BlockEncoding, SparseState>(encA, main_reg, anc_UA, is_full, is_dag);
	}

	/**
	 * @brief Check whether two states are equal
	 * @param state1 First state vector
	 * @param state2 Second state vector
	 * @throws Throws an exception and prints the difference when the states are not equal
	 */
	inline void state_equal_check(std::vector<System> state1, std::vector<System> state2)
	{	
		SortUnconditional()(state1);
		SortUnconditional()(state2);
		if (state1.size() != state2.size())
		{
			fmt::print("Size not equal: {} vs {}", state1.size(), state2.size());
			goto CHECK_FAILED;
		}
		for (size_t i = 0; i < state1.size(); ++i)
		{
			if (state1[i] != state2[i])
			{
				fmt::print("State not equal at index {}:\n", i);
				goto CHECK_FAILED;
			}
		}
		return;
	CHECK_FAILED:
		fmt::print("State 1:\n");
		StatePrint(0 | Detail)(state1);
		fmt::print("State 2:\n");
		StatePrint(0 | Detail)(state2);
		throw_general_runtime_error("Hadamard_Bool: GPU-version failed!");
	}
}

#ifdef USE_CUDA

namespace qram_simulator {

	/**
	 * @brief CUDA parallel operation test class
	 */
	struct ParallelOperationTest : BaseOperator {
		/** @brief Real part */
		double real;

		/** @brief Imaginary part */
		double imag;

		/** @brief Value */
		uint64_t value;

		/**
		 * @brief Constructor
		 * @param real_ Real part
		 * @param imag_ Imaginary part
		 * @param value_ Value
		 */
		ParallelOperationTest(double real_, double imag_, uint64_t value_) :
			real(real_), imag(imag_), value(value_) {
		}

		/**
		 * @brief CPU version (throws a not-implemented exception)
		 * @param state System state vector
		 * @throws Always throws a not-implemented exception
		 */
		void operator()(std::vector<System>& state) const {
			throw_not_implemented();
		}

		/**
		 * @brief CUDA apply the test operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
	};

}

#endif
