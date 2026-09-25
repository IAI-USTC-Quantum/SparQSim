/**
 * @file dark_magic.cpp
 * @brief 暗魔法算子实现
 * @details 实现 dark_magic.h 中声明的特殊优化算子（针对特定电路结构的手工优化路径）
 */
#include "dark_magic.h"

namespace qram_simulator
{
	void Normalize::operator()(std::vector<System>& state) const
	{
		profiler _("Normalize");
		double sum_prob = std::accumulate(
			state.begin(), state.end(), 0.0,
			[](double x, const System& s)
			{
				return x += abs_sqr(s.amplitude);
			}
		);

		sum_prob = 1.0 / std::sqrt(sum_prob);

		if (std::isnan(sum_prob))
			throw_bad_result();

		// 3. renormalize
		std::for_each(exec_policy, state.begin(), state.end(),
			[sum_prob](System& s) { s.amplitude *= sum_prob; }
		);

		// CheckNormalization()(state);
	}

	void Init_Unsafe::operator()(std::vector<System>& system_states) const
	{
		for (auto& s : system_states)
		{
			s.get(id).value = value;
		}
	}
}