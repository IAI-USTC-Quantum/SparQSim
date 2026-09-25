/**
 * @file quantum_interfere_basic.cuh
 * @brief GPU-side basic components for sparse-state interference
 * @details Provides GPU parallel primitives for sparse state updates: grouping by
 *          "all equal except the key" (unq_ele partition table + unique_* kernels),
 *          sorting with the key excluded (SortExceptKey_devfunc), various thrust comparison
 *          functors (compare by key / compare except key / index versions), a state hash functor,
 *          and partition table helpers (SortUniqueElements / EleNum_MoreThanOne / EleNum_NotFull).
 *          This is the common foundation of GPU implementations such as the controlled
 *          rotation in condrot.cuh, and corresponds semantically to the CPU-side
 *          SparQ/include/quantum_interfere_basic.h
 */

#pragma once

#include "quantum_interfere_basic.h"
#include "basic_components.h"
#include "cuda_utils.cuh"
#include "basic_components.cuh"

namespace qram_simulator {

    /**
     * @brief Sparse state partition descriptor unit
     * @details Used to divide the state sequence, sorted by key (the register combination that is
     *          identical except for one register), into contiguous partitions; it is the basis for
     *          group-wise processing in GPU-side interference/rotation operations
     */
    struct unq_ele {
        size_t sptr; /* The position of the first element of the partition */
        unsigned int num; /* The number of elements in the partition */
    };

    /* GPU-version: SortExceptKey() */
    /**
     * @brief Sort the sparse state by key except register idi (GPU version of SortExceptKey)
     * @param idi Register ID used as the key (excluded from the sort)
     * @param state GPU-side sparse state (rearranged in place)
     */
    void SortExceptKey_devfunc(int idi, thrust::device_vector<System>& state);

    /**
     * @brief "Logical index" version of the sort by key except register idi (does not move data)
     * @param idi Register ID used as the key (excluded from the sort)
     * @param state GPU-side sparse state (read-only)
     * @return Vector of sorted indices (owned by a cache, reused on repeated calls)
     */
    const thrust::device_vector<size_t>& SortExceptKey_devfunc_logical(int idi, const thrust::device_vector<System>& state);

    /**
     * @brief Device-side comparison of whether two basis states are equal in all active registers except out_id
     * @param a Left basis state
     * @param b Right basis state
     * @param out_id Excluded register ID
     * @param mp_sz Register table size
     * @param status_bitmap Activation status bitmap
     * @return All active register values except out_id are equal
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
     * @brief Mark the first element of each partition in the sorted state sequence (kernel)
     * @param dat Basis-state array (device pointer)
     * @param num Partition count output (device pointer, a single size_t)
     * @param nsize Total number of basis states
     * @param reg_stat Activation status bitmap
     * @param mpsz Register table size
     * @param id Excluded key register ID
     */
    // find first unique element of a sorted state series
    __global__ void unique_find_elem(System* dat, size_t* num, size_t nsize,
        uint64_t reg_stat, int mpsz, int id);

    /**
     * @brief Mark the first element of each partition in the sorted state sequence (kernel, logical-index version)
     * @param dat Basis-state array (device pointer)
     * @param indices Logical ordering index array (device pointer)
     * @param num Partition count output (device pointer, a single size_t)
     * @param nsize Total number of basis states
     * @param reg_stat Activation status bitmap
     * @param mpsz Register table size
     * @param id Excluded key register ID
     */
    __global__ void unique_find_elem(System* dat, const size_t* indices, size_t* num,
        size_t nsize, uint64_t reg_stat, int mpsz, int id);

    /**
     * @brief Count the number of elements in each partition of the sorted sequence (kernel)
     * @param dat Partition-first marker array (device pointer)
     * @param uele Partition table output (device pointer)
     * @param nsize Marker array length
     * @param nstate Total number of basis states
     */
    // count number of each unique element
    __global__ void unique_count_elem(size_t* dat, unq_ele* uele, size_t nsize, size_t nstate);

    /**
     * @brief Partition the sparse state by key (sort first, then mark; direct-sort version)
     * @param state GPU sparse state
     * @param uele Output partition table
     * @param id Excluded key register ID
     */
    void Unique_count_elem(CuSparseState& state, thrust::device_vector<unq_ele>& uele, int id);

    /**
     * @brief Partition the sparse state by key (reusing the logical-index sort)
     * @param state GPU sparse state
     * @param indices Pre-sorted logical indices (output of SortExceptKey_devfunc_logical)
     * @param uele Output partition table
     * @param id Excluded key register ID
     */
    void Unique_count_elem(CuSparseState& state, const thrust::device_vector<size_t>& indices, thrust::device_vector<unq_ele>& uele, int id);

