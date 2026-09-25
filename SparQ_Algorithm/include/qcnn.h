/**
 * @file qcnn.h
 * @brief 量子卷积网络（QCNN）实验组件
 * @details 面向量子卷积/池化流程的算子与工具：角度函数版条件旋转（CondRot_P）、
 *          全局相位翻转（AllPhaseFlip）、边界映射（isay）、重索引（reindex/set_p）、
 *          幅值加载（AmplitudeLoad）等，以及 img2col/col2img、MNIST 读取等
 *          经典预处理工具。当前整个文件被 #if false 禁用（实验性代码，未纳入构建）。
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
	 * @brief 量子卷积网络组件（实验性，当前禁用）
	 */
	namespace CNN {
		/**
		 * @brief 4x4 图像与卷积核的经典卷积
		 * @param image 展平的输入图像
		 * @param kernel 展平的卷积核
		 * @return 卷积结果
		 */
		std::vector<double> convolve4x4(const std::vector<double>& image, const std::vector<double>& kernel);

		/**
		 * @brief 求不小于输入的最小 2 的幂
		 * @param input 输入值
		 * @return 2 的幂
		 */
		int findpow2(int input);

		/**
		 * @brief img2col：把图像按卷积窗口展开成列向量（经典卷积加速技巧）
		 * @param input 输入图像（展平）
		 * @param kernel_size 卷积核边长
		 * @return 展开后的列向量
		 */
		std::vector<double> img2col(std::vector<double>& input, int kernel_size);

		/**
		 * @brief col2img：img2col 的逆变换，把列向量还原成图像
		 * @param input img2col 的输出
		 * @param kernel_size 卷积核边长
		 * @param pic_size 输出图像边长
		 * @return 还原后的图像
		 */
		std::vector<double> col2img(std::vector<double>& input, int kernel_size, int pic_size);

		//khan

		/**
		 * @brief 反转整数的四个字节（MNIST 大端读取辅助）
		 * @param i 输入整数
		 * @return 字节反转后的整数
		 */
		int reverseInt(int i);

		/**
		 * @brief 读取 MNIST 格式数据文件
		 * @param name 文件路径
		 * @return 像素/标签数据
		 */
		std::vector<int> read_mnist(std::string name);

		/**
		 * @brief 打印整数向量
		 * @param v 待打印向量
		 */
		void print_vector(const std::vector<int>& v);

		/**
		 * @brief 把整数向量保存到文件
		 * @param v 待保存向量
		 * @param filename 目标文件名
		 */
		void save_vector_to_file(const std::vector<int>& v, const std::string& filename);

		/**
		 * @brief 读取指定辅助寄存器上的概率
		 * @param state 系统状态向量
		 * @param anc 辅助寄存器名称
		 * @param anc_cr 另一辅助（卷积结果）寄存器名称
		 * @return 概率值
		 */
		double khan_getProb(std::vector<System>& state, std::string anc, std::string anc_cr);

		/**
		 * @brief 角度函数版条件旋转算子
		 * @details 对 (in, out) 两位施加由角度函数 func(x, norm) 决定的
		 *          2x2 旋转；根据矩阵形态分派对角/非对角/一般三条路径以优化
		 *          稀疏态更新。支持条件控制（ClassControllable）
		 */
		struct CondRot_P {
			/** @brief 角度函数类型：输入值与归一化常数到 2x2 酉矩阵的映射 */
			using angle_function_t = std::function<u22_t(size_t, double)>;

			/** @brief 输入寄存器 ID */
			int in_id;
			/** @brief 输出寄存器 ID */
			int out_id;
			/** @brief 角度函数 */
			angle_function_t func;
			/** @brief 归一化常数 */
			double norm_c;

			ClassControllable

			/**
			 * @brief 构造函数（寄存器名称版）
			 * @param reg_in 输入寄存器名称
			 * @param reg_out 输出寄存器名称
			 * @param angle_function 角度函数
			 * @param norm 归一化常数
			 */
			CondRot_P(std::string reg_in, std::string reg_out, angle_function_t angle_function, double norm);

			/**
			 * @brief 构造函数（寄存器 ID 版）
			 * @param reg_in 输入寄存器 ID
			 * @param reg_out 输出寄存器 ID
			 * @param angle_function 角度函数
			 * @param norm 归一化常数
			 */
			CondRot_P(int reg_in, int reg_out, angle_function_t angle_function, double norm);

			/**
			 * @brief 对状态区间 [l, r) 内的基态施加旋转
			 * @param l 区间左端
			 * @param r 区间右端
			 * @param state 系统状态向量
			 */
			void operate(size_t l, size_t r, std::vector<System>& state) const;

			/**
			 * @brief 判断 2x2 矩阵是否为对角阵
			 * @param data 2x2 矩阵
			 * @return 是否对角
			 */
			static bool _is_diagonal(const u22_t& data);

			/**
			 * @brief 对角矩阵情形的快速路径：只调相位
			 * @param l 区间左端
			 * @param r 区间右端
			 * @param state 系统状态向量
			 * @param mat 对角 2x2 矩阵
			 */
			void _operate_diagonal(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief 判断 2x2 矩阵是否为纯非对角（交换型）矩阵
			 * @param data 2x2 矩阵
			 * @return 是否非对角
			 */
			static bool _is_off_diagonal(const u22_t& data);

			/**
			 * @brief 非对角矩阵情形的快速路径：只做基态配对交换
			 * @param l 区间左端
			 * @param r 区间右端
			 * @param state 系统状态向量
			 * @param mat 非对角 2x2 矩阵
			 */
			void _operate_off_diagonal(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief 一般 2x2 矩阵路径：振幅混合
			 * @param l 区间左端
			 * @param r 区间右端
			 * @param state 系统状态向量
			 * @param mat 一般 2x2 矩阵
			 */
			void _operate_general(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief 对整个状态施加条件旋转
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief 正向角度函数：由输入值构造旋转角对应的 2x2 酉矩阵
		 * @param x 输入值
		 * @param norm 归一化常数
		 * @return 2x2 酉矩阵
		 */
		u22_t conrotfunc_P(size_t x, double norm);

		/**
		 * @brief 逆向角度函数：conrotfunc_P 的逆
		 * @param x 输入值
		 * @param norm 归一化常数
		 * @return 2x2 酉矩阵
		 */
		u22_t conrotfunc_P_inv(size_t x, double norm);

		/**
		 * @brief 读取二维位置 (i, j) 对应基态的概率
		 * @param state 系统状态向量
		 * @param i 行索引
		 * @param j 列索引
		 * @return 概率值
		 */
		double getProb(std::vector<System>& state, int i, int j);

		/**
		 * @brief 对内存做边界填充（padding）
		 * @param memory 原内存
		 * @param size 目标尺寸
		 * @return 填充后的内存
		 */
		memory_t padding(memory_t memory, size_t size);

		/**
		 * @brief 全局相位翻转算子
		 * @details 对所有基态翻转相位（配合振幅放大使用）。
		 *          支持条件控制（ClassControllable）
		 */
		struct AllPhaseFlip
		{
			// std::vector<std::string> regs;

			ClassControllable

			AllPhaseFlip() {};

			/**
			 * @brief 应用全局相位翻转
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief 边界值映射算子（isay）
		 * @details 把寄存器取值落在 [low_bounder, high_bounder] 外的分支
		 *          映射为边界值，用于卷积的边界处理
		 */
		struct isay
		{
			/** @brief 下边界与上边界 */
			size_t low_bounder, high_bounder;
			/** @brief 目标寄存器名称 */
			std::string reg_to_change;

			/**
			 * @brief 构造函数
			 * @param reg_to_change 目标寄存器名称
			 * @param low_bounder 下边界
			 * @param high_bounder 上边界
			 */
			isay(std::string reg_to_change, size_t low_bounder, size_t high_bounder);

			/**
			 * @brief 应用边界映射
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief 重索引算子
		 * @details 把寄存器值整除 div 后取商，实现图像尺寸缩减（池化）类索引变换
		 */
		struct reindex
		{
			/** @brief 整除因子 */
			size_t div;
			/** @brief 目标寄存器名称 */
			std::string reg_to_change;

			/**
			 * @brief 构造函数
			 * @param reg_to_change 目标寄存器名称
			 * @param div 整除因子
			 */
			reindex(std::string reg_to_change, size_t div);

			/**
			 * @brief 应用重索引
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief 位置写入算子
		 * @details 把寄存器值改写为 (col, row, start_index) 组合出的目标索引，
		 *          用于卷积窗口位置编码
		 */
		struct set_p
		{
			/** @brief 列号、行号与起始索引 */
			size_t col, row, start_index;
			/** @brief 目标寄存器名称 */
			std::string reg_to_change;

			/**
			 * @brief 构造函数
			 * @param reg_to_change 目标寄存器名称
			 * @param col 列号
			 * @param row 行号
			 * @param start_index 起始索引
			 */
			set_p(std::string reg_to_change, size_t col, size_t row, size_t start_index);

			/**
			 * @brief 应用位置写入
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/**
		 * @brief 寄存器销毁算子
		 * @details 移除地址寄存器并清理全局注册表项
		 */
		struct killreg
		{
			/** @brief 地址寄存器 ID */
			int addr_reg;

			/**
			 * @brief 构造函数（ID 版）
			 * @param addr_reg 地址寄存器 ID
			 */
			killreg(int addr_reg)
				: addr_reg(addr_reg)
			{}

			/**
			 * @brief 构造函数（名称版）
			 * @param addr_reg 地址寄存器名称
			 */
			killreg(std::string addr_reg)
				: addr_reg(System::get(addr_reg))
			{}

			/**
			 * @brief 应用寄存器销毁
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state);

		};

		/**
		 * @brief 索引计算算子
		 * @details 由 (addr_reg_i, addr_reg_j) 二维坐标与步长 N 计算
		 *          展平后的一维索引并写入 index 寄存器
		 */
		struct indexCal
		{
			/** @brief 行坐标寄存器 ID 与列坐标寄存器 ID */
			int addr_reg_i, addr_reg_j;
			/** @brief 索引寄存器 ID */
			int index;
			/** @brief 每行列数 */
			int N;

			/**
			 * @brief 构造函数（ID 版）
			 * @param addr_reg_i_ 行坐标寄存器 ID
			 * @param addr_reg_j_ 列坐标寄存器 ID
			 * @param data_reg_as 索引寄存器 ID
			 * @param index_ 索引寄存器 ID（别名参数）
			 * @param N_ 每行列数
			 */
			indexCal(int addr_reg_i_, int addr_reg_j_, int data_reg_as, int index_, int N_);

			/**
			 * @brief 构造函数（名称版）
			 * @param addr_reg_i_ 行坐标寄存器名称
			 * @param addr_reg_j_ 列坐标寄存器名称
			 * @param index_ 索引寄存器名称
			 * @param N_ 每行列数
			 */
			indexCal(std::string addr_reg_i_, std::string addr_reg_j_, std::string index_, int N_);

			/**
			 * @brief 应用索引计算
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state);
		};

		/**
		 * @brief 幅值加载算子
		 * @details 以振幅编码方式把值 data 加载到幅值中：
		 *          每个分支前乘 sqrt(p) 以保证归一化
		 */
		struct 	AmplitudeLoad
		{
			/** @brief 数据寄存器 ID（0-p 区间） */
			int addr_reg_data;//0-p
			/** @brief 索引寄存器 ID */
			int index;//0-Api
			/** @brief 待加载数据 */
			int data;//data
			/** @brief 归一化概率（每个分支前乘 sqrt(p)） */
			int p;//为了保证归一化，在每一个分支之前要乘以sqrt(p)

			/**
			 * @brief 构造函数（ID 版）
			 * @param addr_reg_data_ 数据寄存器 ID
			 * @param index_ 索引寄存器 ID
			 * @param data_ 待加载数据
			 * @param p_ 归一化概率
			 */
			AmplitudeLoad(int addr_reg_data_, int index_, int data_, int p_);

			/**
			 * @brief 构造函数（名称版）
			 * @param addr_reg_data_ 数据寄存器名称
			 * @param index_ 索引寄存器名称
			 * @param data_ 待加载数据
			 * @param p_ 归一化概率
			 */
			AmplitudeLoad(std::string addr_reg_data_, std::string index_, std::string data_, int p_);

			/**
			 * @brief 应用幅值加载
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state);
		};

		/**
		 * @brief 计算矩阵的 Frobenius 范数
		 * @param A 输入矩阵
		 * @return Frobenius 范数
		 */
		double get_martix_F_norm(const std::vector<std::vector<double>>& A);

		/** @brief get_martix_F_norm 的自测 */
		void test_get_martix_F_norm();

		/**
		 * @brief 把二维 vector 转换为 Eigen 矩阵
		 * @param vec 二维 vector
		 * @return Eigen::MatrixXd
		 */
		Eigen::MatrixXd convertVecToEigen(const std::vector<std::vector<double>>& vec);

		/** @brief get_martix_Spectral_norm 的自测 */
		void test_get_martix_Spectral_norm();

		/**
		 * @brief 计算矩阵的谱范数（最大奇异值）
		 * @param A 输入矩阵
		 * @return 谱范数
		 */
		double get_martix_Spectral_norm(std::vector<std::vector<double>> A);

		/**
		 * @brief 向量末尾补零对齐
		 * @param pic 输入向量
		 * @return 补零后的向量
		 */
		std::vector<double> vectorappend(std::vector<double> pic);

		/**
		 * @brief 翻转卷积核（互相关与卷积互换）
		 * @param kernel 卷积核
		 * @return 翻转后的卷积核
		 */
		std::vector<double> reversekernel(std::vector<double> kernel);

		/**
		 * @brief 反向传播的边界填充
		 * @param output 输出梯度
		 * @param kernel_size 卷积核边长
		 * @return 填充后的梯度
		 */
		std::vector<double> paddingforback(std::vector<double>& output, int kernel_size);

		/**
		 * @brief 计算向量的 F 范数（Frobenius）
		 * @param input 输入向量
		 * @return 范数值
		 */
		double get_vector_F_form(std::vector<double> input);
	}
}

#endif
