/**
 * @file basic_components.h
 * @brief Basic component definitions
 * @details Defines the core data structures and base classes of the sparse state simulator, including state
 *          storage, system management, and the operator base class
 */

#pragma once

#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "qram_circuit_qutrit.h"
#include "global_macros.h"
#include "cuda/cuda_utils.cuh"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/** @typedef StateInfoType
	 * @brief State info type
	 * @details A tuple containing the register name, state storage type, size, and active status
	 */
	using StateInfoType = std::tuple<std::string, StateStorageType, size_t, bool>;

	/**
	 * @brief Get the name from the state info (const version)
	 * @param m State info tuple
	 * @return Const reference to the register name
	 */
	const std::string& get_name(const StateInfoType& m);

	/**
	 * @brief Get the name from the state info (non-const version)
	 * @param m State info tuple
	 * @return Reference to the register name
	 */
	std::string& get_name(StateInfoType& m);

	/**
	 * @brief Get the type from the state info (const version)
	 * @param m State info tuple
	 * @return Const reference to the state storage type
	 */
	const StateStorageType& get_type(const StateInfoType& m);

	/**
	 * @brief Get the type from the state info (non-const version)
	 * @param m State info tuple
	 * @return Reference to the state storage type
	 */
	StateStorageType& get_type(StateInfoType& m);

	/**
	 * @brief Get the size from the state info
	 * @param m State info tuple
	 * @return Register size
	 */
	size_t get_size(const StateInfoType& m);

	/**
	 * @brief Get a reference to the size in the state info
	 * @param m State info tuple
	 * @return Reference to the register size
	 */
	size_t& get_size(StateInfoType& m);

	/**
	 * @brief Get the active status from the state info
	 * @param m State info tuple
	 * @return Register active status
	 */
	bool get_status(const StateInfoType& m);

	/**
	 * @brief Get a reference to the active status in the state info
	 * @param m State info tuple
	 * @return Reference to the register active status
	 */
	bool& get_status(StateInfoType& m);

	/**
	 * @brief State storage structure
	 * @details The actual storage unit of a quantum register state, using uint64_t to hold the underlying value
	 */
	struct StateStorage
	{
		/** @brief The actually stored value */
		uint64_t value = 0;

		/**
		 * @brief Interpret the value as the specified type
		 * @tparam Ty Target type (signed/unsigned integers, floating point, and bool are supported)
		 * @param size Number of bits
		 * @return The converted value
		 * @throws Throws an exception when the type is not supported
		 */
		template<typename Ty>
		Ty as(size_t size) const
		{
			/* pow2(64) overflows (shift count 64 is UB): mask manually */
			uint64_t truncated_value = value & (size >= 64 ? ~uint64_t{0} : pow2(size) - (size != 0));

			if constexpr (std::is_floating_point_v<Ty>)
			{
				if (size == 64) return truncated_value * 1.0 / 2 / pow2(63);
				return 1.0 * truncated_value / pow2(size);
			}
			else if constexpr (std::is_same_v<Ty, bool>)
			{
				return bool(truncated_value);
			}
			else if constexpr (std::is_integral_v<Ty>)
			{
				if constexpr (std::is_signed_v<Ty>)
				{
					return get_complement(truncated_value, size);
				}
				else
				{
					return truncated_value;
				}
			}
			else {
				throw_invalid_input();
			}
		}

		/**
		 * @brief Constructor
		 */
		HOST_DEVICE StateStorage() {}

		/**
		 * @brief Safely access the value (non-const version)
		 * @param size Number of bits
		 * @return Reference to the value
		 */
		HOST_DEVICE uint64_t& val(size_t size);

		/**
		 * @brief Safely access the value (const version)
		 * @param size Number of bits
		 * @return The value
		 */
		HOST_DEVICE uint64_t val(size_t size) const;

		/**
		 * @brief Equality comparison operator
		 * @param rhs Right-hand operand
		 * @return Whether equal
		 */
		HOST_DEVICE bool operator==(const StateStorage& rhs) const;

		/**
		 * @brief Inequality comparison operator
		 * @param rhs Right-hand operand
		 * @return Whether not equal
		 */
		HOST_DEVICE bool operator!=(const StateStorage& rhs) const;

		/**
		 * @brief Less-than comparison operator
		 * @param rhs Right-hand operand
		 * @return Whether less than
		 */
		HOST_DEVICE bool operator<(const StateStorage& rhs) const;

		/**
		 * @brief Greater-than comparison operator
		 * @param rhs Right-hand operand
		 * @return Whether greater than
		 */
		HOST_DEVICE bool operator>(const StateStorage& rhs) const;

		/**
		 * @brief Convert to string
		 * @param info State info
		 * @return String representation
		 */
		std::string to_string(const StateInfoType& info) const;

		/**
		 * @brief Convert to an IO string
		 * @param info State info
		 * @return String in IO format
		 */
		std::string to_io_string(const StateInfoType& info) const;

		/**
		 * @brief Convert to a binary string
		 * @param info State info
		 * @return String in binary format
		 */
		std::string to_binary_string(const StateInfoType& info) const;

		/**
		 * @brief Flip the specified bit
		 * @param digit Bit index
		 */
		HOST_DEVICE void flip(size_t digit);
	};

	/** @brief Forward declaration: sparse state */
	struct SparseState;

