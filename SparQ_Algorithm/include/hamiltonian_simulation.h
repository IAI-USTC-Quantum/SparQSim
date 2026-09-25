/**
 * @file hamiltonian_simulation.h
 * @brief CKS 量子行走与哈密顿量模拟 / 线性系统求解的算法构件
 * @details 面向 Childs-Kothari-Somma (CKS) 型算法：以稀疏矩阵的 QRAM 紧凑存储
 *          （量化元素数据表 + 每行定长的稀疏列下标表）为基础，提供稀疏矩阵
 *          oracle（元素查询 SparseMatrixOracle1、列下标与稀疏槽位互转的量子
 *          二分查找 SparseMatrixOracle2）、状态准备算子 T、单步量子行走
 *          QuantumWalk 与多步行走管理器 QuantumWalkNSteps。行走算符
 *          W = T† · P0 · T · Swap 的幂次对应矩阵的 Chebyshev 多项式，
 *          LCU 容器按 Chebyshev 系数组合 Σ_j c_j · W^(2j+1) 逼近目标函数。
 *          与 BlockEncoding 模块同属块编码 / 哈密顿量模拟算法体系，
 *          QRAM 访问语义与 SparQ/include/qram.h 保持一致。
 */
#pragma once
#include "sparse_state_simulator.h"
#include "matrix.h"

namespace qram_simulator
{	
	namespace CKS {
		/** @brief 量子行走旋转角函数类型：由量化矩阵元素值 v 及其行列位置 (row, col) 生成 2x2 酉旋转矩阵 */
		using walk_angle_function_t = std::function<u22_t(uint64_t, size_t row, size_t col)>;

			/**
			 * @brief 生成量子行走的 2x2 旋转矩阵（矩阵元素全为非负的情形）
			 * @param mat_data_size 矩阵元素的量化位宽
			 * @param v 量化后的矩阵元素值
			 * @param row 元素所在行号（本重载不使用）
			 * @param col 元素所在列号（本重载不使用）
			 * @param mat 输出缓冲区，按 u22_t 的实虚部交错布局写入 2x2 复数矩阵
			 * @details 设 Amax = 2^mat_data_size - 1，a = v / Amax，生成旋转矩阵
			 *          [[sqrt(a), -sqrt(1-a)], [sqrt(1-a), sqrt(a)]]，
			 *          其旋转角 theta 满足 cos(theta) = sqrt(a)，
			 *          用于量子行走中以 sqrt(a) 的振幅比例编码矩阵元素。
			 */
			HOST_DEVICE	inline void _get_coef_positive_only(size_t mat_data_size, size_t v, size_t row, size_t col, double* mat)
		{
			uint64_t Amax_real = pow2(mat_data_size) - 1;
			v = v % (Amax_real + 1);
			double x = std::sqrt(v * 1.0 / Amax_real);
			double y = std::sqrt(1 - v * 1.0 / Amax_real);

			// return { x, -y,  y, x };
			// use mat index to represent the above matrix
			mat[0] = x;
			mat[1] = 0;
			mat[2] = -y;
			mat[3] = 0;
			mat[4] = y;
			mat[5] = 0;
			mat[6] = x;
			mat[7] = 0;
		}

		//u22_t _get_coef(const SparseMatrix& mat, size_t v, size_t row, size_t col);
		/**
		 * @brief 生成量子行走的 2x2 旋转矩阵（仅正元素情形），以 u22_t 返回
		 * @details 参数含义与 double* 缓冲区版本的重载一致，直接返回旋转矩阵。
		 */
		HOST_DEVICE	inline u22_t _get_coef_positive_only(size_t mat_data_size, size_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_positive_only(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;
		}

		/**
		 * @brief 生成量子行走的 2x2 旋转矩阵（允许负元素的一般情形）
		 * @param mat_data_size 矩阵元素的量化位宽
		 * @param v 量化后的矩阵元素值（按二补码解释）
		 * @param row 元素所在行号（负元素时用于确定符号约定）
		 * @param col 元素所在列号（负元素时用于确定符号约定）
		 * @param mat 输出缓冲区，按 u22_t 的实虚部交错布局写入 2x2 复数矩阵
		 * @details 设 Amax = 2^(mat_data_size-1) - 1。元素非负时与仅正情形一致，
		 *          生成 [[sqrt(a), -sqrt(1-a)], [sqrt(1-a), sqrt(a)]]（a = v/Amax）；
		 *          元素为负时对角元取 ±i·sqrt(|a|)、反对角元取 sqrt(1-|a|)，
		 *          并按 row 与 col 的大小关系选取对角元符号（row > col 取 +i，
		 *          row < col 取 -i），为 Hermitian 矩阵的共轭对称元素
		 *          提供一致的相位约定。
		 */
		HOST_DEVICE	inline void _get_coef_common(size_t mat_data_size, uint64_t v, size_t row, size_t col, double* mat)
		{
			uint64_t Amax_real = pow2(mat_data_size - 1) - 1;
			v = v % pow2(mat_data_size);
			int64_t v_real = get_complement(v, mat_data_size);
			if (v_real >= 0)
			{
				double x = std::sqrt(v_real * 1.0 / Amax_real);
				double y = std::sqrt(1 - v_real * 1.0 / Amax_real);

				// return { x, -y,  y, x };
				// use mat index to represent the above matrix
				mat[0] = x;
				mat[1] = 0;
				mat[2] = -y;
				mat[3] = 0;
				mat[4] = y;
				mat[5] = 0;
				mat[6] = x;
				mat[7] = 0;
			}
			else
			{
				double x = std::sqrt(-v_real * 1.0 / Amax_real);
				double y = std::sqrt(1 + v_real * 1.0 / Amax_real);

				if (row > col)
				{
					//return { complex_t{0, x}, complex_t{y, 0},
					//		 complex_t{y, 0}, complex_t{0, x} };
					// use mat index to represent the above matrix
					mat[0] = 0;
					mat[1] = x;
					mat[2] = y;
					mat[3] = 0;
					mat[4] = y;
					mat[5] = 0;
					mat[6] = 0;
					mat[7] = x;
				}
				else
				{
					//return { complex_t{0, -x}, complex_t{y, 0},
					//		 complex_t{y, 0}, complex_t{0, -x} };
					// use mat index to represent the above matrix
					mat[0] = 0;
					mat[1] = -x;
					mat[2] = y;
					mat[3] = 0;
					mat[4] = y;
					mat[5] = 0;
					mat[6] = 0;
					mat[7] = -x;
				}
			}
		}

		/**
		 * @brief 生成量子行走的 2x2 旋转矩阵（允许负元素的一般情形），以 u22_t 返回
		 * @details 参数含义与 double* 缓冲区版本的重载一致，直接返回旋转矩阵。
		 */
		HOST_DEVICE	inline u22_t _get_coef_common(size_t mat_data_size, uint64_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_common(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;			
		}

		/**
		 * @brief 原地计算 2x2 矩阵的共轭转置（dagger）
		 * @param mat 2x2 复数矩阵（按 u22_t 的实虚部交错布局，原地修改）
		 */
		HOST_DEVICE	inline void u22_dagger(double* mat)
		{
			double tmp;
			mat[1] = -mat[1];
			mat[3] = -mat[3];
			mat[5] = -mat[5];
			mat[7] = -mat[7];

			tmp = mat[2];
			mat[2] = mat[4];
			mat[4] = tmp;

			tmp = mat[5];
			mat[3] = mat[5];
			mat[5] = tmp;	
		}

		/**
		 * @brief 生成量子行走 2x2 旋转矩阵的逆（仅正元素情形）
		 * @details 先生成正向旋转矩阵，再对其取共轭转置（dagger）。
		 *          参数含义与正向版本一致。
		 */
		HOST_DEVICE	inline void  _get_coef_positive_only_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col, double* mat)
		{
			_get_coef_positive_only(mat_data_size, v, row, col, mat);
			u22_dagger(mat);
		}

		/**
		 * @brief 生成量子行走 2x2 旋转矩阵的逆（允许负元素的一般情形）
		 * @details 先生成正向旋转矩阵，再对其取共轭转置（dagger）。
		 *          参数含义与正向版本一致。
		 */
		HOST_DEVICE	inline void _get_coef_common_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col, double* mat)
		{
			_get_coef_common(mat_data_size, v, row, col, mat);
			u22_dagger(mat);
		}

		/**
		 * @brief 生成量子行走 2x2 旋转矩阵的逆（仅正元素情形），以 u22_t 返回
		 * @details 参数含义与 double* 缓冲区版本的重载一致，直接返回逆旋转矩阵。
		 */
		HOST_DEVICE	inline u22_t _get_coef_positive_only_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_positive_only_inv(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;
		}

		/**
		 * @brief 生成量子行走 2x2 旋转矩阵的逆（允许负元素的一般情形），以 u22_t 返回
		 * @details 参数含义与 double* 缓冲区版本的重载一致，直接返回逆旋转矩阵。
		 */
		HOST_DEVICE	inline u22_t _get_coef_common_inv(size_t mat_data_size, uint64_t v, size_t row, size_t col)
		{
			u22_t mat;
			_get_coef_common_inv(mat_data_size, v, row, col, reinterpret_cast<double*>(mat.data()));
			return mat;
		}

			/**
			 * @brief 按稀疏矩阵的符号约定生成量子行走旋转矩阵
			 * @param mat 稀疏矩阵（使用其 positive_only 与 data_size 元数据）
			 * @param v 量化后的矩阵元素值
			 * @param row 元素所在行号
			 * @param col 元素所在列号
			 * @return 2x2 酉旋转矩阵（正向）
			 * @details 矩阵仅含非负元素时走 _get_coef_positive_only 路径，
			 *          否则走允许负元素的 _get_coef_common 路径。
			 */
			inline u22_t make_qw_rotation_matrix(const SparseMatrix& mat, uint64_t v, size_t row, size_t col)
			{
				if (mat.positive_only)
					return _get_coef_positive_only(mat.data_size, v, row, col);
				return _get_coef_common(mat.data_size, v, row, col);
			}

