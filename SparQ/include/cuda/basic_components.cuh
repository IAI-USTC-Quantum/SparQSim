/**
 * @file basic_components.cuh
 * @brief GPU 侧基础组件：寄存器值访问与稀疏态容器
 * @details 提供 CUDA 内核可直接使用的 __host__ __device__ 辅助函数：
 *          寄存器存储值的类型转换（无符号/有符号/浮点/布尔）、System 的
 *          寄存器与振幅访问、以及 GPU 稀疏态容器 CuSparseState
 *          （CPU/GPU 双驻留，按需迁移）与一组 thrust 仿函数
 *          （取模平方、归一化、基态比较/相等、零振幅判定）。
 *          与 CPU 侧 SparQ/include/basic_components.h 的数据结构保持同一内存布局
 */

#pragma once

#include "basic_components.h"
#include "basic.h"

namespace qram_simulator {

	/**
	 * @brief 读取寄存器存储值并按位宽解释为无符号整数
	 * @param storage 寄存器存储单元
	 * @param size 寄存器位宽
	 * @return 截断到位宽后的无符号值
	 */
	inline __host__ __device__ uint64_t cu_as_uint64(const StateStorage& storage, size_t size) {
		uint64_t value = storage.value;
		return value & width_mask(size);
	}

	/**
	 * @brief 读取寄存器存储值并按定点格式解释为浮点数
	 * @details 定点编码：最高位为符号位，其余为小数位，
	 *          故真值 = 补码整数 / 2^{size-1}
	 * @param storage 寄存器存储单元
	 * @param size 寄存器位宽
	 * @return 定点解码后的浮点值
	 */
	inline __host__ __device__ double cu_as_double(const StateStorage& storage, size_t size) {
		uint64_t value = storage.value;
		value &= width_mask(size);
		return value / 2.0 / (1ULL << (size - 1));
	}

	/**
	 * @brief 读取寄存器存储值并解释为布尔
	 * @param storage 寄存器存储单元
	 * @param size 寄存器位宽
	 * @return 值非零时为 true
	 */
	inline __host__ __device__ bool cu_as_bool(const StateStorage& storage, size_t size) {
		uint64_t value = storage.value;
		value &= width_mask(size);
		return bool(value);
	}

	/**
	 * @brief 读取寄存器存储值并按位宽符号扩展为有符号整数
	 * @param storage 寄存器存储单元
	 * @param size 寄存器位宽（0 时返回 0）
	 * @return 符号扩展后的有符号值
	 */
	inline __host__ __device__ int64_t cu_as_int64(const StateStorage& storage, size_t size) {
		uint64_t value = storage.value;
		value &= width_mask(size);
		return size ? (int64_t)(value << (64 - size)) >> (64 - size) : 0;
	}

	/**
	 * @brief 取 System 第 index 个寄存器存储单元（可写引用）
	 * @param system 基态
	 * @param index 寄存器索引
	 * @return 对应 StateStorage 的引用
	 */
	inline __host__ __device__ StateStorage& CuGet(System& system, size_t index)
	{
		return (reinterpret_cast<StateStorage*>(&system.registers))[index];
	}

	/**
	 * @brief 取 System 第 index 个寄存器存储单元（只读）
	 * @param system 基态
	 * @param index 寄存器索引
	 * @return 对应 StateStorage 的副本
	 */
	inline __host__ __device__ StateStorage CuGet(const System& system, size_t index)
	{
		return (reinterpret_cast<const StateStorage*>(&system.registers))[index];
	}

	/**
	 * @brief 取 System 第 index 个寄存器值并解释为无符号整数
	 * @param system 基态
	 * @param index 寄存器索引
	 * @param size 寄存器位宽
	 * @return 无符号值
	 */
	inline __host__ __device__ uint64_t CuGetAsUint64(const System& system, size_t index, size_t size)
	{
		return cu_as_uint64(CuGet(system, index), size);
	}

	/**
	 * @brief 取 System 第 index 个寄存器值并按定点解释为浮点数
	 * @param system 基态
	 * @param index 寄存器索引
	 * @param size 寄存器位宽
	 * @return 定点解码后的浮点值
	 */
	inline __host__ __device__ double CuGetAsDouble(const System& system, size_t index, size_t size)
	{
		return cu_as_double(CuGet(system, index), size);
	}

	/**
	 * @brief 取 System 第 index 个寄存器值并解释为布尔
	 * @param system 基态
	 * @param index 寄存器索引
	 * @param size 寄存器位宽
	 * @return 布尔值
	 */
	inline __host__ __device__ bool CuGetAsBool(const System& system, size_t index, size_t size)
	{
		return cu_as_bool(CuGet(system, index), size);
	}

