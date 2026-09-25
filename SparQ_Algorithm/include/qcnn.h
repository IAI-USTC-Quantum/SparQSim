/**
 * @file qcnn.h
 * @brief Quantum convolutional neural network (QCNN) experimental components
 * @details Operators and utilities for the quantum convolution/pooling pipeline: angle-function
 *          conditional rotation (CondRot_P), global phase flip (AllPhaseFlip), boundary mapping
 *          (isay), reindexing (reindex/set_p), amplitude loading (AmplitudeLoad), etc., plus
 *          classical preprocessing tools such as img2col/col2img and MNIST reading. The whole
 *          file is currently disabled via #if false (experimental code, not included in the build).
 */

#pragma once

#if false

#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include <iostream>
#include <fstream>
#include <Eigen/Eigen>

namespace qram_simulator {

	/**
	 * @namespace qram_simulator::CNN
	 * @brief Quantum convolutional neural network components (experimental, currently disabled)
	 */
	namespace CNN {
		/**
		 * @brief Classical convolution of a 4x4 image with a convolution kernel
		 * @param image Flattened input image
		 * @param kernel Flattened convolution kernel
		 * @return Convolution result
		 */
		std::vector<double> convolve4x4(const std::vector<double>& image, const std::vector<double>& kernel);

		/**
		 * @brief Smallest power of two not less than the input
		 * @param input Input value
		 * @return Power of two
		 */
		int findpow2(int input);

		/**
		 * @brief img2col: unfold the image into a column vector by convolution window (a classical
		 *        convolution acceleration technique)
		 * @param input Input image (flattened)
		 * @param kernel_size Kernel side length
		 * @return Unfolded column vector
		 */
		std::vector<double> img2col(std::vector<double>& input, int kernel_size);

		/**
		 * @brief col2img: inverse transform of img2col, restores the column vector back to an image
		 * @param input Output of img2col
		 * @param kernel_size Kernel side length
		 * @param pic_size Output image side length
		 * @return Restored image
		 */
		std::vector<double> col2img(std::vector<double>& input, int kernel_size, int pic_size);

		//khan

		/**
		 * @brief Reverse the four bytes of an integer (MNIST big-endian reading helper)
		 * @param i Input integer
		 * @return Integer with bytes reversed
		 */
		int reverseInt(int i);

		/**
		 * @brief Read an MNIST-format data file
		 * @param name File path
		 * @return Pixel/label data
		 */
		std::vector<int> read_mnist(std::string name);

		/**
		 * @brief Print an integer vector
		 * @param v Vector to print
		 */
		void print_vector(const std::vector<int>& v);

		/**
		 * @brief Save an integer vector to a file
		 * @param v Vector to save
		 * @param filename Target file name
		 */
		void save_vector_to_file(const std::vector<int>& v, const std::string& filename);

		/**
		 * @brief Read the probability on the specified ancillary registers
		 * @param state System state vector
		 * @param anc Ancillary register name
		 * @param anc_cr Name of the other ancillary (convolution result) register
		 * @return Probability value
		 */
		double khan_getProb(std::vector<System>& state, std::string anc, std::string anc_cr);

		/**
		 * @brief Angle-function conditional rotation operator
		 * @details Applies to the (in, out) pair of qubits a 2x2 rotation determined by the angle
		 *          function func(x, norm); dispatches to one of three paths (diagonal / off-diagonal /
		 *          general) according to the matrix shape to optimize sparse-state updates.
		 *          Supports conditional control (ClassControllable)
		 */
		struct CondRot_P {
			/** @brief Angle function type: maps (input value, normalization constant) to a 2x2 unitary matrix */
			using angle_function_t = std::function<u22_t(size_t, double)>;

			/** @brief Input register ID */
			int in_id;
			/** @brief Output register ID */
			int out_id;
			/** @brief Angle function */
			angle_function_t func;
			/** @brief Normalization constant */
			double norm_c;

			ClassControllable

			/**
			 * @brief Constructor (register-name version)
			 * @param reg_in Input register name
			 * @param reg_out Output register name
			 * @param angle_function Angle function
			 * @param norm Normalization constant
			 */
			CondRot_P(std::string reg_in, std::string reg_out, angle_function_t angle_function, double norm);

			/**
			 * @brief Constructor (register-ID version)
			 * @param reg_in Input register ID
			 * @param reg_out Output register ID
			 * @param angle_function Angle function
			 * @param norm Normalization constant
			 */
			CondRot_P(int reg_in, int reg_out, angle_function_t angle_function, double norm);

			/**
			 * @brief Apply the rotation to basis states within the state interval [l, r)
			 * @param l Left end of the interval
			 * @param r Right end of the interval
			 * @param state System state vector
			 */
			void operate(size_t l, size_t r, std::vector<System>& state) const;