			/**
			 * @brief 按稀疏矩阵的符号约定生成量子行走旋转矩阵的逆（dagger）
			 * @param mat 稀疏矩阵（使用其 positive_only 与 data_size 元数据）
			 * @param v 量化后的矩阵元素值
			 * @param row 元素所在行号
			 * @param col 元素所在列号
			 * @return 2x2 酉旋转矩阵的逆
			 */
			inline u22_t make_qw_rotation_matrix_inv(const SparseMatrix& mat, uint64_t v, size_t row, size_t col)
			{
				if (mat.positive_only)
					return _get_coef_positive_only_inv(mat.data_size, v, row, col);
				return _get_coef_common_inv(mat.data_size, v, row, col);
			}

			/**
			 * @brief 构造延迟求值的行走旋转角函数（正向）
			 * @param mat 稀疏矩阵（捕获其 positive_only 与 data_size）
			 * @return 以 (v, row, col) 为输入、返回 2x2 旋转矩阵的函数对象
			 */
			inline walk_angle_function_t make_func(const SparseMatrix& mat)
			{
				size_t mat_data_size = mat.data_size;
				if (mat.positive_only)
				{
					auto func = [mat_data_size] HOST_DEVICE(uint64_t v, size_t row, size_t col)
						{
							return _get_coef_positive_only(mat_data_size, v, row, col);
						};
					return func;
				}
				auto func = [mat_data_size] HOST_DEVICE(uint64_t v, size_t row, size_t col)
					{
						return _get_coef_common(mat_data_size, v, row, col);
					};
				return func;
			}

			/**
			 * @brief 构造延迟求值的行走旋转角函数（逆向 / dagger）
			 * @param mat 稀疏矩阵（捕获其 positive_only 与 data_size）
			 * @return 以 (v, row, col) 为输入、返回 2x2 逆旋转矩阵的函数对象
			 */
			inline walk_angle_function_t make_func_inv(const SparseMatrix& mat)
			{
				size_t mat_data_size = mat.data_size;
				if (mat.positive_only)
				{
					auto func = [mat_data_size] HOST_DEVICE(uint64_t v, size_t row, size_t col)
						{
							return _get_coef_positive_only_inv(mat_data_size, v, row, col);
						};
					return func;
				}
				auto func = [mat_data_size] HOST_DEVICE(uint64_t v, size_t row, size_t col)
					{
						return _get_coef_common_inv(mat_data_size, v, row, col);
					};
				return func;
			}

			// =============================================================================
			// Primitive + Composite Operators
			// These classes inherit BaseOperator or SelfAdjointOperator and directly
			// manipulate quantum state. They are composed by the flow-control classes below.
			// =============================================================================

			/**
			 * @brief 量子行走旋转角计算算子（自伴）
			 * @details 由量化矩阵元素 v 计算比率 ratio = |a_jk| / Amax（仅正元素时
			 *          Amax = 2^data_size - 1；一般情形 Amax = 2^(data_size-1) - 1，
			 *          v 按二补码解释后取绝对值），将旋转角
			 *          theta = arccos(sqrt(ratio)) / (2*pi) 量化为 Rational 定点值
			 *          并 XOR 到输出寄存器。常与 CondRot_Fixed_Bool 组合，构成广义
			 *          条件旋转 CondRot_General_Bool_QW 的两步等价实现：
			 *          先计算角度、再做固定角度旋转、最后反计算角度。
			 *          支持条件控制（ClassControllable）。
			 */
			struct GetQWRotateAngle_Int_Int_Int : SelfAdjointOperator
			{
				using SelfAdjointOperator::operator();
				using SelfAdjointOperator::dag;

				/** @brief 量化矩阵元素寄存器 ID */
				size_t data_id;
				/** @brief 行号寄存器 ID */
				size_t row_id;
				/** @brief 列号（稀疏槽位）寄存器 ID */
				size_t col_id;
				/** @brief 旋转角输出寄存器 ID（Rational 定点） */
				size_t out_id;
				/** @brief 指向稀疏矩阵（提供量化与符号约定元数据） */
				const SparseMatrix* mat;
				ClassControllable

				/**
				 * @brief 构造函数（寄存器名称版本）
				 * @param data_ 量化矩阵元素寄存器名称
				 * @param row_ 行号寄存器名称
				 * @param col_ 列号（稀疏槽位）寄存器名称
				 * @param out_ 旋转角输出寄存器名称
				 * @param mat_ 稀疏矩阵指针
				 */
				GetQWRotateAngle_Int_Int_Int(
					std::string_view data_, std::string_view row_, std::string_view col_,
					std::string_view out_, const SparseMatrix* mat_)
					: data_id(System::get(data_)), row_id(System::get(row_)),
					col_id(System::get(col_)), out_id(System::get(out_)), mat(mat_)
				{
				}

				/**
				 * @brief 构造函数（寄存器 ID 版本）
				 * @param data_ 量化矩阵元素寄存器 ID
				 * @param row_ 行号寄存器 ID
				 * @param col_ 列号（稀疏槽位）寄存器 ID
				 * @param out_ 旋转角输出寄存器 ID
				 * @param mat_ 稀疏矩阵指针
				 */
				GetQWRotateAngle_Int_Int_Int(
					size_t data_, size_t row_, size_t col_, size_t out_, const SparseMatrix* mat_)
					: data_id(data_), row_id(row_), col_id(col_), out_id(out_), mat(mat_)
				{
				}

				/**
				 * @brief 计算行走旋转角并写入输出寄存器
				 * @param state 系统状态向量
				 * @note 算子自伴：重复调用两次相互抵消。
				 */
				void operator()(std::vector<System>& state) const;
			};

		// Chebyshev approach
		/**
		 * @brief CKS 算法的 Chebyshev 多项式展开系数
		 * @details 为 LCU 组合 Σ_j c_j · W^(2j+1) 提供系数 c_j 与符号：
		 *          展开阶数 b = kappa^2 · log(kappa/eps)，截断点 j0 = sqrt(b·log(4b/eps))。
		 *          b 较大时用 erfc 渐近公式计算 c_j，b 较小时按二项分布
		 *          尾部概率精确求和；奇数 j 项取负号。
		 */
		struct ChebyshevPolynomialCoefficient
		{
			/** @brief 展开阶数参数 b = kappa^2 · log(kappa/eps) */
			size_t b;

			/**
			 * @brief 构造函数
			 * @param b_ Chebyshev 展开阶数参数
			 */
			ChebyshevPolynomialCoefficient(size_t b_)
				:b(b_)
			{ }

			// C(Big, Small) (pick Small from Big)
			// Big*...(Big-Small+1)/(Small*...1)
			/**
			 * @brief 计算按 4^b 缩放的二项式系数 C(Big, Small) / 4^b
			 * @param Big 二项式上参数
			 * @param Small 二项式下参数
			 * @return C(Big, Small) / 4^b
			 * @note 递推过程中一旦中间值超过 2^b 即提前除以 2^b，避免溢出。
			 */
			double C(size_t Big, size_t Small);

			// Given b, provide j from 0 to b-1
			/**
			 * @brief 计算 Chebyshev 展开的第 j 项系数 c_j
			 * @param j 项下标（0 到 b-1）
			 * @return 系数 c_j
			 * @details b > 100 时采用 erfc 渐近公式 c_j = 2·erfc((j+0.5)/sqrt(b))；
			 *          否则按二项分布尾部精确求和 c_j = 4 · Σ_{i=j+1}^{b} C(2b, b+i)。
			 */
			double coef(size_t j);

			// return true if - (odd)
			// return false if + (even)
			/**
			 * @brief 第 j 项的符号
			 * @param j 项下标
			 * @return j 为奇数时返回 true（取负号），偶数返回 false（取正号）
			 */
			bool sign(size_t j);

			/**
			 * @brief 第 j 项对应的量子行走步数
			 * @param j 项下标
			 * @return 行走步数 2j + 1
			 */
			size_t step(size_t j);
		};

		/* For quantum walk */
		// Note: CondRot_General_Bool_QW is kept for future specialized use but not exported to Python.
		// Current code uses GetQWRotateAngle + CondRot_Fixed_Bool instead.
		/**
		 * @brief 量子行走的广义条件旋转算子
		 * @details 依据量化矩阵元素 (v, j, k) 由行走旋转角函数生成 2x2 酉矩阵，
		 *          并作用于布尔输出寄存器：先按输出寄存器排序分组状态分支，
		 *          再按矩阵形态（对角 / 反对角 / 一般）分派到对应实现；
		 *          dag 使用逆旋转角函数。保留供未来专用路径使用，
		 *          当前主路径使用 GetQWRotateAngle + CondRot_Fixed_Bool 的
		 *          两步组合替代。
		 */
		struct CondRot_General_Bool_QW : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief 行号寄存器名称 */
			std::string j;
			/** @brief 列号（稀疏槽位）寄存器名称 */
			std::string k;
			/** @brief 输入（矩阵元素）寄存器名称 */
			std::string in_name;
			/** @brief 输出布尔寄存器名称 */
			std::string out_name;
			/** @brief 行号寄存器 ID */
			size_t j_id;
			/** @brief 列号（稀疏槽位）寄存器 ID */
			size_t k_id;
			/** @brief 输入（矩阵元素）寄存器 ID */
			size_t in_id;
			/** @brief 输出布尔寄存器 ID */
			size_t out_id;
			/** @brief 指向稀疏矩阵（提供量化与符号约定元数据） */
			const SparseMatrix* mat;

			/**
			 * @brief 构造函数
			 * @param j_ 行号寄存器名称
			 * @param k_ 列号（稀疏槽位）寄存器名称
			 * @param reg_in 输入（矩阵元素）寄存器名称
			 * @param reg_out 输出布尔寄存器名称
			 * @param mat 稀疏矩阵指针
			 */
			CondRot_General_Bool_QW(
				std::string_view j_, std::string_view k_, std::string_view reg_in, std::string_view reg_out,
				const SparseMatrix* mat
			)
				: j(j_), k(k_), in_name(reg_in), out_name(reg_out),
				in_id(System::get(reg_in)), out_id(System::get(reg_out)),
				j_id(System::get(j)), k_id(System::get(k)), mat(mat)
			{
			}