    /**
     * @brief Device-side comparison: lexicographic less-than by active registers except id, id bit as tie-breaker
     * @param lhs Left basis state
     * @param rhs Right basis state
     * @param mp_num Register table size
     * @param status_bitmap Activation status bitmap (the id bit is cleared inside the function)
     * @param id Key register ID (compared last)
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
     * @brief Thrust functor for lexicographic less-than with the key excluded
     * @details Caches the register table size and the activation status bitmap at construction time;
     *          the key register id is compared last (stable-sort semantics)
     */
    struct CuSystemLessExceptKey {
        int mp_num;
        int id;
        uint64_t status_bitmap;

        /**
         * @brief Constructor
         * @param id_ Key register ID
         */
        CuSystemLessExceptKey(int id_)
            : id(id_), mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
        }

        /**
         * @brief Compare lhs < rhs (lexicographic except key, key bit as tie-breaker)
         * @param lhs Left basis state
         * @param rhs Right basis state
         * @return Whether less than
         */
        HOST_DEVICE bool operator()(const System& lhs, const System& rhs)
        {
            return cu_compare_less(lhs, rhs, mp_num, status_bitmap, id);
        };
    };

    /**
     * @brief Thrust functor for lexicographic less-than with the key excluded (index version)
     * @details Accesses the basis-state array indirectly through indices, used with thrust::sequence,
     *          implementing a "logical sort" that moves only indices, not data
     */
    struct CuSystemLessExceptKey_Index {
        const System* objects;
        int id;
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief Constructor
         * @param ptr Pointer to the start of the basis-state array
         * @param id_ Key register ID
         */
        CuSystemLessExceptKey_Index(const System* ptr, int id_)
            : objects(ptr), id(id_), mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
        }

        /**
         * @brief Compare the basis states pointed to by indices left/right (lexicographic except key)
         * @param left Left index
         * @param right Right index
         * @return Whether less than
         */
        __host__ __device__ bool operator()(size_t left, size_t right) const {
            const System& lhs = objects[left];
            const System& rhs = objects[right];

            return cu_compare_less(lhs, rhs, mp_num, status_bitmap, id);
        }
    };


    /**
     * @brief Device-side basis-state comparison: all active register values equal
     * @param lhs Left basis state
     * @param rhs Right basis state
     * @param mp_num Register table size
     * @param status_bitmap Activation status bitmap
     * @return Whether all active register values are equal
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
     * @brief Thrust functor for equality with the key excluded
     * @details Caches the register table size at construction time and clears the key register
     *          id bit from the activation bitmap
     */
    struct CuSystemEqualExceptKey {
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief Constructor (clears the activation bit of key register id)
         * @param id Key register ID
         */
        CuSystemEqualExceptKey(int id)
            : mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
            status_bitmap -= pow2(id);
        }

        /**
         * @brief Compare whether two basis states are equal except for the key
         * @param lhs Left basis state
         * @param rhs Right basis state
         * @return Whether equal
         */
        HOST_DEVICE bool operator()(const System& lhs, const System& rhs)
        {
            return cu_compare_equal(lhs, rhs, mp_num, status_bitmap);
        };
    };

    /**
     * @brief Thrust functor for equality with the key excluded (index version)
     * @details Accesses the basis-state array indirectly through indices, used with the logical sort
     */
    struct CuSystemEqualExceptKey_Index {
        const System* objects;
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief Constructor (clears the activation bit of key register id)
         * @param ptr Pointer to the start of the basis-state array
         * @param id Key register ID
         */
        CuSystemEqualExceptKey_Index(const System* ptr, int id)
            : objects(ptr), mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
            status_bitmap -= pow2(id);
        }

        /**
         * @brief Compare whether the basis states pointed to by indices left/right are equal except for the key
         * @param left Left index
         * @param right Right index
         * @return Whether equal
         */
        __host__ __device__ bool operator()(size_t left, size_t right) const {
            const System& lhs = objects[left];
            const System& rhs = objects[right];

            return cu_compare_equal(lhs, rhs, mp_num, status_bitmap);
        }
    };

    /**
     * @brief Device-side basis-state comparison: an active register exists whose values differ
     * @param lhs Left basis state
     * @param rhs Right basis state
     * @param mp_num Register table size
     * @param status_bitmap Activation status bitmap
     * @return Whether any differing register exists
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
     * @brief Thrust functor for inequality with the key excluded (note: semantics is "any unequal position exists")
     * @details Caches the register table size at construction time and clears the key register id activation bit
     */
    struct CuSystemNotEqualExceptKey {
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief Constructor (clears the activation bit of key register id)
         * @param id Key register ID
         */
        CuSystemNotEqualExceptKey(int id)
            : mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
            status_bitmap -= pow2(id);
        }

        /**
         * @brief Compare whether any unequal position except the key exists between two basis states
         * @param lhs Left basis state
         * @param rhs Right basis state
         * @return Whether any unequal position exists
         */
        HOST_DEVICE bool operator()(const System& lhs, const System& rhs)
        {
            return cu_compare_not_equal(lhs, rhs, mp_num, status_bitmap);
        };
    };

    /**
     * @brief Thrust functor for inequality with the key excluded (index version)
     * @details Accesses the basis-state array indirectly through indices, used with the logical sort
     */
    struct CuSystemNotEqualExceptKey_Index {
        const System* objects;
        int mp_num;
        uint64_t status_bitmap;

        /**
         * @brief Constructor (clears the activation bit of key register id)
         * @param ptr Pointer to the start of the basis-state array
         * @param id Key register ID
         */
        CuSystemNotEqualExceptKey_Index(const System* ptr, int id)
            : objects(ptr), mp_num(System::name_register_map.size()), status_bitmap(System::reg_status_bitmap)
        {
            status_bitmap -= pow2(id);
        }

        /**
         * @brief Compare whether any unequal position exists between the basis states at indices left/right
         * @param left Left index
         * @param right Right index
         * @return Whether any unequal position exists
         */
        __host__ __device__ bool operator()(size_t left, size_t right) const {
            const System& lhs = objects[left];
            const System& rhs = objects[right];

            return cu_compare_not_equal(lhs, rhs, mp_num, status_bitmap);
        }
    };

    /**
     * @brief Thrust functor comparing less-than by a single register value only
     */
    struct CuSystemLessByKey {
        size_t id;

        /**
         * @brief Constructor
         * @param id_ Key register ID
         */
        CuSystemLessByKey(size_t id_) : id(id_) {}

        /**
         * @brief Compare the key register values of two basis states
         * @param lhs Left basis state
         * @param rhs Right basis state
         * @return lhs key value < rhs key value
         */
        __host__ __device__ uint64_t operator()(const System& lhs, const System& rhs) const {
            return CuGet(lhs, id).value < CuGet(rhs, id).value;
        }
    };

    /**
     * @brief Thrust functor comparing less-than by a single register value only (index version)
     */
    struct CuSystemLessByKey_Index {
        const System* objects; // Pointer to the raw data
        size_t idi;

        /**
         * @brief Constructor
         * @param ptr Pointer to the start of the basis-state array
         * @param id Key register ID
         */
        CuSystemLessByKey_Index(const System* ptr, size_t id) : objects(ptr), idi(id)
        {
        }

        /**
         * @brief Compare the key register values of the basis states pointed to by indices left/right
         * @param left Left index
         * @param right Right index
         * @return Whether less than
         */
        __host__ __device__ bool operator()(size_t left, size_t right) const {
            const System& lhs = objects[left];
            const System& rhs = objects[right];
            return CuGet(lhs, idi).value < CuGet(rhs, idi).value;
        }
    };

    /**
     * @brief Thrust functor hashing basis states by the combination of active registers
     * @details Uses golden-ratio multiplicative hashing (FNV-style mixing); only registers
     *          marked in count_bitmap are included, for thrust deduplication/grouping
     */
    /* For hash function */
    struct CuStateHashExceptKey {

        size_t name_reg_map_size;
        size_t count_bitmap;

        /**
         * @brief Constructor
         * @param bitmap Bitmap of registers included in the hash
         */
        CuStateHashExceptKey(size_t bitmap)
            : count_bitmap(bitmap)
        {
            name_reg_map_size = System::name_register_map.size();
        }

        /**
         * @brief Compute the hash value of a basis state
         * @param sys Basis state
         * @return Hash value
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
     * @brief Sort the partition table by partition size
     * @param dat Partition table (sorted in place)
     */
    /* Sort the unique elements */
    void SortUniqueElements(thrust::device_vector<unq_ele>& dat);

    /**
     * @brief Count the partitions in the partition table with more than 1 element
     * @details Returns the number of "paired/multi-branch" partitions (the split point when the front
     *          segment of the unique partition table holds multi-element partitions)
     * @param dat Partition table
     * @return Number of partitions with element count > 1
     */
    /* Get the partition point of <1> and <2> */
    size_t EleNum_MoreThanOne(thrust::device_vector<unq_ele>& dat);

    /**
     * @brief Count the partitions in the partition table with fewer than full_size elements
     * @param dat Partition table
     * @param full_size Element count a full partition should have (e.g. 2 when the key register is boolean)
     * @return Number of partitions with element count < full_size
     */
    /* Get the partition point of <N-1> */
    size_t EleNum_NotFull(thrust::device_vector<unq_ele>& dat, size_t full_size);
}
