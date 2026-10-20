/**
 * @file hamiltonian_tridiagonal.cpp
 * @brief CPU implementations of the direct tridiagonal-walk oracles
 * @details See SparQ_Algorithm/include/hamiltonian_simulation_tridiagonal.h:
 *          the oracles are bit-exact arithmetic mirrors of the QRAM-path
 *          composite register maps (combined-memory XOR loads, window binary
 *          search, row-address add/sub, k_comp junk tagging).
 */

#include "hamiltonian_simulation_tridiagonal.h"

namespace qram_simulator {

	namespace CKS {

		void TridiagOracle1::operator()(std::vector<System>& state) const
		{
			profiler _("TridiagOracle1");
#ifdef SINGLE_THREAD
			for (auto& s : state)
			{
#else
#pragma omp parallel for
			for (int i = 0; i < state.size(); ++i)
			{
				auto& s = state[i];
#endif
				uint64_t off = s.GetAs(offset_id, uint64_t);
				uint64_t j = s.GetAs(row_id, uint64_t);
				uint64_t slot = s.GetAs(slot_id, uint64_t);
				s.get(out_id).value ^=
					spec.combined_mem(off + TridiagonalSpec::slot_count * j + slot);
			}
		}

		void TridiagOracle2::operator()(std::vector<System>& state) const
		{
			profiler _("TridiagOracle2");
			uint64_t kmask = pow2(spec.k_width) - 1;
#ifdef SINGLE_THREAD
			for (auto& s : state)
			{
#else
#pragma omp parallel for
			for (int i = 0; i < state.size(); ++i)
			{
				auto& s = state[i];
#endif
				uint64_t j = s.GetAs(row_id, uint64_t);
				uint64_t off = s.GetAs(offset_id, uint64_t);
				uint64_t k = s.GetAs(k_id, uint64_t);
				uint64_t sr = s.GetAs(sr_id, uint64_t);
				uint64_t base = spec.window_base(size_t(j));
				uint64_t row_addr = off + TridiagonalSpec::slot_count * j;
				bool hit = k >= base && k < base + TridiagonalSpec::slot_count;
				uint64_t sr1 = sr ^ (hit ? row_addr + (k - base) : 0);
				uint64_t k1 = k ^ spec.combined_mem(sr1);
				s.get(k_id).value = (sr1 - row_addr) & kmask;
				s.get(sr_id).value = k1 & kmask;
			}
		}

		void TridiagOracle2::dag(std::vector<System>& state) const
		{
			profiler _("TridiagOracle2.dag");
			uint64_t kmask = pow2(spec.k_width) - 1;
#ifdef SINGLE_THREAD
			for (auto& s : state)
			{
#else
#pragma omp parallel for
			for (int i = 0; i < state.size(); ++i)
			{
				auto& s = state[i];
#endif
				uint64_t j = s.GetAs(row_id, uint64_t);
				uint64_t off = s.GetAs(offset_id, uint64_t);
				uint64_t k = s.GetAs(k_id, uint64_t);
				uint64_t sr = s.GetAs(sr_id, uint64_t);
				uint64_t base = spec.window_base(size_t(j));
				uint64_t row_addr = off + TridiagonalSpec::slot_count * j;
				uint64_t k1 = (k + row_addr) & kmask;
				uint64_t k2 = sr ^ spec.combined_mem(k1);
				bool hit = k2 >= base && k2 < base + TridiagonalSpec::slot_count;
				s.get(k_id).value = k2;
				s.get(sr_id).value = k1 ^ (hit ? row_addr + (k2 - base) : 0);
			}
		}

		void TridiagParityPhase::operator()(std::vector<System>& state) const
		{
			profiler _("TridiagParityPhase");
#ifdef SINGLE_THREAD
			for (auto& s : state)
			{
#else
#pragma omp parallel for
			for (int i = 0; i < state.size(); ++i)
			{
				auto& s = state[i];
#endif
				if (s.GetAs(reg_id, uint64_t) & 1)
					s.amplitude = -s.amplitude;
			}
		}

	}

}