			/**
			 * @brief 对状态区间 [l, r) 内的分支执行旋转
			 * @param l 区间左边界
			 * @param r 区间右边界
			 * @param state 系统状态向量
			 * @param func 行走旋转角函数（由矩阵元素与行列位置生成 2x2 矩阵）
			 */
			void operate(size_t l, size_t r, std::vector<System>& state, walk_angle_function_t func) const;

			/**
			 * @brief 检查矩阵是否为对角矩阵
			 * @param data 2x2 矩阵
			 * @return 是否为对角矩阵
			 */
			static bool _is_diagonal(const u22_t& data);

			/**
			 * @brief 对角矩阵操作实现（不产生新分支，原地缩放振幅）
			 * @param l 区间左边界
			 * @param r 区间右边界
			 * @param state 系统状态向量
			 * @param mat 2x2 对角矩阵
			 */
			void _operate_diagonal(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief 检查矩阵是否为反对角矩阵
			 * @param data 2x2 矩阵
			 * @return 是否为反对角矩阵
			 */
			static bool _is_off_diagonal(const u22_t& data);

			/**
			 * @brief 反对角矩阵操作实现（不产生新分支，原地交换翻转布尔值）
			 * @param l 区间左边界
			 * @param r 区间右边界
			 * @param state 系统状态向量
			 * @param mat 2x2 反对角矩阵
			 */
			void _operate_off_diagonal(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief 一般 2x2 矩阵操作实现（可能创建新分支）
			 * @param l 区间左边界
			 * @param r 区间右边界
			 * @param state 系统状态向量
			 * @param mat 2x2 一般酉矩阵
			 */
			void _operate_general(size_t l, size_t r,
				std::vector<System>& state, const u22_t& mat) const;

			/**
			 * @brief 应用广义条件旋转（正向）
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;

			/**
			 * @brief 应用广义条件旋转的 dagger 操作
			 * @param state 系统状态向量
			 */
			void dag(std::vector<System>& state) const;
#ifdef USE_CUDA
			void operator()(CuSparseState& state) const;
			void dag(CuSparseState& state) const;
#endif
		};

		// quantum binary search
		/**
		 * @brief 基于 QRAM 的量子二分查找算子（自伴）
		 * @details 在 QRAM 有序存储区 [offset, offset + total_length) 内查找
		 *          与目标寄存器值相等的地址：每轮由 flag 控制后续轮次的有效性，
		 *          取区间中点地址经 QRAM 加载中值并与目标比较，命中则把中点地址
		 *          XOR 写入结果寄存器并更新 flag 终止有效查找；否则按大小关系
		 *          收缩区间。各轮的临时寄存器用 Push 保存、逆序 Pop 反计算，
		 *          保证整个操作可逆且自伴（impl_dag 直接复用 impl）。
		 */
		struct QuantumBinarySearch : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief QRAM 电路指针（提供被查找的有序内存） */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 查找区间长度 */
			size_t total_length;
			/** @brief 二分查找轮数（log2(total_length) + 1） */
			size_t max_step;

			/** @brief 查找起点偏移寄存器 ID（其值为区间左端地址） */
			size_t address_offset_id;
			/** @brief 目标值寄存器 ID */
			size_t target_id;
			/** @brief 结果寄存器 ID（命中地址以 XOR 方式写入） */
			size_t result_id;

			/**
			 * @brief 构造函数（寄存器名称版本）
			 * @param qram QRAM 电路指针
			 * @param address_offset_register 查找起点偏移寄存器名称
			 * @param total_length_ 查找区间长度
			 * @param target_register 目标值寄存器名称
			 * @param result_register 结果寄存器名称
			 */
			QuantumBinarySearch(qram_qutrit::QRAMCircuit* qram,
				std::string_view address_offset_register,
				size_t total_length_,
				std::string_view target_register,
				std::string_view result_register);

			/**
			 * @brief 构造函数（寄存器 ID 版本）
			 * @param qram QRAM 电路指针
			 * @param address_offset_register 查找起点偏移寄存器 ID
			 * @param total_length_ 查找区间长度
			 * @param target_register 目标值寄存器 ID
			 * @param result_register 结果寄存器 ID
			 */
			QuantumBinarySearch(qram_qutrit::QRAMCircuit* qram,
				size_t address_offset_register,
				size_t total_length_,
				size_t target_register,
				size_t result_register);

			/**
			 * @brief 二分查找的正向实现（同时作为 dagger 实现）
			 * @param state 系统状态向量
			 * @details 前向执行 max_step 轮查找后，逆序反计算全部临时寄存器，
			 *          整体为自伴操作。
			 */
			template<typename Ty>
			void impl(Ty& state) const {
				profiler _("QBS");
				auto flag = AddRegister("flag", Boolean, 1)(state);
				X_Bool(flag, 0)(state);

				auto compare_less = AddRegister("compare_less", Boolean, 1)(state);
				auto compare_equal = AddRegister("compare_equal", Boolean, 1)(state);
				auto left_register = AddRegister("left_register", UnsignedInteger, qram->address_size + 1)(state);
				auto right_register = AddRegister("right_register", UnsignedInteger, qram->address_size + 1)(state);
				auto mid_register = AddRegister("mid_register", UnsignedInteger, qram->address_size + 1)(state);
				auto midval_register = AddRegister("midval_register", UnsignedInteger, qram->address_size)(state);

				// int flag_id = System::get("flag");

				Assign(address_offset_id, left_register)(state);
				Add_UInt_ConstUInt(left_register, total_length, right_register)(state);

				for (size_t iteration_level = 0; iteration_level < max_step; ++iteration_level)
				{
					/* compute the mid */
					GetMid_UInt_UInt(left_register, right_register, mid_register)
						.conditioned_by_nonzeros(flag)(state);
					//(StatePrint(StatePrintDisplay::Detail))(state);

					/* load value */
					QRAMLoad(qram, mid_register, midval_register)
						.conditioned_by_nonzeros(flag)(state);
					//(StatePrint(StatePrintDisplay::Detail))(state);

					/* compare to decide the branch */
					Compare_UInt_UInt(midval_register, target_id, compare_less, compare_equal)
						.conditioned_by_nonzeros(flag)(state);

					/* if found, move the mid register to outside */
					Assign(mid_register, result_id)
						.conditioned_by_nonzeros({ compare_equal, flag })(state);

					if (iteration_level != max_step - 1)
					{
						/* update flag (unnecessary condition) */
						Assign(compare_equal, flag)(state);
						/* update the left/right register with mid register */
						Swap_General_General(left_register, mid_register)
							.conditioned_by_nonzeros({ compare_less, flag })(state);

						X_Bool(compare_less, 0)(state);

						Swap_General_General(right_register, mid_register)
							.conditioned_by_nonzeros({ compare_less, flag })(state);

						Push(mid_register, fmt::format("{}-{}", "mid_register", iteration_level))(state);
						Push(midval_register, fmt::format("{}-{}", "midval_register", iteration_level))(state);
						Push(compare_less, fmt::format("{}-{}", "compare_less", iteration_level))(state);
						Push(compare_equal, fmt::format("{}-{}", "compare_equal", iteration_level))(state);
					}
				}

				// FlipBools(result_register).conditioned_by("flag")(state);

				// todo: auto-uncompute
				// uncompute all garbage variables
				for (size_t iteration_level = max_step; iteration_level --> 0;)
				{
					if (iteration_level != max_step - 1)
					{
						(Pop(compare_equal))(state);
						(Pop(compare_less))(state);
						(Pop(midval_register))(state);
						(Pop(mid_register))(state);

						Swap_General_General(right_register, mid_register)
							.conditioned_by_nonzeros({ compare_less, flag })(state);

						X_Bool(compare_less, 0)(state);
						Swap_General_General(left_register, mid_register)
							.conditioned_by_nonzeros({ compare_less, flag })(state);

						Assign(compare_equal, flag)(state);
					}

					Compare_UInt_UInt(midval_register, target_id, compare_less, compare_equal)
						.conditioned_by_nonzeros(flag)(state);

					QRAMLoad(qram, mid_register, midval_register)
						.conditioned_by_nonzeros(flag)(state);

					GetMid_UInt_UInt(left_register, right_register, mid_register)
						.conditioned_by_nonzeros(flag)(state);
				}

				Add_UInt_ConstUInt(left_register, total_length, right_register)(state);
				Assign(address_offset_id, left_register)(state);
				X_Bool(flag, 0)(state);
				(RemoveRegister(compare_less))(state);
				(RemoveRegister(compare_equal))(state);
				(RemoveRegister(left_register))(state);
				(RemoveRegister(right_register))(state);
				(RemoveRegister(mid_register))(state);
				(RemoveRegister(midval_register))(state);
				(RemoveRegister(flag))(state);
			}
		

			/**
			 * @brief 二分查找的 dagger 实现
			 * @param state 系统状态向量
			 * @details 算子自伴，直接复用正向实现。
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const {
				impl<Ty>(state);
			}

			COMPOSITE_OPERATION
		};

		// quantum binary search
		/**
		 * @brief 量子二分查找的快速版本
		 * @details 在模拟器层面直接对各状态分支执行经典二分查找
		 *          （省去逐轮 QRAM 加载与反计算的开销），并将命中地址 XOR 写入
		 *          结果寄存器，查找语义与 QuantumBinarySearch 一致；
		 *          供 SparseMatrixOracle2 在稀疏槽位定位中使用。
		 */
		struct QuantumBinarySearch_Fast : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief QRAM 电路指针（提供被查找的有序内存） */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 查找区间长度 */
			size_t total_length;
			/** @brief 二分查找轮数（log2(total_length) + 1） */
			size_t max_step;

			/** @brief 查找起点偏移寄存器 ID（其值为区间左端地址） */
			size_t address_offset_id;
			/** @brief 目标值寄存器 ID */
			size_t target_id;
			/** @brief 结果寄存器 ID（命中地址以 XOR 方式写入） */
			size_t result_id;

			//int iteration_level;

			/**
			 * @brief 构造函数（寄存器名称版本）
			 * @param qram QRAM 电路指针
			 * @param address_offset_register 查找起点偏移寄存器名称
			 * @param total_length_ 查找区间长度
			 * @param target_register 目标值寄存器名称
			 * @param result_register 结果寄存器名称
			 */
			QuantumBinarySearch_Fast(qram_qutrit::QRAMCircuit* qram,
				std::string_view address_offset_register,
				size_t total_length_,
				std::string_view target_register,
				std::string_view result_register);

			/**
			 * @brief 构造函数（寄存器 ID 版本）
			 * @param qram QRAM 电路指针
			 * @param address_offset_register 查找起点偏移寄存器 ID
			 * @param total_length_ 查找区间长度
			 * @param target_register 目标值寄存器 ID
			 * @param result_register 结果寄存器 ID
			 */
			QuantumBinarySearch_Fast(qram_qutrit::QRAMCircuit* qram,
				size_t address_offset_register,
				size_t total_length_,
				size_t target_register,
				size_t result_register);

			/**
			 * @brief 在单个状态分支上执行经典二分查找
			 * @param offset 查找区间起始地址
			 * @param target 目标值
			 * @return 命中地址；未命中时返回 0
			 */
			size_t binary_search(size_t offset, size_t target) const;

			/**
			 * @brief 应用快速二分查找（逐分支经典计算）
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
#ifdef USE_CUDA
			/**
			 * @brief CUDA 应用快速二分查找
			 * @param state CUDA 稀疏状态
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/**
		 * @brief 行首地址计算算子（自伴）
		 * @details 执行 row_offset ^= offset + row_sz * row，即行 row 在 QRAM
		 *          行式定长存储中对应段的起始地址（每行固定 row_sz 个槽位）；
		 *          两次调用相互抵消。
		 */
		struct GetRowAddr : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief 段起始偏移寄存器 ID */
			size_t offset_id;
			/** @brief 行号寄存器 ID */
			size_t row_id;
			/** @brief 每行的槽位数 */
			size_t row_sz;
			/** @brief 行首地址输出寄存器 ID（以 XOR 方式写入） */
			size_t row_offset_id;

			/**
			 * @brief 构造函数（寄存器名称版本）
			 * @param reg_offset 段起始偏移寄存器名称
			 * @param reg_row 行号寄存器名称
			 * @param row_sz_ 每行的槽位数
			 * @param reg_row_offset 行首地址输出寄存器名称
			 */
			GetRowAddr(std::string_view reg_offset,
				std::string_view reg_row,
				size_t row_sz_,
				std::string_view reg_row_offset)
			{
				offset_id = System::get(reg_offset);
				row_id = System::get(reg_row);
				row_offset_id = System::get(reg_row_offset);
				row_sz = row_sz_;
			}

			/**
			 * @brief 构造函数（寄存器 ID 版本）
			 * @param reg_offset 段起始偏移寄存器 ID
			 * @param reg_row 行号寄存器 ID
			 * @param row_sz_ 每行的槽位数
			 * @param reg_row_offset 行首地址输出寄存器 ID
			 */
			GetRowAddr(int reg_offset,
				int reg_row,
				size_t row_sz_,
				int reg_row_offset)
			{
				offset_id = reg_offset;
				row_id = reg_row;
				row_offset_id = reg_row_offset;
				row_sz = row_sz_;
			}

			/**
			 * @brief 计算行首地址
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
#ifdef USE_CUDA
			/**
			 * @brief CUDA 计算行首地址
			 * @param state CUDA 稀疏状态
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/**
		 * @brief 矩阵元素存储地址计算算子（自伴）
		 * @details 执行 data_offset ^= offset + row_sz * row + col_sparse，
		 *          即元素（行 row 的第 col_sparse 个稀疏槽位）在 QRAM 数据表中的
		 *          地址；两次调用相互抵消。
		 */
		struct GetDataAddr : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief 数据表起始偏移寄存器 ID */
			size_t offset_id;
			/** @brief 行号寄存器 ID */
			size_t row_id;
			/** @brief 每行的槽位数 */
			size_t row_sz;
			/** @brief 稀疏槽位寄存器 ID */
			size_t col_sparse_id;
			/** @brief 元素地址输出寄存器 ID（以 XOR 方式写入） */
			size_t row_data_id;

			/**
			 * @brief 构造函数（寄存器名称版本）
			 * @param reg_offset 数据表起始偏移寄存器名称
			 * @param reg_row 行号寄存器名称
			 * @param reg_col_sparse 稀疏槽位寄存器名称
			 * @param row_sz_ 每行的槽位数
			 * @param reg_data_offset 元素地址输出寄存器名称
			 */
			GetDataAddr(std::string_view reg_offset, std::string_view reg_row,
				std::string_view reg_col_sparse, size_t row_sz_, std::string_view reg_data_offset)
			{
				offset_id = System::get(reg_offset);
				row_id = System::get(reg_row);
				col_sparse_id = System::get(reg_col_sparse);
				row_data_id = System::get(reg_data_offset);
				row_sz = row_sz_;
			}

			/**
			 * @brief 构造函数（寄存器 ID 版本）
			 * @param reg_offset 数据表起始偏移寄存器 ID
			 * @param reg_row 行号寄存器 ID
			 * @param reg_col_sparse 稀疏槽位寄存器 ID
			 * @param row_sz_ 每行的槽位数
			 * @param reg_data_offset 元素地址输出寄存器 ID
			 */
			GetDataAddr(size_t reg_offset, size_t reg_row,
				size_t reg_col_sparse, size_t row_sz_, size_t reg_data_offset)
			{
				offset_id = reg_offset;
				row_id = reg_row;
				col_sparse_id = reg_col_sparse;
				row_data_id = reg_data_offset;
				row_sz = row_sz_;
			}

			/**
			 * @brief 计算矩阵元素存储地址
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
			/**
			 * @brief CUDA 计算矩阵元素存储地址
			 * @param state CUDA 稀疏状态
			 */
			void operator()(CuSparseState& state) const;
#endif
		};

