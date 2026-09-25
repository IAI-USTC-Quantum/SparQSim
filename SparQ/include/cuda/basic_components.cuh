/**
 * @file basic_components.cuh
 * @brief GPU-side basic components: register value access and sparse state container
 * @details Provides __host__ __device__ helper functions directly usable by CUDA kernels:
 *          type conversion of register stored values (unsigned/signed/floating-point/boolean),
 *          register and amplitude access on System, and the GPU sparse state container
 *          CuSparseState (CPU/GPU dual residency, migrated on demand) plus a set of thrust
 *          functors (modulus squared, normalization, basis-state comparison/equality,
 *          zero-amplitude predicate).
 *          Keeps the same memory layout as the CPU-side data structures in SparQ/include/basic_components.h
 */

#pragma once

#include "basic_components.h"
#include "basic.h"

namespace qram_simulator {

	/**
	 * @brief Get the index-th register storage cell of a System (writable reference)
	 * @param system Basis state
	 * @param index Register index
	 * @return Reference to the corresponding StateStorage
	 */
	inline __host__ __device__ StateStorage& CuGet(System& system, size_t index)
	{
		return (reinterpret_cast<StateStorage*>(&system.registers))[index];
	}

	/**
	 * @brief Get the index-th register storage cell of a System (read-only)
	 * @param system Basis state
	 * @param index Register index
	 * @return Copy of the corresponding StateStorage
	 */
	inline __host__ __device__ StateStorage CuGet(const System& system, size_t index)
	{
		return (reinterpret_cast<const StateStorage*>(&system.registers))[index];
	}

	/**
	 * @brief Get the index-th register value of a System and interpret it as an unsigned integer
	 * @details Reads the stored value and truncates it to the register width (width_mask)
	 * @param system Basis state
	 * @param index Register index
	 * @param size Register bit width
	 * @return Unsigned value
	 */
	inline __host__ __device__ uint64_t CuGetAsUint64(const System& system, size_t index, size_t size)
	{
		return CuGet(system, index).value & width_mask(size);
	}

	/**
	 * @brief Get the index-th register value of a System and interpret it as a boolean
	 * @param system Basis state
	 * @param index Register index
	 * @param size Register bit width
	 * @return Boolean value
	 */
	inline __host__ __device__ bool CuGetAsBool(const System& system, size_t index, size_t size)
	{
		return bool(CuGet(system, index).value & width_mask(size));
	}

	/**
	 * @brief Get the index-th register value of a System and sign-extend it to a signed integer
	 * @param system Basis state
	 * @param index Register index
	 * @param size Register bit width (returns 0 when the width is 0)
	 * @return Signed value
	 */
	inline __host__ __device__ int64_t CuGetAsInt64(const System& system, size_t index, size_t size)
	{
		uint64_t value = CuGet(system, index).value & width_mask(size);
		return size ? (int64_t)(value << (64 - size)) >> (64 - size) : 0;
	}

	/**
	 * @brief Get a raw double pointer to a System's amplitude (writable, [0]=real part [1]=imaginary part)
	 * @param system Basis state
	 * @return Double pointer to the real/imaginary parts of the amplitude
	 */
	inline __host__ __device__ double* CuSystemAmplitude(System& system)
	{
		return reinterpret_cast<double*>(&system.amplitude);
	}

	/**
	 * @brief Get a raw const double pointer to a System's amplitude (read-only)
	 * @param system Basis state
	 * @return Const double pointer to the real/imaginary parts of the amplitude
	 */
	inline __host__ __device__ const double* CuSystemAmplitude(const System& system)
	{
		return reinterpret_cast<const double*>(&system.amplitude);
	}

	/**
	 * @brief Get the real part of a complex number (writable reference)
	 * @param c Complex number
	 * @return Reference to the real part
	 */
	inline __host__ __device__ double& cu_real(complex_t& c)
	{
		return reinterpret_cast<double*>(&c)[0];
	}

