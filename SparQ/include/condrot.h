/**
 * @file condrot.h
 * @brief Controlled rotation gate operation definitions
 * @details Implements condition-based rotation operations, including rational-number conditional rotation and
 *          general-function conditional rotation
 */

#pragma once
#include "quantum_interfere_basic.h"

namespace qram_simulator
{
	/**
	 * @brief Create a rotation function (based on a value and the number of digits)
	 * @param value Input value
	 * @param n_digit Number of digits
	 * @return 2x2 rotation matrix
	 */
	HOST_DEVICE inline u22_t make_func(uint64_t value, size_t n_digit)
	{
		double theta = 0;
		if (n_digit == 64)
			theta = value * 1.0 / 2 / pow2(63);
		else
			theta = value * 1.0 / pow2(n_digit);

		theta *= (2 * pi);

		complex_t u00 = cos(theta),
			u01 = -sin(theta),
			u10 = sin(theta),
			u11 = cos(theta);

		return u22_t{ u00, u01, u10, u11 };
	}

	/**
	 * @brief Create an inverse rotation function (based on a value and the number of digits)
	 * @param value Input value
	 * @param n_digit Number of digits
	 * @return 2x2 inverse rotation matrix
	 */
	HOST_DEVICE inline u22_t make_func_inv(uint64_t value, size_t n_digit)
	{
		double theta = 0;
		if (n_digit == 64)
			theta = value * 1.0 / 2 / pow2(63);
		else
			theta = value * 1.0 / pow2(n_digit);

		theta *= (2 * pi);

		complex_t u00 = cos(theta),
			u01 = sin(theta),
			u10 = -sin(theta),
			u11 = cos(theta);

		return u22_t{ u00, u01, u10, u11 };
	}

	/**
	 * @brief Rational-number controlled rotation gate (single qubit)
	 * @details Performs a controlled rotation on the Boolean output register based on the value of the
	 *          rational input register
	 */
		struct CondRot_Rational_Bool : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Input register ID */
		size_t register_in;

		/** @brief Output register ID */
		size_t register_out;

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Input register name
		 * @param reg_out Output register name
		 * @throws Throws an exception when the types do not match
		 */
		CondRot_Rational_Bool(std::string_view reg_in, std::string_view reg_out)
			:register_in(System::get(reg_in)), register_out(System::get(reg_out))
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(register_in) != Rational ||
				System::type_of(register_out) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Input register ID
		 * @param reg_out Output register ID
		 */
		CondRot_Rational_Bool(size_t reg_in, size_t reg_out)
			:register_in(reg_in), register_out(reg_out)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(register_in) != Rational ||
				System::type_of(register_out) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the controlled rotation operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;
		};

		struct CondRot_Fixed_Bool : CondRot_Rational_Bool
		{
			using CondRot_Rational_Bool::operator();
			using CondRot_Rational_Bool::dag;
			using CondRot_Rational_Bool::CondRot_Rational_Bool;
		};

	/**
	 * @brief General controlled rotation gate (single qubit)
	 * @details Performs a controlled rotation on the Boolean output register based on a general function
	 * @tparam Callable Type of the angle computation function
	 */
	template<typename Callable = std::function<u22_t(uint64_t)>>
	struct CondRot_General_Bool_Fast : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Input register ID */
		size_t in_id;

		/** @brief Output register ID */
		size_t out_id;

		/** @brief Angle computation function */
		Callable func;

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Input register name
		 * @param reg_out Output register name
		 * @param angle_function Angle computation function
		 * @throws Throws an exception when the types do not match or the output register size is not 1
		 */
		CondRot_General_Bool_Fast(std::string_view reg_in, std::string_view reg_out, Callable angle_function)
			: in_id(System::get(reg_in)), out_id(System::get(reg_out)), func(angle_function)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg_out) != Boolean)
				throw_invalid_input();

			if (System::size_of(out_id) != 1)
				throw_invalid_input("Hadamard_Bool: size of output register must be 1");
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Input register ID
		 * @param reg_out Output register ID
		 * @param angle_function Angle computation function
		 */
		CondRot_General_Bool_Fast(size_t reg_in, size_t reg_out, Callable angle_function)
			: in_id(reg_in), out_id(reg_out), func(angle_function)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg_out) != Boolean)
				throw_invalid_input();

			if (System::size_of(out_id) != 1)
				throw_invalid_input("Hadamard_Bool: size of output register must be 1");
