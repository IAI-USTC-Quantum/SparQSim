/**
 * @file system_operations.h
 * @brief System operations definitions
 * @details Implements the basic operations on quantum systems, including system splitting, merging, resetting,
 *          and register splitting/merging/moving/adding/removing operations
 */

#pragma once
#include "basic_components.h"
#include "debugger.h"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Split systems (in-place version)
	 * @param new_state New state vector
	 * @param old_state Old state vector
	 * @param condition_variable_nonzeros Nonzero condition variables
	 * @param condition_variable_all_ones All-ones condition variables
	 * @param condition_variable_by_bit Per-bit condition variables
	 * @param condition_variable_by_value Per-value condition variables
	 */
	void split_systems(std::vector<System>& new_state, std::vector<System>& old_state,
		const std::vector<size_t>& condition_variable_nonzeros,
		const std::vector<size_t>& condition_variable_all_ones,
		const std::vector<std::pair<size_t, size_t>>& condition_variable_by_bit,
		const std::vector<std::pair<size_t, size_t>>& condition_variable_by_value
	);

	/**
	 * @brief Split systems (return-value version)
	 * @param state State vector
	 * @param condition_variable_nonzeros Nonzero condition variables
	 * @param condition_variable_all_ones All-ones condition variables
	 * @param condition_variable_by_bit Per-bit condition variables
	 * @param condition_variable_by_value Per-value condition variables
	 * @return The split state vector
	 */
	std::vector<System> split_systems(std::vector<System>& state,
		const std::vector<size_t>& condition_variable_nonzeros,
		const std::vector<size_t>& condition_variable_all_ones,
		const std::vector<std::pair<size_t, size_t>>& condition_variable_by_bit,
		const std::vector<std::pair<size_t, size_t>>& condition_variable_by_value
	);

	/**
	 * @brief Split a sparse state
	 * @param state Sparse state
	 * @param condition_variable_nonzeros Nonzero condition variables
	 * @param condition_variable_all_ones All-ones condition variables
	 * @param condition_variable_by_bit Per-bit condition variables
	 * @param condition_variable_by_value Per-value condition variables
	 * @return The split sparse state
	 */
	SparseState split_systems(SparseState& state,
		const std::vector<size_t>& condition_variable_nonzeros,
		const std::vector<size_t>& condition_variable_all_ones,
		const std::vector<std::pair<size_t, size_t>>& condition_variable_by_bit,
		const std::vector<std::pair<size_t, size_t>>& condition_variable_by_value
	);

	/**
	 * @brief Merge systems (into a target)
	 * @param to Target state vector
	 * @param from Source state vector
	 */
	void combine_systems(std::vector<System>& to, const std::vector<System>& from);

	/**
	 * @brief Merge sparse states (into a target)
	 * @param to Target sparse state
	 * @param from Source sparse state
	 */
	void combine_systems(SparseState& to, const SparseState& from);

#ifdef USE_CUDA
	/**
	 * @brief CUDA split a sparse state
	 * @param state CUDA sparse state
	 * @param condition_variable_nonzeros Nonzero condition variables
	 * @param condition_variable_all_ones All-ones condition variables
	 * @param condition_variable_by_bit Per-bit condition variables
	 * @param condition_variable_by_value Per-value condition variables
	 * @return The split CUDA sparse state
	 */
	CuSparseState split_systems(CuSparseState& state,
		const std::vector<size_t>& condition_variable_nonzeros,
		const std::vector<size_t>& condition_variable_all_ones,
		const std::vector<std::pair<size_t, size_t>>& condition_variable_by_bit,
		const std::vector<std::pair<size_t, size_t>>& condition_variable_by_value
	);

	/**
	 * @brief CUDA merge sparse states (into a target)
	 * @param to Target CUDA sparse state
	 * @param from Source CUDA sparse state
	 */
	void combine_systems(CuSparseState& to, CuSparseState& from);
#endif

	/**
	 * @def SPLIT_BY_CONDITIONS
	 * @brief Macro that splits systems by conditions
	 */
#define SPLIT_BY_CONDITIONS \
	std::decay_t<decltype(state)> unconditioned_state(0);\
	if (HasCondition)\
	{\
		unconditioned_state = split_systems(state,\
			condition_variable_nonzeros,\
			condition_variable_all_ones,\
			condition_variable_by_bit,\
			condition_variable_by_value\
		);\
	}\
	if (state.size())

	/**
	 * @def MERGE_BY_CONDITIONS
	 * @brief Macro that merges systems by conditions
	 */
