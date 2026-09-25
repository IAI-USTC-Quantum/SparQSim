/**
 * @file measurement.h
 * @brief Seedable measurement / reset / probability query interfaces for sparse states
 * @details Provides first-class, reproducible (seedable) sparse-state operations for dynamic
 *          executors (mid-circuit MEASURE / RESET / QIF, etc.):
 *            - `MeasureZ`: projective Z-basis measurement, samples according to the Born rule,
 *              collapses and renormalizes;
 *            - `Reset`: post-measurement classically conditioned flip, forcibly resets a register
 *              to a given classical value;
 *            - `Probability`: read-only probability query (does not modify the state), used for
 *              condition evaluation in dynamic control flow such as QIF/QWHILE, as well as for
 *              conformance/compliance tests.
 *
 *          Randomness comes from the `qram_simulator::random_engine` singleton and can be
 *          explicitly seeded via `random_engine::set_seed()`, making the sampling results of
 *          `MeasureZ`/`Reset` reproducible (essential for deterministic replay and unit tests of
 *          the dynamic executor).
 *
 * @details Input validation contract (jointly guaranteed by the constructors and `operator()`):
 *          - Every register name/ID must resolve to a register that is currently **active** in
 *            `System`; unknown names, out-of-range IDs, or IDs already removed by
 *            `RemoveRegister` all throw `invalid_argument` (`ValueError` on the Python side) at
 *            construction time.
 *          - The register list within a single constructor call must not contain duplicate IDs
 *            (e.g. `MeasureZ({"a", "a"})`); otherwise `invalid_argument` is thrown.
 *          - The target values of `Reset` and the comparison values of `Probability` must be
 *            representable by the bit width of the corresponding register (i.e. less than
 *            `2^size_of(id)`; unrestricted when `size_of(id) == 64`), otherwise
 *            `invalid_argument` is thrown -- such contradictory targets/values were silently
 *            truncated in the old implementation and are part of what this hardening fix covers.
 *          - `MeasureZ` (and hence `Reset`) explicitly validates, before sampling, that the total
 *            probability of the input state (the sum of `abs(amplitude)^2` over all branches) is
 *            finite and deviates from 1 by less than `kNormalizationThreshold`; otherwise it
 *            throws `runtime_error` (`RuntimeError` on the Python side), instead of directly
 *            comparing the `random_engine::uniform01()` sample against `[0, 1)` as the old
 *            implementation did -- that would silently pile the surplus/deficient probability
 *            mass onto the last branch (the fallback branch) when the total probability deviates
 *            significantly from 1, producing biased sampling without reporting any error.
 *
 * @warning Arbitrary C++ operators produced by `pysparq.dynamic_operator.compile_operator()`
 *          undergo no unitarity proof; it merely runtime-compiles an `operator()`/`dag()` pair,
 *          and the compiler/binding layer will not (and cannot) statically or dynamically verify
 *          that the operator is actually unitary. The QCFD support path (QECC.Lang-driven
 *          qfvm/qnls/qham) forbids using `compile_operator`; all semantics must be expressed
 *          through named, testable built-in operators such as the ones in this file, and verified
 *          via the conformance test matrix provided by `pysparq.conformance`.
 */

#pragma once
#include "basic_components.h"

namespace qram_simulator
{
	/**
	 * @brief Normalization check threshold
	 * @details Before sampling, `MeasureZ` requires `|sum(|amplitude|^2) - 1| < kNormalizationThreshold`,
	 *          consistent with the default threshold (`1e-5`) of `CheckNormalization`.
	 */
	constexpr double kNormalizationThreshold = 1e-5;

