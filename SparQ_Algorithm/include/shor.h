/**
 * @file shor.h
 * @brief Shor's quantum factoring algorithm (standard + semi-classical versions)
 * @details Implements the quantum part of Shor's algorithm via register-level programming:
 *          the modular exponentiation operator ExpMod (|x⟩|z⟩ → |x⟩|z·a^x mod N⟩), the full
 *          phase-estimation-style pipeline (Shor) and the semi-classical (measurement-feedback)
 *          variant SemiClassicalShor, plus classical postprocessing helpers such as the
 *          continued-fractions finisher. The corresponding Python implementation is in
 *          pysparq.algorithms.shor; the C++ experiment entry points are in Experiments/Shor
 */

#pragma once

#include "sparse_state_simulator.h"

namespace qram_simulator {
	/**
	 * @namespace qram_simulator::shor
	 * @brief Shor's factoring algorithm components
	 */
	namespace shor {
		/** @brief Modular exponentiation function type: x ↦ a^x mod N (wrapped from classical precomputation) */
		using ExpModFunc = std::function<size_t(size_t)>;

		/**
		 * @brief Compute the large-exponent modular power a^x mod N
		 * @param a Base
		 * @param x Exponent (arbitrarily large integer)
		 * @param N Modulus (the odd composite to be factored)
		 * @return a^x mod N
		 */
		/* compute a^x mod N for any large x */
		size_t general_expmod(size_t a, size_t x, size_t N);

		/**
		 * @brief Shor execution failure exception
		 * @details Thrown in scenarios such as when the measurement results cannot yield a valid
		 *          period (postprocessing failure)
		 */
		class ShorExecutionFailed : public std::runtime_error
		{
		public:
			/**
			 * @brief Constructor
			 * @param message Exception description
			 */
			ShorExecutionFailed(const std::string& message) : std::runtime_error(message) {}
		};

		/**
		 * @brief Throw a Shor execution failure exception
		 * @param message Exception description
		 */
		inline void throw_bad_shor_result(const std::string& message)
		{
			throw ShorExecutionFailed(message);
		}

		/**
		 * @brief Modular exponentiation quantum operator (self-adjoint)
		 * @details Implements |x⟩|z⟩ → |x⟩|z · (a^x mod N)⟩; the modular exponentiation function
		 *          is supplied by the classically precomputed ExpModFunc (a lookup table of
		 *          a^x mod N over one period r), so the quantum side only performs a
		 *          function-table-lookup-style transform
		 */
		/* compute |x>|z> -> |x>|z ^ (a^x mod N)> */
		struct ExpMod : SelfAdjointOperator
		{
			/** @brief Input (exponent) register ID */
			size_t reg_input;
			/** @brief Output (power value) register ID */
			size_t reg_output;
			/** @brief Classically precomputed modular exponentiation function */
			ExpModFunc anc_func;

			/**
			 * @brief Constructor
			 * @param reg_input_ Input register ID
			 * @param reg_output_ Output register ID
			 * @param func Modular exponentiation function
			 */
			ExpMod(size_t reg_input_, size_t reg_output_, ExpModFunc func)
				:reg_input(reg_input_), reg_output(reg_output_), anc_func(func)
			{}

			/**
			 * @brief Apply the modular exponentiation operation
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief Semi-classical Shor factorizer (measurement-feedback quantum phase estimation)
		 * @details Replaces the full inverse QFT with bit-by-bit measurement + feedback rotation:
		 *          after each measured bit, a conditional phase rotation is applied to the remaining
		 *          superposition based on the bits measured so far, significantly reducing the number
		 *          of qubits required. run() executes the quantum part and reads out via partial
		 *          trace; postprocess() recovers the period via continued fractions and produces
		 *          the factorization result
		 */
		/* Seems good */
		struct SemiClassicalShor
		{
			/** @brief Random base a (coprime with N) */
			size_t a;
			/** @brief Number of binary digits of N */
			size_t n;
			/** @brief Odd composite N to be factored */
			size_t N;
			/** @brief Working register bit width (2n) */
			size_t size;
			/** @brief Final measurement result (filled by run()) */
			size_t meas_result = 0;
			/** @brief Recovered period r (filled by postprocess(), 0 means failure) */
			size_t period = 0;
			/** @brief Factor p (filled by postprocess()) */
			size_t p = 0;
			/** @brief Factor q (filled by postprocess()) */
			size_t q = 0;