#define MERGE_BY_CONDITIONS \
	if (!unconditioned_state.empty()) {\
		combine_systems(state, unconditioned_state);\
	}

	/**
	 * @brief Reset systems
	 * @param state System state vector
	 */
	void reset_systems(std::vector<System>& state);

	/**
	 * @brief Reset a sparse state
	 * @param state Sparse state
	 */
	void reset_systems(SparseState& state);

#ifdef USE_CUDA
	/**
	 * @brief CUDA reset a sparse state
	 * @param state CUDA sparse state
	 */
	void reset_systems(CuSparseState& state);
#endif

	/**
	 * @brief Add systems (with a coefficient)
	 * @param current_state Current state vector
	 * @param new_state New state vector
	 * @param coef Coefficient
	 */
	inline void add_systems(std::vector<System>& current_state, const std::vector<System>& new_state, double coef)
	{
		if (new_state.size() == 0) return;
		size_t original_size = current_state.size();		

		current_state.insert(current_state.end(), new_state.begin(), new_state.end());

		if (std::abs(coef - 1.0) > epsilon)
		{
			for (auto iter = current_state.begin() + original_size; iter != current_state.end(); ++iter)
			{
				iter->amplitude *= coef;
			}
		}

		if (original_size != 0)
			sort_merge_unique_erase(current_state, std::less<System>(),
				std::equal_to<System>(), merge_system, remove_system);
	}

	/**
	 * @brief Add sparse states (with a coefficient)
	 * @param current Current sparse state
	 * @param new_state New sparse state
	 * @param coef Coefficient
	 */
	inline void add_systems(SparseState& current, const SparseState& new_state, double coef)
	{
		return add_systems(current.basis_states, new_state.basis_states, coef);
	}

#ifdef USE_CUDA
	/**
	 * @brief CUDA add sparse states (with a coefficient)
	 * @param current Current CUDA sparse state
	 * @param new_state New CUDA sparse state
	 * @param coef Coefficient
	 */
	void add_systems(CuSparseState& current, const CuSparseState& new_state, double coef);
