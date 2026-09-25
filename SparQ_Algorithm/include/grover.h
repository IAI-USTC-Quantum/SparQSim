/**
 * @file grover.h
 * @brief Grover quantum search algorithm (two interface sets: sparse state / dense state)
 * @details Provides register-level-programming-based Grover components: QRAM phase oracle
 *          (GroverOracle), HPH diffusion operator, full iteration (GroverOperator), repeated
 *          amplitude amplification (GroverAmplify), and quantum counting (GroverCount), which
 *          search for a marked item among N entries with O(√N) complexity. Also contains the
 *          legacy dense-state interface under the grover_dense namespace (operating directly on
 *          the std::vector<complex_t> state vector), as distinct from the new sparse-state
 *          interface in the grover namespace
 */

#pragma once

#include "state_manipulator.h"
#include "simple_quantum_simulator.h"
#include "sparse_state_simulator.h"
#include "hamiltonian_simulation.h"

namespace qram_simulator {
	/**
	 * @namespace qram_simulator::grover_dense
	 * @brief Legacy dense-state interface of Grover's algorithm
	 * @details Operates directly on the full std::vector<complex_t> state vector, together with
	 *          QRAM circuits and noise models, for interfacing with the dense-state simulator of
	 *          the QRAM-Simulator base; new code is advised to use the sparse-state interface
	 *          in the grover namespace
	 */
	namespace grover_dense {
		/**
		 * @brief Apply the QRAM oracle to a dense state
		 * @details Applies the QRAM with address bits [0, n) and data bit n, the remaining bits
		 *          being otherqubit; after execution, verifies state normalization, printing the
		 *          state and throwing an exception on failure
		 * @tparam QRAM QRAM circuit type (qutrit/qubit implementation)
		 * @param state Dense state vector (input and output)
		 * @param n Address bit width
		 * @param qram QRAM circuit pointer
		 * @param version QRAM circuit version string
		 */
		template<typename QRAM>
		void oracle(std::vector<complex_t>& state, size_t n, QRAM* qram, std::string version)
		{
			profiler _("Grover_shots: oracle");

			std::vector<size_t> addrqubit(n);
			iota(addrqubit.begin(), addrqubit.end(), 0);
			std::vector<size_t> dataqubit(1, n);
			std::vector<size_t> otherqubit;

			state = qram->apply(state, addrqubit, dataqubit, otherqubit, version);
			if (std::abs(amp_sum(state) - 1.0) > epsilon)
			{
				double ampsum = amp_sum(state);
				quantum_simulator::print_state(state, false);
				fmt::print("amp_sum={}\n", ampsum);
				fmt::print("{}\n", (*qram)->to_string_full_info());

				throw_bad_result();
			}
		}

		/**
		 * @brief Grover diffusion operator (reflection about the mean)
		 * @param state Dense state vector (input and output)
		 * @param n Address bit width
		 */
		void diffusion(std::vector<complex_t>& state, size_t n);

		/**
		 * @brief Perform a full Grover iteration (oracle + diffusion)
		 * @param state Dense state vector (input and output)
		 * @param n Address bit width
		 * @param pos Position of the marked item in memory
		 * @param repeat Number of iterations
		 * @param oracle Oracle callback (takes a reference to the dense state)
		 */
		void grover(std::vector<complex_t>& state, size_t n, size_t pos, size_t repeat,
			std::function<void(decltype(state))> oracle);