			/**
			 * @brief Constructor
			 * @param a_ Random base (coprime with N)
			 * @param N_ Odd composite to be factored
			 * @param n_ Number of binary digits of N
			 */
			SemiClassicalShor(size_t a_, size_t N_, size_t n_)
				: a(a_), N(N_), n(n_), size(n_ * 2)
			{
			}

			/**
			 * @brief Execute the quantum part (semi-classical phase estimation + partial-trace readout)
			 * @return Measured result as an integer value
			 */
			size_t run();

			/** @brief Classical postprocessing: recover the period via continued fractions and compute factors p, q */
			void postprocess();
		};

		/**
		 * @brief Standard Shor factorization operator (phase-estimation style)
		 * @details After the working register is prepared in superposition, ExpMod performs modular
		 *          exponentiation, then a partial trace over the working register (equivalent to
		 *          inverse-QFT sampling) reads out the phase information, from which the period is
		 *          recovered by classical postprocessing
		 */
		/* Seems good */
		struct Shor
		{
			/** @brief Working register ID (holds the superposed exponent x) */
			size_t work_reg;
			/** @brief Ancillary register ID (holds a^x mod N) */
			size_t ancilla_reg;
			/** @brief Classically precomputed modular exponentiation function */
			ExpModFunc anc_func;

			/**
			 * @brief Constructor
			 * @param work_register Working register ID
			 * @param ancilla_register Ancillary register ID
			 * @param a_ Base a (kept for semantics only; actual computation goes through func)
			 * @param N_ Modulus N (kept for semantics only)
			 * @param func Modular exponentiation function
			 */
			Shor(size_t work_register, size_t ancilla_register, size_t a_, size_t N_, ExpModFunc func)
				: work_reg(work_register), ancilla_reg(ancilla_register), anc_func(func)
			{}

			/**
			 * @brief Execute the quantum part of Shor's algorithm
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief Find the numerator and denominator of the best continued-fractions approximation y/Q
		 *        for a measured value
		 * @param y Phase-estimation measured value
		 * @param Q Denominator upper bound (usually 2^size)
		 * @param N Number to be factored (the approximation's denominator must be less than N)
		 * @return (numerator, denominator) pair of the best approximation
		 */
		std::pair<size_t, size_t> find_best_fraction(size_t y, size_t Q, size_t N);

		/**
		 * @brief Compute the period r from a measurement result
		 * @param meas_result Phase-estimation measured value
		 * @param size Working register bit width
		 * @param N Number to be factored
		 * @return Candidate period (0 means failure)
		 */
		uint64_t compute_period(uint64_t meas_result, size_t size, size_t N);

		/**
		 * @brief Validate a period candidate: r must be even and a^{r/2} ≢ -1 (mod N)
		 * @param period Period candidate
		 * @param a Base
		 * @param N Number to be factored
		 * @throws ShorExecutionFailed Thrown when validation fails
		 */
		void check_period(uint64_t period, uint64_t a, uint64_t N);

		/**
		 * @brief Shor classical postprocessing: recover the period from a measured value and compute factors
		 * @param meas Phase-estimation measured value
		 * @param size Working register bit width
		 * @param a Base
		 * @param N Number to be factored
		 * @return (p, q) factor pair (invalid values on failure)
		 * @throws ShorExecutionFailed Thrown when no valid period can be recovered
		 */
		std::tuple<uint64_t, uint64_t> shor_postprocess(uint64_t meas, size_t size, uint64_t a, uint64_t N);