#endif

	/**
	 * @brief Split-register operation
	 * @details Splits one register into two registers
	 */
	struct SplitRegister {
		/** @brief First register name */
		std::string first_name;

		/** @brief Second register name */
		std::string second_name;

		/** @brief Second register size */
		size_t second_size;

		/**
		 * @brief Constructor (ID version)
		 * @param first_id_ First register ID
		 * @param second_name_ Second register name
		 * @param second_size_ Second register size
		 */
		SplitRegister(size_t first_id_, std::string_view second_name_, size_t second_size_)
			: first_name(System::name_of(first_id_)), second_name(second_name_), second_size(second_size_) { }

		/**
		 * @brief Constructor (name version)
		 * @param first_name_ First register name
		 * @param second_name_ Second register name
		 * @param second_size_ Second register size
		 */
		SplitRegister(std::string_view first_name_, std::string_view second_name_, size_t second_size_)
			: first_name(first_name_), second_name(second_name_), second_size(second_size_) {}

		/**
		 * @brief Apply the split operation
		 * @param state System state vector
		 * @return New register ID
		 */
		size_t operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the split operation (sparse state version)
		 * @param state Sparse state
		 * @return New register ID
		 */
		size_t operator()(SparseState& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the split operation
		 * @param state CUDA sparse state
		 * @return New register ID
		 */
		size_t operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Combine-register operation
	 * @details Merges two registers (removes the second one)
	 */
	struct CombineRegister {
		/** @brief First register name */
		std::string first_name;

		/** @brief Second register name */
		std::string second_name;

		/**
		 * @brief Constructor (ID version)
		 * @param first_id_ First register ID
		 * @param second_id_ Second register ID
		 */
		CombineRegister(size_t first_id_, size_t second_id_)
			: first_name(System::name_of(first_id_)), second_name(System::name_of(second_id_)) { }

		/**
		 * @brief Constructor (name version)
		 * @param first_name_ First register name
		 * @param second_name_ Second register name
		 */
		CombineRegister(std::string_view first_name_, std::string_view second_name_)
			: first_name(first_name_), second_name(second_name_) { }

		/**
		 * @brief Apply the combine operation
		 * @param state System state vector
		 * @return Combined register ID
		 */
		size_t operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the combine operation (sparse state version)
		 * @param state Sparse state
		 * @return Combined register ID
		 */
		size_t operator()(SparseState& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the combine operation
		 * @param state CUDA sparse state
		 * @return Combined register ID
		 */
		size_t operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Move-register-back operation
	 * @details Changes the position of a register (unsafe, must not be called as a subprocedure)
	 */
	struct MoveBackRegister
	{
		/** @brief Register ID */
		size_t register_id;

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Register name
		 */
		MoveBackRegister(std::string_view reg_in);

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 */
		MoveBackRegister(size_t reg_in);

		/**
		 * @brief Apply the move-back operation
		 * @param states System state vector
		 */
		void operator()(std::vector<System>& states) const;

		/**
		 * @brief Apply the move-back operation (sparse state version)
		 * @param state Sparse state
		 */
		void operator()(SparseState& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the move-back operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Add-register operation
	 * @details Adds a new register to the system
	 */
	struct AddRegister {
		/** @brief Register name */
		std::string register_name;

		/** @brief Register type */
		StateStorageType type;

		/** @brief Register size */
		size_t size;

		/**
		 * @brief Constructor
		 * @param register_name_ Register name
		 * @param type_ Register type
		 * @param size_ Register size
		 */
		AddRegister(std::string_view register_name_, StateStorageType type_, size_t size_);

		/**
		 * @brief Apply the add operation
		 * @param state System state vector
		 * @return New register ID
		 */
		size_t operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the add operation (sparse state version)
		 * @param state Sparse state
		 * @return New register ID
		 */
		size_t operator()(SparseState& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the add operation
		 * @param state CUDA sparse state
		 * @return New register ID
		 */
		size_t operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Add-register-and-apply-Hadamard operation
	 * @details Adds a new register and initializes it to a uniform superposition state
	 */
	struct AddRegisterWithHadamard {
		/** @brief Register name */
		std::string register_name;

		/** @brief Register type */
		StateStorageType type;

		/** @brief Register size */
		size_t size;

		/**
		 * @brief Constructor
		 * @param register_name_ Register name
		 * @param type_ Register type
		 * @param size_ Register size
		 */
		AddRegisterWithHadamard(std::string_view register_name_, StateStorageType type_, size_t size_);

		/**
		 * @brief Apply the add-and-Hadamard operation
		 * @param state System state vector
		 * @return New register ID
		 */
		size_t operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the add-and-Hadamard operation (sparse state version)
		 * @param state Sparse state
		 * @return New register ID
		 */
		size_t operator()(SparseState& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the add-and-Hadamard operation
		 * @param state CUDA sparse state
		 * @return New register ID
		 */
		size_t operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Remove-register operation
	 * @details Removes the specified register from the system
	 */
	struct RemoveRegister {
		/** @brief Register ID */
		size_t register_id;

		/**
		 * @brief Constructor (name version)
		 * @param register_name Register name
		 */
		RemoveRegister(std::string_view register_name);

		/**
		 * @brief Constructor (ID version)
		 * @param register_name_ Register ID
		 */
		RemoveRegister(size_t register_name_);

		/**
		 * @brief Apply the remove operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the remove operation (sparse state version)
		 * @param state Sparse state
		 */
		void operator()(SparseState& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the remove operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Push operation
	 * @details Pushes the register state onto a temporary stack
	 */
	struct Push : BaseOperator
	{
		using BaseOperator::operator();

		/** @brief Temporary register name */
		std::string garbage_name;

		/** @brief Register ID */
		size_t reg_id;

		/**
		 * @brief Constructor (name version)
		 * @param regname_ Register name
		 * @param garbage_name_ Temporary register name
		 */
		Push(std::string_view regname_, std::string_view garbage_name_)
			:reg_id(System::get(regname_)), garbage_name(garbage_name_)
		{ }

		/**
		 * @brief Constructor (ID version)
		 * @param regname_ Register ID
		 * @param garbage_name_ Temporary register name
		 */
		Push(size_t regname_, std::string_view garbage_name_)
			: reg_id(regname_), garbage_name(garbage_name_)
		{ }

		/**
		 * @brief Apply the push operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the push operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Pop operation
	 * @details Restores the register state from the temporary stack
	 */
	struct Pop : BaseOperator
	{
		using BaseOperator::operator();

		/** @brief Register ID */
		size_t reg_id;

		/** @brief Register name */
		std::string reg_name;

		/**
		 * @brief Constructor (name version)
		 * @param reg_name_ Register name
		 */
		Pop(std::string_view reg_name_) : reg_id(System::get(reg_name_)), reg_name(reg_name_)
		{ }

		/**
		 * @brief Constructor (ID version)
		 * @param reg_name_ Register ID
		 */
		Pop(size_t reg_name_) : reg_id(reg_name_)
		{ }

		/**
		 * @brief Apply the pop operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the pop operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Clear-zero operation
	 * @details Removes the state components whose amplitudes are close to zero
	 */
	struct ClearZero : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Clear-zero threshold */
		double eps;

		/**
		 * @brief Default constructor (uses the default epsilon)
		 */
		ClearZero() :eps(epsilon) {};

		/**
		 * @brief Constructor (with a specified threshold)
		 * @param eps_ Clear-zero threshold
		 */
		ClearZero(double eps_) :eps(eps_) {};

		/**
		 * @brief Apply the clear-zero operation
		 * @param system_states System state vector
		 */
		void operator()(std::vector<System>& system_states) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the clear-zero operation
		 * @param s CUDA sparse state
		 */
		void operator()(CuSparseState& s) const;
#endif
	};

	/**
	 * @brief State loading operation
	 * @details Loads a quantum state from a file
	 */
	struct StateLoad
	{
		/** @brief Main register name */
		std::string main_reg;

		/** @brief Ancilla register UA name */
		std::string anc_UA;

		/** @brief Ancilla register 4 name */
		std::string anc_4;

		/** @brief Ancilla register 3 name */
		std::string anc_3;

		/** @brief Ancilla register 2 name */
		std::string anc_2;

		/** @brief Ancilla register 1 name */
		std::string anc_1;

		/** @brief Data size */
		size_t data_size;

		/** @brief Rational number size */
		size_t rational_size;

		/** @brief Save name */
		std::string savename;

		/**
		 * @brief Constructor
		 * @param main_reg Main register name
		 * @param anc_UA Ancilla register UA name
		 * @param anc_4 Ancilla register 4 name
		 * @param anc_3 Ancilla register 3 name
		 * @param anc_2 Ancilla register 2 name
		 * @param anc_1 Ancilla register 1 name
		 * @param ds Data size
		 * @param rs Rational number size
		 */
		StateLoad(
			std::string main_reg,
			std::string anc_UA,
			std::string anc_4,
			std::string anc_3,
			std::string anc_2,
			std::string anc_1,
			size_t ds,
			size_t rs) :
			main_reg(main_reg), anc_UA(anc_UA), anc_4(anc_4), anc_3(anc_3), anc_2(anc_2), anc_1(anc_1),
			data_size(ds), rational_size(rs) {}

		/**
		 * @brief Load a state from a file
		 * @param savename_ File name
		 * @return System state vector
		 */
		std::vector<System> operator()(const std::string& savename_) const;

		/**
		 * @brief Parse an amplitude value
		 * @param line File line
		 * @return Complex amplitude
		 */
		complex_t load_amplitude(const std::string& line) const;

		/**
		 * @brief Parse a register value
		 * @param line File line
		 * @param reg Register name
		 * @return Register value
		 */
		size_t load_reg(const std::string& line, const std::string& reg) const;

		/**
		 * @brief Parse a branch
		 * @param line File line
		 * @return System state
		 */
		System load_branch(const std::string& line) const;

		/**
		 * @brief Check whether a line is a branch line
		 * @param line File line
		 * @return Whether the line is a branch line
		 */
		bool is_branch(const std::string& line) const;
	};

	/**
	 * @brief Print a state to a file
	 * @param state System state vector
	 * @param filename File name
	 * @param precision Precision (16 by default)
	 * @throws Throws an exception when the file cannot be opened
	 */
	inline void print_state_to_file(std::vector<System>& state, const std::string &filename, int precision = 16)
	{
		{
			std::ofstream f_state(filename);
			if (!f_state.is_open()) {
				throw std::runtime_error("Failed to open file.");
			}
			f_state.close();
		}
		if (precision == 0)
			throw_invalid_input();

		{
			std::ofstream f_state(filename, std::ios_base::app);
			if (!f_state.is_open()) {
				throw std::runtime_error("Failed to open file.");
			}
			f_state << fmt::format("Print State To File:\n");

			for (auto& s : state) {
				f_state << fmt::format("{}\n", s.to_string(precision));
			}

			f_state.close();
		}
	}
}