	/**
	 * @brief 取 System 第 index 个寄存器值并符号扩展为有符号整数
	 * @param system 基态
	 * @param index 寄存器索引
	 * @param size 寄存器位宽
	 * @return 有符号值
	 */
	inline __host__ __device__ int64_t CuGetAsInt64(const System& system, size_t index, size_t size)
	{
		return cu_as_int64(CuGet(system, index), size);
	}

	/**
	 * @brief 取 System 振幅的裸 double 指针（可写，[0]=实部 [1]=虚部）
	 * @param system 基态
	 * @return 振幅实部/虚部的 double 指针
	 */
	inline __host__ __device__ double* CuSystemAmplitude(System& system)
	{
		return reinterpret_cast<double*>(&system.amplitude);
	}

	/**
	 * @brief 取 System 振幅的裸 const double 指针（只读）
	 * @param system 基态
	 * @return 振幅实部/虚部的 const double 指针
	 */
	inline __host__ __device__ const double* CuSystemAmplitude(const System& system)
	{
		return reinterpret_cast<const double*>(&system.amplitude);
	}

	/**
	 * @brief 取复数实部（可写引用）
	 * @param c 复数
	 * @return 实部引用
	 */
	inline __host__ __device__ double& cu_real(complex_t& c)
	{
		return reinterpret_cast<double*>(&c)[0];
	}

	/**
	 * @brief 取复数实部（只读）
	 * @param c 复数
	 * @return 实部值
	 */
	inline __host__ __device__ double cu_real(const complex_t& c)
	{
		return reinterpret_cast<const double*>(&c)[0];
	}

	/**
	 * @brief 取复数虚部（可写引用）
	 * @param c 复数
	 * @return 虚部引用
	 */
	inline __host__ __device__ double& cu_imag(complex_t& c)
	{
		return reinterpret_cast<double*>(&c)[1];
	}

	/**
	 * @brief 取复数虚部（只读）
	 * @param c 复数
	 * @return 虚部值
	 */
	inline __host__ __device__ double cu_imag(const complex_t& c)
	{
		return reinterpret_cast<const double*>(&c)[1];
	}

	/**
	 * @brief 计算基态振幅的模平方 |amp|²
	 * @param s 基态
	 * @return 模平方
	 */
	inline __host__ __device__ double CuAbsSqr(const System& s)
	{
		const double* amplitude = CuSystemAmplitude(s);
		return amplitude[0] * amplitude[0] + amplitude[1] * amplitude[1];
	}


	/**
	 * @brief GPU 稀疏态容器（CPU/GPU 双驻留）
	 * @details 以 _on_gpu 标记当前驻留侧：CPU 侧为 std::vector<System>，
	 *          GPU 侧为 thrust::device_vector<System>；提供 move/copy 迁移与
	 *          迭代器访问（迭代器访问前强制拉回 CPU）。
	 *          拷贝/移动构造与赋值均按源侧选择性复制，避免无谓的设备传输。
	 *          对应 CPU 侧的 SparseState（std::vector<System> 别名）
	 */
	struct CuSparseState
	{
		/** @brief CPU 侧基态向量类型 */
		using vector_type = std::vector<System>;

		/** @brief CPU 侧稀疏态数据 */
		std::vector<System> sparse_state_cpu;
		/** @brief GPU 侧基态索引（排序/归并辅助） */
		thrust::device_vector<size_t> gpu_indices;
		/** @brief GPU 侧稀疏态数据 */
		thrust::device_vector<System> sparse_state_gpu;

		/** @brief 当前是否驻留在 GPU */
		bool _on_gpu = false;

		CuSparseState();
		CuSparseState(size_t size);

		/**
		 * @brief 拷贝构造（按源驻留侧复制）
		 * @param other 源容器
		 */
		CuSparseState(const CuSparseState& other)
		{
			if (other._on_gpu)
			{
				sparse_state_gpu = other.sparse_state_gpu;
				_on_gpu = true;
			}
			else
			{
				sparse_state_cpu = other.sparse_state_cpu;
				_on_gpu = false;
			}
		}

		/**
		 * @brief 移动构造（按源驻留侧移动）
		 * @param other 源容器
		 */
		CuSparseState(CuSparseState&& other)
		{
			if (other._on_gpu)
			{
				sparse_state_gpu = std::move(other.sparse_state_gpu);
				_on_gpu = true;
			}
			else
			{
				sparse_state_cpu = std::move(other.sparse_state_cpu);
				_on_gpu = false;
			}
		}