		/**
		 * @brief Full standard Shor factorization pipeline (C++ experiment entry point)
		 * @details Random (or specified) base a → precompute the modular exponentiation table →
		 *          quantum phase estimation → partial-trace readout → continued-fractions
		 *          postprocessing to output the factors
		 * @param N Odd composite to be factored
		 * @param ainput Optionally specified base (chosen at random by default)
		 * @return 0 means the pipeline completed; 1 means a and N are not coprime (in that case
		 *         gcd(a,N) is already a factor)
		 */
		inline int common_shor(size_t N, std::optional<size_t> ainput = std::nullopt)
		{
			size_t n = log2(N) + 1;
			size_t size = n * 2;

			uint64_t a;
			if (ainput.has_value())
				a = ainput.value();
			else
				a = uint64_t(random_engine::rng() * (N - 1) + 1);

			// check whether a and N are coprime
			if (std::gcd(a, N) != 1)
			{
				fmt::print("a = {} and N = {} are not coprime\n", a, N);
				return 1;
			}

			auto work_reg = System::add_register("work_reg", UnsignedInteger, size);
			auto anc_reg = System::add_register("anc_reg", UnsignedInteger, n);

			std::vector<System> state;
			state.emplace_back();

			std::vector<uint64_t> axmodn;
			uint64_t value = 1;
			axmodn.push_back(value);
			for (size_t i = 1; i < N; ++i)
			{
				uint64_t next_val = axmodn.back() * a % N;
				if (next_val == 1)
					break;
				else
					axmodn.push_back(next_val);
			}

			size_t r = axmodn.size();
			fmt::print("r = {} (N = {}, n = {}, a = {})\n", r, N, n, a);

			ExpModFunc func = [r, &axmodn](size_t x) -> size_t
				{
					x = x % r;
					return axmodn[x];
				};

			Shor(work_reg, anc_reg, a, N, func)(state);

			//(RemoveRegister(anc_reg))(state);
			//SortByAmplitude()(state);
			//StatePrint(Detail | Prob)(state);

			auto&& [meas_res, _] = PartialTrace(std::vector{ work_reg })(state);
			auto&& [p, q] = shor_postprocess(meas_res[0], size, a, N);
		}

		/**
		 * @brief Full semi-classical Shor factorization pipeline (C++ experiment entry point)
		 * @details Same classical preparation as common_shor, but the quantum part instead uses
		 *          SemiClassicalShor's bit-by-bit measurement-feedback phase estimation
		 * @param N Odd composite to be factored
		 * @param ainput Optionally specified base (chosen at random by default)
		 * @return 0 means the pipeline completed; 1 means a and N are not coprime (in that case
		 *         gcd(a,N) is already a factor)
		 */
		inline auto semi_classical_shor(size_t N, std::optional<size_t> ainput = std::nullopt)
		{
			size_t n = log2(N) + 1;

			size_t a;
			if (ainput.has_value())
				a = ainput.value();
			else
				a = size_t(random_engine::rng() * (N - 1) + 1);

			// check whether a and N are coprime
			if (std::gcd(a, N) != 1)
			{
				fmt::print("a = {} and N = {} are not coprime\n", a, N);
				return 1;
			}

			std::vector<uint64_t> axmodn;
			uint64_t value = 1;
			axmodn.push_back(value);
			for (size_t i = 1; i < N; ++i)
			{
				uint64_t next_val = axmodn.back() * a % N;
				if (next_val == 1)
					break;
				else
					axmodn.push_back(next_val);
			}

			size_t r = axmodn.size();

			fmt::print("r = {} (N = {}, n = {}, a = {})\n", r, N, n, a);

			SemiClassicalShor obj(a, N, n);
			uint64_t result = obj.run();
			obj.postprocess();

			return 0;
		}
	}
} // namespace qram_simulator
