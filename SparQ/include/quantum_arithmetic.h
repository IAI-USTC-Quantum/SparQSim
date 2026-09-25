/**
 * @file quantum_arithmetic.h
 * @brief Quantum arithmetic operations definitions
 * @details Implements various quantum arithmetic operations, including flip, shift, multiplication,
 *          addition, comparison, etc.
 *
 * @section unitary_notes Unitarity notes
 *
 * Quantum operators must satisfy the unitary property (U^†U = I), which requires operations to be reversible.
 * The operators in this file fall into two categories:
 *
 * 1. Out-of-place operations (e.g. Add_UInt_UInt):
 *    - The result is stored in a separate output register
 *    - Unitarity is guaranteed by bitwise XOR: result ^= f(inputs)
 *    - Unitarity is automatic because XOR is self-inverse
 *
 * 2. In-place operations (e.g. Add_UInt_UInt_InPlace, Add_ConstUInt_InPlace):
 *    - The result directly modifies the input register
 *    - An explicit dagger() method is required to guarantee reversibility
 *    - The inverse operation is usually implemented with modular arithmetic: y = (y + (2^N - x)) % 2^N
 *
 * @section type_safety_notes Type safety notes
 *
 * All operators check the following in debug mode (non-QRAM_Release):
 * - Types of input/output registers (UnsignedInteger/SignedInteger/Boolean/Rational)
 * - Whether register sizes match
 * - Valid ranges of operands (e.g. the number of bits to shift)
 *
 * In Release mode these checks are compiled out for the best performance.
 */

