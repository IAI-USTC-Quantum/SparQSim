/**
 * @file state_preparation.h
 * @brief QRAM-based arbitrary sparse state preparation
 * @details Prepares the target distribution via layer-by-layer amplitude splitting over a binary
 *          tree: each layer splits 1 rotation qubit off the working register, uses the QRAM to
 *          read the parent/child node amplitudes and compute the ratio angle, applies the
 *          conditional rotation (CondRot_Fixed_Bool) and then unloads the ancillary registers.
 *          Contains the operator version (State_Prep_via_QRAM) and a demo driver (the
 *          state_preparation_demo namespace). The corresponding Python implementation is in
 *          pysparq.algorithms.state_preparation; the C++ experiment entry point is in
 *          Experiments/StatePreparation
 */

#pragma once
#include "state_manipulator.h"
#include "simple_quantum_simulator.h"
#include "sparse_state_simulator.h"
#include "global_macros.h"

namespace qram_simulator {
	/**
	 * @namespace qram_simulator::state_prep
	 * @brief State preparation operators
	 */
	namespace state_prep {
		/**
		 * @brief QRAM-based state preparation operator (composite operator)
		 * @details Prepares the target distribution stored in the QRAM into the working register
		 *          bit by bit: layer k splits off the rotation qubit, builds the parent/child
		 *          addresses (addr_parent/addr_child) and parent/child data
		 *          (data_parent/data_child); after the QRAM load, the rotation angle is computed
		 *          with Div_Sqrt_Arccos_UInt_UInt / GetRotateAngle_Int_Int, the conditional
		 *          rotation writes in the amplitude ratio, and then a dagger sequence unloads all
		 *          ancillary quantities. Supports conditional control (ClassControllable)
		 */
		struct State_Prep_via_QRAM : BaseOperator
		{
			/** @brief Working register name (holds the preparation result; also serves as the address prefix) */
			std::string work_qubit;
			/** @brief Address bit width (= working register bit width, the number of binary-tree layers) */
			size_t addr_size;
			/** @brief Data register bit width (holds amplitude numerators/denominators) */
			size_t data_size;
			/** @brief Rational register bit width (holds intermediate division and arccosine results) */
			size_t rational_size;
			/** @brief QRAM circuit pointer holding the target distribution */
			qram_qutrit::QRAMCircuit* qram;

			ClassControllable

			/**
			 * @brief Constructor
			 * @param qram_ QRAM circuit pointer holding the target distribution
			 * @param work_qubit_ Working register name
			 * @param dsz Data register bit width
			 * @param rsz Rational register bit width
			 */
			State_Prep_via_QRAM(qram_qutrit::QRAMCircuit* qram_,
				std::string_view work_qubit_,
				size_t dsz,
				size_t rsz) : work_qubit(work_qubit_), addr_size(System::size_of(work_qubit)), data_size(dsz), rational_size(rsz), qram(qram_)
			{
				//QRAMLoad::version = qram_version;
			};