		/*
		|offset>|i>|s_j>|0>			->
		|offset>|i>|s_j>|a_{ij}>
		*/
		/**
		 * @brief 稀疏矩阵 oracle 一号：矩阵元素查询（自伴）
		 * @details 按（行 i，行内稀疏槽位 s_j）查询 QRAM 数据表，加载对应的量化
		 *          矩阵元素 a_{ij}，即 |offset>|i>|s_j>|0> -> |offset>|i>|s_j>|a_{ij}>。
		 *          元素地址由 GetDataAddr 计算（offset + row_size*i + s_j），
		 *          加载后再反计算地址寄存器，保持整体自伴。
		 */
		struct SparseMatrixOracle1 : SelfAdjointOperator
		{
			using SelfAdjointOperator::operator();
			using SelfAdjointOperator::dag;

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 数据表偏移寄存器名称 */
			std::string reg_offset;
			/** @brief 行号寄存器名称 */
			std::string reg_row;
			/** @brief 稀疏槽位寄存器名称（元素在行紧凑存储中的位置） */
			std::string reg_col_id; // position in the sparse-compact storage
			/** @brief 查询结果（量化元素）输出寄存器名称 */
			std::string reg_output;
			/** @brief 每行的槽位数 */
			size_t row_size;

			/**
			 * @brief 构造函数
			 * @param qram QRAM 电路指针
			 * @param reg_offset 数据表偏移寄存器名称
			 * @param reg_row 行号寄存器名称
			 * @param reg_col_id 稀疏槽位寄存器名称
			 * @param reg_output 查询结果输出寄存器名称
			 * @param row_size_ 每行的槽位数
			 */
			SparseMatrixOracle1(qram_qutrit::QRAMCircuit* qram,
				std::string_view reg_offset,
				std::string_view reg_row,
				std::string_view reg_col_id,
				std::string_view reg_output,
				size_t row_size_);

			/**
			 * @brief 元素查询的正向实现（同时作为 dagger 实现）
			 * @param state 系统状态向量
			 * @details 计算 data_addr = offset + row_size*i + s_j，
			 *          QRAM 加载元素到输出寄存器后再次调用 GetDataAddr
			 *          反计算地址寄存器。
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				AddRegister("data_addr", UnsignedInteger, qram->address_size)(state);
				GetDataAddr(reg_offset, reg_row, reg_col_id, row_size, "data_addr")(state);
				QRAMLoad(qram, "data_addr", reg_output)(state);
				GetDataAddr(reg_offset, reg_row, reg_col_id, row_size, "data_addr")(state);
				RemoveRegister("data_addr")(state);
			}

			/**
			 * @brief 元素查询的 dagger 实现
			 * @param state 系统状态向量
			 * @details 算子自伴，直接复用正向实现。
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				impl<Ty>(state);
			}

			COMPOSITE_OPERATION
		};

		/*
		|offset>|i>|j>|0>			->
		|offset>|i>|s_j>
		*/
		/**
		 * @brief 稀疏矩阵 oracle 二号：列下标到稀疏槽位的转换
		 * @details 通过量子二分查找，把矩阵列下标 j 转换为该元素在行紧凑存储中的
		 *          稀疏槽位 s_j，即 |offset>|i>|j> -> |offset>|i>|s_j>。
		 *          行的稀疏表首地址由 GetRowAddr 计算；查找得到的是稀疏表内的
		 *          绝对地址，需经 QRAM 加载列下标并减去行首地址还原为槽位编号。
		 */
		struct SparseMatrixOracle2 : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 稀疏表偏移寄存器名称 */
			std::string reg_sparse_offset;
			/** @brief 行号寄存器名称 */
			std::string reg_row;
			/** @brief 列号寄存器名称（矩阵中的实际列下标） */
			std::string reg_col; // the column in the matrix
			/** @brief 二分查找结果寄存器名称 */
			std::string reg_search_result;
			/** @brief 每行的槽位数 */
			size_t row_size;