#endif
		}

		/**
		 * @brief Operate on a pair
		 * @param zero |0> branch index
		 * @param one |1> branch index
		 * @param state System state vector
		 */
		void operate_pair(size_t zero, size_t one, std::vector<System>& state) const
		{
			StateStorage& storage = state[zero].get(in_id);
			uint64_t v = storage.as<uint64_t>(System::size_of(in_id));
			u22_t mat = func(v);

			complex_t a = state[zero].amplitude;
			complex_t b = state[one].amplitude;
			state[zero].amplitude = a * mat[0] + b * mat[1];
			state[one].amplitude = a * mat[2] + b * mat[3];
		}

		/**
		 * @brief Operate on the |0> branch alone
		 * @param zero |0> branch index
		 * @param state System state vector
		 */
		void operate_alone_zero(size_t zero, std::vector<System>& state) const
		{
			StateStorage& storage = state[zero].get(in_id);
			uint64_t v = storage.as<uint64_t>(System::size_of(in_id));
			u22_t mat = func(v);

			state.push_back(state[zero]);
			state.back().get(out_id).value = 1;

			state[zero].amplitude *= mat[0];
			state.back().amplitude *= mat[2];
		}

		/**
		 * @brief Operate on the |1> branch alone
		 * @param one |1> branch index
		 * @param state System state vector
		 */
		void operate_alone_one(size_t one, std::vector<System>& state) const
		{
			StateStorage& storage = state[one].get(in_id);
			uint64_t v = storage.as<uint64_t>(System::size_of(in_id));
			u22_t mat = func(v);

			state.push_back(state[one]);
			state.back().get(out_id).value = 0;

			state.back().amplitude *= mat[1];
			state[one].amplitude *= mat[3];
		}

		/**
		 * @brief Apply the general controlled rotation operation (V2 implementation)
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const
		{
			profiler _("CondRot_General_Bool_v2");

			if (!state.size()) return;

#ifdef SAFE_HASH
			StateLessExceptKey pred(out_id);
			std::map<System, size_t, StateLessExceptKey> buckets(pred);
#else
			auto hash_func = StateHashExceptKey(out_id);
			for (auto& s : state)
				s.cached_hash = hash_func(s);

			std::unordered_map<size_t, size_t> buckets;
#endif
			size_t current_size = state.size();

			for (size_t i = 0; i < current_size; ++i)
			{
#ifdef SAFE_HASH
				const auto& s = state[i];
#else
				const auto& s = state[i].cached_hash;
#endif
				auto iter = buckets.find(s);
				if (iter == buckets.end())
				{
					buckets.insert({ s, i });
					continue;
				}
				else
				{
#ifdef CHECK_HASH
					auto pred = StateEqualExceptKey(out_id);
					if (!pred(state[iter->second], state[i]))
						throw_general_runtime_error();
#endif
					StateStorage& storage = state[iter->second].get(out_id);
					if (storage.as<bool>(1))
					{
						operate_pair(i, iter->second, state);
					}
					else
					{
						operate_pair(iter->second, i, state);
					}
					buckets.erase(iter);
				}
			}

			for (auto& stored_key : buckets)
			{
				StateStorage& storage = state[stored_key.second].get(out_id);
				if (storage.as<bool>(1))
				{
					operate_alone_one(stored_key.second, state);
				}
				else
				{
					operate_alone_zero(stored_key.second, state);
				}
			}
			ClearZero()(state);
		}

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the general controlled rotation operation
		 * @param s CUDA sparse state
		 */
		void operator()(CuSparseState& s) const;
#endif
	};

	template<typename Callable = std::function<u22_t(uint64_t)>>
	using CondRot_General_Bool = CondRot_General_Bool_Fast<Callable>;

	template<typename Callable = std::function<u22_t(uint64_t)>>
	using CondRot_General_Bool_fast [[deprecated("use CondRot_General_Bool_Fast")]] = CondRot_General_Bool_Fast<Callable>;

}