	/**
	 * @brief Projective Z-basis measurement
	 * @details Performs a computational-basis (Z-basis) measurement on one or more registers:
	 *          1. Validates that the total probability of the input state is finite and
	 *             approximately 1 (see the file-level documentation);
	 *          2. Samples one outcome among the basis-state branches according to the Born rule
	 *             using `random_engine::uniform01()` (seedable via `set_seed`);
	 *          3. Removes branches inconsistent with the sampled outcome and renormalizes the
	 *             remaining amplitudes;
	 *          4. Returns the sampled register values together with the probability of that outcome.
	 *
	 *          This operation is non-unitary and irreversible (measurement collapse), so no `dag()`
	 *          is provided.
	 */
	struct MeasureZ
	{
		/** @brief List of register IDs to measure */
		std::vector<size_t> registers;

		/**
		 * @brief Constructor (register name list version)
		 * @throws invalid_argument Name not found, or the list contains duplicate registers
		 */
		MeasureZ(const std::vector<std::string>& register_names);

		/**
		 * @brief Constructor (register ID list version)
		 * @throws invalid_argument ID out of range/inactive, or the list contains duplicate registers
		 */
		MeasureZ(const std::vector<size_t>& register_ids);

		/** @brief Constructor (single register name version) */
		MeasureZ(std::string_view register_name);

		/** @brief Constructor (single register ID version) */
		MeasureZ(size_t register_id);

		/**
		 * @brief Perform the measurement
		 * @param state System state vector (collapsed and renormalized in place)
		 * @return {list of sampled register values, probability of that outcome}
		 * @throws invalid_argument When the state is empty
		 * @throws runtime_error When the total probability is non-finite or clearly deviates from 1
		 *                      (see file-level documentation)
		 */
		std::pair<std::vector<uint64_t>, double> operator()(std::vector<System>& state) const;

		/** @brief SparseState version */
		std::pair<std::vector<uint64_t>, double> operator()(SparseState& state) const
		{
			return (*this)(state.basis_states);
		}
	};

	/**
	 * @brief Seedable RESET (measurement + classically conditioned flip)
	 * @details Physically, resetting a register that may be in a superposition can only be done by
	 *          "conditionally flipping according to the classical result after measurement"
	 *          (matching active reset on real hardware and the semantics of the OriginIR-ext
	 *          `RESET` instruction):
	 *            1. Perform one projective measurement (collapse + renormalize) on the target
	 *               register with `MeasureZ`;
	 *            2. Since after the collapse the register's value in all remaining branches equals
	 *               the measurement outcome, directly overwriting it with the target value is
	 *               equivalent to a classical bit flip on a definite value; it cannot merge
	 *               illegally with other branches and is therefore well-defined.
	 *
	 *          The default target value is 0 (corresponding to `RESET` to |0>).
	 */
	struct Reset
	{
		/** @brief List of register IDs to reset */
		std::vector<size_t> registers;

		/** @brief List of reset target values (one-to-one with registers) */
		std::vector<uint64_t> target_values;

		/**
		 * @brief Constructor (name list, all reset to 0 by default)
		 * @throws invalid_argument Name not found, or the list contains duplicate registers
		 */
		explicit Reset(const std::vector<std::string>& register_names);

		/**
		 * @brief Constructor (name list + target value list)
		 * @throws invalid_argument Name not found, duplicate registers, or a target value outside
		 *         the range representable by the corresponding register's bit width
		 */
		Reset(const std::vector<std::string>& register_names, const std::vector<uint64_t>& targets);

		/**
		 * @brief Constructor (ID list, all reset to 0 by default)
		 * @throws invalid_argument ID out of range/inactive, or the list contains duplicate registers
		 */
		explicit Reset(const std::vector<size_t>& register_ids);

		/**
		 * @brief Constructor (ID list + target value list)
		 * @throws invalid_argument ID out of range/inactive, duplicate registers, or a target value
		 *         outside the range representable by the corresponding register's bit width
		 */
		Reset(const std::vector<size_t>& register_ids, const std::vector<uint64_t>& targets);

		/**
		 * @brief Constructor (single register name + target value, default 0)
		 * @throws invalid_argument Name not found, or the target value exceeds the register's bit width
		 */
		explicit Reset(std::string_view register_name, uint64_t target = 0);