	/**
	 * @brief Get the real part of a complex number (read-only)
	 * @param c Complex number
	 * @return Real part value
	 */
	inline __host__ __device__ double cu_real(const complex_t& c)
	{
		return reinterpret_cast<const double*>(&c)[0];
	}

	/**
	 * @brief Get the imaginary part of a complex number (writable reference)
	 * @param c Complex number
	 * @return Reference to the imaginary part
	 */
	inline __host__ __device__ double& cu_imag(complex_t& c)
	{
		return reinterpret_cast<double*>(&c)[1];
	}

	/**
	 * @brief Get the imaginary part of a complex number (read-only)
	 * @param c Complex number
	 * @return Imaginary part value
	 */
	inline __host__ __device__ double cu_imag(const complex_t& c)
	{
		return reinterpret_cast<const double*>(&c)[1];
	}

	/**
	 * @brief Compute the modulus squared |amp|² of a basis-state amplitude
	 * @param s Basis state
	 * @return Modulus squared
	 */
	inline __host__ __device__ double CuAbsSqr(const System& s)
	{
		const double* amplitude = CuSystemAmplitude(s);
		return amplitude[0] * amplitude[0] + amplitude[1] * amplitude[1];
	}


	/**
	 * @brief GPU sparse state container (CPU/GPU dual residency)
	 * @details Uses _on_gpu to mark the current residency side: CPU side is std::vector<System>,
	 *          GPU side is thrust::device_vector<System>; provides move/copy migration and
	 *          iterator access (iterator access forcibly pulls data back to the CPU first).
	 *          Copy/move construction and assignment selectively copy according to the source
	 *          side, avoiding unnecessary device transfers.
	 *          Corresponds to the CPU-side SparseState (an alias of std::vector<System>)
	 */
	struct CuSparseState
	{
		/** @brief CPU-side basis-state vector type */
		using vector_type = std::vector<System>;

		/** @brief CPU-side sparse state data */
		std::vector<System> sparse_state_cpu;
		/** @brief GPU-side sparse state data */
		thrust::device_vector<System> sparse_state_gpu;

		/** @brief Whether currently resident on the GPU */
		bool _on_gpu = false;

		CuSparseState();
		CuSparseState(size_t size);

		/**
		 * @brief Copy constructor (copies according to the source residency side)
		 * @param other Source container
		 */
		CuSparseState(const CuSparseState& other)
		{
			if (other._on_gpu)
			{
				sparse_state_gpu = other.sparse_state_gpu;
				_on_gpu = true;
			}
			else
			{
				sparse_state_cpu = other.sparse_state_cpu;
				_on_gpu = false;
			}
		}

		/**
		 * @brief Move constructor (moves according to the source residency side)
		 * @param other Source container
		 */
		CuSparseState(CuSparseState&& other)
		{
			if (other._on_gpu)
			{
				sparse_state_gpu = std::move(other.sparse_state_gpu);
				_on_gpu = true;
			}
			else
			{
				sparse_state_cpu = std::move(other.sparse_state_cpu);
				_on_gpu = false;
			}
		}

		/**
		 * @brief Copy assignment (copies according to the source residency side)
		 * @param other Source container
		 * @return Reference to self
		 */
		CuSparseState& operator=(const CuSparseState& other)
		{
			if (other._on_gpu)
			{
				sparse_state_gpu = other.sparse_state_gpu;
				_on_gpu = true;
			}
			else
			{
				sparse_state_cpu = other.sparse_state_cpu;
				_on_gpu = false;
			}
			return *this;
		}

		/**
		 * @brief Move assignment (moves according to the source residency side)
		 * @param other Source container
		 * @return Reference to self
		 */
		CuSparseState& operator=(CuSparseState&& other)
		{
			if (other._on_gpu)
			{
				sparse_state_gpu = std::move(other.sparse_state_gpu);
				_on_gpu = true;
			}
			else
			{
				sparse_state_cpu = std::move(other.sparse_state_cpu);
				_on_gpu = false;
			}
			return *this;
		}