		/**
		 * @brief Run Grover search with multiple samples and tally the measurement results
		 * @details Each sample re-prepares the initial state, performs the specified number of
		 *          Grover iterations, then measures, keeping only the lowest n bits (address bits)
		 *          of the measurement result
		 * @tparam QRAM QRAM circuit type
		 * @param n Address bit width
		 * @param pos Position of the marked item in memory
		 * @param shots Number of samples
		 * @param repeat Number of Grover iterations per sample
		 * @param noise Noise model parameters for each operation type
		 * @param version QRAM circuit version string
		 * @return Vector of length 2^n whose i-th entry is the number of times address i was measured
		 */
		template<typename QRAM>
		std::vector<size_t> grover_shots(size_t n, size_t pos, size_t shots, size_t repeat,
			const std::map<OperationType, double>& noise, std::string version)
		{
			std::vector<size_t> measurements(pow2(n), 0);
			std::vector<complex_t> state;
			memory_t memory(pow2(n), 0);
			memory[pos] = 1;
			QRAM qram(n, 1, memory);
			qram->set_noise_models(noise);

			for (size_t i = 0; i < shots; ++i) {
				profiler _("Grover_shots: Mainloop");
				quantum_simulator::init_n_state(state, n + 1);
				auto oracle_ = std::bind(oracle<QRAM>, std::placeholders::_1, n, &qram, version);
				grover(state, n, pos, repeat, oracle_);
				size_t m = quantum_simulator::measure(state);
				if (m == state.size())
					throw_bad_result();
				// only extract the lowest n
				m -= ((m >> n) << n);
				++measurements[m];
			}
			return measurements;
		}
	}

	/**
	 * @namespace qram_simulator::grover
	 * @brief Sparse-state interface of Grover's algorithm (register-level programming)
	 */
	namespace grover
	{
		/**
		 * @brief QRAM-based Grover phase oracle
		 * @details Loads the memory data into the data register via QRAM, applies a phase flip
		 *          to the branches equal to the search target, then unloads (uncomputes) the data
		 *          register, realizing phase-kickback-style marking: |x⟩|0⟩ → (-1)^{f(x)} |x⟩|0⟩.
		 *          Supports conditional control (ClassControllable)
		 */
		struct GroverOracle
		{
			/** @brief QRAM address register ID */
			size_t qram_address_id;
			/** @brief QRAM data register ID */
			size_t qram_data_id;
			/** @brief Search-target register ID (holds the value to match) */
			size_t search_data_id;
			/** @brief QRAM circuit (qutrit/qubit implementation) pointer */
			qram_qutrit::QRAMCircuit* qram;

			ClassControllable

			/**
			 * @brief Constructor (register-ID version)
			 * @param qram_ QRAM circuit pointer
			 * @param qram_address_id_ Address register ID
			 * @param qram_data_id_ Data register ID
			 * @param search_data_id_ Search-target register ID
			 */
			GroverOracle(qram_qutrit::QRAMCircuit* qram_, size_t qram_address_id_,
				size_t qram_data_id_, size_t search_data_id_)
				:
				qram(qram_),
				qram_address_id(qram_address_id_),
				qram_data_id(qram_data_id_),
				search_data_id(search_data_id_)
			{
			}

			/**
			 * @brief Constructor (register-name version)
			 * @param qram_ QRAM circuit pointer
			 * @param qram_address_ Address register name
			 * @param qram_data_ Data register name
			 * @param search_data_ Search-target register name
			 */
			GroverOracle(qram_qutrit::QRAMCircuit* qram_, std::string_view qram_address_,
				std::string_view qram_data_, std::string_view search_data_)
				:
				qram(qram_),
				qram_address_id(System::get(qram_address_)),
				qram_data_id(System::get(qram_data_)),
				search_data_id(System::get(search_data_))
			{
			}

			/**
			 * @brief Apply the oracle operation
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;

		};

		/**
		 * @brief H-P-H diffusion operator
		 * @details The H⊗n · phase flip · H⊗n form of the Grover diffusion operator: applies
		 *          Hadamard to the address register, then flips the phase of the |0...0⟩ branch,
		 *          then applies Hadamard again, realizing reflection about the uniform
		 *          superposition state. Supports conditional control (ClassControllable)
		 */
		struct HPH
		{
			/** @brief Address register ID */
			size_t qram_address_id;
			/** @brief Register bit width (cached from the global register table at construction) */
			size_t size;

			ClassControllable

			/**
			 * @brief Constructor (register-ID version)
			 * @param qram_address_id_ Address register ID
			 */
			HPH(size_t qram_address_id_)
				: qram_address_id(qram_address_id_),
				size(System::size_of(qram_address_id_))
			{ }

			/**
			 * @brief Constructor (register-name version)
			 * @param qram_address_name Address register name
			 * @param size_ Register bit width
			 */
			HPH(std::string qram_address_name, size_t size_)
				: qram_address_id(System::get(qram_address_name)), size(size_)
			{ }