			/**
			 * @brief 构造函数
			 * @param qram QRAM 电路指针
			 * @param reg_sparse_offset 稀疏表偏移寄存器名称
			 * @param reg_row_ 行号寄存器名称
			 * @param reg_col_ 列号寄存器名称
			 * @param reg_search_result_ 二分查找结果寄存器名称
			 * @param row_size 每行的槽位数
			 */
			SparseMatrixOracle2(qram_qutrit::QRAMCircuit* qram,
				std::string_view reg_sparse_offset,
				std::string_view reg_row_,
				std::string_view reg_col_,
				std::string_view reg_search_result_,
				size_t row_size);

			/**
			 * @brief 列下标到稀疏槽位转换的正向实现
			 * @param state 系统状态向量
			 * @details 依次：GetRowAddr 计算行稀疏表首地址 -> 量子二分查找
			 *          列下标所在槽位 -> QRAM 加载还原 -> 交换与减去行首地址，
			 *          使列号寄存器最终持有槽位编号 s_j；实现自带详细步骤注释。
			 */
			template<typename Ty>
			void impl(Ty& state) const
			{
				// |i>|j> -> |i>|s_j>

				// |offset>|i>|j>
				AddRegister("row_addr", UnsignedInteger, qram->address_size)(state);

				// |offset>|i>|j>|row_addr = offset + i * row_size>
				GetRowAddr(reg_sparse_offset, reg_row, row_size, "row_addr")(state);

				// |offset>|i>|j>|row_addr>|result = s_j>
				//QuantumBinarySearch(qram, "row_addr", row_size, reg_col, reg_search_result)(state);
				QuantumBinarySearch_Fast(qram, "row_addr", row_size, reg_col, reg_search_result)(state);

				// |offset>|i>|0>|row_addr>|result> 
				QRAMLoad(qram, reg_search_result, reg_col)(state);

				// |offset>|i>|result>|row_addr>|0> 
				Swap_General_General(reg_col, reg_search_result)(state);

				// |offset>|i>|s_j>|row_addr>|0> 
				Add_AnyInt_AnyInt_InPlace(reg_col, "row_addr").dag(state);

				// |offset>|i>|s_j>|0>|0> 
				GetRowAddr(reg_sparse_offset, reg_row, row_size, "row_addr")(state);

				RemoveRegister("row_addr")(state);
			}


			/**
			 * @brief 列下标到稀疏槽位转换的 dagger 实现
			 * @param state 系统状态向量
			 * @details 按正向实现的逆序执行，把稀疏槽位 s_j 还原为列下标 j。
			 */
			template<typename Ty>
			void impl_dag(Ty& state) const
			{
				AddRegister("row_addr", UnsignedInteger, qram->address_size)(state);

				// |offset>|i>|s_j>|0>|0>|row_addr>
				GetRowAddr(reg_sparse_offset, reg_row, row_size, "row_addr")(state);
				Add_AnyInt_AnyInt_InPlace(reg_col, "row_addr")(state);
				Swap_General_General(reg_col, reg_search_result)(state);
				QRAMLoad(qram, reg_search_result, reg_col)(state);
				//QuantumBinarySearch(qram, "row_addr", row_size, reg_col, reg_search_result)(state);
				QuantumBinarySearch_Fast(qram, "row_addr", row_size, reg_col, reg_search_result)(state);
				GetRowAddr(reg_sparse_offset, reg_row, row_size, "row_addr")(state);

				RemoveRegister("row_addr")(state);
			}

			COMPOSITE_OPERATION
		};

		/*
		* To compute k from j,l, out-of-place
		* Implemented by QRAM query
		*
		* |offset>|l>|z>			->
		* |offset>|l>|z + k>
		*/

		/**
		 * @brief 由稀疏槽位计算列下标（非原地，QRAM 查询实现）
		 * @details |offset>|l>|z> -> |offset>|l>|z + k>：
		 *          以稀疏表偏移加槽位 l 为地址查询 QRAM，得到对应的列下标 k
		 *          并累加（XOR）到目标寄存器，查询后反计算地址寄存器。
		 */
		struct SparseMatrixOracle2_ComputeCol : BaseOperator
		{
			using BaseOperator::operator();

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 稀疏表偏移寄存器名称 */
			std::string sparse_offset;
			/** @brief 列下标寄存器名称（查询输出） */
			std::string k; // j
			/** @brief 稀疏槽位寄存器名称 */
			std::string l; // s_j
			/** @brief 临时地址寄存器名称 */
			std::string addr_offset;
			/** @brief 每行的槽位数 */
			size_t row_size;

			/**
			 * @brief 构造函数
			 * @param qram QRAM 电路指针
			 * @param sparse_offset 稀疏表偏移寄存器名称
			 * @param k 列下标寄存器名称
			 * @param l 稀疏槽位寄存器名称
			 * @param addr_offset 临时地址寄存器名称
			 * @param row_size 每行的槽位数
			 */
			SparseMatrixOracle2_ComputeCol(qram_qutrit::QRAMCircuit* qram,
				std::string_view sparse_offset,
				std::string_view k,
				std::string_view l,
				std::string_view addr_offset,
				size_t row_size);

			/**
			 * @brief 应用列下标计算
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		/*
		* To compute l from j,k, out-of-place
		* Implemented by QBS
		*
		|offset>|k>|z>			->
		|offset>|k>|z + l>
		*/
		/**
		 * @brief 由列下标计算稀疏槽位（非原地，量子二分查找实现）
		 * @details |offset>|k>|z> -> |offset>|k>|z + l>：
		 *          在行稀疏表区间内以列下标 k 为目标做量子二分查找，
		 *          将命中的槽位地址（含表偏移）累加到 l 后再减去偏移，
		 *          还原为相对槽位编号。
		 */
		struct SparseMatrixOracle2_ComputeSparsity : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 稀疏表偏移寄存器名称 */
			std::string sparse_offset;
			/** @brief 列下标寄存器名称 */
			std::string k; // j
			/** @brief 稀疏槽位寄存器名称 */
			std::string l; // s_j
			/** @brief 每行的槽位数 */
			size_t row_size;

			/**
			 * @brief 构造函数
			 * @param qram QRAM 电路指针
			 * @param sparse_offset 稀疏表偏移寄存器名称
			 * @param k 列下标寄存器名称
			 * @param l 稀疏槽位寄存器名称
			 * @param row_size 每行的槽位数
			 */
			SparseMatrixOracle2_ComputeSparsity(
				qram_qutrit::QRAMCircuit* qram,
				std::string_view sparse_offset,
				std::string_view k,
				std::string_view l,
				size_t row_size);

			/**
			 * @brief 应用稀疏槽位计算
			 * @param state 系统状态向量
			 */
			void operator()(std::vector<System>& state) const;
		};

		// prepare from |j> to |\psi_j>
		/**
		 * @brief CKS 量子行走的状态准备算子 T
		 * @details 对行下标 j 制备该行的"平方根振幅"叠加态：
		 *          |j>|0> -> Σ_k sqrt(A_{j,s_k}) |j>|k>（k 为行内稀疏槽位）。
		 *          流程：对槽位寄存器 k 做 Hadamard 均匀叠加 -> Oracle1 加载量化
		 *          元素 d[j,k] -> Oracle2 的 dagger 把 k 由槽位映射为实际列下标 ->
		 *          GetQWRotateAngle + CondRot_Fixed_Bool 完成以 sqrt(A_{j,k}) 为
		 *          比例的条件旋转 -> 依次反计算各 oracle 恢复寄存器。
		 */
		struct T : BaseOperator
		{
			using BaseOperator::operator();
			using BaseOperator::dag;

			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 数据表偏移寄存器名称 */
			std::string reg_data_offset;
			/** @brief 稀疏表偏移寄存器名称 */
			std::string reg_sparse_offset;
			/** @brief 行号寄存器名称 */
			std::string reg_j;
			/** @brief 布尔旗标 b1 寄存器名称 */
			std::string reg_b1;
			/** @brief 槽位 / 列号寄存器名称 */
			std::string reg_k;
			/** @brief 布尔旗标 b2 寄存器名称（条件旋转目标） */
			std::string reg_b2;
			/** @brief 二分查找结果寄存器名称 */
			std::string reg_search_result;
			/** @brief 每行的非零元（槽位）数 */
			size_t nnz_col;
			/** @brief 临时数据寄存器的位宽（取地址位宽与元素位宽的较大值） */
			size_t data_size;
			/** @brief 指向稀疏矩阵 */
			const SparseMatrix* mat;

			/**
			 * @brief 构造函数
			 * @param qram_ QRAM 电路指针
			 * @param reg_data_offset_ 数据表偏移寄存器名称
			 * @param reg_sparse_offset_ 稀疏表偏移寄存器名称
			 * @param reg_j_ 行号寄存器名称
			 * @param reg_b1_ 布尔旗标 b1 寄存器名称
			 * @param reg_k_ 槽位 / 列号寄存器名称
			 * @param reg_b2_ 布尔旗标 b2 寄存器名称
			 * @param reg_search_result_ 二分查找结果寄存器名称
			 * @param nnz_col_ 每行的非零元（槽位）数
			 * @param data_size_ 临时数据寄存器的位宽
			 * @param mat_ 稀疏矩阵指针
			 */
			T(qram_qutrit::QRAMCircuit* qram_,
				std::string_view reg_data_offset_,
				std::string_view reg_sparse_offset_,
				std::string_view reg_j_, std::string_view reg_b1_,
				std::string_view reg_k_, std::string_view reg_b2_,
				std::string_view reg_search_result_,
				size_t nnz_col_, size_t data_size_,
				const SparseMatrix* mat_)
				: qram(qram_), reg_data_offset(reg_data_offset_), reg_sparse_offset(reg_sparse_offset_),
				reg_j(reg_j_), reg_b1(reg_b1_), reg_k(reg_k_), reg_b2(reg_b2_),
				reg_search_result(reg_search_result_), nnz_col(nnz_col_), data_size(data_size_),
				mat(mat_)
			{
			}

