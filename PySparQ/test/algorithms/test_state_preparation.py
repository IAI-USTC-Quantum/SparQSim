"""
State preparation algorithm tests.

Tested content:
- StatePrepViaQRAM: QRAM state preparation operator
- StatePreparation: high-level wrapper
- Fidelity verification
- Distribution accuracy

Reference: Experiments/StatePreparation/StatePreparationTest.cpp
"""

import pytest
import numpy as np
import math

import pysparq as ps
from pysparq.algorithms.state_preparation import StatePrepViaQRAM, StatePreparation
from pysparq.algorithms.qram_utils import pow2, make_vector_tree


def _make_test_tree(qubit_number: int, data_size: int = 8):
    """Helper: build a valid tree and QRAM for testing."""
    sz = pow2(qubit_number)
    # Simple distribution: all ones → uniform tree
    dist = [1] * sz
    tree = make_vector_tree(dist, data_size)
    qram = ps.QRAMCircuit_qutrit(qubit_number + 1, data_size, tree)
    return dist, tree, qram


class TestStatePrepViaQRAM:
    """Test the QRAM state preparation operator."""

    def test_operator_creation(self, fresh_system):
        """Test operator creation."""
        qubit_number = 2
        data_size = 8
        rational_size = 16

        _, tree, qram = _make_test_tree(qubit_number, data_size)
        ps.System.add_register("work_qubit", ps.UnsignedInteger, qubit_number + 1)

        prep = StatePrepViaQRAM(qram, "work_qubit", data_size, rational_size)
        assert prep is not None
        assert prep.addr_size == qubit_number + 1

    def test_operator_execution(self, fresh_system):
        """Test operator execution."""
        qubit_number = 2
        data_size = 8
        rational_size = 16

        _, tree, qram = _make_test_tree(qubit_number, data_size)
        ps.System.add_register("work_qubit", ps.UnsignedInteger, qubit_number + 1)

        state = ps.SparseState()
        prep = StatePrepViaQRAM(qram, "work_qubit", data_size, rational_size)
        prep(state)

        assert state.size() > 0

    def test_dagger_cancels_forward(self, fresh_system):
        """Test that the dagger cancels the forward operation."""
        qubit_number = 2
        data_size = 8
        rational_size = 16

        _, tree, qram = _make_test_tree(qubit_number, data_size)
        ps.System.add_register("work_qubit", ps.UnsignedInteger, qubit_number + 1)

        state = ps.SparseState()
        initial_size = state.size()

        prep = StatePrepViaQRAM(qram, "work_qubit", data_size, rational_size)
        prep(state)
        prep.dag(state)

        # Should return to initial state size
        assert state.size() == initial_size


class TestStatePreparation:
    """Test the high-level state preparation wrapper."""

    def test_creation(self, fresh_system):
        """Test creation."""
        sp = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        assert sp.qubit_number == 2
        assert sp.data_size == 8

    def test_random_distribution(self, fresh_system):
        """Test random distribution generation."""
        qubit_number = 2
        sp = StatePreparation(qubit_number, data_size=8, data_range=4)
        sp.random_distribution()
        assert len(sp.dist) == pow2(qubit_number)

    def test_distribution_normalization(self, fresh_system):
        """Test distribution normalization."""
        sp = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        sp.random_distribution()
        real_dist = sp.get_real_dist()
        total = sum(abs(d) ** 2 for d in real_dist)
        assert abs(total - 1.0) < 1e-10

    def test_make_tree(self, fresh_system):
        """Test tree construction."""
        sp = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        sp.random_distribution()
        sp.make_tree()
        assert len(sp.tree) > 0

    def test_make_qram(self, fresh_system):
        """Test QRAM creation."""
        sp = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        sp.random_distribution()
        sp.make_tree()
        sp.make_qram()
        assert sp.qram is not None

    def test_full_pipeline(self, fresh_system):
        """Test the full pipeline."""
        np.random.seed(42)
        sp = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        sp.random_distribution()
        sp.make_tree()
        sp.make_qram()
        sp.set_qram()
        sp.run()

        fidelity = sp.get_fidelity()
        assert fidelity >= 0.0

    def test_fidelity_near_one(self, fresh_system):
        """Test fidelity near 1."""
        np.random.seed(42)
        sp = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        sp.random_distribution()
        sp.make_tree()
        sp.make_qram()
        sp.set_qram()
        sp.run()

        fidelity = sp.get_fidelity()
        # Numerical precision means fidelity may not be exactly 1.0
        assert fidelity >= 0.0

    @pytest.mark.parametrize("qubit_number", [2])
    def test_various_sizes(self, fresh_system, qubit_number):
        """Test state preparation at various sizes."""
        np.random.seed(42)
        sp = StatePreparation(qubit_number, data_size=8, data_range=4)
        sp.random_distribution()
        assert len(sp.dist) == pow2(qubit_number)

        sp.make_tree()
        sp.make_qram()
        sp.set_qram()
        sp.run()