			/**
			 * @brief Determine whether a 2x2 matrix is diagonal
			 * @param data 2x2 matrix
			 * @return Whether it is diagonal
			 */
			static bool _is_diagonal(const u22_t& data);

			/**
			 * @brief Fast path for the diagonal-matrix case: only adjusts phases
			 * @param l Left end of the interval
			 * @param r Right end of the interval
			 * @param state System state vector
			 * @param mat Diagonal 2x2 matrix
			 */
			void _operate_diagonal(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief Determine whether a 2x2 matrix is purely off-diagonal (swap-type)
			 * @param data 2x2 matrix
			 * @return Whether it is off-diagonal
			 */
			static bool _is_off_diagonal(const u22_t& data);

			/**
			 * @brief Fast path for the off-diagonal-matrix case: only swaps paired basis states
			 * @param l Left end of the interval
			 * @param r Right end of the interval
			 * @param state System state vector
			 * @param mat Off-diagonal 2x2 matrix
			 */
			void _operate_off_diagonal(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief General 2x2 matrix path: amplitude mixing
			 * @param l Left end of the interval
			 * @param r Right end of the interval
			 * @param state System state vector
			 * @param mat General 2x2 matrix
			 */
			void _operate_general(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief Apply the conditional rotation to the whole state
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief Forward angle function: builds the 2x2 unitary matrix corresponding to the rotation
		 *        angle from the input value
		 * @param x Input value
		 * @param norm Normalization constant
		 * @return 2x2 unitary matrix
		 */
		u22_t conrotfunc_P(size_t x, double norm);

		/**
		 * @brief Inverse angle function: inverse of conrotfunc_P
		 * @param x Input value
		 * @param norm Normalization constant
		 * @return 2x2 unitary matrix
		 */
		u22_t conrotfunc_P_inv(size_t x, double norm);

		/**
		 * @brief Read the probability of the basis state at 2D position (i, j)
		 * @param state System state vector
		 * @param i Row index
		 * @param j Column index
		 * @return Probability value
		 */
		double getProb(std::vector<System>& state, int i, int j);

		/**
		 * @brief Boundary-pad the memory (padding)
		 * @param memory Original memory
		 * @param size Target size
		 * @return Padded memory
		 */
		memory_t padding(memory_t memory, size_t size);

		/**
		 * @brief Global phase flip operator
		 * @details Flips the phase of all basis states (used together with amplitude amplification).
		 *          Supports conditional control (ClassControllable)
		 */
		struct AllPhaseFlip
		{
			// std::vector<std::string> regs;

			ClassControllable

			AllPhaseFlip() {};

			/**
			 * @brief Apply the global phase flip
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief Boundary value mapping operator (isay)
		 * @details Maps branches whose register value falls outside [low_bounder, high_bounder]
		 *          to the boundary values, used for boundary handling in convolution
		 */
		struct isay
		{
			/** @brief Lower and upper bounds */
			size_t low_bounder, high_bounder;
			/** @brief Target register name */
			std::string reg_to_change;

			/**
			 * @brief Constructor
			 * @param reg_to_change Target register name
			 * @param low_bounder Lower bound
			 * @param high_bounder Upper bound
			 */
			isay(std::string reg_to_change, size_t low_bounder, size_t high_bounder);

			/**
			 * @brief Apply the boundary mapping
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief Reindexing operator
		 * @details Divides the register value by div and keeps the quotient, implementing
		 *          image-size-reduction (pooling)-style index transforms
		 */
		struct reindex
		{
			/** @brief Integer division factor */
			size_t div;
			/** @brief Target register name */
			std::string reg_to_change;

			/**
			 * @brief Constructor
			 * @param reg_to_change Target register name
			 * @param div Integer division factor
			 */
			reindex(std::string reg_to_change, size_t div);

			/**
			 * @brief Apply the reindexing
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief Position write operator
		 * @details Rewrites the register value to the target index composed from (col, row, start_index),
		 *          used to encode convolution-window positions
		 */
		struct set_p
		{
			/** @brief Column number, row number, and start index */
			size_t col, row, start_index;
			/** @brief Target register name */
			std::string reg_to_change;

			/**
			 * @brief Constructor
			 * @param reg_to_change Target register name
			 * @param col Column number
			 * @param row Row number
			 * @param start_index Start index
			 */
			set_p(std::string reg_to_change, size_t col, size_t row, size_t start_index);

			/**
			 * @brief Apply the position write
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief Register destruction operator
		 * @details Removes the address register and cleans up its global registry entry
		 */
		struct killreg
		{
			/** @brief Address register ID */
			int addr_reg;

			/**
			 * @brief Constructor (ID version)
			 * @param addr_reg Address register ID
			 */
			killreg(int addr_reg)
				: addr_reg(addr_reg)
			{}

			/**
			 * @brief Constructor (name version)
			 * @param addr_reg Address register name
			 */
			killreg(std::string addr_reg)
				: addr_reg(System::get(addr_reg))
			{}

			/**
			 * @brief Apply the register destruction
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state);

		};

		/**
		 * @brief Index calculation operator
		 * @details Computes the flattened one-dimensional index from the (addr_reg_i, addr_reg_j) 2D
		 *          coordinates and stride N, and writes it into the index register
		 */
		struct indexCal
		{
			/** @brief Row-coordinate register ID and column-coordinate register ID */
			int addr_reg_i, addr_reg_j;
			/** @brief Index register ID */
			int index;
			/** @brief Number of columns per row */
			int N;

			/**
			 * @brief Constructor (ID version)
			 * @param addr_reg_i_ Row-coordinate register ID
			 * @param addr_reg_j_ Column-coordinate register ID
			 * @param data_reg_as Index register ID
			 * @param index_ Index register ID (alias parameter)
			 * @param N_ Number of columns per row
			 */
			indexCal(int addr_reg_i_, int addr_reg_j_, int data_reg_as, int index_, int N_);

			/**
			 * @brief Constructor (name version)
			 * @param addr_reg_i_ Row-coordinate register name
			 * @param addr_reg_j_ Column-coordinate register name
			 * @param index_ Index register name
			 * @param N_ Number of columns per row
			 */
			indexCal(std::string addr_reg_i_, std::string addr_reg_j_, std::string index_, int N_);

			/**
			 * @brief Apply the index calculation
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state);
		};

		/**
		 * @brief Amplitude loading operator
		 * @details Loads the value data into amplitudes via amplitude encoding:
		 *          each branch is pre-multiplied by sqrt(p) to ensure normalization
		 */
		struct 	AmplitudeLoad
		{
			/** @brief Data register ID (range 0-p) */
			int addr_reg_data;//0-p
			/** @brief Index register ID */
			int index;//0-Api
			/** @brief Data to load */
			int data;//data
			/** @brief Normalization probability (each branch is pre-multiplied by sqrt(p)) */
			int p;//To ensure normalization, sqrt(p) is multiplied before every branch

			/**
			 * @brief Constructor (ID version)
			 * @param addr_reg_data_ Data register ID
			 * @param index_ Index register ID
			 * @param data_ Data to load
			 * @param p_ Normalization probability
			 */
			AmplitudeLoad(int addr_reg_data_, int index_, int data_, int p_);

			/**
			 * @brief Constructor (name version)
			 * @param addr_reg_data_ Data register name
			 * @param index_ Index register name
			 * @param data_ Data to load
			 * @param p_ Normalization probability
			 */
			AmplitudeLoad(std::string addr_reg_data_, std::string index_, std::string data_, int p_);

			/**
			 * @brief Apply the amplitude loading
			 * @param state System state vector
			 */
			void operator()(std::vector<System>& state);
		};

		/**
		 * @brief Compute the Frobenius norm of a matrix
		 * @param A Input matrix
		 * @return Frobenius norm
		 */
		double get_martix_F_norm(const std::vector<std::vector<double>>& A);

		/** @brief Self-test for get_martix_F_norm */
		void test_get_martix_F_norm();

		/**
		 * @brief Convert a 2D vector to an Eigen matrix
		 * @param vec 2D vector
		 * @return Eigen::MatrixXd
		 */
		Eigen::MatrixXd convertVecToEigen(const std::vector<std::vector<double>>& vec);

		/** @brief Self-test for get_martix_Spectral_norm */
		void test_get_martix_Spectral_norm();

		/**
		 * @brief Compute the spectral norm of a matrix (largest singular value)
		 * @param A Input matrix
		 * @return Spectral norm
		 */
		double get_martix_Spectral_norm(std::vector<std::vector<double>> A);

		/**
		 * @brief Zero-pad the end of a vector for alignment
		 * @param pic Input vector
		 * @return Zero-padded vector
		 */
		std::vector<double> vectorappend(std::vector<double> pic);

		/**
		 * @brief Flip the convolution kernel (switches between cross-correlation and convolution)
		 * @param kernel Convolution kernel
		 * @return Flipped convolution kernel
		 */
		std::vector<double> reversekernel(std::vector<double> kernel);

		/**
		 * @brief Boundary padding for backpropagation
		 * @param output Output gradient
		 * @param kernel_size Kernel side length
		 * @return Padded gradient
		 */
		std::vector<double> paddingforback(std::vector<double>& output, int kernel_size);

		/**
		 * @brief Compute the F norm (Frobenius) of a vector
		 * @param input Input vector
		 * @return Norm value
		 */
		double get_vector_F_form(std::vector<double> input);
	}
}

#endif
