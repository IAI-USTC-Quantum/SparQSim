/**
 * @file quantum_interfere_basic.cuh
 * @brief GPU 侧稀疏态干涉基础组件
 * @details 提供稀疏态更新的 GPU 并行原语：按"除键外全等"分组（unq_ele 分区表 +
 *          unique_* 内核）、除键外排序（SortExceptKey_devfunc）、各类 thrust
 *          比较仿函数（按键比较 / 除键外比较 / 索引版）、状态哈希仿函数，
 *          以及分区表辅助（SortUniqueElements / EleNum_MoreThanOne / EleNum_NotFull）。
 *          是 condrot.cuh 等条件旋转 GPU 实现的公共底座，
 *          与 CPU 侧 SparQ/include/quantum_interfere_basic.h 的语义对应
 */

#pragma once

#include "quantum_interfere_basic.h"
#include "basic_components.h"
#include "cuda_utils.cuh"
#include "basic_components.cuh"

namespace qram_simulator {

    /**
     * @brief 稀疏态分区描述单元
     * @details 用于把按键（除某寄存器外全等的寄存器组合）排序后的状态序列
     *          划分为连续分区，是 GPU 侧干涉/旋转操作按组处理的基础
     */
    struct unq_ele {
        size_t sptr; /* The position of the first element of the partition */
        unsigned int num; /* The number of elements in the partition */
    };

    /* GPU-version: SortExceptKey() */
    /**
     * @brief 除 idi 寄存器外按键排序稀疏态（GPU 版 SortExceptKey）
     * @param idi 作为键的寄存器 ID（排序时排除）
     * @param state GPU 侧稀疏态（原地重排）
     */
    void SortExceptKey_devfunc(int idi, thrust::device_vector<System>& state);

    /**
     * @brief 除 idi 寄存器外按键排序的"逻辑索引"版本（不搬动数据）
     * @param idi 作为键的寄存器 ID（排序时排除）
     * @param state GPU 侧稀疏态（只读）
     * @return 排序后的下标索引向量（由缓存持有，重复调用复用）
     */
    const thrust::device_vector<size_t>& SortExceptKey_devfunc_logical(int idi, const thrust::device_vector<System>& state);

    /**
     * @brief 设备侧比较两基态是否除 out_id 外全部激活寄存器相等
     * @param a 左基态
     * @param b 右基态
     * @param out_id 排除的寄存器 ID
     * @param mp_sz 寄存器表大小
     * @param status_bitmap 激活状态位图
     * @return 除 out_id 外激活寄存器值全部相等
     */
    // compare two states
    __device__ inline bool compare_equal_dev(const System& a, const System& b, int out_id,
        int mp_sz, uint64_t status_bitmap)
    {
        for (int i = mp_sz - 1; i >= 0; --i)
        {
            if (!((status_bitmap >> i) & 1))
                continue;
            if (i == out_id)
                continue;
            if ((CuGet(a, i).value != CuGet(b, i).value))
                return false;
        }
        return true;
    }

    /**
     * @brief 在排序后的状态序列中标记各分区首元素（内核）
     * @param dat 基态数组（设备指针）
     * @param num 分区计数输出（设备指针，单个 size_t）
     * @param nsize 基态总数
     * @param regbit 键寄存器位宽（未用，保留参数）
     * @param reg_stat 激活状态位图
     * @param mpsz 寄存器表大小
     * @param id 排除的键寄存器 ID
     */
    // find first unique element of a sorted state series
    __global__ void unique_find_elem(System* dat, size_t* num, size_t nsize,
        int regbit, size_t reg_stat, int mpsz, int id);

    /**
     * @brief 统计排序后各分区的元素个数（内核）
     * @param dat 分区首标记数组（设备指针）
     * @param uele 分区表输出（设备指针）
     * @param nsize 标记数组长度
     * @param nstate 基态总数
     */
    // count number of each unique element
    __global__ void unique_count_elem(size_t* dat, unq_ele* uele, size_t nsize, size_t nstate);

