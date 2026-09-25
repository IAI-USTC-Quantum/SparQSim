/**
 * @file sparse_state_simulator.cpp
 * @brief 稀疏态模拟器公共实现
 * @details 实现 sparse_state_simulator.h 声明的公共包含入口与相关辅助逻辑
 */
#include "sparse_state_simulator.h"
#include "debugger.h"

namespace qram_simulator
{

	std::string SparseState::to_string(int32_t display, int precision) const
	{
		StatePrint printer(display);
		printer.precision = precision;
		return printer.to_string(
			const_cast<std::vector<System>&>(basis_states));
	}

} // namespace qram_simulator