		/**
		 * @brief Constructor (single register ID + target value, default 0)
		 * @throws invalid_argument ID out of range/inactive, or the target value exceeds the register's bit width
		 */
		explicit Reset(size_t register_id, uint64_t target = 0);

		/**
		 * @brief Perform the reset
		 * @param state System state vector (collapsed, renormalized, and overwritten with target values in place)
		 * @return List of register values measured before the reset (for diagnostics/logging)
		 */
		std::vector<uint64_t> operator()(std::vector<System>& state) const;

		/** @brief SparseState version */
		std::vector<uint64_t> operator()(SparseState& state) const
		{
			return (*this)(state.basis_states);
		}
	};

	/**
	 * @brief Read-only Born probability query
	 * @details Computes the probability of the event that the given registers take the given values,
	 *          without modifying the state in any way. Used for `QIF`/`QWHILE` condition evaluation
	 *          in dynamic executors, Born-rule verification in conformance tests, and estimating
	 *          branch probabilities before actually measuring/resetting.
	 */
	struct Probability
	{
		/** @brief List of register IDs involved in the query */
		std::vector<size_t> registers;

		/** @brief List of target values (one-to-one with registers) */
		std::vector<uint64_t> values;

		/**
		 * @brief Constructor (name -> value map version)
		 * @throws invalid_argument Name not found, or a value exceeds the corresponding register's bit width
		 */
		explicit Probability(const std::map<std::string_view, uint64_t>& assignments);

		/**
		 * @brief Constructor (ID -> value map version)
		 * @throws invalid_argument ID out of range/inactive, or a value exceeds the corresponding register's bit width
		 */
		explicit Probability(const std::map<size_t, uint64_t>& assignments);

		/**
		 * @brief Constructor (name list + value list version)
		 * @throws invalid_argument Name not found, duplicate registers, or a value exceeds the bit width
		 */
		Probability(const std::vector<std::string>& register_names, const std::vector<uint64_t>& target_values);

		/**
		 * @brief Constructor (ID list + value list version)
		 * @throws invalid_argument ID out of range/inactive, duplicate registers, or a value exceeds the bit width
		 */
		Probability(const std::vector<size_t>& register_ids, const std::vector<uint64_t>& target_values);

		/**
		 * @brief Constructor (single register name + value)
		 * @throws invalid_argument Name not found, or the value exceeds the register's bit width
		 */
		Probability(std::string_view register_name, uint64_t value);

		/**
		 * @brief Constructor (single register ID + value)
		 * @throws invalid_argument ID out of range/inactive, or the value exceeds the register's bit width
		 */
		Probability(size_t register_id, uint64_t value);

		/**
		 * @brief Compute the probability of this assignment combination
		 * @param state System state vector (read-only, not modified)
		 * @return Probability (in [0, 1]; returns 1 for an empty constraint)
		 */
		double operator()(const std::vector<System>& state) const;

		/** @brief SparseState version */
		double operator()(const SparseState& state) const
		{
			return (*this)(state.basis_states);
		}

		/**
		 * @brief Compute the full outcome distribution of a single register (read-only)
		 * @param state System state vector
		 * @param register_id Register ID
		 * @return Mapping from register values to probabilities
		 * @throws invalid_argument ID out of range/inactive
		 */
		static std::map<uint64_t, double> distribution(const std::vector<System>& state, size_t register_id);

		/** @brief SparseState version (by ID) */
		static std::map<uint64_t, double> distribution(const SparseState& state, size_t register_id)
		{
			return distribution(state.basis_states, register_id);
		}

		/** @brief By-register-name version */
		static std::map<uint64_t, double> distribution(const std::vector<System>& state, std::string_view register_name);

		/** @brief SparseState version (by name) */
		static std::map<uint64_t, double> distribution(const SparseState& state, std::string_view register_name)
		{
			return distribution(state.basis_states, register_name);
		}
	};
}