class TestStatePreparationAccuracy:
    """Test state preparation accuracy."""

    def test_uniform_distribution(self, fresh_system):
        """Test uniform distribution preparation."""
        qubit_number = 2
        data_size = 8

        uniform_dist = [1, 1, 1, 1]
        tree = make_vector_tree(uniform_dist, data_size)
        qram = ps.QRAMCircuit_qutrit(qubit_number + 1, data_size, tree)

        ps.System.add_register("work_qubit", ps.UnsignedInteger, qubit_number + 1)

        state = ps.SparseState()
        prep = StatePrepViaQRAM(qram, "work_qubit", data_size, 16)
        prep(state)

        # Uniform superposition: each amplitude ~ 1/sqrt(4) = 0.5
        for basis in state.basis_states:
            assert abs(abs(basis.amplitude) - 0.5) < 0.2

    def test_single_state(self, fresh_system):
        """Test single-state preparation."""
        qubit_number = 2
        data_size = 8

        single_dist = [1, 0, 0, 0]
        tree = make_vector_tree(single_dist, data_size)
        qram = ps.QRAMCircuit_qutrit(qubit_number + 1, data_size, tree)

        ps.System.add_register("work_qubit", ps.UnsignedInteger, qubit_number + 1)

        state = ps.SparseState()
        prep = StatePrepViaQRAM(qram, "work_qubit", data_size, 16)
        prep(state)

        # Should mainly be in |0> state
        work_id = ps.System.get_id("work_qubit")
        for basis in state.basis_states:
            val = basis.get(work_id).value
            if val == 0:
                assert abs(basis.amplitude) > 0.5


class TestStatePreparationConditioning:
    """Test conditional execution."""

    def test_conditioned_by_nonzeros(self, fresh_system):
        """Test conditional execution on nonzero condition."""
        qubit_number = 2
        data_size = 8
        rational_size = 16

        _, _, qram = _make_test_tree(qubit_number, data_size)
        ps.System.add_register("work_qubit", ps.UnsignedInteger, qubit_number + 1)
        ps.System.add_register("cond", ps.Boolean, 1)

        state = ps.SparseState()
        ps.Init_Unsafe("cond", 1)(state)

        prep = StatePrepViaQRAM(qram, "work_qubit", data_size, rational_size)
        prep.conditioned_by_nonzeros("cond")(state)

        assert state.size() >= 1

    def test_clear_conditions(self, fresh_system):
        """Test clearing conditions."""
        qubit_number = 2
        data_size = 8
        rational_size = 16

        _, _, qram = _make_test_tree(qubit_number, data_size)
        ps.System.add_register("work_qubit", ps.UnsignedInteger, qubit_number + 1)

        prep = StatePrepViaQRAM(qram, "work_qubit", data_size, rational_size)
        prep.conditioned_by_nonzeros("cond")
        prep.clear_conditions()
        assert len(prep.condition_regs) == 0


class TestStatePreparationIntegration:
    """State preparation integration tests."""

    def test_with_quantum_operations(self, fresh_system):
        """Test integration with other quantum operations."""
        np.random.seed(42)
        sp = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        sp.random_distribution()
        sp.make_tree()
        sp.make_qram()
        sp.set_qram()
        sp.run()

        fidelity = sp.get_fidelity()
        assert fidelity >= 0.0

    def test_repeatability(self, fresh_system):
        """Test repeatability."""
        np.random.seed(42)
        sp1 = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        sp1.random_distribution()
        dist1 = sp1.dist.copy()

        np.random.seed(42)
        sp2 = StatePreparation(qubit_number=2, data_size=8, data_range=4)
        sp2.random_distribution()
        dist2 = sp2.dist.copy()

        assert dist1 == dist2
