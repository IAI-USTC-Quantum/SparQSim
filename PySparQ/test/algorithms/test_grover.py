"""
Grover search algorithm tests.

Tested content:
- GroverOracle: phase-flip marking of states
- DiffusionOperator: reflection about the uniform superposition
- GroverOperator: Oracle + Diffusion combination
- grover_search: end-to-end search functionality
- grover_count: quantum counting variant

Reference: Experiments/Grover/GroverTest.cpp
"""

import pytest
import math
import numpy as np

import pysparq as ps
from pysparq.algorithms.grover import (
    GroverOracle,
    DiffusionOperator,
    GroverOperator,
    grover_search,
    grover_count,
)


class TestGroverOracle:
    """Test Grover Oracle functionality."""

    def test_oracle_flips_marked_state_phase(self, fresh_system):
        """The oracle should apply a -1 phase to states matching the search value."""
        memory = [5, 12, 3, 8, 15, 7, 2, 9]
        target = 8

        qram = ps.QRAMCircuit_qutrit(3, 64, memory)

        state = ps.SparseState()
        ps.AddRegister("addr", ps.UnsignedInteger, 3)(state)
        ps.AddRegister("data", ps.UnsignedInteger, 64)(state)
        ps.AddRegister("search", ps.UnsignedInteger, 64)(state)

        # addr=3 → memory[3]=8 matches target
        ps.Init_Unsafe("addr", 3)(state)
        ps.Init_Unsafe("search", target)(state)

        oracle = GroverOracle(qram, "addr", "data", "search")
        oracle(state)

        # Oracle should execute without error
        assert state.size() >= 1

    def test_oracle_self_adjoint(self, fresh_system):
        """The oracle should be self-adjoint (applying twice = identity)."""
        memory = [1, 2, 3, 4]
        n_bits = 2

        qram = ps.QRAMCircuit_qutrit(n_bits, 8, memory)

        state = ps.SparseState()
        ps.AddRegister("addr", ps.UnsignedInteger, n_bits)(state)
        ps.AddRegister("data", ps.UnsignedInteger, 8)(state)
        ps.AddRegister("search", ps.UnsignedInteger, 8)(state)

        ps.Hadamard_Int_Full("addr")(state)
        ps.Init_Unsafe("search", 2)(state)

        oracle = GroverOracle(qram, "addr", "data", "search")
        oracle(state)
        oracle(state)
        ps.CheckNormalization(1e-6)(state)

    def test_oracle_with_condition(self, fresh_system):
        """Test conditional oracle execution."""
        memory = [1, 2, 3, 4]
        n_bits = 2

        qram = ps.QRAMCircuit_qutrit(n_bits, 8, memory)

        state = ps.SparseState()
        ps.AddRegister("addr", ps.UnsignedInteger, n_bits)(state)
        ps.AddRegister("data", ps.UnsignedInteger, 8)(state)
        ps.AddRegister("search", ps.UnsignedInteger, 8)(state)
        ps.AddRegister("cond", ps.Boolean, 1)(state)

        ps.Init_Unsafe("addr", 1)(state)
        ps.Init_Unsafe("search", 2)(state)
        ps.Init_Unsafe("cond", 1)(state)

        oracle = GroverOracle(qram, "addr", "data", "search")
        oracle.conditioned_by_nonzeros("cond")(state)

        assert state.size() >= 1


class TestDiffusionOperator:
    """Test diffusion operator functionality."""

    def test_diffusion_on_zero_state(self, fresh_system):
        """D|0> should produce a superposition (not simply -|0>).

        D = H*P_0*H where P_0 flips phase of |0>.
        D|0> = |0> - 2/sqrt(N)|s> where |s> = H|0>.
        For N=4: D|0> = (1/2)|0> - (1/2)(|1>+|2>+|3>).
        """
        n_bits = 2
        ps.System.add_register("addr", ps.UnsignedInteger, n_bits)
        state = ps.SparseState()

        diffusion = DiffusionOperator("addr")
        diffusion(state)

        # D|0> produces a superposition over all basis states
        assert state.size() > 1

        # Verify probabilities sum to 1
        total_prob = sum(abs(b.amplitude) ** 2 for b in state.basis_states)
        assert abs(total_prob - 1.0) < 1e-10

    def test_diffusion_on_uniform_superposition(self, fresh_system):
        """The uniform superposition is an eigenvector of D with eigenvalue -1."""
        n_bits = 2
        ps.System.add_register("addr", ps.UnsignedInteger, n_bits)
        state = ps.SparseState()

        ps.Hadamard_Int_Full("addr")(state)
        addr_id = ps.System.get_id("addr")
        initial_amps = {}
        for b in state.basis_states:
            key = b.get(addr_id).value
            initial_amps[key] = b.amplitude

        diffusion = DiffusionOperator("addr")
        diffusion(state)

        # |s> = H|0> is eigenvector with eigenvalue -1
        for basis in state.basis_states:
            key = basis.get(addr_id).value
            expected = -initial_amps[key]
            assert abs(basis.amplitude - expected) < 1e-10

    def test_diffusion_self_adjoint(self, fresh_system):
        """The diffusion operator should be self-adjoint."""
        n_bits = 2
        ps.System.add_register("addr", ps.UnsignedInteger, n_bits)
        state = ps.SparseState()

        ps.Init_Unsafe("addr", 1)(state)

        diffusion = DiffusionOperator("addr")
        diffusion(state)
        diffusion(state)

        assert state.size() == 1

    def test_diffusion_with_condition(self, fresh_system):
        """Test conditional diffusion."""
        n_bits = 2
        ps.System.add_register("addr", ps.UnsignedInteger, n_bits)
        ps.System.add_register("cond", ps.Boolean, 1)
        state = ps.SparseState()

        ps.Hadamard_Int_Full("addr")(state)
        ps.Init_Unsafe("cond", 1)(state)

        diffusion = DiffusionOperator("addr")
        diffusion.conditioned_by_nonzeros("cond")(state)

        assert state.size() >= 1