		/**
		 * @brief 拷贝赋值（按源驻留侧复制）
		 * @param other 源容器
		 * @return 自身引用
		 */
		CuSparseState& operator=(const CuSparseState& other)
		{
			if (other._on_gpu)
			{
				sparse_state_gpu = other.sparse_state_gpu;
				_on_gpu = true;
			}
			else
			{
				sparse_state_cpu = other.sparse_state_cpu;
				_on_gpu = false;
			}
			return *this;
		}

		/**
		 * @brief 移动赋值（按源驻留侧移动）
		 * @param other 源容器
		 * @return 自身引用
		 */
		CuSparseState& operator=(CuSparseState&& other)
		{
			if (other._on_gpu)
			{
				sparse_state_gpu = std::move(other.sparse_state_gpu);
				_on_gpu = true;
			}
			else
			{
				sparse_state_cpu = std::move(other.sparse_state_cpu);
				_on_gpu = false;
			}
			return *this;
		}

		/**
		 * @brief 由 CPU 侧 SparseState 构造
		 * @param other CPU 侧稀疏态
		 */
		CuSparseState(const SparseState& other)
		{
			sparse_state_cpu = other.basis_states;
			_on_gpu = false;
		}

		/**
		 * @brief 由基态向量构造（CPU 驻留）
		 * @param other 基态向量
		 */
		CuSparseState(const std::vector<System>& other)
		{
			sparse_state_cpu = other;
			_on_gpu = false;
		}

		/**
		 * @brief 由设备向量构造（GPU 驻留）
		 * @param other GPU 侧基态向量
		 */
		CuSparseState(const thrust::device_vector<System>& other)
		{
			sparse_state_gpu = other;
			_on_gpu = true;
		}

		/**
		 * @brief 由设备向量迭代器区间构造（GPU 驻留）
		 * @param begin 起始迭代器
		 * @param end 结束迭代器
		 */
		CuSparseState(thrust::device_vector<System>::iterator begin, thrust::device_vector<System>::iterator end)
		{
			sparse_state_gpu = thrust::device_vector<System>(begin, end);
			_on_gpu = true;
		}

		/** @brief 把数据迁回 CPU（释放 GPU 显存） */
		void move_to_cpu();

		/** @brief 把数据复制回 CPU（保留 GPU 副本） */
		void copy_to_cpu();

		/** @brief 把数据迁上 GPU（释放 CPU 内存） */
		void move_to_gpu();

		/** @brief 获取 CPU 侧数据副本（触发拷贝） */
		std::vector<System> get_cpu_copy() const;

		/** @brief 是否驻留在 GPU */
		bool on_gpu() const;

		/** @brief 是否驻留在 CPU */
		bool on_cpu() const;

		/** @brief 是否为空 */
		bool empty() const;

		/** @brief 基态数目（按当前驻留侧统计） */
		size_t size() const;

		/** @brief 访问末尾基态（先拉回 CPU） */
		System& back() { copy_to_cpu(); return sparse_state_cpu.back(); }
		/** @brief 正向起始迭代器（先拉回 CPU） */
		vector_type::iterator begin() { copy_to_cpu(); return sparse_state_cpu.begin(); }
		/** @brief 正向结束迭代器（先拉回 CPU） */
		vector_type::iterator end() { copy_to_cpu(); return sparse_state_cpu.end(); }
		/** @brief 反向起始迭代器（先拉回 CPU） */
		vector_type::reverse_iterator rbegin() { copy_to_cpu(); return sparse_state_cpu.rbegin(); }
		/** @brief 反向结束迭代器（先拉回 CPU） */
		vector_type::reverse_iterator rend() { copy_to_cpu(); return sparse_state_cpu.rend(); }
	};


	/**
	 * @brief 取基态振幅模平方的 thrust 仿函数
	 */
	struct AbsSqrFunctor {
		/**
		 * @brief 计算基态振幅模平方
		 * @param s 基态
		 * @return |amp|²
		 */
		__host__ __device__ double operator()(const System& s) const {
			return CuAbsSqr(s);
		}
	};

	/**
	 * @brief 按常数因子缩放振幅的 thrust 仿函数（归一化用）
	 */
	struct Normalize_Functor {
		/** @brief 缩放因子 */
		double factor;

		/**
		 * @brief 构造函数
		 * @param factor_ 缩放因子
		 */
		Normalize_Functor(double factor_) : factor(factor_) {}