			/**
			 * @brief Forward preparation implementation
			 * @details Executes layer by layer (k = 0 … addr_size-1): split the rotation qubit →
			 *          build parent/child addresses and data → QRAM load → compute the rotation
			 *          angle → conditional rotation → unload; the last layer handles the leaf-node
			 *          angle with GetRotateAngle_Int_Int
			 * @tparam Ty State type (std::vector<System> or SparseState)
			 * @param state System state
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				profiler _("State_Prep_via_QRAM");

				AddRegister("addr_parent", UnsignedInteger, addr_size + 1)(state);
				AddRegister("addr_child", UnsignedInteger, addr_size + 1)(state);
				AddRegister("data_parent", SignedInteger, data_size)(state);
				AddRegister("data_child", SignedInteger, data_size)(state);
				AddRegister("div_result", Rational, rational_size)(state);

				for (size_t k = 0; k < addr_size; ++k) {
					auto target = SplitRegister(work_qubit, "rotation", 1)(state);
					std::get<1>(System::name_register_map[System::get("rotation")]) = Boolean;
					Add_ConstUInt_InPlace("addr_parent", pow2(k) - 1)(state);
					Add_UInt_UInt_InPlace(work_qubit, "addr_parent")(state);
					Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
					X_Bool("addr_child", 0)(state);
					if (k != addr_size - 1)
					{
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						{
							profiler _("StatePrep::CondRot");
							CondRot_Fixed_Bool("div_result", "rotation")(state);
						}
						ClearZero()(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
					}
					else
					{
						ShiftLeft_InPlace("addr_parent", 1)(state);
						X_Bool("addr_parent", 0)(state);
						Add_ConstUInt_InPlace("addr_child", 1)(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
						{
							profiler _("StatePrep::CondRot");
							CondRot_Fixed_Bool("div_result", "rotation")(state);
						}
						ClearZero()(state);
						GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Add_ConstUInt_InPlace("addr_child", 1).dag(state);
						X_Bool("addr_parent", 0)(state);
						ShiftRight_InPlace("addr_parent", 1)(state);
					}
					X_Bool("addr_child", 0)(state);
					Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
					Add_UInt_UInt_InPlace(work_qubit, "addr_parent").dag(state);
					Add_ConstUInt_InPlace("addr_parent", pow2(k) - 1).dag(state);
					//StatePrint(0, 10)(state);
					CombineRegister(work_qubit, "rotation")(state);
					ShiftLeft_InPlace(work_qubit, 1)(state);
				}
				ShiftRight_InPlace(work_qubit, 1)(state);
				RemoveRegister("addr_parent")(state);
				RemoveRegister("data_parent")(state);
				RemoveRegister("addr_child")(state);
				RemoveRegister("data_child")(state);
				RemoveRegister("div_result")(state);

				ClearZero()(state);
			}

			/**
			 * @brief Inverse (dagger) preparation implementation
			 * @details Strict reverse order of impl: the layer order is reversed
			 *          (k = addr_size-1 … 0), and within each layer the operations run in the
			 *          opposite order via .dag(), used to restore a prepared state back to the
			 *          initial state
			 * @tparam Ty State type
			 * @param state System state
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				profiler _("State_Prep_via_QRAM::dag");

				AddRegister("addr_parent", UnsignedInteger, addr_size + 1)(state);
				AddRegister("addr_child", UnsignedInteger, addr_size + 1)(state);
				AddRegister("data_parent", SignedInteger, data_size)(state);
				AddRegister("data_child", SignedInteger, data_size)(state);
				AddRegister("div_result", Rational, rational_size)(state);

				ShiftLeft_InPlace(work_qubit, 1)(state);
				for (size_t k = 0; k <= addr_size - 1; ++k) {
					ShiftRight_InPlace(work_qubit, 1)(state);
					auto target = SplitRegister(work_qubit, "rotation", 1)(state);
					std::get<1>(System::name_register_map[System::get("rotation")]) = Boolean;
					Add_ConstUInt_InPlace("addr_parent", pow2(addr_size - 1 - k) - 1)(state);
					Add_UInt_UInt_InPlace(work_qubit, "addr_parent")(state);
					Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
					X_Bool("addr_child", 0)(state);
					if (k != 0)
					{
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						{
							profiler _("StatePrep::CondRot::dag");
							// size_t original_size = state.size();
							// fmt::print("stateprep original_size {}\n", original_size);
							CondRot_Fixed_Bool("div_result", "rotation").dag(state);
						}
						ClearZero()(state);
						Div_Sqrt_Arccos_UInt_UInt("data_child", "data_parent", "div_result")(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
					}
					else
					{
						ShiftLeft_InPlace("addr_parent", 1)(state);
						X_Bool("addr_parent", 0)(state);
						Add_ConstUInt_InPlace("addr_child", 1)(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
						{
							profiler _("StatePrep::CondRot::dag");
							// size_t original_size = state.size();
							// fmt::print("stateprep original_size {}\n", original_size);
							CondRot_Fixed_Bool("div_result", "rotation").dag(state);
						}
						ClearZero()(state);
						GetRotateAngle_Int_Int("data_parent", "data_child", "div_result")(state);
						QRAMLoad(qram, "addr_parent", "data_parent")(state);
						QRAMLoad(qram, "addr_child", "data_child")(state);
						Add_ConstUInt_InPlace("addr_child", 1).dag(state);
						X_Bool("addr_parent", 0)(state);
						ShiftRight_InPlace("addr_parent", 1)(state);
					}
					X_Bool("addr_child", 0)(state);
					Mult_UInt_ConstUInt("addr_parent", 2, "addr_child")(state);
					Add_UInt_UInt_InPlace(work_qubit, "addr_parent").dag(state);
					Add_ConstUInt_InPlace("addr_parent", pow2(addr_size - 1 - k) - 1).dag(state);
					CombineRegister(work_qubit, "rotation")(state);

				}

				RemoveRegister("addr_parent")(state);
				RemoveRegister("data_parent")(state);
				RemoveRegister("addr_child")(state);
				RemoveRegister("data_child")(state);
				RemoveRegister("div_result")(state);

				ClearZero()(state);
			}

			COMPOSITE_OPERATION
		};

	}

	/**
	 * @namespace qram_simulator::state_preparation_demo
	 * @brief State preparation demo driver (for experiments)
	 */
	namespace state_preparation_demo {