			/**
			 * @brief 状态准备 T 的正向实现
			 * @param system_states 系统状态向量
			 * @details 函数体内以逐行态注释标注了每一步寄存器变换
			 *          （Hadamard 叠加、oracle 加载 / 映射、条件旋转与反计算）。
			 */
			template<typename Ty>
			void impl(Ty& system_states) const
			{
				profiler _("T");
				AddRegister("data", UnsignedInteger, data_size)(system_states);

				// |j> -> |j>��|s_k>
				Hadamard_Int(reg_k, log2(nnz_col))(system_states);

				// |j>��|s_k> -> |j>��|s_k>|d[j,k]>
				SparseMatrixOracle1(qram, reg_data_offset,
					reg_j, reg_k, "data", nnz_col)(system_states);

				// |j>��|s_k>(a|0>+b|1>) -> |j>��|k>(a|0>+b|1>)
				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col).dag(system_states);

				// |j>��|s_k>|d[j,k]> -> |j>��|s_k>|d[j,k]>(a|0>+b|1>)
				// Two-step equivalent of CondRot_General_Bool_QW: compute angle then apply fixed rotation
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				CondRot_Fixed_Bool("rotation", reg_b2)(system_states);
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				RemoveRegister("rotation")(system_states);

				// |j>��|s_k>(a|0>+b|1>) -> |j>��|k>(a|0>+b|1>)
				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col)(system_states);

				// |j>��|s_k>|d[j,k]>(a|0>+b|1>) -> |j>��|s_k>(a|0>+b|1>)
				SparseMatrixOracle1(qram, reg_data_offset,
					reg_j, reg_k, "data", nnz_col)(system_states);
				RemoveRegister("data")(system_states);

				// |j>��|s_k>(a|0>+b|1>) -> |j>��|k>(a|0>+b|1>)
				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col).dag(system_states);