		/**
		 * @brief 实部与虚部同乘因子
		 * @param s 基态
		 */
		__host__ __device__ void operator()(System& s) const {
			double* amplitude = CuSystemAmplitude(s);
			amplitude[0] *= factor;
			amplitude[1] *= factor;
		}
	};

	/**
	 * @brief 基态按键（激活寄存器值序列）作字典序小于比较的 thrust 仿函数
	 * @details 构造时缓存全局寄存器表大小与激活状态位图，
	 *          比较时仅计入激活寄存器（status_bitmap 掩码）
	 */
	struct SystemLess_Functor
	{
		/** @brief 构造时缓存的全局寄存器表大小 */
		size_t name_reg_map_size;
		/** @brief 构造时缓存的寄存器激活状态位图 */
		size_t status_bitmap = 0;

		SystemLess_Functor()
			: name_reg_map_size(System::name_register_map.size()),
			status_bitmap(System::reg_status_bitmap)
		{
			//for (size_t i = 0; i < System::name_register_map.size(); ++i)
			//{
			//	if (System::status_of(i))
			//		status_bitmap |= pow2(i);
			//}
			
		}

		/**
		 * @brief 字典序小于比较（仅激活寄存器）
		 * @param a 左基态
		 * @param b 右基态
		 * @return a < b
		 */
		__host__ __device__ bool operator()(const System& a, const System& b) {
			for (size_t i = 0; i < name_reg_map_size; ++i)
			{
				auto regl = CuGet(a, i).value * ((status_bitmap >> i) & 1);
				auto regr = CuGet(b, i).value * ((status_bitmap >> i) & 1);
				if (regl < regr)
					return true;
				else if (regl > regr)
					return false;
			}
			return false;
		}
	};


	/**
	 * @brief 基态按键（激活寄存器值序列）相等比较的 thrust 仿函数
	 * @details 构造时缓存全局寄存器表大小与激活状态位图；
	 *          支持两个基态或 thrust::tuple<System, System>（zip 迭代器）输入
	 */
	struct SystemEqual_Functor
	{
		/** @brief 构造时缓存的全局寄存器表大小 */
		size_t name_reg_map_size;
		/** @brief 构造时缓存的寄存器激活状态位图 */
		size_t status_bitmap = 0;

		SystemEqual_Functor()
			: name_reg_map_size(System::name_register_map.size()),
			status_bitmap(System::reg_status_bitmap)
		{
			//for (size_t i = 0; i < System::name_register_map.size(); ++i)
			//{
			//	if (System::status_of(i))
			//		status_bitmap |= pow2(i);
			//}
		}

		/**
		 * @brief 相等比较（仅激活寄存器）
		 * @param a 左基态
		 * @param b 右基态
		 * @return 激活寄存器值全部相等
		 */
		__host__ __device__ bool operator()(const System& a, const System& b) {
			for (size_t i = 0; i < name_reg_map_size; ++i)
			{
				auto regl = CuGet(a, i).value * ((status_bitmap >> i) & 1);
				auto regr = CuGet(b, i).value * ((status_bitmap >> i) & 1);
				if (regl != regr)
					return false;
			}
			return true;
		}

		/**
		 * @brief 相等比较（zip 迭代器的基态对版本）
		 * @param system_pair 基态对
		 * @return 激活寄存器值全部相等
		 */
		__host__ __device__ bool operator()(const thrust::tuple<System, System>& system_pair) {
			for (size_t i = 0; i < name_reg_map_size; ++i)
			{
				const System& a = thrust::get<0>(system_pair);
				const System& b = thrust::get<1>(system_pair);
				auto regl = CuGet(a, i).value * ((status_bitmap >> i) & 1);
				auto regr = CuGet(b, i).value * ((status_bitmap >> i) & 1);
				if (regl != regr)
					return false;
			}
			return true;
		}
	};

	/**
	 * @brief 判定基态振幅模平方小于阈值的 thrust 仿函数（清除零振幅分支用）
	 */
	struct AmplitudeZero_Functor
	{
		/** @brief 阈值 ε */
		double eps;

		/**
		 * @brief 构造函数
		 * @param eps_ 阈值 ε
		 */
		AmplitudeZero_Functor(double eps_) : eps(eps_) {}

		/**
		 * @brief 判定 |amp|² < ε
		 * @param s 基态
		 * @return 是否视为零振幅
		 */
		__host__ __device__ bool operator()(const System& s) const {
			const double* amplitude = CuSystemAmplitude(s);
			return (amplitude[0] * amplitude[0] + amplitude[1] * amplitude[1]) < eps;
		}
	};

	}