    /**
     * @brief 统计稀疏态按键分区（先排序后标记，直接排序版）
     * @param state GPU 稀疏态
     * @param uele 输出分区表
     * @param id 排除的键寄存器 ID
     */
    void Unique_count_elem(CuSparseState& state, thrust::device_vector<unq_ele>& uele, int id);

    /**
     * @brief 统计稀疏态按键分区（复用逻辑索引版排序）
     * @param state GPU 稀疏态
     * @param indices 预先排好的逻辑索引（SortExceptKey_devfunc_logical 输出）
     * @param uele 输出分区表
     * @param id 排除的键寄存器 ID
     */
    void Unique_count_elem(CuSparseState& state, const thrust::device_vector<size_t>& indices, thrust::device_vector<unq_ele>& uele, int id);

    /**
     * @brief 设备侧基态比较：除 id 外按激活寄存器字典序小于，id 位作决胜
     * @param lhs 左基态
     * @param rhs 右基态
     * @param mp_num 寄存器表大小
     * @param status_bitmap 激活状态位图（函数内会清掉 id 位）
     * @param id 键寄存器 ID（最后参与比较）
     * @return lhs < rhs
     */
    HOST_DEVICE inline bool cu_compare_less(const System& lhs, const System& rhs, int mp_num, uint64_t status_bitmap, int id)
    {
        status_bitmap -= pow2(id);
        for (int i = 0; i < mp_num; ++i)
        {
            size_t flag = (status_bitmap >> i) & 1;
            uint64_t value_left = CuGet(lhs, i).value * flag;
            uint64_t value_right = CuGet(rhs, i).value * flag;
            if (value_left < value_right) {
                return true;
            }
            else if (value_left > value_right) {
                return false;
            }
        }
        return CuGet(lhs, id).value < CuGet(rhs, id).value;
    }

    /**
     * @brief 除键外字典序小于的 thrust 仿函数
     * @details 构造时缓存寄存器表大小与激活状态位图，
     *          键寄存器 id 排在最后参与比较（稳定排序语义）
     */
    struct CuSystemLessExceptKey {
        int mp_num;
        int id;
        uint64_t status_bitmap;

        /**
         * @brief 构造函数
         * @param id_ 键寄存器 ID
         */
        CuSystemLessExceptKey(int id_)
            : id(id_), mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
        }