				CheckNan()(system_states);
			}

			/**
			 * @brief 状态准备 T 的 dagger 实现
			 * @param system_states 系统状态向量
			 * @details 按正向流程的逆序反计算（含逆条件旋转），
			 *          并插入 CheckNan / ClearZero / CheckNormalization 校验。
			 */
			template<typename Ty>
			void impl_dag(Ty& system_states) const
			{
				profiler _("T.dag");
				AddRegister("data", UnsignedInteger, data_size)(system_states);

				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col)(system_states);

				CheckNan()(system_states);

				SparseMatrixOracle1(qram, reg_data_offset,
					reg_j, reg_k, "data", nnz_col)(system_states);

				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col)(system_states);

				// Two-step inverse of CondRot_General_Bool_QW: self-adjoint angle + inverse fixed rotation
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				CondRot_Fixed_Bool("rotation", reg_b2).dag(system_states);
				GetQWRotateAngle_Int_Int_Int("data", reg_j, reg_k, "rotation", mat)(system_states);
				RemoveRegister("rotation")(system_states);
				ClearZero()(system_states);

				SparseMatrixOracle2(qram, reg_sparse_offset,
					reg_j, reg_k, reg_search_result, nnz_col).dag(system_states);

				CheckNormalization()(system_states);

				SparseMatrixOracle1(qram, reg_data_offset,
					reg_j, reg_k, "data", nnz_col)(system_states);

				Hadamard_Int(reg_k, log2(nnz_col))(system_states);
				ClearZero()(system_states);

				RemoveRegister("data")(system_states);
			}

			COMPOSITE_OPERATION
		};

		// =============================================================================
		// Flow-Control / Algorithm Classes
		// These classes orchestrate operators and manage state for algorithm execution.
		// They own registers and coordinate quantum operations for testing/verification.
		// =============================================================================

		/**
		 * @brief 单步量子行走算子（CKS 行走）
		 * @details 行走算符 W = T† · P0 · T · Swap 的电路实现：P0 为对行走辅助
		 *          寄存器 (b1, k, b2, k_comp) 全零态的相位翻转，
		 *          Swap 交换 (j, b1, j_comp) 与 (k, b2, k_comp) 的行列角色。
		 *          行走的谱由矩阵的本征值决定，其幂次 W^(2j+1) 的矩阵元对应
		 *          矩阵的 Chebyshev 多项式，供 LCU 容器组合逼近目标函数。
		 */
		struct QuantumWalk : BaseOperator
		{
			/** @brief 行走相关的寄存器名称（j/b1/k/b2/j_comp/k_comp 及数据、稀疏偏移） */
			std::string j, b1, k, b2, j_comp, k_comp, data_offset, sparse_offset;
			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 稀疏矩阵副本 */
			SparseMatrix mat;

			/**
			 * @brief 构造函数
			 * @param qram_ QRAM 电路指针
			 * @param j_ 行号寄存器名称
			 * @param b1_ 布尔旗标 b1 寄存器名称
			 * @param k_ 列号寄存器名称
			 * @param b2_ 布尔旗标 b2 寄存器名称
			 * @param j_comp_ j 侧辅助寄存器名称
			 * @param k_comp_ k 侧辅助寄存器名称
			 * @param data_offset_ 数据表偏移寄存器名称
			 * @param sparse_offset_ 稀疏表偏移寄存器名称
			 * @param mat_ 稀疏矩阵
			 */
			QuantumWalk(
				qram_qutrit::QRAMCircuit* qram_,
				std::string_view j_, std::string_view b1_,
				std::string_view k_, std::string_view b2_,
				std::string_view j_comp_,
				std::string_view k_comp_,
				std::string_view data_offset_,
				std::string_view sparse_offset_,
				const SparseMatrix& mat_)
				:qram(qram_), j(j_), b1(b1_), k(k_), b2(b2_),
				j_comp(j_comp_), k_comp(k_comp_),
				data_offset(data_offset_), sparse_offset(sparse_offset_),
				mat(mat_)
			{}

			/**
			 * @brief 应用单步量子行走
			 * @param system_states 系统状态向量
			 * @details 依次执行 T† -> 相位翻转 P0 -> T -> 行列交换（Swap）。
			 */
			template<typename Ty>
			void impl(Ty& system_states) const
			{
				profiler _("QuantumWalk");
				size_t addr_size = log2(mat.get_data().size());
				size_t data_size = mat.data_size;
				size_t reg_size = std::max(addr_size, data_size);
				size_t offset = mat.get_sparsity_offset();
				//size_t n_row = mat.n_row;
				size_t nnz_col = mat.nnz_col;

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, reg_size, &mat)
					.dag(system_states);

				// (StatePrint(StatePrintDisplay::Detail))(system_states);

				ZeroConditionalPhaseFlip({ b1, k, b2, k_comp })
					(system_states);

				//RangeConditionalPhaseFlip(j, n_row)
				//	(system_states);

				// (StatePrint(StatePrintDisplay::Detail))(system_states);

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, reg_size, &mat)
					(system_states);

				// (StatePrint(StatePrintDisplay::Detail))(system_states);

				Swap_General_General(j, k)(system_states);
				Swap_General_General(b1, b2)(system_states);
				Swap_General_General(j_comp, k_comp)(system_states);
			}

			/**
			 * @brief 单步量子行走的 dagger 实现
			 * @param system_states 系统状态向量
			 * @note 未实现，调用时抛出异常。
			 */
			template<typename Ty>
			void impl_dag(Ty& system_states) const
			{
				throw_not_implemented("QuantumWalk::impl_dag is not implemented");
			}

			COMPOSITE_OPERATION
		};

		/**
		 * @brief 多步量子行走管理器
		 * @details 负责行走所需寄存器环境的创建与 n 步行走态的制备：
		 *          初始化时按稀疏矩阵布局创建（或接入）QRAM 电路并登记行走寄存器
		 *          （j/b1/k/b2/j_comp/k_comp 及数据、稀疏偏移寄存器）；
		 *          MakeNStepState 先做 Hadamard 均匀输入与首步行走（T · Swap · T†），
		 *          随后迭代单步行走（相位翻转 + T + Swap + T†），制备与行走幂次
		 *          对应的量子态，供 LCU 容器按 Chebyshev 系数组合。
		 */
		template<typename Ty = SparseState>
		class QuantumWalkNSteps
		{
		public:
			/** @brief 数据表偏移寄存器名称 */
			std::string data_offset = "data_offset";
			/** @brief 稀疏表偏移寄存器名称 */
			std::string sparse_offset = "sparse_offset";
			/** @brief 行号（输入向量）寄存器名称 */
			std::string j = "row_id";
			/** @brief 布尔旗标 b1 寄存器名称 */
			std::string b1 = "reg_b1";
			/** @brief 列号寄存器名称 */
			std::string k = "col_id";
			/** @brief 布尔旗标 b2 寄存器名称 */
			std::string b2 = "reg_b2";
			/** @brief j 侧辅助寄存器名称 */
			std::string j_comp = "j_comp";
			/** @brief k 侧辅助寄存器名称 */
			std::string k_comp = "k_comp";
			/** @brief 稀疏矩阵副本 */
			SparseMatrix mat;
			/** @brief QRAM 地址位宽、元素量化位宽、稀疏表偏移、矩阵阶数与每行非零元数 */
			size_t addr_size, data_size, offset, n_row, nnz_col;
			/** @brief 默认寄存器位宽（地址位宽与元素位宽的较大值） */
			size_t default_register_size;
			/** @brief QRAM 电路指针 */
			qram_qutrit::QRAMCircuit* qram;
			/** @brief 建议的状态规模预留常量 */
			constexpr static int suggest_reserve = 1024000;

			/**
			 * @brief 构造函数（接入外部 QRAM 电路）
			 * @param mat_ 稀疏矩阵
			 * @param qram_ 外部 QRAM 电路指针（生命周期由调用方管理）
			 */
			QuantumWalkNSteps(const SparseMatrix& mat_,
				qram_qutrit::QRAMCircuit* qram_)
			{
				mat = mat_;
				auto&& data = mat.get_data();
				addr_size = log2(data.size());
				data_size = mat.data_size;
				offset = mat.get_sparsity_offset();
				n_row = mat.n_row;
				nnz_col = mat.nnz_col;
				default_register_size = std::max(addr_size, data_size);
				qram = qram_;
			}

			/**
			 * @brief 构造函数（内部创建 QRAM 电路）
			 * @param mat_ 稀疏矩阵（按其紧凑布局构建 QRAM 内存）
			 * @note 由本对象持有 QRAM 电路并在析构时释放。
			 */
			QuantumWalkNSteps(const SparseMatrix& mat_)
			{
				mat = mat_;
				auto&& data = mat.get_data();
				addr_size = log2(data.size());
				data_size = mat.data_size;
				offset = mat.get_sparsity_offset();
				n_row = mat.n_row;
				nnz_col = mat.nnz_col;

				default_register_size = std::max(addr_size, data_size);

				qram = new qram_qutrit::QRAMCircuit(addr_size, data_size, std::move(data));
			}

			/**
			 * @brief 析构函数
			 * @note QRAM 电路由本对象删除，接入外部电路的场景需自行保证所有权约定。
			 */
			~QuantumWalkNSteps()
			{
				delete qram;
			}

			/**
			 * @brief 获取输入向量所在的寄存器名称
			 * @return 行号寄存器 j 的名称
			 */
			std::string GetVecInputReg() const
			{
				return j;
			}

			/**
			 * @brief 获取输入寄存器的初始化位宽
			 * @return log2(n_row)，即表示行号所需的位数
			 */
			size_t get_init_size() const
			{
				return log2(mat.n_row);
			}

			/**
			 * @brief 登记行走所需的全部寄存器
			 * @details 在 System 中注册数据 / 稀疏偏移寄存器与
			 *          j/b1/k/b2/j_comp/k_comp 行走寄存器。
			 */
			void InitEnvironment()
			{
				/* Register Init */
				System::add_register(data_offset, UnsignedInteger, default_register_size);
				System::add_register(sparse_offset, UnsignedInteger, default_register_size);
				System::add_register(j, UnsignedInteger, default_register_size);
				System::add_register(b1, Boolean, 1);
				System::add_register(k, UnsignedInteger, default_register_size);
				System::add_register(b2, Boolean, 1);
				System::add_register(j_comp, UnsignedInteger, default_register_size);
				System::add_register(k_comp, UnsignedInteger, default_register_size);
			}

			/**
			 * @brief 创建初始系统状态
			 * @return 初始化了稀疏偏移寄存器（置为矩阵稀疏表偏移）的系统状态
			 */
			Ty CreateSys()
			{
				Ty system_states(1);
				Init_Unsafe(sparse_offset, offset)(system_states);
				return system_states;
			}

			/**
			 * @brief 制备 n 步量子行走的系统状态
			 * @param n_steps 行走步数（0 表示仅做 Hadamard 均匀叠加）
			 * @return 行走 n 步后的系统状态
			 * @details 流程：创建状态 -> 对 j 寄存器做 Hadamard 均匀输入 ->
			 *          首步行走（T · Swap · T†）-> 迭代 n_steps-1 次单步行走。
			 */
			Ty MakeNStepState(size_t n_steps)
			{
				profiler _("MakeNStepState");
				auto system_states = CreateSys();

				Hadamard_Int(j, get_init_size())(system_states);
				ClearZero()(system_states);
				if (n_steps == 0)
					return system_states;

				FirstStep(system_states);

				fmt::print("State size = {}\n", system_states.size());
				for (size_t i = 0; i < n_steps - 1; ++i)
				{
					StepImplOneStep(system_states);
				}
				return system_states;
			}

			/**
			 * @brief 行走的首步（不含相位翻转）
			 * @param system_states 系统状态向量
			 * @details 执行 T -> 行列交换（Swap）-> T†（不含相位翻转）。
			 */
			void FirstStep(Ty& system_states)
			{
				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, default_register_size, &mat)
					(system_states);

				Swap_General_General(j, k)(system_states);
				Swap_General_General(b1, b2)(system_states);
				Swap_General_General(j_comp, k_comp)(system_states);

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, default_register_size, &mat)
					.dag(system_states);
			}
			/**
			 * @brief 单步行走的实现
			 * @param system_states 系统状态向量
			 * @details 执行相位翻转 P0 -> T -> 行列交换（Swap）-> T†，
			 *          即行走算符的单个幂次。
			 */
			void StepImplOneStep(Ty& system_states)
			{
				ZeroConditionalPhaseFlip({ j_comp, k_comp, b1, k, b2 })
					(system_states);

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, default_register_size, &mat)
					(system_states);

				CheckNan()(system_states);

				Swap_General_General(j, k)(system_states);
				Swap_General_General(b1, b2)(system_states);
				Swap_General_General(j_comp, k_comp)(system_states);

				T(qram, data_offset, sparse_offset, j, b1, k, b2, k_comp,
					nnz_col, default_register_size, &mat)
					.dag(system_states);

				CheckNan()(system_states);
			}
			/**
			 * @brief 推进两步行走
			 * @param system_states 系统状态向量
			 * @details 连续执行两次单步行走，对应 LCU 展开中步数每次增加 2
			 *          （2j+1 -> 2(j+1)+1）。
			 */
			void Step(Ty& system_states)
			{
				profiler _("Step");
				StepImplOneStep(system_states);
				StepImplOneStep(system_states);
			}
		};

		/**
		 * @brief CKS 线性系统求解的 LCU 容器（通用版本）
		 * @details 以线性组合 Σ_j c_j · W^(2j+1)（j = 0..j0）逼近矩阵 Chebyshev
		 *          级数对应的目标算子：每一项由 QuantumWalkNSteps 独立制备对应
		 *          步数的行走态，按系数与符号累加进当前态并归并去重。
		 *          展开阶数 b = kappa^2 · log(kappa/eps)，
		 *          截断点 j0 = sqrt(b · log(4b/eps))。
		 */
		struct LCU_Container
		{
			/** @brief 已累加的 LCU 组合态 */
			std::vector<System> current_state;
			/** @brief 条件数 kappa */
			double kappa;
			/** @brief 目标精度 eps */
			double eps;
			/** @brief Chebyshev 展开阶数参数 b */
			size_t b;
			/** @brief LCU 求和截断点 */
			size_t j0;
			/** @brief 多步量子行走管理器 */
			QuantumWalkNSteps<std::vector<System>> quantum_walk_obj;
			/** @brief Chebyshev 系数计算器 */
			ChebyshevPolynomialCoefficient chebyshev_obj;

			/**
			 * @brief 构造函数
			 * @param mat 稀疏矩阵
			 * @param kappa_ 条件数
			 * @param eps_ 目标精度
			 * @details 计算 b 与 j0 并初始化行走寄存器环境。
			 */
			LCU_Container(const SparseMatrix& mat, double kappa_, double eps_) :
				quantum_walk_obj(mat),
				kappa(kappa_), eps(eps_),
				b(static_cast<size_t>(kappa* kappa* (std::log(kappa) - std::log(eps)))),
				chebyshev_obj(b),
				j0(static_cast<size_t>(std::sqrt(b* (std::log(4 * b) - std::log(eps)))))
			{
				quantum_walk_obj.InitEnvironment();
			}

			/**
			 * @brief 获取输入向量所在的寄存器名称
			 * @return 行走管理器的输入寄存器名称
			 */
			auto GetInputVecReg()
			{
				return quantum_walk_obj.GetVecInputReg();
			}

			/**
			 * @brief 制备第 j 项对应的行走态
			 * @param j 项下标
			 * @return 行走 2j+1 步后的系统状态
			 */
			std::vector<System> state_of_j(size_t j);

			/**
			 * @brief 将新状态按系数累加进 LCU 组合态
			 * @param new_state 待累加的状态
			 * @param coef Chebyshev 系数
			 * @param sign 是否取负号（奇数 j 项）
			 */
			void add(std::vector<System> new_state, double coef, bool sign);

			/**
			 * @brief 执行完整的 LCU 迭代
			 * @details 遍历 j = 0..j0：制备行走 2j+1 步的状态、按系数与符号
			 *          累加，并在每轮做排序归并以控制状态规模。
			 */
			void iterate();
		};

		/* The noisefree version */
		/* It will iterate over the original state,
		   without making extra copies.

		   This optimization cannot be applied to
		   noisy simulation.
		*/

		/**
		 * @brief CKS 线性系统求解的 LCU 容器（无噪声优化版本）
		 * @details 在同一份行走态上原地迭代而不为每个 LCU 项复制状态，
		 *          因此仅适用于无噪声模拟。ExternalInput 注入输入并完成首步
		 *          行走；Step 逐项推进 LCU 迭代（j 从 0 到 j0，系数之和 a 作为
		 *          LCU 归一化因子）；PartialTrace 对行走辅助寄存器做后选择，
		 *          给出成功概率。
		 */
		template<typename StateTy = SparseState>
		struct LCU_Container_NoiseFree
		{
			/** @brief 已累加的 LCU 组合态 */
			StateTy current_state;
			/** @brief 当前行走态（在各 LCU 项间原地推进） */
			StateTy step_state;
			/** @brief 多步量子行走管理器 */
			QuantumWalkNSteps<StateTy> quantum_walk_obj;
			/** @brief 条件数 kappa */
			double kappa;
			/** @brief 目标精度 eps */
			double eps;
			/** @brief Chebyshev 展开阶数参数 b */
			size_t b;
			/** @brief LCU 求和截断点 */
			size_t j0;
			size_t j = 0; // iteration variable
			/** @brief 已累加的 Chebyshev 系数之和（LCU 归一化因子） */
			double a = 0;
			/** @brief Chebyshev 系数计算器 */
			ChebyshevPolynomialCoefficient chebyshev_obj;

			/**
			 * @brief 构造函数
			 * @param mat 稀疏矩阵
			 * @param kappa 条件数
			 * @param eps 目标精度
			 * @details 计算 b 与 j0、初始化行走寄存器环境并创建行走态。
			 */
			LCU_Container_NoiseFree(const SparseMatrix& mat, double kappa, double eps) :
				quantum_walk_obj(mat),
				kappa(kappa), eps(eps),
				b(static_cast<size_t>(kappa* kappa* (std::log(kappa) - std::log(eps)))),
				chebyshev_obj(b),
				j0(static_cast<size_t>(std::sqrt(b* (std::log(4 * b) - std::log(eps)))))
			{
				quantum_walk_obj.InitEnvironment();
				step_state = quantum_walk_obj.CreateSys();
			}

			/**
			 * @brief 获取输入向量所在的寄存器名称
			 * @return 行走管理器的输入寄存器名称
			 */
			auto GetInputVecReg() const
			{
				return quantum_walk_obj.GetVecInputReg();
			}

			/**
			 * @brief 获取 QRAM 地址位宽
			 * @return 行走管理器的 addr_size
			 */
			size_t get_addr_size() const
			{
				return quantum_walk_obj.addr_size;
			}

			/**
			 * @brief 注入外部输入（默认构造参数版本）
			 * @details 在输入寄存器上应用输入算子 Ty，清理零振幅后执行首步行走。
			 */
			template<typename Ty>
			void ExternalInput()
			{
				(Ty(GetInputVecReg()))(step_state);
				ClearZero()(step_state);
				quantum_walk_obj.FirstStep(step_state);
				fmt::print("State size = {}\n", step_state.size());
			}

			/**
			 * @brief 注入外部输入（带附加参数版本）
			 * @param args 转发给输入算子 Ty 构造函数的附加参数
			 * @details 在输入寄存器上应用输入算子 Ty，清理零振幅后执行首步行走。
			 */
			template<typename Ty, typename ...Args>
			void ExternalInput(Args &&...args)
			{
				Ty(GetInputVecReg(), std::forward<Args>(args)...)(step_state);
				ClearZero()(step_state);
				quantum_walk_obj.FirstStep(step_state);
				fmt::print("State size = {}\n", step_state.size());
			}

			/**
			 * @brief 注入外部输入（算子实例版本）
			 * @param op 作用于输入寄存器的输入算子
			 * @details 应用输入算子，清理零振幅后执行首步行走。
			 */
			void ExternalInput_V2(const BaseOperator& op)
			{
				op(step_state);
				ClearZero()(step_state);
				quantum_walk_obj.FirstStep(step_state);
				fmt::print("State size = {}\n", step_state.size());
			}

			/**
			 * @brief 推进一个 LCU 项
			 * @return 尚未到达截断点时返回 true，迭代结束返回 false
			 * @details j 非 0 时先把行走态推进两步（步数 2j+1），
			 *          再累加系数到 a 并把当前行走态按系数与符号加入组合态。
			 */
			bool Step() {
				if (j <= j0) {
					if (j != 0)
					{
						quantum_walk_obj.Step(step_state);
					}
				
					a += chebyshev_obj.coef(j);
					Add(step_state, chebyshev_obj.coef(j), chebyshev_obj.sign(j));
					++j;
					return true;
				}
				else
					return false;
			}

			/**
			 * @brief 将状态按系数累加进 LCU 组合态
			 * @param new_state 待累加的状态
			 * @param coef Chebyshev 系数
			 * @param sign 是否取负号（奇数 j 项）
			 */
			void Add(const StateTy& new_state, double coef, bool sign)
			{
				if (sign)
					coef *= -1;

				add_systems(current_state, new_state, coef);
			}

			// inline std::vector<complex_t> _impl_get_output(StateTy& state) const
			// {
			// 	std::vector<complex_t> m(quantum_walk_obj.n_row, 0);
			// 	auto id = System::get(GetInputVecReg());
			// 	for (System& s : state)
			// 	{
			// 		StateStorage& st = s.get(id);
			// 		uint64_t v = st.as<uint64_t>(System::size_of(id));
			// 		m[s.GetAs(id, uint64_t)] = s.amplitude;
			// 	}
			// 	return m;
			// }

			/**
			 * @brief 计算后选择成功概率（内部实现）
			 * @param state 系统状态向量（会被部分迹选择修改）
			 * @return LCU 后选择的成功概率
			 * @details 对行走辅助寄存器（b1、k、b2、j_comp、k_comp、偏移寄存器）
			 *          全零且 j 落在 [0, n_row) 的分支做部分迹选择，
			 *          结合 LCU 归一化因子 a 得到成功概率
			 *          （PartialTraceSelect 返回 1/sqrt(p)）。
			 */
			double _impl_partial_trace(StateTy& state) const
			{
				double ret = PartialTraceSelect({
					  {quantum_walk_obj.b1, 0},
					  {quantum_walk_obj.k, 0},
					  {quantum_walk_obj.b2, 0},
					  {quantum_walk_obj.j_comp, 0},
					  {quantum_walk_obj.k_comp, 0},
					  {quantum_walk_obj.sparse_offset, quantum_walk_obj.offset},
					  {quantum_walk_obj.data_offset, 0}
					})(state);
	
				ret *= PartialTraceSelectRange(
					quantum_walk_obj.j,
					{ 0, quantum_walk_obj.n_row - 1 })(state);
	
				return 1.0 / ret / ret / a / a;
			}

			/**
			 * @brief 计算后选择成功概率（破坏性版本）
			 * @return 成功概率
			 * @note 会修改 current_state。
			 */
			double PartialTrace()
			{
				return _impl_partial_trace(current_state);
			}

			/**
			 * @brief 计算后选择成功概率（非破坏性版本）
			 * @return (后选择后的状态副本, 成功概率)
			 */
			std::tuple<StateTy, double> PartialTrace_Nondestructive()  const
			{
				StateTy state = current_state;
				double ret = _impl_partial_trace(state);
				return { state, ret };
			}

		};

		/* Directly use the theory to validate */
		/**
		 * @brief CKS 线性系统求解的经典理论验证容器
		 * @details 直接用稠密矩阵按 Chebyshev 三项递推
		 *          T_{n+1} = 2·A'·T_n - T_{n-1} 计算行走幂次对应的向量
		 *          （A' 为按量化幅度与 nnz_col 归一化后的稠密矩阵），
		 *          与量子实现的 LCU 组合对照，用于经典层面验证算法正确性。
		 */
		struct LCU_Container_Theory
		{
			/** @brief 稀疏矩阵 */
			SparseMatrix mat;
			/** @brief 归一化后的稠密矩阵 */
			DenseMatrix<complex_t> densemat;
			/** @brief 条件数 kappa */
			double kappa;
			/** @brief 目标精度 eps */
			double eps;
			/** @brief Chebyshev 展开阶数参数 b */
			size_t b;
			/** @brief LCU 求和截断点 */
			size_t j0;
			size_t j = 0; // iteration variable
			/** @brief 已累加的 Chebyshev 系数之和（LCU 归一化因子） */
			double a = 0;
			/** @brief Chebyshev 系数计算器 */
			ChebyshevPolynomialCoefficient chebyshev_obj;
			/** @brief 已累加的 LCU 组合向量 */
			DenseVector<complex_t> current_state;
			/** @brief 当前行走幂次对应的向量 */
			DenseVector<complex_t> step_state;
			DenseVector<complex_t> vec0; // for chebyshev iteration
			DenseVector<complex_t> vec1; // for chebyshev iteration

			/**
			 * @brief 构造函数
			 * @param mat_ 稀疏矩阵
			 * @param kappa_ 条件数
			 * @param eps_ 目标精度
			 * @details 构建归一化稠密矩阵，并以均匀归一化向量初始化
			 *          Chebyshev 递推的前两项（vec0、vec1）。
			 */
			LCU_Container_Theory(const SparseMatrix& mat_, double kappa_, double eps_) :
				mat(mat_),
				densemat(sparse2dense<complex_t>(mat)),
				kappa(kappa_),
				eps(eps_),
				b(static_cast<size_t>(kappa* kappa* (std::log(kappa) - std::log(eps)))),
				chebyshev_obj(b),
				j0(static_cast<size_t>(std::sqrt(b* (std::log(4 * b) - std::log(eps))))),
				current_state(mat.n_row),
				step_state(mat.n_row),
				vec0(mat.n_row),
				vec1(mat.n_row)
			{
				densemat = densemat / pow2(mat.data_size);
				densemat = densemat / mat.nnz_col;
				std::fill(step_state.data.begin(), step_state.data.end(), 1.0);
				step_state = step_state / step_state.norm2();
				vec0 = step_state;
				vec1 = densemat * vec0;
			}

			/**
			 * @brief 将向量按系数累加进 LCU 组合向量
			 * @param new_state 待累加的向量
			 * @param coef Chebyshev 系数
			 * @param sign 是否取负号（奇数 j 项）
			 */
			inline void Add(const DenseVector<complex_t>& new_state,
				double coef, bool sign)
			{
				if (sign)
					coef *= -1;

				for (size_t i = 0; i < current_state.data.size(); ++i)
				{
					current_state[i] += coef * new_state[i];
				}
			}

			/**
			 * @brief 构造当前 LCU 项对应的行走幂次向量
			 * @return Chebyshev 递推得到的 T(2j+1) 向量
			 * @details j = 0 时返回初始的 vec1；否则按三项递推推进两步，
			 *          对应量子行走每次调用 Step 前进两步。
			 */
			DenseVector<complex_t> MakeStepState();

			/**
			 * @brief 推进一个 LCU 项
			 * @return 尚未到达截断点时返回 true，迭代结束返回 false
			 */
			bool Step();

			/**
			 * @brief 获取最终解向量与成功概率
			 * @return (归一化解向量, 成功概率 = ||current||^2 / a^2)
			 */
			std::pair<DenseVector<complex_t>, double> GetOutput() const;
		};


		/**
		 * @brief 线性系统求解的经典参考实现（全 1 右端项）
		 * @param mat 稀疏矩阵
		 * @return 归一化的解向量（复数）
		 * @details 以全 1 向量为右端项调用带右端项版本。
		 */
		std::vector<complex_t> my_linear_solver_reference(const SparseMatrix& mat);

		/**
		 * @brief 线性系统求解的经典参考实现（指定右端项）
		 * @param mat 稀疏矩阵
		 * @param vec 右端项向量
		 * @return 归一化的解向量（复数）
		 * @details 用 Eigen 稀疏线性求解器求 A x = vec 后按 2 范数归一化，
		 *          作为量子算法结果的经典对照。
		 */
		std::vector<complex_t> my_linear_solver_reference(const SparseMatrix& mat, const DenseVector<double>& vec);

		// Deprecated alias; see docs/naming_conventions.md. Remove in the next major version.
		using QuantumBinarySearchFast [[deprecated("use QuantumBinarySearch_Fast")]] = QuantumBinarySearch_Fast;
	} // namespace CKS
}
