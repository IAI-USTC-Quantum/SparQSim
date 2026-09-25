#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include <iostream>
#include <fstream>
#include "matrix.h"
#include <Eigen/Eigen>
#include <filesystem>

// #define OPTIMIZE_HADAM
#define OPTIMIZE_HADAM_INT
#define OPTIMIZE_ROT
// #define OPTIMIZE_PREP

/**
 * @file block_encoding.h
 * @brief 块编码实验的公共工具与编译开关
 * @details 提供复数/实数向量的格式化打印与结果向量补零扩展辅助函数，
 *          并集中定义块编码相关实现优化开关（OPTIMIZE_HADAM_INT、
 *          OPTIMIZE_ROT 等）。具体的块编码算子见 BlockEncoding/ 子目录
 *          （block_encoding_tridiagonal.h、block_encoding_via_QRAM.h、make_qram.h）
 */

namespace qram_simulator {
	/**
	 * @brief 按指定精度打印复数向量
	 * @param zvec 复数向量
	 * @param precision 小数位数
	 */
	inline void print_complex_vec(const std::vector<complex_t>& zvec, int precision)
	{
		fmt::print("\n");
		for (auto& z : zvec)
		{
			fmt::print("({:.{}f}, {:.{}f}i)  ", z.real(), precision, z.imag(), precision);
		}
		fmt::print("\n");
	}

	/**
	 * @brief 把复数结果向量补零扩展到主寄存器空间的 4 倍维度
	 * @param vec 原始结果向量
	 * @param size 主寄存器空间的维数
	 * @return 补零后的新向量
	 */
	inline std::vector<complex_t> get_output(const std::vector<complex_t> &vec, int size)
	{
		int dim = 4 * size; //size is the dimensionality of main_reg space.
		std::vector<complex_t> newVec(dim, complex_t(0.0, 0.0));
		std::copy(vec.begin(), vec.end(), newVec.begin());
		return newVec;
	}

	/**
	 * @brief 把实数结果向量补零扩展到主寄存器空间的 4 倍维度（get_output 的实数版）
	 * @param vec 原始结果向量
	 * @param size 主寄存器空间的维数
	 * @return 补零后的新向量
	 */
	inline std::vector<double> get_output(const std::vector<double> &vec, int size)
	{
		int dim = 4 * size; //size is the dimensionality of main_reg space.
		std::vector<double> newVec(dim, 0.0);
		std::copy(vec.begin(), vec.end(), newVec.begin());
		return newVec;
	}
}