#ifdef USE_CUDA
	/** @brief Forward declaration: CUDA sparse state */
	struct CuSparseState;
#endif

	/**
	 * @brief System class
	 * @details Core class managing quantum registers and system state, containing static register information
	 *          and dynamic state data
	 */
	struct System 
	{
#ifdef CACHED_REGISTER_SIZE
		/** @brief Initial preallocated capacity on CPU / fixed capacity on CUDA */
		constexpr static size_t InitialRegisterCapacity = CACHED_REGISTER_SIZE;
#else
		/** @brief Default initial preallocated capacity */
		constexpr static size_t InitialRegisterCapacity = 64;
#endif
		/** @brief Kept for old-code compatibility; on CPU this value is no longer the register count limit */
		constexpr static size_t CachedRegisterSize = InitialRegisterCapacity;
#ifdef USE_CUDA
		static_assert(
			CachedRegisterSize <= 64,
			"CUDA builds require CachedRegisterSize <= 64");
#endif

		/** @brief Register info map */
		inline static std::vector<StateInfoType> name_register_map;

		/** @brief Hash index from name to index (O(1) lookup) */
		inline static std::unordered_map<std::string, size_t> name_to_index;

		/** @brief Whether the hash index is valid (invalidated by operations such as MoveRegister) */
		inline static bool name_index_valid = true;

		/** @brief Register a name into the hash index */
		inline static void register_name(std::string name, size_t idx) {
			name_to_index.emplace(std::move(name), idx);
		}

		/** @brief Remove a name from the hash index */
		inline static void unregister_name(std::string_view name) {
			name_to_index.erase(std::string(name));
		}

		/** @brief Invalidate the hash index (called after operations such as MoveRegister) */
		inline static void invalidate_name_index() { name_index_valid = false; }

		/** @brief Rebuild the hash index (lazily triggered) */
		inline static void rebuild_name_index() {
			name_to_index.clear();
			for (size_t i = 0; i < name_register_map.size(); ++i) {
				if (status_of(i)) {
					name_to_index.emplace(std::string(name_of(i)), i);
				}
			}
		}

		/** @brief Register status bitmap (CUDA fast path; on CPU, StateInfoType is authoritative) */
		inline static uint64_t reg_status_bitmap = 0;

		/** @brief Maximum qubit count statistic */
		inline static size_t max_qubit_count = 0;

		/** @brief Maximum register count statistic */
		inline static size_t max_register_count = 0;

		/** @brief Maximum system size statistic */
		inline static size_t max_system_size = 0;

		/** @brief Temporal register stack */
		inline static std::vector<size_t> temporal_registers;

		/** @brief Reusable register list */
		inline static std::vector<size_t> reusable_registers;

		/** @brief State amplitude */
		complex_t amplitude = 1.0;

		/** @brief The CUDA device path requires a fixed, trivially copyable register layout */
#ifdef USE_CUDA
		std::array<StateStorage, CachedRegisterSize> registers;
#else
		/** @brief CPU register storage; preallocated, then grows on demand */
		mutable std::vector<StateStorage> registers;
#endif

		/**
		 * @brief Get the state component at the given position (non-const version)
		 * @param id Register ID
		 * @return Reference to the state storage
		 */
#ifdef USE_CUDA
		HOST_DEVICE StateStorage& get(size_t id) {
			return registers[id];
		}
#else
		StateStorage& get(size_t id) {
			if (id >= name_register_map.size())
				throw std::runtime_error("Register not found.");
			ensure_register_count(name_register_map.size());
			return registers[id];
		}
#endif

		/**
		 * @brief Get the state component at the given position (const version)
		 * @param id Register ID
		 * @return Const reference to the state storage
		 */
#ifdef USE_CUDA
		HOST_DEVICE const StateStorage& get(size_t id) const {
			return registers[id];
		}
#else
		const StateStorage& get(size_t id) const {
			if (id >= name_register_map.size())
				throw std::runtime_error("Register not found.");
			ensure_register_count(name_register_map.size());
			return registers[id];
		}

		/**
		 * @brief Ensure the CPU basis state has at least count register slots
		 * @details get() synchronizes to the full current register table at once, so that earlier
		 *          references are not invalidated by later vector growth when the same operation
		 *          acquires multiple register references in sequence.
		 * @param count Number of register slots needed
		 */
		void ensure_register_count(size_t count) const {
			if (registers.size() < count)
				registers.resize(count);
		}
#endif

		/**
		 * @brief Clear register allocation information
		 */
		static void clear();

		/**
		 * @brief Get the total number of qubits
		 * @return Number of qubits
		 */
		static size_t get_qubit_count();

		/**
		 * @brief Get the number of activated registers
		 * @return Number of activated registers
		 */
		static size_t get_activated_register_size();

		/**
		 * @brief Get the ID of the last activated register
		 * @return Register ID
		 */
		static size_t get_last_activated_register();

		/**
		 * @brief Access the last activated register (non-const version)
		 * @return Reference to the state storage
		 */
		StateStorage& last_register();

		/**
		 * @brief Access the last activated register (const version)
		 * @return Const reference to the state storage
		 */
		const StateStorage& last_register() const;

		/**
		 * @brief Update the maximum system size
		 * @param new_size New size
		 */
		static void update_max_size(size_t new_size);

		/**
		 * @brief Get register ID by name
		 * @param name Register name
		 * @return Register ID
		 */
		static size_t get(std::string_view name);

		/**
		 * @brief Get register info by name
		 * @param name Register name
		 * @return State info
		 */
		static StateInfoType get_register_info(std::string_view name);

		/**
		 * @brief Get register name by ID
		 * @param id Register ID
		 * @return Const reference to the register name
		 */
		static const std::string& name_of(size_t id);

		/**
		 * @brief Get register size by name
		 * @param name Register name
		 * @return Register size
		 */
		static size_t size_of(std::string_view name);

		/**
		 * @brief Get register size by ID
		 * @param id Register ID
		 * @return Register size
		 */
		static size_t size_of(size_t id);

		/**
		 * @brief Get register type by name
		 * @param name Register name
		 * @return State storage type
		 */
		static StateStorageType type_of(std::string_view name);

		/**
		 * @brief Get register type by ID
		 * @param id Register ID
		 * @return State storage type
		 */
		static StateStorageType type_of(size_t id);

		/**
		 * @brief Get register active status by name
		 * @param name Register name
		 * @return Active status (true = active)
		 */
		static bool status_of(std::string_view name);

		/**
		 * @brief Get register active status by ID
		 * @param id Register ID
		 * @return Active status (true = active)
		 */
		static bool status_of(size_t id);

		/**
		 * @brief Add a register status bitmap flag
		 * @param pos Position
		 */
		static void add_register_status_bitmap(size_t pos);

		/**
		 * @brief Remove a register status bitmap flag
		 * @param pos Position
		 */
		static void remove_register_status_bitmap(size_t pos);

		/**
		 * @brief Add a new register
		 * @param name Register name
		 * @param type State storage type
		 * @param size Register size
		 * @return Register ID
		 */
		static size_t add_register(std::string_view name, StateStorageType type, size_t size);

		/**
		 * @brief Add a register synchronously (initial state is 0)
		 * @param name Register name
		 * @param type State storage type
		 * @param size Register size
		 * @param system_states System state vector
		 * @return Register ID
		 */
		static size_t add_register_synchronous(
			std::string_view name, StateStorageType type, size_t size,
			std::vector<System>& system_states);

		/**
		 * @brief Add a register synchronously (initial state is 0, SparseState version)
		 * @param name Register name
		 * @param type State storage type
		 * @param size Register size
		 * @param system_states Sparse state
		 * @return Register ID
		 */
		static size_t add_register_synchronous(
			std::string_view name, StateStorageType type, size_t size,
			SparseState& system_states);

		/**
		 * @brief Remove a register by ID
		 * @param id Register ID
		 */
		static void remove_register(size_t id);

		/**
		 * @brief Remove a register by name
		 * @param name Register name
		 */
		static void remove_register(std::string_view name);

		/**
		 * @brief Remove a register synchronously (by ID)
		 * @param id Register ID
		 * @param state System state vector
		 */
		static void remove_register_synchronous(size_t id,
			std::vector<System>& state);

		/**
		 * @brief Remove a register synchronously (by name)
		 * @param name Register name
		 * @param state System state vector
		 */
		static void remove_register_synchronous(std::string_view name,
			std::vector<System>& state);

		/**
		 * @brief Remove a register synchronously (SparseState version, by ID)
		 * @param id Register ID
		 * @param state Sparse state
		 */
		static void remove_register_synchronous(size_t id,
			SparseState& state);

		/**
		 * @brief Remove a register synchronously (SparseState version, by name)
		 * @param name Register name
		 * @param state Sparse state
		 */
		static void remove_register_synchronous(std::string_view name,
			SparseState& state);

		/**
		 * @brief Constructor
		 */
#ifdef USE_CUDA
		HOST_DEVICE System() {}
#else
		System()
			: registers(name_register_map.size())
		{
			const size_t reserve_count =
				InitialRegisterCapacity > name_register_map.size()
				? InitialRegisterCapacity
				: name_register_map.size();
			registers.reserve(reserve_count);
		}
#endif

		/**
		 * @brief Less-than comparison operator
		 * @param rhs Right-hand system
		 * @return Whether less than
		 */
		HOST_DEVICE bool operator<(const System& rhs) const;

		/**
		 * @brief Equality comparison operator
		 * @param rhs Right-hand system
		 * @return Whether equal
		 */
		HOST_DEVICE bool operator==(const System& rhs) const;

		/**
		 * @brief Inequality comparison operator
		 * @param rhs Right-hand system
		 * @return Whether not equal
		 */
		HOST_DEVICE bool operator!=(const System& rhs) const;

		/**
		 * @brief Convert to string
		 * @return String representation
		 */
		std::string to_string() const;

		/**
		 * @brief Convert to string (with given precision)
		 * @param precision Precision
		 * @return String representation
		 */
		std::string to_string(int precision) const;
	};

	/**
	 * @brief Merge two systems
	 * @details Adds the amplitude of s2 to s1 and sets s2.amplitude to 0
	 * @param s1 First system (destination)
	 * @param s2 Second system (source)
	 */
	void merge_system(System& s1, System& s2);

	/**
	 * @brief Remove systems close to zero
	 * @param s System
	 * @return true if it should be removed
	 */
	bool remove_system(const System& s);

	/**
	 * @brief Sparse state class
	 * @details A sparse quantum state represented as a vector of basis states
	 */
	struct SparseState
	{
		/** @brief Basis state vector */
		std::vector<System> basis_states;

		/** @brief Vector type alias */
		using vector_type = std::vector<System>;

		/**
		 * @brief Default constructor
		 */
		SparseState() {
			basis_states.emplace_back();
		}

		/**
	 * @brief Constructor with a given size
	 * @param size Size
		 */
		SparseState(size_t size)
			: basis_states(size) {}

		/**
	 * @brief Copy constructor
	 * @param basis_states_ Basis state vector
		 */
		SparseState(const std::vector<System>& basis_states_) 
			: basis_states(basis_states_) {}

		/**
	 * @brief Move constructor
	 * @param basis_states_ Basis state vector
		 */
		SparseState(std::vector<System>&& basis_states_)
			: basis_states(std::move(basis_states_)) {}

		/**
	 * @brief Copy constructor
	 * @param other Another sparse state
		 */
		SparseState(const SparseState& other)
			: basis_states(other.basis_states) {}

		/**
	 * @brief Move constructor
	 * @param other Another sparse state
		 */
		SparseState(SparseState&& other)
			: basis_states(std::move(other.basis_states)) {}

		/**
	 * @brief Copy assignment operator
	 * @param other Another sparse state
	 * @return Reference to itself
		 */
		SparseState& operator=(const SparseState& other) {
			basis_states = other.basis_states;
			return *this;
		}

		/**
	 * @brief Move assignment operator
	 * @param other Another sparse state
	 * @return Reference to itself
		 */
		SparseState& operator=(SparseState&& other) {
			basis_states = std::move(other.basis_states);
			return *this;
		}

		/**
	 * @brief Get the last element (non-const version)
	 * @return Reference to the last system
		 */
		System& back() { return basis_states.back(); }

		/**
	 * @brief Get the last element (const version)
	 * @return Const reference to the last system
		 */
		const System& back() const { return basis_states.back(); }

		/**
	 * @brief Get the starting iterator
	 * @return Starting iterator
		 */
		vector_type::iterator begin() { return basis_states.begin(); }
		vector_type::const_iterator begin() const { return basis_states.begin(); }
		vector_type::iterator end() { return basis_states.end(); }
		vector_type::const_iterator end() const { return basis_states.end(); }
		vector_type::reverse_iterator rbegin() { return basis_states.rbegin(); }
		vector_type::const_reverse_iterator rbegin() const { return basis_states.rbegin(); }
		vector_type::reverse_iterator rend() { return basis_states.rend(); }
		vector_type::const_reverse_iterator rend() const { return basis_states.rend(); }

		/**
	 * @brief Subscript operator
	 * @param i Index
	 * @return Reference to the system
		 */
		System& operator[](size_t i) { return basis_states[i]; }
		const System& operator[](size_t i) const { return basis_states[i]; }

		/**
	 * @brief Get the size
	 * @return Number of basis states
		 */
		size_t size() const { return basis_states.size(); }

		/**
	 * @brief Check whether empty
	 * @return Whether empty
		 */
		bool empty() const { return basis_states.empty(); }

		/**
	 * @brief Format the state as a string
	 * @param display Display mode (see StatePrintDisplay)
	 * @param precision Precision (number of decimal places)
	 * @return Formatted state string
		 */
		std::string to_string(int32_t display = 0, int precision = 0) const;
	};

	// Forward declaration of StatePrint (defined in debugger.h)
	struct StatePrint;

	/** @brief Device type enum */
	enum DeviceType { CPU, GPU, ANY };

	/** @brief Forward declaration: CUDA sparse state */
	struct CuSparseState;

	/**
	 * @brief Operator base class
	 * @details Abstract base class of all quantum operators, defining the basic operator interface
	 */
	struct BaseOperator
	{
		/**
		 * @brief Apply the operator (pure virtual function)
		 * @param state System state vector
		 */
		virtual void operator()(std::vector<System>& state) const = 0;

		/**
		 * @brief Apply the conjugate transpose (dagger) operation
		 * @param state System state vector
		 * @throws Throws a not-implemented exception by default
		 */
		inline virtual void dag(std::vector<System>& state) const
		{
			throw_not_implemented("Dagger is not implemented.");
		}

		/**
		 * @brief Apply the operator to a SparseState
		 * @param state Sparse state
		 */
		inline void operator()(SparseState& state) const
		{
			(*this)(state.basis_states);
		}

		/**
		 * @brief Apply dagger to a SparseState
		 * @param state Sparse state
		 */
		inline virtual void dag(SparseState& state) const
		{
			this->dag(state.basis_states);
		}
#ifdef USE_CUDA
		/**
		 * @brief Apply the operator to a CuSparseState (CUDA version)
		 * @param state CUDA sparse state
		 */
		virtual void operator()(CuSparseState& state) const;

		/**
		 * @brief Apply dagger to a CuSparseState (CUDA version)
		 * @param state CUDA sparse state
		 */
		virtual void dag(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Self-adjoint operator class
	 * @details Inherits from BaseOperator; the dagger of a self-adjoint operator equals itself
	 */
	class SelfAdjointOperator : public BaseOperator {
	public:
		using BaseOperator::operator();
		using BaseOperator::dag;

		/**
		 * @brief Apply the dagger operation (the dagger of a self-adjoint operator equals itself)
		 * @param state System state vector
		 */
		inline void dag(std::vector<System>& state) const override {
			(*this)(state);
		}

		/**
		 * @brief Apply dagger to a SparseState
		 * @param state Sparse state
		 */
		inline void dag(SparseState& state) const override {
			(*this)(state);
		}

#ifdef USE_CUDA
		/**
		 * @brief Apply dagger to a CuSparseState
		 * @param state CUDA sparse state
		 */
		void dag(CuSparseState& state) const override;
#endif
	};


#ifdef SINGLE_THREAD
	/** @brief Execution policy: single-threaded */
	constexpr auto exec_policy = std::execution::seq;
#else
	/** @brief Execution policy: parallel */
	constexpr auto exec_policy = std::execution::par;
#endif


}
