/**
 * @file sparse_state_simulator.cuh
 * @brief GPU-side sparse state simulator entry point
 * @details Aggregate-includes the common headers needed by the CUDA backend (GPU-version QRAM
 *          circuits, basic components, interference components, controlled rotation), and provides
 *          the GPU convenience wrapper for block-encoding extraction, cu_extract_block_encoding
 *          (using CuSparseState as the state container)
 */

#pragma once
#include "sparse_state_simulator.h"
#include "cuda/qram_circuit_qutrit.cuh"
#include "basic_components.cuh"
#include "quantum_interfere_basic.cuh"
#include "condrot.cuh"

namespace qram_simulator {
	/**
	 * @brief Extract the block-encoding matrix on a GPU sparse state (numerical verification helper)
	 * @details Calls the generic _extract_block_encoding with CuSparseState as the state container,
	 *          numerically extracting the encoded block (⟨0|_{anc}⊗I) U (|0|_{anc}⊗I)
	 * @tparam BlockEncoding Block-encoding operator type
	 * @param encA Block-encoding operator instance
	 * @param main_reg Main register name
	 * @param anc_UA Block-encoding ancilla register name
	 * @param is_full When true, extract the full unitary matrix instead of the encoded block
	 * @param is_dag When true, extract the dagger direction
	 * @return The extracted complex matrix
	 */
	template<typename BlockEncoding>
	DenseMatrix<complex_t> cu_extract_block_encoding(BlockEncoding encA, std::string main_reg, std::string anc_UA,
		bool is_full = false, bool is_dag = false)
	{
		return _extract_block_encoding<BlockEncoding, CuSparseState>(encA, main_reg, anc_UA, is_full, is_dag);
	}

	}