#pragma once
#include "basic_components.h"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Boolean flip operation
	 * @details Flips all bits in the register (bitwise NOT), implementing y = ~y
	 *
	 * @note Unitarity: self-adjoint operator (SelfAdjointOperator), i.e. U^† = U
	 * @note Data type: any integer type (UnsignedInteger/SignedInteger)
	 * @note Overflow behavior: only bits within the register size are affected; high bits are flipped
	 *       but truncated by the mask
	 *
	 * @pre the input register must be active
	 *
	 * @par Example
	 * @code
	 * // 4-bit register, initial value 0b1010 (10)
	 * FlipBools("reg");  // Result: 0b0101 (5)
	 * @endcode
	 */
	struct FlipBools : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		ClassControllable

		/** @brief Register ID */
		size_t id;

		/**
		 * @brief Constructor (name version)
		 * @param reg Register name
		 */
		FlipBools(std::string_view reg)
			:id(System::get(reg)) { }

		/**
		 * @brief Constructor (ID version)
		 * @param id_ Register ID
		 */
		FlipBools(size_t id_)
			:id(id_)
		{}

		/**
		 * @brief Apply the flip operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the flip operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Two-bit swap operation
	 * @details Swaps single bits at the specified positions of two registers
	 *
	 * Implementation: swaps the values of the specified bits of the two registers using a temporary variable
	 * This is a self-adjoint operation; applying it twice yields the identity operation
	 *
	 * @note Unitarity: self-adjoint operator, Swap^2 = I
	 * @note Data type: boolean bits of registers of any type
	 *
	 * @pre the lhs and rhs registers must be active
	 * @pre digit1 and digit2 must be within the valid bit range of their registers [0, size)
	 *
	 * @par Example
	 * @code
	 * // reg1 = 0b1010, reg2 = 0b0101
	 * Swap_Bool_Bool("reg1", 0, "reg2", 1);  // Swaps bit 0 of reg1 and bit 1 of reg2
	 * @endcode
	 */
	struct Swap_Bool_Bool : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Left operand bit index */
		size_t digit1;

		/** @brief Right operand bit index */
		size_t digit2;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg1 First register name
		 * @param d1 First bit index
		 * @param reg2 Second register name
		 * @param d2 Second bit index
		 * @throws Throws an exception when a bit index is out of the register size
		 */
		Swap_Bool_Bool(std::string_view reg1, size_t d1,
			std::string_view reg2, size_t d2)
			: lhs(System::get(reg1)), rhs(System::get(reg2)),
			digit1(d1), digit2(d2)
		{
			/* Size check */
#ifndef QRAM_Release
			if (d1 >= System::size_of(lhs) || d2 >= System::size_of(rhs))
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param id1 First register ID
		 * @param d1 First bit index
		 * @param id2 Second register ID
		 * @param d2 Second bit index
		 * @throws Throws an exception when a bit index is out of the register size
		 */
		Swap_Bool_Bool(size_t id1, size_t d1,
			size_t id2, size_t d2) : lhs(id1), rhs(id2), digit1(d1), digit2(d2)
		{
			/* Size check */
#ifndef QRAM_Release
			if (d1 >= System::size_of(lhs) || d2 >= System::size_of(rhs))
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the swap operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the swap operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Rotate-left operation
	 * @details Rotates the register value left by the specified number of bits; the overflowing high bits wrap
	 *          around to the low bits
	 *
	 * Mathematical definition: y = (y << digit) | (y >> (N - digit)), where N is the register bit width
	 *
	 * @note Unitarity: rotation is a bijection, guaranteeing unitarity
	 * @note dagger operation: the dagger of a left rotation by d bits is a right rotation by d bits
	 *       (or a left rotation by N-d bits)
	 * @note Data type: UnsignedInteger recommended, may also be used with SignedInteger
	 *
	 * @pre register_1 must be active
	 * @pre digit <= register size (digit == size is equivalent to the identity operation)
	 *
	 * @par Example
	 * @code
	 * // 4-bit register, initial value 0b1010
	 * ShiftLeft_InPlace("reg", 1);  // Result: 0b0101 (rotate left by 1 bit)
	 * ShiftLeft_InPlace("reg", 2);  // Result: 0b1010 (rotate left by 2 bits)
	 * @endcode
	 */
	struct ShiftLeft_InPlace : BaseOperator {
		using BaseOperator::operator();
				/**  Apply the dagger operation (calls ShiftRight_InPlace) */
		void dag(std::vector<System>& state) const;
#ifdef USE_CUDA
		/**  CUDA: apply the dagger operation */
		void dag(CuSparseState& state) const;
#endif

		/** @brief Register ID */
		size_t register_1;

		/** @brief Number of bits to shift */
		size_t digit;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg1 Register name
		 * @param d Number of bits to shift
		 * @throws Throws an exception when a register type is not an integer type
		 */
		ShiftLeft_InPlace(std::string_view reg1, size_t d)
			:register_1(System::get(reg1)), digit(d)
		{
			/* Type check */
#ifndef QRAM_Release
			auto type = System::type_of(register_1);
			if (type != UnsignedInteger && type != SignedInteger)
				throw_invalid_input();
			if (d > System::size_of(register_1))
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg1 Register ID
		 * @param d Number of bits to shift
		 * @throws Throws an exception when a register type is not an integer type
		 */
		ShiftLeft_InPlace(size_t reg1, size_t d)
			:register_1(reg1), digit(d)
		{
			/* Type check */
#ifndef QRAM_Release
			auto type = System::type_of(register_1);
			if (type != UnsignedInteger && type != SignedInteger)
				throw_invalid_input();
			if (d > System::size_of(register_1))
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the left-rotate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the left-rotate operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};


	/**
	 * @brief Rotate-right operation
	 * @details Rotates the register value right by the specified number of bits; the overflowing low bits wrap
	 *          around to the high bits
	 *
	 * Mathematical definition: y = (y >> digit) | (y << (N - digit)), where N is the register bit width
	 *
	 * @note Unitarity: rotation is a bijection, guaranteeing unitarity
	 * @note dagger operation: the dagger of a right rotation by d bits is a left rotation by d bits
	 *       (or a right rotation by N-d bits)
	 * @note Data type: UnsignedInteger recommended, may also be used with SignedInteger
	 *
	 * @pre register_1 must be active
	 * @pre digit <= register size (digit == size is equivalent to the identity operation)
	 *
	 * @par Example
	 * @code
	 * // 4-bit register, initial value 0b1010
	 * ShiftRight_InPlace("reg", 1);  // Result: 0b0101 (rotate right by 1 bit)
	 * @endcode
	 */
	struct ShiftRight_InPlace : BaseOperator {
		using BaseOperator::operator();
				/**  Apply the dagger operation (calls ShiftLeft_InPlace) */
		void dag(std::vector<System>& state) const;
#ifdef USE_CUDA
		/**  CUDA: apply the dagger operation */
		void dag(CuSparseState& state) const;
#endif

		/** @brief Register ID */
		size_t register_1;

		/** @brief Number of bits to shift */
		size_t digit;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg1 Register name
		 * @param d Number of bits to shift
		 * @throws Throws an exception when a register type is not an integer type
		 */
		ShiftRight_InPlace(std::string_view reg1, size_t d)
			:register_1(System::get(reg1)), digit(d)
		{
			/* Type check */
#ifndef QRAM_Release
			auto type = System::type_of(register_1);
			if (type != UnsignedInteger && type != SignedInteger)
				throw_invalid_input();
			if (d > System::size_of(register_1))
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg1 Register ID
		 * @param d Number of bits to shift
		 * @throws Throws an exception when a register type is not an integer type
		 */
		ShiftRight_InPlace(size_t reg1, size_t d)
			:register_1(reg1), digit(d)
		{
			/* Type check */
#ifndef QRAM_Release
			auto type = System::type_of(register_1);
			if (type != UnsignedInteger && type != SignedInteger)
				throw_invalid_input();
			if (d > System::size_of(register_1))
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the right-rotate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the right-rotate operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer multiply-by-constant operation (Out-of-place)
	 * @details Implements out-of-place multiplication: res ^= lhs * mult
	 *
	 * Unitary guarantee: implemented via XOR, res = res ⊕ (lhs * mult)
	 * Applied twice: res ⊕ (lhs * mult) ⊕ (lhs * mult) = res, i.e. U^2 = I
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: both input and output must be UnsignedInteger
	 * @note Overflow behavior: the multiplication result is truncated to the output register size
	 *
	 * @pre the lhs and res registers must be of UnsignedInteger type
	 * @pre all registers must be active
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", UnsignedInteger, 4);
	 * auto res = System::add_register("res", UnsignedInteger, 4);
	 * Init_Unsafe(lhs, 3);  // lhs = 3
	 * // res = 0 ⊕ (3 * 4) = 12
	 * Mult_UInt_ConstUInt("lhs", 4, "res");
	 * @endcode
	 */
	struct Mult_UInt_ConstUInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Multiplier (constant) */
		size_t mult_int;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Input register name
		 * @param mult Multiplier constant
		 * @param reg_out Output register name
		 */
		Mult_UInt_ConstUInt(std::string_view reg_in, size_t mult, std::string_view reg_out)
			: lhs(System::get(reg_in)), res(System::get(reg_out)), mult_int(mult)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif	
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Input register ID
		 * @param mult Multiplier constant
		 * @param reg_out Output register ID
		 */
		Mult_UInt_ConstUInt(size_t reg_in, size_t mult, size_t reg_out)
			: lhs(reg_in), res(reg_out), mult_int(mult)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif	
		}

		/**
		 * @brief Apply the multiplication operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the multiplication operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Accumulate multiply-by-constant operation (In-place)
	 * @details Implements in-place accumulate-multiply: res += lhs * mult (mod 2^N)
	 *         Note: the lhs register is not modified; only res is updated.
	 *
	 * This is an in-place operation; an explicit dagger implementation is required to guarantee unitarity.
	 * Forward:  res += lhs * mult (mod 2^N)  [lhs unchanged]
	 * Dagger:   res -= lhs * mult (mod 2^N)  [lhs unchanged]
	 *
	 * @note Unitarity: lhs is unchanged, only res is updated via modular addition/subtraction;
	 *       bijectivity is guaranteed by the range of lhs
	 * @note Data type: lhs and res should both be UnsignedInteger
	 * @note Overflow behavior: the result wraps around modulo 2^N at the res register size
	 *
	 * @pre the res register must have enough bits to store the result
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", UnsignedInteger, 4);
	 * auto res = System::add_register("res", UnsignedInteger, 4);
	 * Init_Unsafe(lhs, 2);  // lhs = 2
	 * Init_Unsafe(res, 3);  // res = 3
	 * // res = 3 + (2 * 4) = 11
	 * Add_Mult_UInt_ConstUInt_InPlace("lhs", 4, "res");
	 * @endcode
	 */
	struct Add_Mult_UInt_ConstUInt_InPlace : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Multiplier (constant) */
		size_t mult_int;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Input register name
		 * @param mult Multiplier constant
		 * @param reg_out Output register name (result accumulated here)
		 * @throws Throws an exception when a register type is not UnsignedInteger
		 */
		Add_Mult_UInt_ConstUInt_InPlace(std::string_view reg_in, size_t mult, std::string_view reg_out)
			: mult_int(mult), lhs(System::get(reg_in)), res(System::get(reg_out))
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Input register ID
		 * @param mult Multiplier constant
		 * @param reg_out Output register ID (result accumulated here)
		 * @throws Throws an exception when a register type is not UnsignedInteger
		 */
		Add_Mult_UInt_ConstUInt_InPlace(size_t reg_in, size_t mult, size_t reg_out)
			: mult_int(mult), lhs(reg_in), res(reg_out)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the accumulate-multiply operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the accumulate-multiply operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;

		/**
		 * @brief CUDA: apply the dagger operation
		 * @param state CUDA sparse state
		 */
		void dag(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Modular multiplication
	 * @details Computes |y⟩ → |y * a^(2^x) mod N⟩
	 *
	 * When a and N are coprime, Mod_Mult_UInt_ConstUInt_InPlace is a unitary operation.
	 *
	 * @section Mod_Mult_unitary Unitarity notes for Mod_Mult_UInt_ConstUInt_InPlace
	 *
	 * Unitarity conditions for Mod_Mult_UInt_ConstUInt_InPlace:
	 * 1. a and N must be coprime (gcd(a, N) = 1)
	 * 2. When the condition holds, the inverse operation is y * a^(2^x*(N-2)) mod N (Fermat's little theorem)
	 *
	 * @section Mod_Mult_usage Usage example
	 *
	 * @code
	 * auto reg = System::add_register("y", UnsignedInteger, 4);
	 * auto cond = System::add_register("ctrl", Boolean, 1);
	 * Mod_Mult_UInt_ConstUInt_InPlace(reg, 7, 2, 15).conditioned_by_all_ones(cond)(state);
	 * // Computes: y = y * 7^4 mod 15 = y * 4 mod 15
	 * @endcode
	 */
	struct Mod_Mult_UInt_ConstUInt_InPlace : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Operand register ID */
		size_t reg;

		/** @brief Base */
		uint64_t a;

		/** @brief Exponent bit (computes a^(2^x)) */
		uint64_t x;

		/** @brief Modulus */
		uint64_t N;

		/** @brief Precomputed operand opnum = a^(2^x) mod N */
		uint64_t opnum;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_name Operand register name
		 * @param a Base
		 * @param x Exponent bit
		 * @param N Modulus
		 * @throws Throws an exception when a and N are not coprime
		 */
		Mod_Mult_UInt_ConstUInt_InPlace(std::string_view reg_name, uint64_t a, uint64_t x, uint64_t N);

		/**
		 * @brief Constructor (ID version)
		 * @param reg_id Operand register ID
		 * @param a Base
		 * @param x Exponent bit
		 * @param N Modulus
		 * @throws Throws an exception when a and N are not coprime
		 */
		Mod_Mult_UInt_ConstUInt_InPlace(size_t reg_id, uint64_t a, uint64_t x, uint64_t N);

		/**
		 * @brief Execute the modular multiplication
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Execute the inverse modular multiplication
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: execute the modular multiplication
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;

		/**
		 * @brief CUDA: execute the inverse modular multiplication
		 * @param state CUDA sparse state
		 */
		void dag(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer addition operation (Out-of-place)
	 * @details Implements out-of-place addition: res ^= lhs + rhs
	 *
	 * Unitary guarantee: implemented via XOR, res = res ⊕ (lhs + rhs)
	 * Applied twice: res ⊕ (lhs + rhs) ⊕ (lhs + rhs) = res, i.e. U^2 = I
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must all be UnsignedInteger
	 * @note Overflow behavior: the addition result is truncated to the output register size before the XOR
	 *
	 * @pre the lhs, rhs, and res registers must be of UnsignedInteger type
	 * @pre all registers must be active
	 * @pre the res register is usually initialized to 0, but may hold any value (XOR semantics)
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", UnsignedInteger, 4);
	 * auto rhs = System::add_register("rhs", UnsignedInteger, 4);
	 * auto res = System::add_register("res", UnsignedInteger, 4);
	 * Init_Unsafe(lhs, 3);  // lhs = 3
	 * Init_Unsafe(rhs, 5);  // rhs = 5
	 * // res = 0 ⊕ (3 + 5) = 8
	 * Add_UInt_UInt("lhs", "rhs", "res");
	 * @endcode
	 */
	struct Add_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Result register name
		 */
		Add_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_))
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif	
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ Result register ID
		 */
		Add_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_)
			: lhs(lhs_), rhs(rhs_), res(res_)
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif	
		}

		/**
		 * @brief Apply the addition operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the addition operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief In-place unsigned integer addition operation (In-place)
	 * @details Implements in-place addition: rhs += lhs
	 *
	 * This is an in-place operation; an explicit dagger implementation is required to guarantee unitarity.
	 * dagger implementation: rhs += (2^N - lhs) mod 2^N, where N is the rhs register bit width
	 *
	 * @note Unitarity: bijectivity is guaranteed by modular addition
	 * @note Data type: lhs and rhs should both be UnsignedInteger
	 * @note Overflow behavior: the result wraps around modulo 2^N at the rhs register size
	 *
	 * @pre lhs and rhs must be active
	 *
	 * @note lhs and rhs may have different sizes; lhs is read as an integer, and rhs undergoes
	 *       modular addition at its own bit width.
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", UnsignedInteger, 4);
	 * auto rhs = System::add_register("rhs", UnsignedInteger, 4);
	 * Init_Unsafe(lhs, 7);  // lhs = 7
	 * Init_Unsafe(rhs, 3);  // rhs = 3
	 * // rhs = 3 + 7 = 10
	 * Add_UInt_UInt_InPlace("lhs", "rhs");
	 * // dagger: rhs = 10 + (16 - 7) % 16 = 3 (restores the original value)
	 * op.dag(state);
	 * @endcode
	 */
	struct Add_UInt_UInt_InPlace : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name (addend)
		 * @param rhs_ Right operand register name (augend; result stored here)
		 * @throws Throws an exception when a register type is not UnsignedInteger or sizes do not match
		 */
		Add_UInt_UInt_InPlace(std::string_view lhs_, std::string_view rhs_)
			:lhs(System::get(lhs_)), rhs(System::get(rhs_))
		{
			if (lhs == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID (addend)
		 * @param rhs_ Right operand register ID (augend; result stored here)
		 * @throws Throws an exception when a register type is not UnsignedInteger or sizes do not match
		 */
		Add_UInt_UInt_InPlace(size_t lhs_, size_t rhs_)
			: lhs(lhs_), rhs(rhs_)
		{
			if (lhs == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the in-place addition operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the in-place addition operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;

		/**
		 * @brief CUDA: apply the dagger operation
		 * @param state CUDA sparse state
		 */
		void dag(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer add-constant operation (Out-of-place)
	 * @details Implements out-of-place add-constant: res ^= lhs + add_int
	 *
	 * Unitary guarantee: implemented via XOR, res = res ⊕ (lhs + add_int)
	 * Applied twice: res ⊕ (lhs + add_int) ⊕ (lhs + add_int) = res
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs and res must both be UnsignedInteger
	 * @note Overflow behavior: the addition result is truncated to the output register size before the XOR
	 *
	 * @pre the lhs and res registers must be of UnsignedInteger type
	 * @pre all registers must be active
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", UnsignedInteger, 4);
	 * auto res = System::add_register("res", UnsignedInteger, 4);
	 * Init_Unsafe(lhs, 6);  // lhs = 6
	 * // res = 0 ⊕ (6 + 4) = 10
	 * Add_UInt_ConstUInt("lhs", 4, "res");
	 * @endcode
	 */
	struct Add_UInt_ConstUInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Addend (constant) */
		size_t add_int;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Input register name
		 * @param add Addend constant
		 * @param reg_out Output register name
		 */
		Add_UInt_ConstUInt(std::string_view reg_in, size_t add, std::string_view reg_out)
			: lhs(System::get(reg_in)), res(System::get(reg_out)), add_int(add)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif		
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Input register ID
		 * @param add Addend constant
		 * @param reg_out Output register ID
		 */
		Add_UInt_ConstUInt(size_t reg_in, size_t add, size_t reg_out)
			: lhs(reg_in), res(reg_out), add_int(add)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif		
		}

		/**
		 * @brief Apply the add-constant operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the add-constant operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Add-constant operation (In-place)
	 * @details Implements in-place add-constant: reg_in += add_int (mod 2^N)
	 *
	 * This is an in-place operation; an explicit dagger implementation is required to guarantee unitarity.
	 * dagger implementation: reg_in += (2^N - add_int) mod 2^N, where N is the register bit width
	 *
	 * @note Unitarity: bijectivity is guaranteed by modular addition
	 * @note Data type: reg_in is recommended to be UnsignedInteger
	 * @note Overflow behavior: the result wraps around modulo 2^N at the register size
	 *
	 * @pre reg_in must be active
	 * @pre add_int should be less than 2^N (N is the register bit width), otherwise the behavior
	 *       depends on modular arithmetic
	 *
	 * @par Example
	 * @code
	 * auto reg = System::add_register("reg", UnsignedInteger, 4);
	 * Init_Unsafe(reg, 12);  // reg = 12
	 * // reg = (12 + 3) % 16 = 15
	 * Add_ConstUInt_InPlace("reg", 3);
	 * // dagger: reg = (15 + 13) % 16 = 12 (restores the original value)
	 * op.dag(state);
	 * @endcode
	 */
	struct Add_ConstUInt_InPlace : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Addend (constant) */
		size_t add_int;

		/** @brief Input register ID */
		size_t reg_in;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in_ Input register name (result stored here)
		 * @param add Addend constant
		 * @throws Throws an exception when a register type is not an integer type
		 */
		Add_ConstUInt_InPlace(std::string_view reg_in_, size_t add) :
			reg_in(System::get(reg_in_)), add_int(add)
		{
			/* Type check */
#ifndef QRAM_Release
			auto type = System::type_of(reg_in);
			if (type != UnsignedInteger && type != SignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Input register ID (result stored here)
		 * @param add Addend constant
		 * @throws Throws an exception when a register type is not an integer type
		 */
		Add_ConstUInt_InPlace(size_t reg_in, size_t add)
			: reg_in(reg_in), add_int(add)
		{
			/* Type check */
#ifndef QRAM_Release
			auto type = System::type_of(reg_in);
			if (type != UnsignedInteger && type != SignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the add-constant operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the add-constant operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;

		/**
		 * @brief CUDA: apply the dagger operation
		 * @param state CUDA sparse state
		 */
		void dag(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Division square-root arccosine operation (Out-of-place)
	 * @details Implements: res ^= arccos(sqrt(lhs / rhs)) / π / 2
	 *
	 * Used to compute quantum rotation angles, common in quantum machine learning algorithms.
	 *
	 * @note Unitarity: self-adjoint operator, implemented via XOR
	 * @note Data type: lhs and rhs must be UnsignedInteger, res must be Rational
	 * @note Numeric range: the result is in [0, 0.5], encoded as a rational
	 *
	 * @pre lhs and rhs must be of UnsignedInteger type
	 * @pre res must be of Rational type
	 * @pre lhs < rhs (otherwise the sqrt argument exceeds the [0,1] range and may produce NaN)
	 * @pre all registers must be active
	 *
	 * @par Mathematical formula
	 * output = arccos(√(lhs / rhs)) / (2π)
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", UnsignedInteger, 4);
	 * auto rhs = System::add_register("rhs", UnsignedInteger, 4);
	 * auto res = System::add_register("res", Rational, 8);
	 * Init_Unsafe(lhs, 1);  // lhs = 1
	 * Init_Unsafe(rhs, 4);  // rhs = 4
	 * // res = arccos(sqrt(1/4)) / 2π = arccos(0.5) / 2π = 1/6 ≈ 0.167
	 * Div_Sqrt_Arccos_UInt_UInt("lhs", "rhs", "res");
	 * @endcode
	 */
	struct Div_Sqrt_Arccos_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t register_lhs;

		/** @brief Right operand register ID */
		size_t register_rhs;

		/** @brief Output register ID */
		size_t register_out;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param register_lhs Left operand register name
		 * @param register_rhs Right operand register name
		 * @param register_out Output register name
		 */
		Div_Sqrt_Arccos_UInt_UInt(std::string_view register_lhs, std::string_view register_rhs, std::string_view register_out)
			:register_lhs(System::get(register_lhs)),
			register_rhs(System::get(register_rhs)),
			register_out(System::get(register_out))
		{
			/* Type check */
#ifndef QRAM_Release
		if (System::type_of(register_lhs) != UnsignedInteger ||
			System::type_of(register_rhs) != UnsignedInteger ||
			System::type_of(register_out) != Rational)
			throw_invalid_input();
#endif		
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_lhs Left operand register ID
		 * @param reg_rhs Right operand register ID
		 * @param reg_out Output register ID
		 */
		Div_Sqrt_Arccos_UInt_UInt(size_t reg_lhs, size_t reg_rhs, size_t reg_out)
			:register_lhs(reg_lhs),
			register_rhs(reg_rhs),
			register_out(reg_out)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(register_lhs) != UnsignedInteger ||
				System::type_of(register_rhs) != UnsignedInteger ||
				System::type_of(register_out) != Rational)
				throw_invalid_input();
#endif		
		}

		/**
		 * @brief Apply the arithmetic operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the arithmetic operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Square-root division arccosine operation (Out-of-place)
	 * @details Implements: res ^= arccos(lhs / sqrt(rhs)) / π / 2
	 *
	 * Used to compute quantum rotation angles, common in quantum amplitude encoding.
	 *
	 * @note Unitarity: self-adjoint operator, implemented via XOR
	 * @note Data type: lhs must be SignedInteger, rhs must be UnsignedInteger, res must be Rational
	 * @note Numeric range: the result is in [0, 1), encoded as a rational
	 *
	 * @pre lhs must be of SignedInteger type
	 * @pre rhs must be of UnsignedInteger type
	 * @pre res must be of Rational type
	 * @pre |lhs| <= sqrt(rhs) (guarantees the arccos argument is within [-1,1])
	 * @pre all registers must be active
	 *
	 * @par Mathematical formula
	 * output = arccos(lhs / √rhs) / (2π)
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", SignedInteger, 4);
	 * auto rhs = System::add_register("rhs", UnsignedInteger, 4);
	 * auto res = System::add_register("res", Rational, 8);
	 * Init_Unsafe(lhs, 1);   // lhs = 1
	 * Init_Unsafe(rhs, 4);   // rhs = 4
	 * // res = arccos(1/2) / 2π = 1/6 ≈ 0.167
	 * Sqrt_Div_Arccos_Int_UInt("lhs", "rhs", "res");
	 * @endcode
	 */
	struct Sqrt_Div_Arccos_Int_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t register_lhs;

		/** @brief Right operand register ID */
		size_t register_rhs;

		/** @brief Output register ID */
		size_t register_out;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs Left operand register name
		 * @param rhs Right operand register name
		 * @param out Output register name
		 */
		Sqrt_Div_Arccos_Int_UInt(std::string_view lhs, std::string_view rhs, std::string_view out)
			:register_lhs(System::get(lhs)),
			register_rhs(System::get(rhs)),
			register_out(System::get(out))
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(register_lhs) != SignedInteger ||
				System::type_of(register_rhs) != UnsignedInteger ||
				System::type_of(register_out) != Rational)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_lhs Left operand register ID
		 * @param reg_rhs Right operand register ID
		 * @param reg_out Output register ID
		 */
		Sqrt_Div_Arccos_Int_UInt(size_t reg_lhs, size_t reg_rhs, size_t reg_out)
			:register_lhs(reg_lhs),
			register_rhs(reg_rhs),
			register_out(reg_out)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(register_lhs) != SignedInteger ||
				System::type_of(register_rhs) != UnsignedInteger ||
				System::type_of(register_out) != Rational)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the arithmetic operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the arithmetic operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};


	/**
	 * @brief Get rotation angle operation (Out-of-place)
	 * @details Computes the polar angle from lhs to rhs: res ^= atan2(rhs, lhs) / (2π)
	 *
	 * Converts the Cartesian coordinates (lhs, rhs) to a polar angle; the result is encoded in the range [0, 1).
	 *
	 * @note Unitarity: self-adjoint operator, implemented via XOR
	 * @note Data type: lhs and rhs may be SignedInteger or UnsignedInteger, res must be Rational
	 * @note Numeric range: the result is in [0, 1) (a normalized angle)
	 *
	 * @pre res must be of Rational type
	 * @pre all registers must be active
	 *
	 * @par Mathematical formula
	 * - If lhs == 0 and rhs >= 0: output = 0.25 (90°)
	 * - If lhs == 0 and rhs < 0: output = 0.75 (270°)
	 * - Otherwise: output = atan2(rhs, lhs) / (2π) normalized to [0,1)
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", SignedInteger, 4);
	 * auto rhs = System::add_register("rhs", SignedInteger, 4);
	 * auto res = System::add_register("res", Rational, 8);
	 * Init_Unsafe(lhs, 1);  // lhs = 1
	 * Init_Unsafe(rhs, 1);  // rhs = 1
	 * // res = atan2(1, 1) / 2π = 0.125 (45°)
	 * GetRotateAngle_Int_Int("lhs", "rhs", "res");
	 * @endcode
	 */
	struct GetRotateAngle_Int_Int : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t register_lhs;

		/** @brief Right operand register ID */
		size_t register_rhs;

		/** @brief Output register ID */
		size_t register_out;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_lhs Left operand register name
		 * @param reg_rhs Right operand register name
		 * @param reg_out Output register name
		 */
		GetRotateAngle_Int_Int(std::string_view reg_lhs, std::string_view reg_rhs, std::string_view reg_out)
			:register_lhs(System::get(reg_lhs)),
			register_rhs(System::get(reg_rhs)),
			register_out(System::get(reg_out))
		{
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_lhs Left operand register ID
		 * @param reg_rhs Right operand register ID
		 * @param reg_out Output register ID
		 */
		GetRotateAngle_Int_Int(size_t reg_lhs, size_t reg_rhs, size_t reg_out)
			:register_lhs(reg_lhs),
			register_rhs(reg_rhs),
			register_out(reg_out)
		{
		}

		/**
		 * @brief Apply the arithmetic operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the arithmetic operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer subtraction operation (Out-of-place)
	 * @details Implements out-of-place subtraction: res ^= lhs - rhs
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must all be UnsignedInteger
	 * @note Width and truncation: operands are zero-extended; the difference is evaluated on the
	 *       unsigned 64-bit wrapping domain,
	 *       then XORed into res after taking mod 2^res_width (docs/operators.md "Width and Truncation Convention")
	 */
	struct Sub_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Result register name
		 */
		Sub_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_))
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ Result register ID
		 */
		Sub_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_)
			: lhs(lhs_), rhs(rhs_), res(res_)
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the subtraction operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the subtraction operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer negation operation (Out-of-place)
	 * @details Implements out-of-place negation: res ^= 0 - reg
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: reg and res must both be UnsignedInteger
	 * @note Width and truncation: operands are zero-extended; negative values are evaluated on the unsigned 64-bit
	 *       wrapping domain
	 *       (equivalent to 64-bit two's complement negation), then XORed into res after mod 2^res_width
	 *       (docs/operators.md "Width and Truncation Convention")
	 */
	struct Neg_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Input register ID */
		size_t reg;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_ Input register name
		 * @param res_ Result register name
		 */
		Neg_UInt(std::string_view reg_, std::string_view res_)
			: reg(System::get(reg_)), res(System::get(res_))
		{
			if (res == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_ Input register ID
		 * @param res_ Result register ID
		 */
		Neg_UInt(size_t reg_, size_t res_)
			: reg(reg_), res(res_)
		{
			if (res == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the negation operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the negation operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Signed integer absolute value operation (Out-of-place)
	 * @details Implements out-of-place absolute value: res ^= |reg|, where reg is read with
	 *          two's complement sign extension
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: reg must be SignedInteger, res must be UnsignedInteger
	 * @note Width and truncation: reg is sign-extended to 64 bits, then the absolute value is taken; the result is
	 *       then XORed into res after taking mod 2^res_width (docs/operators.md "Width and Truncation Convention")
	 * @note Boundary behavior: when the input is the most negative value (INT64_MIN for w = 64),
	 *       -v still wraps to itself, and the output keeps the bit pattern of the most negative value
	 */
	struct Abs_SInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Input register ID */
		size_t reg;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_ Input register name
		 * @param res_ Result register name
		 */
		Abs_SInt(std::string_view reg_, std::string_view res_)
			: reg(System::get(reg_)), res(System::get(res_))
		{
			if (res == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != SignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_ Input register ID
		 * @param res_ Result register ID
		 */
		Abs_SInt(size_t reg_, size_t res_)
			: reg(reg_), res(res_)
		{
			if (res == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != SignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the absolute value operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the absolute value operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer multiplication operation (Out-of-place)
	 * @details Implements out-of-place multiplication: res ^= lhs * rhs
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must all be UnsignedInteger
	 * @note Width and truncation: operands are zero-extended; the low 64 bits of the full-precision
	 *       128-bit product are taken,
	 *       then XORed into res after taking mod 2^res_width (docs/operators.md "Width and Truncation Convention")
	 */
	struct Mul_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Result register name
		 */
		Mul_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_))
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ Result register ID
		 */
		Mul_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_)
			: lhs(lhs_), rhs(rhs_), res(res_)
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the multiplication operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the multiplication operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer division operation (Out-of-place)
	 * @details Implements out-of-place division: res ^= lhs / rhs (integer division, rounding down)
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must all be UnsignedInteger
	 * @note Total domain: when the divisor is zero the quotient is 0 and no domain exception is thrown
	 *       (reversibility requires the operator
	 *       to be a deterministic function on all basis vectors; docs/operators.md "Width and Truncation Convention");
	 *       overflow information is reported separately by dedicated flag operators
	 * @note Width and truncation: operands are zero-extended, quotient domain <= 64 bits,
	 *       then XORed into res after taking mod 2^res_width
	 */
	struct Div_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Result register name
		 */
		Div_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_))
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ Result register ID
		 */
		Div_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_)
			: lhs(lhs_), rhs(rhs_), res(res_)
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the division operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the division operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer square root operation (Out-of-place)
	 * @details Implements out-of-place square root: res ^= floor(sqrt(reg))
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: reg and res must both be UnsignedInteger
	 * @note Implementation detail: a pure integer bit-by-bit square root algorithm (isqrt_u64) with no
	 *       floating point involved,
	 *       CPU and CUDA results are bit-for-bit identical
	 * @note Width and truncation: operands are zero-extended, quotient domain <= 64 bits,
	 *       then XORed into res after taking mod 2^res_width (docs/operators.md "Width and Truncation Convention")
	 */
	struct Sqrt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Input register ID */
		size_t reg;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_ Input register name
		 * @param res_ Result register name
		 */
		Sqrt_UInt(std::string_view reg_, std::string_view res_)
			: reg(System::get(reg_)), res(System::get(res_))
		{
			if (res == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_ Input register ID
		 * @param res_ Result register ID
		 */
		Sqrt_UInt(size_t reg_, size_t res_)
			: reg(reg_), res(res_)
		{
			if (res == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the square root operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the square root operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Boolean conditional select operation (Out-of-place)
	 * @details Implements out-of-place two-way select: res ^= (cond ? lhs : rhs), cond reads bit 0
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: cond must be Boolean (width 1),
	 *       lhs, rhs, and res must all be UnsignedInteger
	 * @note Width and truncation: operands are zero-extended; the selected value is
	 *       then XORed into res after taking mod 2^res_width (docs/operators.md "Width and Truncation Convention")
	 */
	struct Select_Bool_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Condition register ID */
		size_t cond;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param cond_ Condition register name
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Result register name
		 */
		Select_Bool_UInt_UInt(std::string_view cond_, std::string_view lhs_, std::string_view rhs_, std::string_view res_)
			: cond(System::get(cond_)), lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_))
		{
			if (res == cond || res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(cond) != Boolean || System::size_of(cond) != 1)
				throw_invalid_input();
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param cond_ Condition register ID
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ Result register ID
		 */
		Select_Bool_UInt_UInt(size_t cond_, size_t lhs_, size_t rhs_, size_t res_)
			: cond(cond_), lhs(lhs_), rhs(rhs_), res(res_)
		{
			if (res == cond || res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(cond) != Boolean || System::size_of(cond) != 1)
				throw_invalid_input();
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the conditional select operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the conditional select operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer bitwise AND operation (Out-of-place)
	 * @details Implements out-of-place bitwise AND: res ^= lhs & rhs
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must all be UnsignedInteger
	 * @note Width and truncation: operands are zero-extended; the result is
	 *       then XORed into res after taking mod 2^res_width (docs/operators.md "Width and Truncation Convention")
	 */
	struct And_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Result register name
		 */
		And_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_))
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ Result register ID
		 */
		And_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_)
			: lhs(lhs_), rhs(rhs_), res(res_)
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the bitwise AND operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the bitwise AND operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer bitwise OR operation (Out-of-place)
	 * @details Implements out-of-place bitwise OR: res ^= lhs | rhs
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must all be UnsignedInteger
	 * @note Width and truncation: operands are zero-extended; the result is
	 *       then XORed into res after taking mod 2^res_width (docs/operators.md "Width and Truncation Convention")
	 */
	struct Or_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Result register name
		 */
		Or_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_))
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ Result register ID
		 */
		Or_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_)
			: lhs(lhs_), rhs(rhs_), res(res_)
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the bitwise OR operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the bitwise OR operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer bitwise XOR operation (Out-of-place)
	 * @details Implements out-of-place bitwise XOR: res ^= lhs ^ rhs
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must all be UnsignedInteger
	 * @note Width and truncation: operands are zero-extended; the result is
	 *       then XORed into res after taking mod 2^res_width (docs/operators.md "Width and Truncation Convention")
	 */
	struct Xor_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Result register ID */
		size_t res;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Result register name
		 */
		Xor_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_))
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ Result register ID
		 */
		Xor_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_)
			: lhs(lhs_), rhs(rhs_), res(res_)
		{
			if (res == lhs || res == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the bitwise XOR operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the bitwise XOR operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Signed integer less-than comparison operation (Out-of-place)
	 * @details Implements out-of-place signed comparison: flag ^= (lhs < rhs)
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs and rhs must be SignedInteger, flag must be Boolean (width 1)
	 * @note The predicate is evaluated on the full-precision domain: operands are sign-extended to 64 bits
	 *       and then compared
	 *       (docs/operators.md "Width and Truncation Convention")
	 */
	struct Less_SInt_SInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief Less-than result flag register ID */
		size_t flag_id;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param flag_ Register name of the less-than result flag
		 */
		Less_SInt_SInt(std::string_view lhs_, std::string_view rhs_, std::string_view flag_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), flag_id(System::get(flag_))
		{
			if (flag_id == lhs || flag_id == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != SignedInteger ||
				System::type_of(rhs) != SignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param flag_ Register ID of the less-than result flag
		 */
		Less_SInt_SInt(size_t lhs_, size_t rhs_, size_t flag_)
			: lhs(lhs_), rhs(rhs_), flag_id(flag_)
		{
			if (flag_id == lhs || flag_id == rhs)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != SignedInteger ||
				System::type_of(rhs) != SignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the signed less-than comparison operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the signed less-than comparison operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned addition carry flag operation (Out-of-place flag operator)
	 * @details Reports the carry-out of lhs + rhs relative to res width w: flag ^= carry_out_w(lhs + rhs)
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must be UnsignedInteger, flag must be Boolean (width 1)
	 * @note The out parameter only provides the width and its value is never read: res is only used to determine
	 *       the target width w of the carry test,
	 *       this operator does not read or write res (docs/operators.md "Width and Truncation Convention")
	 * @note The predicate is evaluated on the full-precision domain: when w = 64 it is decided by 64-bit wraparound,
	 *       when w < 64 it is decided by a + b >= 2^w
	 */
	struct Carry_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief ID of the out register providing the target width (its value is not read) */
		size_t res;

		/** @brief Carry result flag register ID */
		size_t flag_id;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Name of the out register providing the target width
		 * @param flag_ Register name of the carry result flag
		 */
		Carry_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_, std::string_view flag_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_)), flag_id(System::get(flag_))
		{
			if (flag_id == lhs || flag_id == rhs || flag_id == res)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ ID of the out register providing the target width
		 * @param flag_ Register ID of the carry result flag
		 */
		Carry_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_, size_t flag_)
			: lhs(lhs_), rhs(rhs_), res(res_), flag_id(flag_)
		{
			if (flag_id == lhs || flag_id == rhs || flag_id == res)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the carry flag operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the carry flag operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Signed addition overflow flag operation (Out-of-place flag operator)
	 * @details Reports signed overflow of lhs + rhs relative to res width w: flag ^= overflow_w(lhs + rhs)
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs and rhs must be SignedInteger, res must be UnsignedInteger,
	 *       flag must be Boolean (width 1)
	 * @note The out parameter only provides the width and its value is never read: res is only used to determine
	 *       the target width w of the overflow test,
	 *       this operator does not read or write res (docs/operators.md "Width and Truncation Convention")
	 * @note The predicate is evaluated on the full-precision domain: operands are sign-extended, truncated to w bits,
	 *       and decided by same-sign addition with the result changing sign
	 */
	struct Overflow_SInt_SInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief ID of the out register providing the target width (its value is not read) */
		size_t res;

		/** @brief Overflow result flag register ID */
		size_t flag_id;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Name of the out register providing the target width
		 * @param flag_ Register name of the overflow result flag
		 */
		Overflow_SInt_SInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_, std::string_view flag_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_)), flag_id(System::get(flag_))
		{
			if (flag_id == lhs || flag_id == rhs || flag_id == res)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != SignedInteger ||
				System::type_of(rhs) != SignedInteger ||
				System::type_of(res) != UnsignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ ID of the out register providing the target width
		 * @param flag_ Register ID of the overflow result flag
		 */
		Overflow_SInt_SInt(size_t lhs_, size_t rhs_, size_t res_, size_t flag_)
			: lhs(lhs_), rhs(rhs_), res(res_), flag_id(flag_)
		{
			if (flag_id == lhs || flag_id == rhs || flag_id == res)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != SignedInteger ||
				System::type_of(rhs) != SignedInteger ||
				System::type_of(res) != UnsignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the overflow flag operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the overflow flag operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned multiplication overflow flag operation (Out-of-place flag operator)
	 * @details Reports whether lhs * rhs exceeds the representable range of res width w:
	 *          flag ^= (lhs * rhs not contained in w bits)
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: lhs, rhs, and res must be UnsignedInteger, flag must be Boolean (width 1)
	 * @note The out parameter only provides the width and its value is never read: res is only used to determine
	 *       the target width w of the overflow test,
	 *       this operator does not read or write res (docs/operators.md "Width and Truncation Convention")
	 * @note The predicate is evaluated on the full-precision domain: the product is decomposed
	 *       into 64-bit high/low halves (equivalent to 128-bit precision),
	 *       overflow is decided when the high 64 bits are nonzero or the low 64 bits exceed the w-bit range
	 */
	struct MulOverflow_UInt_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs;

		/** @brief Right operand register ID */
		size_t rhs;

		/** @brief ID of the out register providing the target width (its value is not read) */
		size_t res;

		/** @brief Overflow result flag register ID */
		size_t flag_id;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param lhs_ Left operand register name
		 * @param rhs_ Right operand register name
		 * @param res_ Name of the out register providing the target width
		 * @param flag_ Register name of the overflow result flag
		 */
		MulOverflow_UInt_UInt(std::string_view lhs_, std::string_view rhs_, std::string_view res_, std::string_view flag_)
			: lhs(System::get(lhs_)), rhs(System::get(rhs_)), res(System::get(res_)), flag_id(System::get(flag_))
		{
			if (flag_id == lhs || flag_id == rhs || flag_id == res)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lhs_ Left operand register ID
		 * @param rhs_ Right operand register ID
		 * @param res_ ID of the out register providing the target width
		 * @param flag_ Register ID of the overflow result flag
		 */
		MulOverflow_UInt_UInt(size_t lhs_, size_t rhs_, size_t res_, size_t flag_)
			: lhs(lhs_), rhs(rhs_), res(res_), flag_id(flag_)
		{
			if (flag_id == lhs || flag_id == rhs || flag_id == res)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(lhs) != UnsignedInteger ||
				System::type_of(rhs) != UnsignedInteger ||
				System::type_of(res) != UnsignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the multiplication overflow flag operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the multiplication overflow flag operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer zero-test operation (Out-of-place flag operator)
	 * @details Implements out-of-place zero test: flag ^= (reg == 0)
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: reg must be UnsignedInteger, flag must be Boolean (width 1)
	 * @note The predicate is evaluated on the full-precision domain: operands are zero-extended and compared with 0
	 *       (docs/operators.md "Width and Truncation Convention")
	 */
	struct IsZero_UInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Input register ID */
		size_t reg;

		/** @brief Zero-test result flag register ID */
		size_t flag_id;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_ Input register name
		 * @param flag_ Register name of the zero-test result flag
		 */
		IsZero_UInt(std::string_view reg_, std::string_view flag_)
			: reg(System::get(reg_)), flag_id(System::get(flag_))
		{
			if (flag_id == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != UnsignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_ Input register ID
		 * @param flag_ Register ID of the zero-test result flag
		 */
		IsZero_UInt(size_t reg_, size_t flag_)
			: reg(reg_), flag_id(flag_)
		{
			if (flag_id == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != UnsignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the zero-test operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the zero-test operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Signed integer negative-test operation (Out-of-place flag operator)
	 * @details Implements out-of-place negative test: flag ^= (reg < 0)
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: reg must be SignedInteger, flag must be Boolean (width 1)
	 * @note The predicate is evaluated on the full-precision domain: operands are two's complement
	 *       sign-extended and compared with 0
	 *       (docs/operators.md "Width and Truncation Convention")
	 */
	struct Negative_SInt : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Input register ID */
		size_t reg;

		/** @brief Negative-test result flag register ID */
		size_t flag_id;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_ Input register name
		 * @param flag_ Register name of the negative-test result flag
		 */
		Negative_SInt(std::string_view reg_, std::string_view flag_)
			: reg(System::get(reg_)), flag_id(System::get(flag_))
		{
			if (flag_id == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != SignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_ Input register ID
		 * @param flag_ Register ID of the negative-test result flag
		 */
		Negative_SInt(size_t reg_, size_t flag_)
			: reg(reg_), flag_id(flag_)
		{
			if (flag_id == reg)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(reg) != SignedInteger ||
				System::type_of(flag_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Apply the negative-test operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the negative-test operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Arbitrary integer accumulation operation (In-place)
	 * @details Implements in-place addition: lhs += rhs, supporting signed and unsigned integers
	 *
	 * This is an in-place operation; an explicit dagger implementation is required to guarantee unitarity.
	 * dagger implementation: lhs -= rhs (mod 2^N)
	 *
	 * @note Unitarity: bijectivity is guaranteed by modular arithmetic
	 * @note Data type: lhs and rhs may be UnsignedInteger or SignedInteger
	 * @note Overflow behavior: the result wraps around modulo 2^N at the lhs register size
	 * @note Type combination: mixed types are supported (e.g. lhs is SignedInteger, rhs is UnsignedInteger)
	 * @note Read extension (width and truncation convention): rhs is extended per its declared register type --
	 *       UnsignedInteger is zero-extended and SignedInteger sign-extended; width and type are
	 *       read at execution time, not snapshotted at construction time
	 *
	 * @pre lhs and rhs must be of integer type (UnsignedInteger or SignedInteger, debug checked)
	 * @pre lhs and rhs must not be the same register (alias check, always-on)
	 * @pre all registers must be active
	 *
	 * @warning Overflow behavior of signed integers is undefined (C++ standard); use with caution
	 *
	 * @par Example
	 * @code
	 * auto lhs = System::add_register("lhs", UnsignedInteger, 4);
	 * auto rhs = System::add_register("rhs", UnsignedInteger, 4);
	 * Init_Unsafe(lhs, 8);  // lhs = 8
	 * Init_Unsafe(rhs, 6);  // rhs = 6
	 * // lhs = (8 + 6) % 16 = 14
	 * Add_AnyInt_AnyInt_InPlace("lhs", "rhs");
	 * // dagger: lhs = (14 - 6) % 16 = 8 (restores the original value)
	 * op.dag(state);
	 * @endcode
	 */
	struct Add_AnyInt_AnyInt_InPlace : BaseOperator
	{
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Left operand register ID */
		size_t lhs_id;

		/** @brief Right operand register ID */
		size_t rhs_id;

		/**
		 * @brief Constructor (name version)
		 * @param reg_lhs Left operand register name
		 * @param reg_rhs Right operand register name
		 */
		Add_AnyInt_AnyInt_InPlace(std::string_view reg_lhs, std::string_view reg_rhs)
			: lhs_id(System::get(reg_lhs)), rhs_id(System::get(reg_rhs))
		{
			if (lhs_id == rhs_id)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			const auto lhs_type = System::type_of(lhs_id);
			const auto rhs_type = System::type_of(rhs_id);
			if (lhs_type != UnsignedInteger && lhs_type != SignedInteger)
				throw_invalid_input();
			if (rhs_type != UnsignedInteger && rhs_type != SignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_lhs Left operand register ID
		 * @param reg_rhs Right operand register ID
		 */
		Add_AnyInt_AnyInt_InPlace(size_t reg_lhs, size_t reg_rhs)
			: lhs_id(reg_lhs), rhs_id(reg_rhs)
		{
			if (lhs_id == rhs_id)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			const auto lhs_type = System::type_of(lhs_id);
			const auto rhs_type = System::type_of(rhs_id);
			if (lhs_type != UnsignedInteger && lhs_type != SignedInteger)
				throw_invalid_input();
			if (rhs_type != UnsignedInteger && rhs_type != SignedInteger)
				throw_invalid_input();
#endif
		}

		ClassControllable

		/**
		 * @brief Read the right operand with extension according to the register's declared type
		 * @details UnsignedInteger is zero-extended; SignedInteger is sign-extended (two's complement bit pattern).
		 *          Width and type are read at execution time (width and truncation convention).
		 * @param s Current basis vector
		 * @param id Right operand register ID
		 * @return The extended 64-bit bit pattern
		 */
		static uint64_t _extended_rhs(const System& s, size_t id);

		/**
		 * @brief Apply the accumulate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the accumulate operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;

		/**
		 * @brief CUDA: apply the dagger operation
		 * @param state CUDA sparse state
		 */
		void dag(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Assignment operation (Out-of-place)
	 * @details Implements register copy: register_2 ^= register_1
	 *
	 * This is the standard implementation of the "copy" operation in quantum computing. Implemented
	 * via XOR; applying it twice restores the original value.
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because XOR is self-inverse
	 * @note Data type: any type, as long as the two registers have the same size
	 * @note Semantics: register_2 = register_2 ⊕ register_1
	 *           If register_2 is initially 0, the effect is register_2 = register_1
	 *
	 * @pre register_1 and register_2 must have the same size
	 * @pre all registers must be active
	 *
	 * @par Example
	 * @code
	 * auto src = System::add_register("src", UnsignedInteger, 4);
	 * auto dst = System::add_register("dst", UnsignedInteger, 4);
	 * Init_Unsafe(src, 7);  // src = 7
	 * Init_Unsafe(dst, 0);  // dst = 0
	 * // dst = 0 ⊕ 7 = 7
	 * Assign("src", "dst");
	 * // Applied again: dst = 7 ⊕ 7 = 0 (restores the original value)
	 * Assign("src", "dst");
	 * @endcode
	 */
	struct Assign : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief First register ID */
		size_t register_1;

		/** @brief Second register ID */
		size_t register_2;

		/**
		 * @brief Constructor (name version)
		 * @param reg1 First register name
		 * @param reg2 Second register name
		 */
		Assign(std::string_view reg1, std::string_view reg2)
			:register_1(System::get(reg1)), register_2(System::get(reg2))
		{
			if (register_1 == register_2)
				throw_invalid_input();
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg1 First register ID
		 * @param reg2 Second register ID
		 */
		Assign(size_t reg1, size_t reg2)
			:register_1(reg1), register_2(reg2)
		{
			if (register_1 == register_2)
				throw_invalid_input();
		}

		ClassControllable

		/**
		 * @brief Apply the assignment operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the assignment operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsigned integer comparison operation (Out-of-place)
	 * @details Compares two unsigned integers, outputting the less-than and equal flags
	 *
	 * Implementation:
	 * - compare_less_id ^= (left < right)
	 * - compare_equal_id ^= (left == right)
	 *
	 * @note Unitarity: self-adjoint operator, implemented via XOR
	 * @note Data type: left_id and right_id must be UnsignedInteger; compare_less_id and compare_equal_id
	 *       must be Boolean
	 * @note Semantics: the output flags are XORed into the result registers; if initially 0,
	 *       the comparison result is stored directly
	 *
	 * @pre left_id and right_id must be of UnsignedInteger type
	 * @pre compare_less_id and compare_equal_id must be of Boolean type (size 1)
	 * @pre all registers must be active
	 *
	 * @par Example
	 * @code
	 * auto left = System::add_register("left", UnsignedInteger, 4);
	 * auto right = System::add_register("right", UnsignedInteger, 4);
	 * auto less = System::add_register("less", Boolean, 1);
	 * auto equal = System::add_register("equal", Boolean, 1);
	 * Init_Unsafe(left, 3);
	 * Init_Unsafe(right, 5);
	 * // less = (3 < 5) = 1, equal = (3 == 5) = 0
	 * Compare_UInt_UInt("left", "right", "less", "equal");
	 * @endcode
	 */
	struct Compare_UInt_UInt : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t left_id;

		/** @brief Right operand register ID */
		size_t right_id;

		/** @brief Less-than comparison result register ID */
		size_t compare_less_id;

		/** @brief Equality comparison result register ID */
		size_t compare_equal_id;

		/**
		 * @brief Constructor (name version)
		 * @param left_register Left operand register name
		 * @param right_register Right operand register name
		 * @param compare_less Register name of the less-than result
		 * @param compare_equal Register name of the equality result
		 */
		Compare_UInt_UInt(
			std::string_view left_register,
			std::string_view right_register,
			std::string_view compare_less,
			std::string_view compare_equal)
			: left_id(System::get(left_register)), right_id(System::get(right_register)),
			compare_less_id(System::get(compare_less)), compare_equal_id(System::get(compare_equal))
		{
			if (compare_less_id == compare_equal_id ||
				compare_less_id == left_id || compare_less_id == right_id ||
				compare_equal_id == left_id || compare_equal_id == right_id)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(left_id) != UnsignedInteger ||
				System::type_of(right_id) != UnsignedInteger ||
				System::type_of(compare_less_id) != Boolean ||
				System::type_of(compare_equal_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lreg Left operand register ID
		 * @param rreg Right operand register ID
		 * @param compare_less Register ID of the less-than result
		 * @param compare_equal Register ID of the equality result
		 */
		Compare_UInt_UInt(size_t lreg, size_t rreg,
			size_t compare_less, size_t compare_equal)
			:left_id(lreg), right_id(rreg),
			compare_less_id(compare_less), compare_equal_id(compare_equal)
		{
			if (compare_less_id == compare_equal_id ||
				compare_less_id == left_id || compare_less_id == right_id ||
				compare_equal_id == left_id || compare_equal_id == right_id)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(left_id) != UnsignedInteger ||
				System::type_of(right_id) != UnsignedInteger ||
				System::type_of(compare_less_id) != Boolean ||
				System::type_of(compare_equal_id) != Boolean)
				throw_invalid_input();
#endif
		}

		ClassControllable

		/**
		 * @brief Apply the comparison operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the comparison operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Less-than comparison operation (Out-of-place)
	 * @details Compares two unsigned integers, outputting only the less-than flag
	 *
	 * Implementation: compare_less_id ^= (left < right)
	 *
	 * @note Unitarity: self-adjoint operator, implemented via XOR
	 * @note Data type: left_id and right_id must be UnsignedInteger, compare_less_id must be Boolean
	 * @note Semantics: the output flags are XORed into the result registers; if initially 0,
	 *       the comparison result is stored directly
	 *
	 * @pre left_id and right_id must be of UnsignedInteger type
	 * @pre compare_less_id must be of Boolean type (size 1)
	 * @pre all registers must be active
	 *
	 * @par Example
	 * @code
	 * auto left = System::add_register("left", UnsignedInteger, 4);
	 * auto right = System::add_register("right", UnsignedInteger, 4);
	 * auto less = System::add_register("less", Boolean, 1);
	 * Init_Unsafe(left, 3);
	 * Init_Unsafe(right, 5);
	 * // less = (3 < 5) = 1
	 * Less_UInt_UInt("left", "right", "less");
	 * @endcode
	 */
	struct Less_UInt_UInt : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t left_id;

		/** @brief Right operand register ID */
		size_t right_id;

		/** @brief Less-than result register ID */
		size_t compare_less_id;

		/**
		 * @brief Constructor (name version)
		 * @param lreg Left operand register name
		 * @param rreg Right operand register name
		 * @param compare_less Register name of the less-than result
		 */
		Less_UInt_UInt(
			std::string_view lreg,
			std::string_view rreg,
			std::string_view compare_less)
		{
			left_id = System::get(lreg);
			right_id = System::get(rreg);
			compare_less_id = System::get(compare_less);
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(left_id) != UnsignedInteger ||
				System::type_of(right_id) != UnsignedInteger ||
				System::type_of(compare_less_id) != Boolean)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param lreg Left operand register ID
		 * @param rreg Right operand register ID
		 * @param compare_less Register ID of the less-than result
		 */
		Less_UInt_UInt(size_t lreg, size_t rreg, size_t compare_less)
			: left_id(lreg), right_id(rreg), compare_less_id(compare_less)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(left_id) != UnsignedInteger ||
				System::type_of(right_id) != UnsignedInteger ||
				System::type_of(compare_less_id) != Boolean)
				throw_invalid_input();
#endif
		}

		ClassControllable

		/**
		 * @brief Apply the less-than comparison operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the less-than comparison operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief General swap operation (In-place)
	 * @details Swaps the complete values of two registers
	 *
	 * Implementation: swaps the value fields of the two registers using std::swap.
	 * This is a self-adjoint operation; applying it twice yields the identity operation.
	 *
	 * @note Unitarity: self-adjoint operator, Swap^2 = I
	 * @note Data type: any type, but the two registers must have the same size
	 * @note Implementation detail: register values are swapped directly, without XOR or arithmetic operations
	 *
	 * @pre id1 and id2 must have the same size
	 * @pre all registers must be active
	 *
	 * @par Example
	 * @code
	 * auto reg1 = System::add_register("reg1", UnsignedInteger, 4);
	 * auto reg2 = System::add_register("reg2", UnsignedInteger, 4);
	 * Init_Unsafe(reg1, 5);  // reg1 = 5
	 * Init_Unsafe(reg2, 10); // reg2 = 10
	 * // After the swap: reg1 = 10, reg2 = 5
	 * Swap_General_General("reg1", "reg2");
	 * @endcode
	 */
	struct Swap_General_General : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief First register ID */
		size_t id1;

		/** @brief Second register ID */
		size_t id2;

		/**
		 * @brief Constructor (name version)
		 * @param regname1 First register name
		 * @param regname2 Second register name
		 * @throws Throws an exception when register sizes do not match
		 */
		Swap_General_General(std::string_view regname1, std::string_view regname2)
		{
			id1 = System::get(regname1);
			id2 = System::get(regname2);
			if (id1 == id2)
				throw_invalid_input();

			/* Type check */
#ifndef QRAM_Release
			if (System::size_of(id1) != System::size_of(id2))
				throw_invalid_input();
#endif		
		}

		/**
		 * @brief Constructor (ID version)
		 * @param regname1 First register ID
		 * @param regname2 Second register ID
		 */
		Swap_General_General(size_t regname1, size_t regname2)
			: id1(regname1), id2(regname2)
		{
			if (id1 == id2)
				throw_invalid_input();
			/* Type check */
#ifndef QRAM_Release
			if (System::size_of(id1) != System::size_of(id2))
				throw_invalid_input();
#endif		
		}

		ClassControllable

		/**
		 * @brief Apply the swap operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the swap operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Get midpoint operation (Out-of-place)
	 * @details Computes the midpoint of two unsigned integers: mid ^= (left + right) / 2
	 *
	 * Commonly used for binary search and midpoint computation in quantum algorithms.
	 *
	 * @note Unitarity: self-adjoint operator, implemented via XOR
	 * @note Data type: left_id, right_id, and mid_id must all be UnsignedInteger
	 * @note Overflow behavior: the addition may overflow, but the result after division is correct
	 *       (integer division, rounding down)
	 *
	 * @pre left_id, right_id, and mid_id must be of UnsignedInteger type
	 * @pre the three registers must have the same size
	 * @pre all registers must be active
	 *
	 * @par Example
	 * @code
	 * auto left = System::add_register("left", UnsignedInteger, 4);
	 * auto right = System::add_register("right", UnsignedInteger, 4);
	 * auto mid = System::add_register("mid", UnsignedInteger, 4);
	 * Init_Unsafe(left, 0);
	 * Init_Unsafe(right, 10);
	 * // mid = (0 + 10) / 2 = 5
	 * GetMid_UInt_UInt("left", "right", "mid");
	 * @endcode
	 */
	struct GetMid_UInt_UInt : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Left operand register ID */
		size_t left_id;

		/** @brief Right operand register ID */
		size_t right_id;

		/** @brief Midpoint result register ID */
		size_t mid_id;

		/**
		 * @brief Constructor (name version)
		 * @param left_register_ Left operand register name
		 * @param right_register_ Right operand register name
		 * @param mid_register_ Midpoint result register name
		 */
		GetMid_UInt_UInt(
			std::string_view left_register_,
			std::string_view right_register_,
			std::string_view mid_register_) :
			left_id(System::get(left_register_)),
			right_id(System::get(right_register_)),
			mid_id(System::get(mid_register_))

		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(left_id) != UnsignedInteger ||
				System::type_of(right_id) != UnsignedInteger ||
				System::type_of(mid_id) != UnsignedInteger)
				throw_invalid_input();

			if (System::size_of(left_id) != System::size_of(right_id))
				throw_invalid_input();
			if (System::size_of(right_id) != System::size_of(mid_id))
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (ID version)
		 * @param left_register_ Left operand register ID
		 * @param right_register_ Right operand register ID
		 * @param mid_register_ Midpoint result register ID
		 */
		GetMid_UInt_UInt(
			size_t left_register_, size_t right_register_, size_t mid_register_)
			:left_id(left_register_), right_id(right_register_),
			mid_id(mid_register_)
		{
			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(left_id) != UnsignedInteger ||
				System::type_of(right_id) != UnsignedInteger ||
				System::type_of(mid_id) != UnsignedInteger)
				throw_invalid_input();

			if (System::size_of(left_id) != System::size_of(right_id))
				throw_invalid_input();
			if (System::size_of(right_id) != System::size_of(mid_id))
				throw_invalid_input();
#endif
		}

		ClassControllable

		/**
		 * @brief Apply the midpoint computation operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA: apply the midpoint computation operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/** @brief Generic arithmetic function type */
	using GenericArithmetic = std::function<std::vector<size_t>(const std::vector<size_t>&)>;

	/**
	 * @brief Custom arithmetic operation (Out-of-place)
	 * @details Allows the user to define a custom arithmetic function and apply it to the quantum state
	 *
	 * The user can provide a function func: vector<size_t> -> vector<size_t>,
	 * The operation passes the input register values to func and then XORs the output into the output registers.
	 *
	 * @note Unitarity: self-adjoint operator (U^† = U), because it is implemented with XOR
	 * @note Constraint: func must be a deterministic pure function, otherwise unitarity cannot be guaranteed
	 *
	 * ────────────────────────────────────────────────────────────────────────
	 * @par Design notes: the "simulator privilege" semantics of CustomArithmetic and its future evolution
	 *
	 * The current implementation is a **"simulator privilege" primitive**: on each basis state of
	 * the SparseState it directly
	 * calls a host C++ / Python callback func, XORing f(x) into the output register. This is equivalent to
	 * an idealized oracle U_f|x⟩|y⟩ = |x⟩|y ⊕ f(x)⟩, but **with no corresponding physical quantum
	 * circuit** -- real hardware cannot "read out basis state values and then call an arbitrary function". Therefore:
	 *
	 *   1. CustomArithmetic is only meaningful in a sparse state simulator;
	 *   2. Any pass that "lowers an algorithm to a physical backend / Clifford+T / OriginIR"
	 *      should not treat CustomArithmetic as a valid target primitive;
	 *   3. Upper-layer DSLs (e.g. qec_compiler/dsl_runtime/dsl) should not allow YAML composites to
	 *      reference CustomArithmetic directly, otherwise the compiled circuit cannot be physically executed.
	 *
	 * @par Future evolution: build quantum-libm + intelligent lowering strategy
	 *
	 * By analogy with the C standard library: sin/exp/log in math.h are not CPU instructions; libm implements them with
	 * a software library from a small set of ALU primitives (+, −, ×, ÷, FMA, sqrt) + range reduction + polynomial
	 * approximation + table lookup. Similarly, in the quantum world we should:
	 *
	 *   ┌──────────────────────────────────────────────────────────────────┐
	 *   │ Tier 0 — quantum arithmetic ISA:                                    │
	 *   │   QAdd / QSub / QMul / QDiv / QMod / QShift / QSwap / Toffoli /     │
	 *   │ multi-controlled X, Clifford+T basis, and other existing primitives │
	 *   ├──────────────────────────────────────────────────────────────────┤
	 *   │ Tier 1 — reciprocal / square root / modular inverse (Newton iteration over Tier 0) │
	 *   ├──────────────────────────────────────────────────────────────────┤
	 *   │ Tier 2 — elementary functions: sin/cos/exp/log via range reduction +                    │
	 *   │ polynomial + QROM coefficient table lookup + Horner (all composed from Tier 0 / Tier 1) │
	 *   ├──────────────────────────────────────────────────────────────────┤
	 *   │ Tier 3 — black-box lookup tables: QROM / SELECT-SWAP (Babbush et al. 2018)           │
	 *   │ Suitable for small discrete domains; expands into an O(2^n) multi-controlled X chain │
	 *   └──────────────────────────────────────────────────────────────────┘
	 *
	 * Under this architecture, CustomArithmetic is no longer an endpoint but a trait interface:
	 *
	 *   - It accepts the user-provided func along with metadata such as domain / precision / target backend;
	 *   - An **intelligent lowering strategy selector** chooses the landing path based on this information:
	 *       * Domain <= 2^k (k small, typically k <= 8) -> QROM expansion
	 *       * func is a polynomial / analytic smooth function -> polynomial approximation + Horner
	 *       * func is structured arithmetic such as modular exponentiation / modular multiplication
	 *         -> use dedicated primitives such as Mod_Mult directly
	 *       * Simulator-only target and func is hard to decompose -> keep as a host callback (i.e. current behavior)
	 *   - The selector need not be simple code, but a cost model considering N (bit width), ε (error budget),
	 *     T-count budget, fault tolerance, and other multi-dimensional constraints.
	 *
	 * @par Special note on Shor (mod-pow)
	 *
	 * The controlled modular exponentiation in Shor's algorithm **should not** take the
	 * CustomArithmetic route -- it is essentially a controlled-Mod_Mult chain
	 * (an in-place operation), whereas CustomArithmetic's XOR semantics only describe
	 * the out-of-place |x⟩|y⟩→|x⟩|y⊕f(x)⟩, and LUT expansion would prevent Shor
	 * from scaling to 2048 bits. The correct approach is to call
	 * Mod_Mult_UInt_ConstUInt_InPlace directly, accumulating the product over j = 0..2n-1 of
	 * a^(2^j) mod N, each factor controlled by bit j of work_reg. Shor's
	 * lowering optimizations (windowed arithmetic, Beauregard, Häner-Roetteler-
	 * Soeken, etc.) are all built on this in-place chain rather than on LUTs.
	 *
	 * @warning Until this architecture lands, please treat CustomArithmetic as
	 *          a "simulator-only oracle"; do not reference it in algorithm descriptions
	 *          targeting physical hardware (DSL composites, IR codegen passes).
	 * ────────────────────────────────────────────────────────────────────────
	 *
	 * @pre the numbers of input and output registers must be correctly specified in the constructor
	 * @pre func must be deterministic (the same input always produces the same output)
	 * @pre all registers must be active
	 *
	 * @warning The user is responsible for ensuring func does not cause information loss (i.e. func
	 *          should be a deterministic function of its input)
	 *
	 * @par Example
	 * @code
	 * // Custom function: output = input * 2
	 * GenericArithmetic double_func = [](const std::vector<size_t>& inputs) {
	 *     return std::vector<size_t>{inputs[0] * 2};
	 * };
	 *
	 * auto inp = System::add_register("inp", UnsignedInteger, 4);
	 * auto out = System::add_register("out", UnsignedInteger, 4);
	 * Init_Unsafe(inp, 7);  // inp = 7
	 *
	 * std::vector<std::string> regs = {"inp", "out"};
	 * CustomArithmetic arith(regs, 1, 1, double_func);
	 * // out = 0 ⊕ (7 * 2) = 14
	 * arith(state);
	 * @endcode
	 */
	struct CustomArithmetic : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief List of input register IDs */
		std::vector<size_t> input_ids;

		/** @brief List of output register IDs */
		std::vector<size_t> output_ids;

		/** @brief Custom arithmetic function */
		GenericArithmetic func;

		/**
		 * @brief Constructor (ID version)
		 * @param input_registers List of input register IDs
		 * @param input_size Number of input registers
		 * @param output_size Number of output registers
		 * @param func Custom arithmetic function
		 * @throws Throws an exception when input and output sizes do not match
		 */
		CustomArithmetic(const std::vector<size_t>& input_registers,
			size_t input_size, size_t output_size,
			GenericArithmetic func)
			: func(func)
		{
			if (input_ids.size() != output_ids.size())
			{
				throw std::invalid_argument("Input and output registers size does not match.");
			}
			input_ids.resize(input_size);
			output_ids.resize(output_size);
			for (size_t i = 0; i < input_size; ++i)
			{
				input_ids[i] = input_registers[i];
			}
			for (size_t i = 0; i < output_size; ++i)
			{
				output_ids[i] = input_registers[i + input_size];
			}
		}

		/**
		 * @brief Constructor (name version)
		 * @param input_registers List of input register names
		 * @param input_size Number of input registers
		 * @param output_size Number of output registers
		 * @param func Custom arithmetic function
		 */
		CustomArithmetic(const std::vector<std::string>& input_registers,
			size_t input_size, size_t output_size, GenericArithmetic func)
			: func(func)
		{
			if ((input_size + output_size) != input_registers.size())
			{
				throw std::invalid_argument("Input registers size does not match input size + output size.");
			}
			input_ids.resize(input_size);
			output_ids.resize(output_size);
			for (size_t i = 0; i < input_size; ++i)
			{
				input_ids[i] = System::get(input_registers[i]);
			}
			for (size_t i = 0; i < output_size; ++i)
			{
				output_ids[i] = System::get(input_registers[i + input_size]);
			}
		}

		ClassControllable

		/**
		 * @brief Apply the custom arithmetic operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const
		{
#ifdef SINGLE_THREAD
			for (auto& s : state)
			{
#else
#pragma omp parallel for
			for (int64_t i = 0; i < state.size(); ++i)
			{
				auto& s = state[i];
#endif
				if (ConditionNotSatisfied(s))
					continue;

				/* obtain the value of the input registers */
				std::vector<size_t> input_values(input_ids.size());
				for (size_t i = 0; i < input_ids.size(); ++i)
				{
					input_values[i] = s.GetAs(input_ids[i], uint64_t);
				}
				/* apply the custom function */
				std::vector<size_t> output_values = func(input_values);

				/* write the output reversibly via XOR, masked to the output width
				   (width and truncation convention, docs/operators.md) */
				for (size_t i = 0; i < output_ids.size(); ++i)
				{
					const size_t out_size = System::size_of(output_ids[i]);
					auto& out_reg = s.get(output_ids[i]);
					out_reg.value =
						(out_reg.value ^ output_values[i]) &
						(out_size == 64 ? ~uint64_t{0} : pow2(out_size) - 1);
				}
			}
		}
	};

	// Deprecated aliases; see docs/naming_conventions.md. Remove in the next major version.
	using AddAssign_AnyInt_AnyInt_InPlace [[deprecated("use Add_AnyInt_AnyInt_InPlace")]] = Add_AnyInt_AnyInt_InPlace;
	using Div_Sqrt_Arccos_Int_Int [[deprecated("use Div_Sqrt_Arccos_UInt_UInt")]] = Div_Sqrt_Arccos_UInt_UInt;
	using Sqrt_Div_Arccos_Int_Int [[deprecated("use Sqrt_Div_Arccos_Int_UInt")]] = Sqrt_Div_Arccos_Int_UInt;
} // namespace qram_simulator
