/**
 * @file basic_gates.h
 * @brief Quantum gate operation definitions
 * @details Contains implementations of basic quantum operations such as single-qubit gates, multi-qubit gates,
 *          and controlled gates, supporting standard gates such as Phase, Rotation, Pauli (X/Y/Z), S, T,
 *          RX/RY/RZ, SX, U2, and U3
 */

#pragma once
#include "sparse_state_simulator.h"
#include "matrix.h"
#include <Eigen/Eigen>

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Quantum gate base class
	 * @details Base class of all quantum gates, managing the register ID and qubit index
	 */
	struct GateBase {
		/** @brief Register ID */
		size_t id;

		/** @brief Qubit index */
		size_t digit;

		/**
		 * @brief Constructor (name + bit index)
		 * @param reg_ Register name
		 * @param digit_ Qubit index
		 * @throws Throws an exception when the bit index is out of range
		 */
		GateBase(std::string_view reg_, size_t digit_) 
			: id(System::get(reg_)), digit(digit_) 
		{
			size_t size = System::size_of(id);
			if (digit >= size)
				throw_invalid_input();
		}

		/**
		 * @brief Constructor (ID + bit index)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 */
		GateBase(size_t id_, size_t digit_) : id(id_), digit(digit_)
		{
			size_t size = System::size_of(id);
			if (digit >= size)
				throw_invalid_input();
		}

		/**
		 * @brief Constructor (name only, default bit index 0)
		 * @param reg_ Register name
		 */
		GateBase(std::string_view reg_) : GateBase(System::get(reg_), 0) {}

		/**
		 * @brief Constructor (ID only, default bit index 0)
		 * @param id_ Register ID
		 */
		GateBase(size_t id_) : GateBase(id_, 0) {}
	};

	/**
	 * @brief Phase gate
	 * @details Applies the phase rotation e^{iλ} on the specified qubit
	 */
	struct Phase_Bool : BaseOperator, GateBase {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Phase angle (radians) */
		double lambda;

		ClassControllable

		/**
		 * @brief Constructor (name + bit index + phase angle)
		 * @param reg_ Register name
		 * @param digit_ Qubit index
		 * @param lambda_ Phase angle (radians)
		 */
		Phase_Bool(std::string_view reg_, size_t digit_, double lambda_)
			: GateBase(System::get(reg_), digit_), lambda(lambda_)
		{
		}

		/**
		 * @brief Constructor (ID + bit index + phase angle)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 * @param lambda_ Phase angle (radians)
		 */
		Phase_Bool(size_t id_, size_t digit_, double lambda_)
			: GateBase(id_, digit_), lambda(lambda_)
		{
		}

		/**
		 * @brief Constructor (name + phase angle, default bit index 0)
		 * @param reg_ Register name
		 * @param lambda_ Phase angle (radians)
		 */
		Phase_Bool(std::string_view reg_, double lambda_) : Phase_Bool(reg_, 0, lambda_) {}

		/**
		 * @brief Constructor (ID + phase angle, default bit index 0)
		 * @param id_ Register ID
		 * @param lambda_ Phase angle (radians)
		 */
		Phase_Bool(size_t id_, double lambda_) : Phase_Bool(id_, 0, lambda_) {}

		/**
		 * @brief Apply the phase gate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;
	};

	/**
	 * @brief Rotation gate base class
	 * @details Implements a generic 2x2 unitary matrix rotation operation
	 */
	struct Rot_Bool : BaseOperator, GateBase {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Angle function type */
		using angle_function_t = std::function<u22_t(size_t)>;

		/** @brief Bit mask */
		uint64_t mask;

		/** @brief 2x2 rotation matrix */
		u22_t mat;

		ClassControllable

		/**
		 * @brief Constructor (name + bit index + matrix)
		 * @param reg_ Register name
		 * @param digit_ Qubit index
		 * @param mat 2x2 unitary matrix
		 */
		Rot_Bool(std::string_view reg_, size_t digit_, u22_t mat)
			: GateBase(System::get(reg_), digit_), mat(mat)
		{
			mask = pow2(digit);
		}

		/**
		 * @brief Constructor (ID + bit index + matrix)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 * @param mat 2x2 unitary matrix
		 */
		Rot_Bool(size_t id_, size_t digit_, u22_t mat)
			: GateBase(id_, digit_), mat(mat)
		{
			mask = pow2(digit);
		}

		/**
		 * @brief Constructor (name + matrix, single-bit register)
		 * @param reg_ Register name
		 * @param mat 2x2 unitary matrix
		 * @throws Throws an exception when the register size is not 1
		 */
		Rot_Bool(std::string_view reg_, u22_t mat) : Rot_Bool(reg_, 0, mat) {
			if (System::size_of(reg_) != 1) {
				throw_invalid_input();
			}
		}

		/**
		 * @brief Constructor (ID + matrix, single-bit register)
		 * @param id_ Register ID
		 * @param mat 2x2 unitary matrix
		 * @throws Throws an exception when the register size is not 1
		 */
		Rot_Bool(size_t id_, u22_t mat) : Rot_Bool(id_, 0, mat) {
			if (System::size_of(id_) != 1) {
				throw_invalid_input();
			}
		}

		/**
		 * @brief Perform the rotation over the given range
		 * @param l Left boundary
		 * @param r Right boundary
		 * @param state System state vector
		 */
		void operate(size_t l, size_t r, std::vector<System>& state) const;

		/**
		 * @brief Check whether the matrix is diagonal
		 * @param data 2x2 matrix
		 * @return Whether the matrix is diagonal
		 */
		static bool _is_diagonal(const u22_t& data);

		/**
		 * @brief Diagonal matrix operation implementation
		 * @param l Left boundary
		 * @param r Right boundary
		 * @param state System state vector
		 * @param mat 2x2 diagonal matrix
		 */
		void _operate_diagonal(size_t l, size_t r,
			std::vector<System>& state, const u22_t& mat) const;

		/**
		 * @brief Check whether the matrix is anti-diagonal
		 * @param data 2x2 matrix
		 * @return Whether the matrix is anti-diagonal
		 */
		static bool _is_off_diagonal(const u22_t& data);

		/**
		 * @brief Anti-diagonal matrix operation implementation
		 * @param l Left boundary
		 * @param r Right boundary
		 * @param state System state vector
		 * @param mat 2x2 anti-diagonal matrix
		 */
		void _operate_off_diagonal(size_t l, size_t r,
			std::vector<System>& state, const u22_t& mat) const;

		/**
		 * @brief General matrix operation implementation
		 * @param l Left boundary
		 * @param r Right boundary
		 * @param state System state vector
		 * @param mat 2x2 general unitary matrix
		 */
		void _operate_general(size_t l, size_t r,
			std::vector<System>& state, const u22_t& mat) const;

		/**
		 * @brief Paired operation (branches where both |0> and |1> exist)
		 * @param zero |0> branch index
		 * @param one |1> branch index
		 * @param state System state vector
		 */
		void operate_pair(size_t zero, size_t one, std::vector<System>& state) const;

		/**
		 * @brief Operate on the |0> branch alone
		 * @param zero |0> branch index
		 * @param state System state vector
		 */
		void operate_alone_zero(size_t zero, std::vector<System>& state) const;

		/**
		 * @brief Operate on the |1> branch alone
		 * @param one |1> branch index
		 * @param state System state vector
		 */
		void operate_alone_one(size_t one, std::vector<System>& state) const;

		/**
		 * @brief Apply the rotation gate operation
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
		 * @brief CUDA diagonal matrix operation
		 */
		void cu_operate_diagonal(CuSparseState& s, complex_t u00, complex_t u11) const;

		/**
		 * @brief CUDA anti-diagonal matrix operation
		 */
		void cu_operate_off_diagonal(CuSparseState& s, complex_t u01, complex_t u10) const;

		/**
		 * @brief CUDA general matrix operation
		 */
		void cu_operate_general(CuSparseState& s, complex_t u00, complex_t u01, complex_t u10, complex_t u11) const;

		/**
		 * @brief CUDA apply the rotation gate operation
		 * @param s CUDA sparse state
		 */
		void operator()(CuSparseState& s) const;
#endif
	};

	/**
	 * @brief X gate (Pauli-X / NOT gate)
	 * @details Flips the qubit state |0> <-> |1>
	 */
	struct X_Bool : SelfAdjointOperator, GateBase {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		ClassControllable

		/**
		 * @brief Constructor (name + bit index)
		 * @param reg_ Register name
		 * @param digit_ Qubit index
		 */
		X_Bool(std::string_view reg_, size_t digit_)
			: GateBase(System::get(reg_), digit_)
		{
		}

		/**
		 * @brief Constructor (ID + bit index)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 */
		X_Bool(size_t id_, size_t digit_)
			: GateBase(id_, digit_)
		{
		}

		/**
		 * @brief Constructor (name, default bit index 0)
		 * @param reg_ Register name
		 */
		X_Bool(std::string_view reg_) : X_Bool(reg_, 0) {}

		/**
		 * @brief Constructor (ID, default bit index 0)
		 * @param id_ Register ID
		 */
		X_Bool(size_t id_) : X_Bool(id_, 0) {}

		/**
		 * @brief Apply the X gate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the X gate operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Y gate (Pauli-Y gate)
	 * @details Rotates by an angle of π around the Y axis on the Bloch sphere
	 */
	struct Y_Bool : SelfAdjointOperator, GateBase {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;
		using GateBase::GateBase;

		ClassControllable

		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Extract the matrix representation of the Y gate
		 * @return Dense matrix of the Y gate
		 */
		DenseMatrix<complex_t> extract_matrix();
	};

	/**
	 * @brief Z gate (Pauli-Z gate)
	 * @details Phase-flip gate, applying a phase of π
	 */
	struct Z_Bool : Phase_Bool
	{
		using Phase_Bool::operator();
		using Phase_Bool::dag;
		using Phase_Bool::Phase_Bool;

// Convenience constructors for single-qubit Z gate (lambda = pi)
		Z_Bool(std::string_view reg_) : Phase_Bool(reg_, 0, pi) {}

		/**
		 * @brief Constructor (ID, default bit index 0)
		 * @param id_ Register ID
		 */
		Z_Bool(size_t id_) : Phase_Bool(id_, 0, pi) {}
//void display() const override;
	};

	/**
	 * @brief S gate
	 * @details Phase gate, applying a phase of π/2
	 */
	struct S_Bool : Phase_Bool
	{
		using Phase_Bool::operator();
		using Phase_Bool::dag;
		using Phase_Bool::Phase_Bool;

// Convenience constructors for S gate (lambda = pi/2)
		S_Bool(std::string_view reg_) : Phase_Bool(reg_, 0, pi / 2) {}

		/**
		 * @brief Constructor (ID, default bit index 0)
		 * @param id_ Register ID
		 */
		S_Bool(size_t id_) : Phase_Bool(id_, 0, pi / 2) {}
//void display() const override;
	};

	/**
	 * @brief T gate
	 * @details Phase gate, applying a phase of π/4
	 */
	struct T_Bool : Phase_Bool
	{
		using Phase_Bool::operator();
		using Phase_Bool::dag;
		using Phase_Bool::Phase_Bool;

// Convenience constructors for T gate (lambda = pi/4)
		T_Bool(std::string_view reg_) : Phase_Bool(reg_, 0, pi / 4) {}

		/**
		 * @brief Constructor (ID, default bit index 0)
		 * @param id_ Register ID
		 */
		T_Bool(size_t id_) : Phase_Bool(id_, 0, pi / 4) {}
//void display() const override;
	};

	/**
	 * @brief RX gate (rotation around the X axis)
	 * @details Rotates by the given angle around the X axis on the Bloch sphere
	 */
	struct RX_Bool : Rot_Bool
	{
		using Rot_Bool::operator();
		using Rot_Bool::dag;

		/** @brief Rotation matrix */
		u22_t mat;

		/**
		 * @brief Constructor (name + bit index + angle)
		 * @param reg_ Register name
		 * @param digit_ Qubit index
		 * @param angle_ Rotation angle
		 */
		RX_Bool(std::string_view reg_, size_t digit_, double angle_);

		/**
		 * @brief Constructor (ID + bit index + angle)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 * @param angle_ Rotation angle
		 */
		RX_Bool(size_t id_, size_t digit_, double angle_);

		/**
		 * @brief Constructor (name + angle, default bit index 0)
		 * @param reg_ Register name
		 * @param angle_ Rotation angle
		 */
		RX_Bool(std::string_view reg_, double angle_) : RX_Bool(reg_, 0, angle_) {}

		/**
		 * @brief Constructor (ID + angle, default bit index 0)
		 * @param id_ Register ID
		 * @param angle_ Rotation angle
		 */
		RX_Bool(size_t id_, double angle_) : RX_Bool(id_, 0, angle_) {}

		/**
		 * @brief Apply the RX gate operation
		 * @param state System state vector
		 */
		inline void operator()(std::vector<System>& state) const
		{
			Rot_Bool::operator()(state);
		}
	};

	/**
	 * @brief RY gate (rotation around the Y axis)
	 * @details Rotates by the given angle around the Y axis on the Bloch sphere
	 */
	struct RY_Bool : Rot_Bool
	{
		using Rot_Bool::operator();
		using Rot_Bool::dag;

		/** @brief Rotation matrix */
		u22_t mat;

		/**
		 * @brief Constructor (name + bit index + angle)
		 * @param reg Register name
		 * @param digit_ Qubit index
		 * @param angle_ Rotation angle
		 */
		RY_Bool(std::string_view reg, size_t digit_, double angle_);

		/**
		 * @brief Constructor (ID + bit index + angle)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 * @param angle_ Rotation angle
		 */
		RY_Bool(size_t id_, size_t digit_, double angle_);

		/**
		 * @brief Constructor (name + angle, default bit index 0)
		 * @param reg_ Register name
		 * @param angle_ Rotation angle
		 */
		RY_Bool(std::string_view reg_, double angle_) : RY_Bool(reg_, 0, angle_) {}

		/**
		 * @brief Constructor (ID + angle, default bit index 0)
		 * @param id_ Register ID
		 * @param angle_ Rotation angle
		 */
		RY_Bool(size_t id_, double angle_) : RY_Bool(id_, 0, angle_) {}

		/**
		 * @brief Apply the RY gate operation
		 * @param state System state vector
		 */
		inline void operator()(std::vector<System>& state) const
		{
			Rot_Bool::operator()(state);
		}
	};

	/**
	 * @brief RZ gate (rotation around the Z axis)
	 * @details Rotates by the given angle around the Z axis on the Bloch sphere
	 */
	struct RZ_Bool : BaseOperator, GateBase {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Rotation angle */
		double angle;

		ClassControllable

		/**
		 * @brief Constructor (name + bit index + angle)
		 * @param reg_ Register name
		 * @param digit_ Qubit index
		 * @param angle_ Rotation angle
		 */
		RZ_Bool(std::string_view reg_, size_t digit_, double angle_);

		/**
		 * @brief Constructor (ID + bit index + angle)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 * @param angle_ Rotation angle
		 */
		RZ_Bool(size_t id_, size_t digit_, double angle_);

		/**
		 * @brief Constructor (name + angle, default bit index 0)
		 * @param reg_ Register name
		 * @param angle_ Rotation angle
		 */
		RZ_Bool(std::string_view reg_, double angle_) : RZ_Bool(reg_, 0, angle_) {}

		/**
		 * @brief Constructor (ID + angle, default bit index 0)
		 * @param id_ Register ID
		 * @param angle_ Rotation angle
		 */
		RZ_Bool(size_t id_, double angle_) : RZ_Bool(id_, 0, angle_) {}

		/**
		 * @brief Apply the RZ gate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;
	};

	/**
	 * @brief SX gate (square root of the X gate)
	 * @details The sqrt(X) gate, half the rotation of the X gate
	 */
	struct SX_Bool : Rot_Bool
	{
		using Rot_Bool::operator();
		using Rot_Bool::dag;

		/** @brief Rotation matrix */
		u22_t mat;

		/**
		 * @brief Constructor (name + bit index)
		 * @param reg_ Register name
		 * @param digit_ Qubit index
		 */
		SX_Bool(std::string_view reg_, size_t digit_);

		/**
		 * @brief Constructor (ID + bit index)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 */
		SX_Bool(size_t id_, size_t digit_);

		/**
		 * @brief Constructor (name, default bit index 0)
		 * @param reg_ Register name
		 */
		SX_Bool(std::string_view reg_) : SX_Bool(reg_, 0) {}

		/**
		 * @brief Constructor (ID, default bit index 0)
		 * @param id_ Register ID
		 */
		SX_Bool(size_t id_) : SX_Bool(id_, 0) {}

		/**
		 * @brief Apply the SX gate operation
		 * @param state System state vector
		 */
		inline void operator()(std::vector<System>& state) const
		{
			Rot_Bool::operator()(state);
		}
	};

	/**
	 * @brief U2 gate (general single-qubit gate, 2 parameters)
	 * @details A general single-qubit gate using the two parameters phi and lambda
	 */
	struct U2_Bool : Rot_Bool
	{
		using Rot_Bool::operator();
		using Rot_Bool::dag;

		/** @brief Rotation matrix */
		u22_t mat;

		/** @brief phi parameter */
		double phi;

		/** @brief lambda parameter */
		double lambda;

		/**
		 * @brief Constructor (name + bit index + phi + lambda)
		 * @param reg_ Register name
		 * @param digit_ Qubit index
		 * @param phi phi parameter
		 * @param lambda lambda parameter
		 */
		U2_Bool(std::string_view reg_, size_t digit_, double phi, double lambda);

		/**
		 * @brief Constructor (ID + bit index + phi + lambda)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 * @param phi phi parameter
		 * @param lambda lambda parameter
		 */
		U2_Bool(size_t id_, size_t digit_, double phi, double lambda);

		/**
		 * @brief Constructor (name + phi + lambda, default bit index 0)
		 * @param reg_ Register name
		 * @param phi phi parameter
		 * @param lambda lambda parameter
		 */
		U2_Bool(std::string_view reg_, double phi, double lambda) : U2_Bool(reg_, 0, phi, lambda) {}

		/**
		 * @brief Constructor (ID + phi + lambda, default bit index 0)
		 * @param id_ Register ID
		 * @param phi phi parameter
		 * @param lambda lambda parameter
		 */
		U2_Bool(size_t id_, double phi, double lambda) : U2_Bool(id_, 0, phi, lambda) {}

		/**
		 * @brief Apply the U2 gate operation
		 * @param state System state vector
		 */
		inline void operator()(std::vector<System>& state) const
		{
			Rot_Bool::operator()(state);
		}
	};

	/**
	 * @brief U3 gate (general single-qubit gate, 3 parameters)
	 * @details The most general single-qubit gate, using the three parameters theta, phi, and lambda
	 */
	struct U3_Bool : Rot_Bool
	{
		using Rot_Bool::operator();
		using Rot_Bool::dag;

		/** @brief theta parameter */
		double theta;

		/** @brief phi parameter */
		double phi;

		/** @brief lambda parameter */
		double lambda;

		/**
		 * @brief Constructor (name + bit index + theta + phi + lambda)
		 * @param reg Register name
		 * @param digit_ Qubit index
		 * @param theta theta parameter
		 * @param phi phi parameter
		 * @param lambda lambda parameter
		 */
		U3_Bool(std::string_view reg, size_t digit_, double theta, double phi, double lambda);

		/**
		 * @brief Constructor (ID + bit index + theta + phi + lambda)
		 * @param id_ Register ID
		 * @param digit_ Qubit index
		 * @param theta theta parameter
		 * @param phi phi parameter
		 * @param lambda lambda parameter
		 */
		U3_Bool(size_t id_, size_t digit_, double theta, double phi, double lambda);

		/**
		 * @brief Constructor (name + theta + phi + lambda, default bit index 0)
		 * @param reg_ Register name
		 * @param theta theta parameter
		 * @param phi phi parameter
		 * @param lambda lambda parameter
		 */
		U3_Bool(std::string_view reg_, double theta, double phi, double lambda) : U3_Bool(reg_, 0, theta, phi, lambda) {}

		/**
		 * @brief Constructor (ID + theta + phi + lambda, default bit index 0)
		 * @param id_ Register ID
		 * @param theta theta parameter
		 * @param phi phi parameter
		 * @param lambda lambda parameter
		 */
		U3_Bool(size_t id_, double theta, double phi, double lambda) : U3_Bool(id_, 0, theta, phi, lambda) {}

		/**
		 * @brief Apply the U3 gate operation
		 * @param state System state vector
		 */
		inline void operator()(std::vector<System>& state) const
		{
			Rot_Bool::operator()(state);
		}
	};

	// Deprecated aliases kept for source compatibility; see docs/naming_conventions.md.
	// Remove in the next major version.
	using Xgate_Bool [[deprecated("use X_Bool")]] = X_Bool;
	using Ygate_Bool [[deprecated("use Y_Bool")]] = Y_Bool;
	using Zgate_Bool [[deprecated("use Z_Bool")]] = Z_Bool;
	using Sgate_Bool [[deprecated("use S_Bool")]] = S_Bool;
	using Tgate_Bool [[deprecated("use T_Bool")]] = T_Bool;
	using RXgate_Bool [[deprecated("use RX_Bool")]] = RX_Bool;
	using RYgate_Bool [[deprecated("use RY_Bool")]] = RY_Bool;
	using RZgate_Bool [[deprecated("use RZ_Bool")]] = RZ_Bool;
	using SXgate_Bool [[deprecated("use SX_Bool")]] = SX_Bool;
	using U2gate_Bool [[deprecated("use U2_Bool")]] = U2_Bool;
	using U3gate_Bool [[deprecated("use U3_Bool")]] = U3_Bool;
}