		/**
		 * @brief Sparse state demo carrier
		 * @details Registers the parent/child addresses, parent/child data, temporary bit, and
		 *          rational registers at construction, holds the initial |0⟩ state; provides
		 *          clear, sort, print, and run entry points
		 */
		struct SparseStateDemo
		{
			/** @brief Address bit width */
			size_t addr_size;
			/** @brief Data bit width */
			size_t data_size;
			/** @brief Rational bit width */
			size_t rational_size;
			/** @brief Sparse state (vector of basis states) */
			std::vector<System> system_states;
			/** @brief QRAM circuit pointer (filled by set_qram after make_qram) */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief QRAM circuit version string */
			std::string qram_version;

			/**
			 * @brief Constructor: registers all registers and prepares the |0⟩ initial state
			 * @param asz Address bit width
			 * @param dsz Data bit width
			 * @param rsz Rational bit width
			 * @param qram_version_ QRAM circuit version
			 */
			SparseStateDemo(size_t asz, size_t dsz, size_t rsz, std::string qram_version_)
				: addr_size(asz), data_size(dsz), rational_size(rsz), qram_version(qram_version_)
			{
				System::add_register("addr_parent", UnsignedInteger, asz + 1);
				System::add_register("addr_child", UnsignedInteger, asz + 1);
				System::add_register("data_parent", SignedInteger, dsz);
				System::add_register("data_child", SignedInteger, dsz);
				System::add_register("temp_bit", Boolean, 1);
				System::add_register("div_result", Rational, rsz);
				system_states.emplace_back();

				QRAMLoad::version = qram_version;
			}

			/** @brief Clear the sparse state (back to the single-branch |0⟩) */
			void clear_state();

			/** @brief Sort the sparse state by basis-state key */
			void sort_state();

			/** @brief Print the sparse state to a string */
			std::string to_string() const;

			/** @brief Execute the state preparation pipeline */
			void run();
		};

		/**
		 * @brief Full state preparation demo driver
		 * @details Combines the classical side (random distribution generation, binary tree
		 *          construction, QRAM construction) with the quantum side (preparation execution
		 *          on the SparseStateDemo carrier), and supports noise injection and fidelity
		 *          statistics
		 */
		class StatePreparation
		{
		public:
			/** @brief Quantum-side sparse state carrier */
			SparseStateDemo sparse_state;
			/** @brief Number of working-register qubits (= number of binary-tree layers) */
			size_t qubit_number;
			/** @brief Data bit width */
			size_t data_size;
			/** @brief Data value range upper bound */
			size_t data_range;
			/** @brief Target distribution (classical side, indexed by address) */
			std::vector<size_t> dist;
			/** @brief Amplitude binary tree (classical side) */
			std::vector<size_t> tree;
			/** @brief QRAM circuit pointer */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief QRAM circuit version string */
			std::string qram_version;

			/**
			 * @brief Constructor
			 * @param qn Number of working-register qubits
			 * @param data_sz Data bit width
			 * @param data_range_ Data value range upper bound
			 * @param qram_version_ QRAM circuit version
			 */
			StatePreparation(size_t qn, size_t data_sz, size_t data_range_, std::string qram_version_)
				: qubit_number(qn), data_size(data_sz), data_range(data_range_),
				sparse_state(qn, data_sz, std::min(size_t(50), data_sz * 2), qram_version_),
				qram_version(qram_version_)
			{
			}

			/** @brief Generate a random target distribution */
			void random_distribution();

			/** @brief Print the target distribution */
			void show_distribution();

			/** @brief Get the normalized real-valued target distribution */
			std::vector<double> get_real_dist();

			/** @brief Build the amplitude binary tree from the target distribution */
			void make_tree();

			/** @brief Print the amplitude binary tree */
			void show_tree();

			/** @brief Construct the QRAM memory from the binary tree */
			void make_qram();

			/** @brief Inject the constructed QRAM circuit into the carrier */
			void set_qram();

			/**
			 * @brief Set the QRAM noise model
			 * @param noise Noise parameters (error rates per operation type)
			 */
			void set_noise(const noise_t& noise);

			/** @brief Compute the fidelity between the prepared state and the target distribution */
			double get_fidelity() const;

			/** @brief Compute and print the fidelity */
			double get_fidelity_show() const;

			/** @brief Print the current sparse state (first 10 lines, with details) */
			inline void print_state() {
				StatePrint(0 | Detail)(sparse_state.system_states);
			}

			/** @brief Execute the full preparation pipeline */
			void run();

			/** @brief Clear the carrier's sparse state */
			inline void clear_state() { sparse_state.clear_state(); }
		};

	} // namespace state_preparation_demo
} // namespace qram_simulator