		/**
		 * @brief Construct from a CPU-side SparseState
		 * @param other CPU-side sparse state
		 */
		CuSparseState(const SparseState& other)
		{
			sparse_state_cpu = other.basis_states;
			_on_gpu = false;
		}

		/**
		 * @brief Construct from a basis-state vector (CPU residency)
		 * @param other Basis-state vector
		 */
		CuSparseState(const std::vector<System>& other)
		{
			sparse_state_cpu = other;
			_on_gpu = false;
		}

		/**
		 * @brief Construct from a device vector (GPU residency)
		 * @param other GPU-side basis-state vector
		 */
		CuSparseState(const thrust::device_vector<System>& other)
		{
			sparse_state_gpu = other;
			_on_gpu = true;
		}

		/**
		 * @brief Construct from a device-vector iterator range (GPU residency)
		 * @param begin Starting iterator
		 * @param end Ending iterator
		 */
		CuSparseState(thrust::device_vector<System>::iterator begin, thrust::device_vector<System>::iterator end)
		{
			sparse_state_gpu = thrust::device_vector<System>(begin, end);
			_on_gpu = true;
		}

		/** @brief Move the data back to the CPU (releases GPU device memory) */
		void move_to_cpu();

		/** @brief Copy the data back to the CPU (keeps the GPU copy) */
		void copy_to_cpu();

		/** @brief Move the data onto the GPU (releases CPU memory) */
		void move_to_gpu();

		/** @brief Get a copy of the CPU-side data (triggers a copy) */
		std::vector<System> get_cpu_copy() const;

		/** @brief Whether resident on the GPU */
		bool on_gpu() const;

		/** @brief Whether resident on the CPU */
		bool on_cpu() const;

		/** @brief Whether empty */
		bool empty() const;

		/** @brief Number of basis states (counted on the current residency side) */
		size_t size() const;

		/** @brief Access the last basis state (pulls back to the CPU first) */
		System& back() { copy_to_cpu(); return sparse_state_cpu.back(); }
		/** @brief Forward start iterator (pulls back to the CPU first) */
		vector_type::iterator begin() { copy_to_cpu(); return sparse_state_cpu.begin(); }
		/** @brief Forward end iterator (pulls back to the CPU first) */
		vector_type::iterator end() { copy_to_cpu(); return sparse_state_cpu.end(); }
		/** @brief Reverse start iterator (pulls back to the CPU first) */
		vector_type::reverse_iterator rbegin() { copy_to_cpu(); return sparse_state_cpu.rbegin(); }
		/** @brief Reverse end iterator (pulls back to the CPU first) */
		vector_type::reverse_iterator rend() { copy_to_cpu(); return sparse_state_cpu.rend(); }
	};


	/**
 * @brief Thrust functor taking the modulus squared of a basis-state amplitude
 */
struct AbsSqrFunctor {
	/**
	 * @brief Compute the modulus squared of a basis-state amplitude
	 * @param s Basis state
	 * @return |amp|²
		 */
		__host__ __device__ double operator()(const System& s) const {
			return CuAbsSqr(s);
		}
	};

	/**
 * @brief Thrust functor that scales amplitudes by a constant factor (for normalization)
 */
struct Normalize_Functor {
	/** @brief Scale factor */
	double factor;

	/**
	 * @brief Constructor
	 * @param factor_ Scale factor
	 */
		Normalize_Functor(double factor_) : factor(factor_) {}

		/**
		 * @brief Multiply both the real and imaginary parts by the factor
		 * @param s Basis state
		 */
		__host__ __device__ void operator()(System& s) const {
			double* amplitude = CuSystemAmplitude(s);
			amplitude[0] *= factor;
			amplitude[1] *= factor;
		}
	};

	/**
 * @brief Thrust functor for lexicographic less-than comparison of basis states by key
 *        (the sequence of active register values)
 * @details Caches the global register table size and the activation status bitmap at
 *          construction time; only active registers count in the comparison (status_bitmap mask)
 */
struct SystemLess_Functor
{
	/** @brief Global register table size cached at construction time */
	size_t name_reg_map_size;
	/** @brief Register activation status bitmap cached at construction time */
	size_t status_bitmap = 0;