        /**
         * @brief 比较 lhs < rhs（除键外字典序，键位决胜）
         * @param lhs 左基态
         * @param rhs 右基态
         * @return 是否小于
         */
        HOST_DEVICE bool operator()(const System& lhs, const System& rhs)
        {
            return cu_compare_less(lhs, rhs, mp_num, status_bitmap, id);
        };
    };

    /**
     * @brief 除键外字典序小于的 thrust 仿函数（索引版）
     * @details 通过下标间接访问基态数组，配合 thrust::sequence 使用，
     *          实现只搬动索引不搬动数据的"逻辑排序"
     */
    struct CuSystemLessExceptKey_Index {
        const System* objects;
        int id;
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief 构造函数
         * @param ptr 基态数组首指针
         * @param id_ 键寄存器 ID
         */
        CuSystemLessExceptKey_Index(const System* ptr, int id_)
            : objects(ptr), id(id_), mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
        }

        /**
         * @brief 比较下标 left/right 指向的基态（除键外字典序）
         * @param left 左下标
         * @param right 右下标
         * @return 是否小于
         */
        __host__ __device__ bool operator()(size_t left, size_t right) const {
            const System& lhs = objects[left];
            const System& rhs = objects[right];

            return cu_compare_less(lhs, rhs, mp_num, status_bitmap, id);
        }
    };


    /**
     * @brief 设备侧基态比较：激活寄存器值全部相等
     * @param lhs 左基态
     * @param rhs 右基态
     * @param mp_num 寄存器表大小
     * @param status_bitmap 激活状态位图
     * @return 激活寄存器值是否全部相等
     */
    HOST_DEVICE inline bool cu_compare_equal(const System& lhs, const System& rhs, int mp_num, uint64_t status_bitmap)
    {
        for (int i = 0; i < mp_num; ++i)
        {
            size_t flag = (status_bitmap >> i) & 1;
            size_t value_left = CuGet(lhs, i).value * flag;
            size_t value_right = CuGet(rhs, i).value * flag;
            if (value_left != value_right) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 除键外相等的 thrust 仿函数
     * @details 构造时缓存寄存器表大小，并从激活位图中清掉键寄存器 id 位
     */
    struct CuSystemEqualExceptKey {
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief 构造函数（清掉键寄存器 id 的激活位）
         * @param id 键寄存器 ID
         */
        CuSystemEqualExceptKey(int id)
            : mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
            status_bitmap -= pow2(id);
        }

        /**
         * @brief 比较两基态除键外是否相等
         * @param lhs 左基态
         * @param rhs 右基态
         * @return 是否相等
         */
        HOST_DEVICE bool operator()(const System& lhs, const System& rhs)
        {
            return cu_compare_equal(lhs, rhs, mp_num, status_bitmap);
        };
    };

    /**
     * @brief 除键外相等的 thrust 仿函数（索引版）
     * @details 通过下标间接访问基态数组，配合逻辑排序使用
     */
    struct CuSystemEqualExceptKey_Index {
        const System* objects;
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief 构造函数（清掉键寄存器 id 的激活位）
         * @param ptr 基态数组首指针
         * @param id 键寄存器 ID
         */
        CuSystemEqualExceptKey_Index(const System* ptr, int id)
            : objects(ptr), mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
            status_bitmap -= pow2(id);
        }

        /**
         * @brief 比较下标 left/right 指向的基态除键外是否相等
         * @param left 左下标
         * @param right 右下标
         * @return 是否相等
         */
        __host__ __device__ bool operator()(size_t left, size_t right) const {
            const System& lhs = objects[left];
            const System& rhs = objects[right];

            return cu_compare_equal(lhs, rhs, mp_num, status_bitmap);
        }
    };

    /**
     * @brief 设备侧基态比较：存在激活寄存器取值不等
     * @param lhs 左基态
     * @param rhs 右基态
     * @param mp_num 寄存器表大小
     * @param status_bitmap 激活状态位图
     * @return 是否存在不等位
     */
    HOST_DEVICE inline bool cu_compare_not_equal(const System& lhs, const System& rhs, int mp_num, uint64_t status_bitmap)
    {
        for (int i = 0; i < mp_num; ++i)
        {
            size_t flag = (status_bitmap >> i) & 1;
            size_t value_left = CuGet(lhs, i).value * flag;
            size_t value_right = CuGet(rhs, i).value * flag;
            if (value_left == value_right) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 除键外不等的 thrust 仿函数（注意：语义为"存在任何不等位"）
     * @details 构造时缓存寄存器表大小，并清掉键寄存器 id 的激活位
     */
    struct CuSystemNotEqualExceptKey {
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief 构造函数（清掉键寄存器 id 的激活位）
         * @param id 键寄存器 ID
         */
        CuSystemNotEqualExceptKey(int id)
            : mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
            status_bitmap -= pow2(id);
        }

        /**
         * @brief 比较两基态是否存在除键外的不等位
         * @param lhs 左基态
         * @param rhs 右基态
         * @return 是否存在不等位
         */
        HOST_DEVICE bool operator()(const System& lhs, const System& rhs)
        {
            return cu_compare_not_equal(lhs, rhs, mp_num, status_bitmap);
        };
    };

    /**
     * @brief 除键外不等的 thrust 仿函数（索引版）
     * @details 通过下标间接访问基态数组，配合逻辑排序使用
     */
    struct CuSystemNotEqualExceptKey_Index {
        const System* objects;
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief 构造函数（清掉键寄存器 id 的激活位）
         * @param ptr 基态数组首指针
         * @param id 键寄存器 ID
         */
        CuSystemNotEqualExceptKey_Index(const System* ptr, int id)
            : objects(ptr), mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
            status_bitmap -= pow2(id);
        }

        /**
         * @brief 比较下标 left/right 指向的基态是否存在不等位
         * @param left 左下标
         * @param right 右下标
         * @return 是否存在不等位
         */
        __host__ __device__ bool operator()(size_t left, size_t right) const {
            const System& lhs = objects[left];
            const System& rhs = objects[right];

            return cu_compare_not_equal(lhs, rhs, mp_num, status_bitmap);
        }
    };

    /**
     * @brief 仅按单个寄存器值比较小于的 thrust 仿函数
     */
    struct CuSystemLessByKey {
        size_t id;

        /**
         * @brief 构造函数
         * @param id_ 键寄存器 ID
         */
        CuSystemLessByKey(size_t id_) : id(id_) {}

        /**
         * @brief 比较两基态键寄存器值
         * @param lhs 左基态
         * @param rhs 右基态
         * @return lhs 键值 < rhs 键值
         */
        __host__ __device__ uint64_t operator()(const System& lhs, const System& rhs) const {
            return CuGet(lhs, id).value < CuGet(rhs, id).value;
        }
    };

    /**
     * @brief 仅按单个寄存器值比较小于的 thrust 仿函数（索引版）
     */
    struct CuSystemLessByKey_Index {
        const System* objects; // 指向原始数据的指针
        size_t idi;

        /**
         * @brief 构造函数
         * @param ptr 基态数组首指针
         * @param id 键寄存器 ID
         */
        CuSystemLessByKey_Index(const System* ptr, size_t id) : objects(ptr), idi(id)
        {
        }

        /**
         * @brief 比较下标 left/right 指向基态的键寄存器值
         * @param left 左下标
         * @param right 右下标
         * @return 是否小于
         */
        __host__ __device__ bool operator()(size_t left, size_t right) const {
            const System& lhs = objects[left];
            const System& rhs = objects[right];
            return CuGet(lhs, idi).value < CuGet(rhs, idi).value;
        }
    };

    /**
     * @brief 基态按激活寄存器组合哈希的 thrust 仿函数
     * @details 使用黄金比例乘法散列（FNV 风格混合），只计入
     *          count_bitmap 标记的寄存器，供 thrust 去重/分组使用
     */
    /* For hash function */
    struct CuStateHashExceptKey {

        size_t name_reg_map_size;
        size_t count_bitmap;

        /**
         * @brief 构造函数
         * @param bitmap 计入哈希的寄存器位图
         */
        CuStateHashExceptKey(size_t bitmap)
            : count_bitmap(bitmap)
        {
            name_reg_map_size = System::name_register_map.size();
        }

        /**
         * @brief 计算基态哈希值
         * @param sys 基态
         * @return 哈希值
         */
        __host__ __device__ uint64_t operator()(const System& sys) const {
            const uint64_t prime = 0x9e3779b97f4a7c15;
            uint64_t hash = 0;
            for (size_t i = 0; i < name_reg_map_size; ++i)
            {
                uint64_t new_hash = hash ^ CuGet(sys, i).value;
                new_hash *= prime;

                hash = ((count_bitmap >> i) & 1) ? new_hash : hash;
            }
            return hash;
        }
    };

    /**
     * @brief 对分区表按分区大小排序
     * @param dat 分区表（原地排序）
     */
    /* Sort the unique elements */
    void SortUniqueElements(thrust::device_vector<unq_ele>& dat);

    /**
     * @brief 求分区表中元素数大于 1 的分区个数
     * @details 返回"成对/多分支"分区数（unique 分区表前段为多元素分区时的切分点）
     * @param dat 分区表
     * @return 元素数 > 1 的分区个数
     */
    /* Get the partition point of <1> and <2> */
    size_t EleNum_MoreThanOne(thrust::device_vector<unq_ele>& dat);

    /**
     * @brief 求分区表中元素数不足 full_size 的分区个数
     * @param dat 分区表
     * @param full_size 满员分区应有的元素数（如键寄存器为布尔时的 2）
     * @return 元素数 < full_size 的分区个数
     */
    /* Get the partition point of <N-1> */
    size_t EleNum_NotFull(thrust::device_vector<unq_ele>& dat, size_t full_size);
}
