/**
 * @file sparse_state_simulator.cuh
 * @brief GPU 侧稀疏态模拟器入口
 * @details 汇总引入 CUDA 后端所需的公共头（GPU 版 QRAM 电路、基础组件、
 *          干涉基组件、条件旋转），并提供块编码提取的 GPU 便捷封装
 *          cu_extract_block_encoding（以 CuSparseState 为状态容器）
 */

#pragma once
#include "sparse_state_simulator.h"
#include "cuda/qram_circuit_qutrit.cuh"
#include "basic_components.cuh"
#include "quantum_interfere_basic.cuh"
#include "condrot.cuh"

namespace qram_simulator {
	/**
	 * @brief 在 GPU 稀疏态上提取块编码矩阵（数值验证辅助）
	 * @details 以 CuSparseState 为状态容器调用通用 _extract_block_encoding，
	 *          数值提取编码块 (⟨0|_{anc}⊗I) U (|0|_{anc}⊗I)
	 * @tparam BlockEncoding 块编码算子类型
	 * @param encA 块编码算子实例
	 * @param main_reg 主寄存器名称
	 * @param anc_UA 块编码辅助寄存器名称
	 * @param is_full true 时提取完整酉矩阵而非编码块
	 * @param is_dag true 时提取 dagger 方向
	 * @return 提取得到的复数矩阵
	 */
	template<typename BlockEncoding>
	DenseMatrix<complex_t> cu_extract_block_encoding(BlockEncoding encA, std::string main_reg, std::string anc_UA,
		bool is_full = false, bool is_dag = false)
	{
		return _extract_block_encoding<BlockEncoding, CuSparseState>(encA, main_reg, anc_UA, is_full, is_dag);
	}

	}