		SystemLess_Functor()
			: name_reg_map_size(System::name_register_map.size()),
			status_bitmap(System::reg_status_bitmap)
		{
			//for (size_t i = 0; i < System::name_register_map.size(); ++i)
			//{
			//	if (System::status_of(i))
			//		status_bitmap |= pow2(i);
			//}
			
		}

		/**
		 * @brief Lexicographic less-than comparison (active registers only)
		 * @param a Left basis state
		 * @param b Right basis state
		 * @return a < b
		 */
		__host__ __device__ bool operator()(const System& a, const System& b) {
			for (size_t i = 0; i < name_reg_map_size; ++i)
			{
				auto regl = CuGet(a, i).value * ((status_bitmap >> i) & 1);
				auto regr = CuGet(b, i).value * ((status_bitmap >> i) & 1);
				if (regl < regr)
					return true;
				else if (regl > regr)
					return false;
			}
			return false;
		}
	};


	/**
 * @brief Thrust functor for equality comparison of basis states by key
 *        (the sequence of active register values)
 * @details Caches the global register table size and the activation status bitmap at
 *          construction time; accepts either two basis states or a
 *          thrust::tuple<System, System> (zip iterator) input
 */
struct SystemEqual_Functor
{
	/** @brief Global register table size cached at construction time */
	size_t name_reg_map_size;
	/** @brief Register activation status bitmap cached at construction time */
	size_t status_bitmap = 0;

		SystemEqual_Functor()
			: name_reg_map_size(System::name_register_map.size()),
			status_bitmap(System::reg_status_bitmap)
		{
			//for (size_t i = 0; i < System::name_register_map.size(); ++i)
			//{
			//	if (System::status_of(i))
			//		status_bitmap |= pow2(i);
			//}
		}

		/**
		 * @brief Equality comparison (active registers only)
		 * @param a Left basis state
		 * @param b Right basis state
		 * @return All active register values are equal
		 */
		__host__ __device__ bool operator()(const System& a, const System& b) {
			for (size_t i = 0; i < name_reg_map_size; ++i)
			{
				auto regl = CuGet(a, i).value * ((status_bitmap >> i) & 1);
				auto regr = CuGet(b, i).value * ((status_bitmap >> i) & 1);
				if (regl != regr)
					return false;
			}
			return true;
		}

		/**
		 * @brief Equality comparison (basis-state pair version for zip iterators)
		 * @param system_pair Basis-state pair
		 * @return All active register values are equal
		 */
		__host__ __device__ bool operator()(const thrust::tuple<System, System>& system_pair) {
			for (size_t i = 0; i < name_reg_map_size; ++i)
			{
				const System& a = thrust::get<0>(system_pair);
				const System& b = thrust::get<1>(system_pair);
				auto regl = CuGet(a, i).value * ((status_bitmap >> i) & 1);
				auto regr = CuGet(b, i).value * ((status_bitmap >> i) & 1);
				if (regl != regr)
					return false;
			}
			return true;
		}
	};

	/**
 * @brief Thrust functor testing whether the modulus squared of a basis-state amplitude is
 *        below a threshold (for pruning zero-amplitude branches)
 */
struct AmplitudeZero_Functor
{
	/** @brief Threshold ε */
	double eps;

	/**
	 * @brief Constructor
	 * @param eps_ Threshold ε
	 */
		AmplitudeZero_Functor(double eps_) : eps(eps_) {}

		/**
		 * @brief Test whether |amp|² < ε
		 * @param s Basis state
		 * @return Whether it counts as a zero amplitude
		 */
		__host__ __device__ bool operator()(const System& s) const {
			const double* amplitude = CuSystemAmplitude(s);
			return (amplitude[0] * amplitude[0] + amplitude[1] * amplitude[1]) < eps;
		}
	};

	}
