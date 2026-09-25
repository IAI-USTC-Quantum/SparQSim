/**
 * @file sparse_state_simulator.cpp
 * @brief Common implementation of the sparse-state simulator
 * @details Implements the common include entry and related helper logic declared in sparse_state_simulator.h
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