			/**
			 * @brief Apply the diffusion operation
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;

		};

		/**
		 * @brief Single full Grover iteration operator
		 * @details Combines GroverOracle (phase marking) with HPH (diffusion reflection) to form
		 *          one standard Grover iteration G = HPH · Oracle.
		 *          Supports conditional control (ClassControllable)
		 */
		struct GroverOperator
		{
			/** @brief QRAM address register ID */
			size_t qram_address_id;
			/** @brief QRAM data register ID */
			size_t qram_data_id;
			/** @brief Search-target register ID */
			size_t search_data_id;
			/** @brief QRAM circuit pointer */
			qram_qutrit::QRAMCircuit* qram;

			ClassControllable

			/**
			 * @brief Constructor
			 * @param qram_ QRAM circuit pointer
			 * @param qram_address_id_ Address register ID
			 * @param qram_data_id_ Data register ID
			 * @param search_data_id_ Search-target register ID
			 */
			GroverOperator(qram_qutrit::QRAMCircuit* qram_, size_t qram_address_id_,
				size_t qram_data_id_, size_t search_data_id_)
				: qram(qram_), qram_address_id(qram_address_id_),
				qram_data_id(qram_data_id_),
				search_data_id(search_data_id_)
			{
			}

			/**
			 * @brief Apply a single Grover iteration
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief Multi-round amplitude amplification operator
		 * @details Executes the GroverOperator iteration n_repeats times in a row, amplifying
		 *          the measurement probability of the marked item to close to 1
		 */
		struct GroverAmplify
		{
			/** @brief Number of iterations */
			size_t n_repeats;

			/** @brief QRAM address register ID */
			size_t qram_address_id;
			/** @brief QRAM data register ID */
			size_t qram_data_id;
			/** @brief Search-target register ID */
			size_t search_data_id;
			/** @brief Bit width of the search-target value */
			size_t data_size;
			/** @brief QRAM circuit pointer */
			qram_qutrit::QRAMCircuit* qram;

			/**
			 * @brief Constructor
			 * @param qram_ QRAM circuit pointer
			 * @param qram_address_id_ Address register ID
			 * @param search_data_id_ Search-target register ID
			 * @param data_size_ Search-target value bit width
			 * @param n_repeats_ Number of iterations
			 */
			GroverAmplify(qram_qutrit::QRAMCircuit* qram_, size_t qram_address_id_, size_t search_data_id_,
				size_t data_size_, size_t n_repeats_)
				: qram(qram_), qram_address_id(qram_address_id_), n_repeats(n_repeats_),
				search_data_id(search_data_id_), data_size(data_size_)
			{}

			/**
			 * @brief Apply multi-round amplitude amplification
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state);

		};

		/**
		 * @brief Quantum counting operator
		 * @details Circuit structure: Hadamard prepares the counting register in superposition →
		 *          powers of the Grover iteration controlled by the counting register (c-U^{2^k})
		 *          → inverse QFT on the counting register, thereby estimating the number of marked
		 *          items M (phase-estimation view: sin²θ = M/N)
		 */
		struct GroverCount
		{
			/*
			-- H -- (c-U) - iQFT
			*/
			/** @brief Counting register ID */
			size_t count_reg;
			/** @brief QRAM address register ID */
			size_t addr_reg;
			/** @brief QRAM data register ID */
			size_t data_reg;
			/** @brief Search-target register ID */
			size_t search_data_reg;
			/** @brief QRAM circuit pointer */
			qram_qutrit::QRAMCircuit* qram;

			/**
			 * @brief Constructor
			 * @param qram_ QRAM circuit pointer
			 * @param count_reg_ Counting register ID
			 * @param addr_reg_ Address register ID
			 * @param data_reg_ Data register ID
			 * @param search_data_reg_ Search-target register ID
			 */
			GroverCount(qram_qutrit::QRAMCircuit* qram_, size_t count_reg_, size_t addr_reg_,
				size_t data_reg_, size_t search_data_reg_)
				: qram(qram_), count_reg(count_reg_), addr_reg(addr_reg_),
				data_reg(data_reg_), search_data_reg(search_data_reg_)
			{}

			/**
			 * @brief Perform quantum counting
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state);
		};
	}

} // namespace qram_simulator