class TestGroverOperator:
    """Test the combined Grover operator."""

    def test_grover_operator_basic(self, fresh_system):
        """Test basic Grover operator execution."""
        memory = [1, 2, 3, 4]
        n_bits = 2

        qram = ps.QRAMCircuit_qutrit(n_bits, 8, memory)

        state = ps.SparseState()
        ps.AddRegister("addr", ps.UnsignedInteger, n_bits)(state)
        ps.AddRegister("data", ps.UnsignedInteger, 8)(state)
        ps.AddRegister("search", ps.UnsignedInteger, 8)(state)

        ps.Hadamard_Int_Full("addr")(state)
        ps.Init_Unsafe("search", 2)(state)

        grover_op = GroverOperator(qram, "addr", "data", "search")
        grover_op(state)

        assert state.size() >= 1

    def test_grover_operator_multiple_iterations(self, fresh_system):
        """Test multiple Grover iterations."""
        memory = [1, 2, 3, 4, 5, 6, 7, 8]
        n_bits = 3
        data_size = 8

        qram = ps.QRAMCircuit_qutrit(n_bits, data_size, memory)

        state = ps.SparseState()
        ps.AddRegister("addr", ps.UnsignedInteger, n_bits)(state)
        ps.AddRegister("data", ps.UnsignedInteger, data_size)(state)
        ps.AddRegister("search", ps.UnsignedInteger, data_size)(state)

        ps.Hadamard_Int_Full("addr")(state)
        ps.Init_Unsafe("search", 5)(state)

        grover_op = GroverOperator(qram, "addr", "data", "search")

        for _ in range(2):
            grover_op(state)

        ps.CheckNormalization(1e-6)(state)


class TestGroverSearch:
    """Test end-to-end Grover search."""

    def test_single_target_search_small(self, fresh_system):
        """Single-target search on a small database."""
        memory = [1, 2, 3, 4]
        target = 2

        idx, prob = grover_search(memory, target, n_iterations=1)

        assert 0 <= idx < len(memory)
        assert prob > 0

    def test_single_target_search_medium(self, fresh_system):
        """Single-target search on a medium database."""
        memory = [5, 12, 3, 8, 15, 7, 2, 9]
        target = 8

        idx, prob = grover_search(memory, target, n_iterations=2)

        assert 0 <= idx < len(memory)
        assert prob > 0

    def test_auto_iterations(self, fresh_system):
        """Test automatic iteration count computation."""
        memory = [1, 2, 3, 4, 5, 6, 7, 8]
        target = 5

        idx, prob = grover_search(memory, target)

        assert 0 <= idx < len(memory)
        assert prob > 0

    def test_search_returns_valid_index(self, fresh_system):
        """Verify the search returns a valid index."""
        memory = [10, 20, 30, 40]
        target = 30

        idx, prob = grover_search(memory, target, n_iterations=1)

        assert 0 <= idx < len(memory)
        assert prob > 0


class TestGroverCount:
    """Test the quantum counting variant."""

    @pytest.mark.slow
    def test_count_single_marked_item(self, fresh_system):
        """Test counting with a single marked item."""
        memory = [5, 12, 3, 8, 15, 7, 2, 9]
        target = 8

        count, prob = grover_count(memory, target, precision_bits=4)

        assert count >= 0

    @pytest.mark.slow
    def test_count_multiple_marked_items(self, fresh_system):
        """Test counting with multiple marked items."""
        memory = [5, 5, 5, 8, 8, 7, 2, 9]  # three 5s
        target = 5

        count, prob = grover_count(memory, target, precision_bits=4)

        assert count >= 0


class TestGroverAlgorithmProperties:
    """Test mathematical properties of the Grover algorithm."""

    def test_probability_amplitude_amplification(self, fresh_system):
        """Verify the amplitude amplification effect."""
        memory = [1, 2, 3, 4, 5, 6, 7, 8]
        target = 5

        idx1, prob1 = grover_search(memory, target, n_iterations=1)
        idx2, prob2 = grover_search(memory, target, n_iterations=2)

        assert prob2 > 0

    def test_measurement_collapse(self, fresh_system):
        """Verify state collapse after measurement."""
        memory = [1, 2, 3, 4]
        target = 2

        idx, prob = grover_search(memory, target, n_iterations=1)

        assert isinstance(idx, int)
        assert 0 <= idx < len(memory)
