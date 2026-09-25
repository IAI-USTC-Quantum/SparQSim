"""
PySparQ test shared configuration and fixtures.

Provides:
- fresh_system: fixture that automatically clears System state
- pytest marker configuration

Helper functions:
- get_amplitude: get the amplitude of a specific basis state
- get_reg_value: get a register value
- verify_unitarity_self_adjoint: verify unitarity of a self-adjoint operator
- verify_unitarity_explicit_dag: verify unitarity of an operator with explicit dag
"""

import pytest
import numpy as np


def pytest_configure(config):
    """Register custom pytest markers."""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )


@pytest.fixture(autouse=True)
def fresh_system():
    """Automatically clear System state before and after each test.

    This keeps tests isolated from each other and avoids state pollution.
    """
    import pysparq as ps

    ps.System.clear()
    yield
    ps.System.clear()


@pytest.fixture
def small_qram():
    """Create a small QRAM for fast tests.

    Returns:
        Callable: accepts (memory, n_bits, data_size) arguments and returns a QRAMCircuit_qutrit
    """

    def _create(memory: list, n_bits: int = None, data_size: int = 8):
        import pysparq as ps
        import math

        if n_bits is None:
            n_bits = int(math.log2(len(memory))) + 1 if len(memory) > 0 else 1
        return ps.QRAMCircuit_qutrit(n_bits, data_size, memory)

    return _create


# ---------------------------------------------------------------------------
# Helper Functions
# ---------------------------------------------------------------------------


def get_amplitude(state, reg_values: dict) -> complex:
    """Get the amplitude of a specific basis state.

    Args:
        state: SparseState instance
        reg_values: dict mapping register names to expected values

    Returns:
        The complex amplitude of the matching basis state, or 0 if not found

    Example:
        >>> amp = get_amplitude(state, {"addr": 3, "data": 8})
    """
    import pysparq as ps

    for basis in state.basis_states:
        match = True
        for reg_name, expected_val in reg_values.items():
            reg_id = ps.System.get_id(reg_name)
            if basis.get(reg_id).value != expected_val:
                match = False
                break
        if match:
            return basis.amplitude
    return complex(0, 0)


def get_reg_value(basis, reg: str | int) -> int:
    """Get the value of a register in a basis state.

    Args:
        basis: System basis state (from state.basis_states)
        reg: register name or ID

    Returns:
        The integer value stored in the register
    """
    import pysparq as ps

    if isinstance(reg, str):
        reg_id = ps.System.get_id(reg)
    else:
        reg_id = reg
    return basis.get(reg_id).value


def verify_unitarity_self_adjoint(
    state,
    operator,
    rtol: float = 1e-6,
    atol: float = 1e-10,
) -> bool:
    """Verify unitarity of a self-adjoint operator: U * U = I.

    A self-adjoint operator satisfies U^dagger = U, so applying it twice should return
    the initial state.

    Args:
        state: SparseState instance
        operator: object with an operator interface
        rtol: relative tolerance (unused, kept for compatibility)
        atol: absolute tolerance

    Returns:
        True if the verification passes

    Example:
        >>> state = ps.SparseState()
        >>> # ... initialize the state ...
        >>> assert verify_unitarity_self_adjoint(state, operator)
    """
    import pysparq as ps

    # Record the initial amplitudes
    initial_amplitudes = {}
    for basis in state.basis_states:
        # Use register values as the key
        key = tuple(
            (ps.System.name_of(i), basis.get(i).value)
            for i in range(len(basis.registers))
        )
        initial_amplitudes[key] = basis.amplitude

    # Apply the operator twice
    operator(state)
    operator(state)

    # Verify that the state returns to the initial one
    for basis in state.basis_states:
        key = tuple(
            (ps.System.name_of(i), basis.get(i).value)
            for i in range(len(basis.registers))
        )
        expected = initial_amplitudes.get(key, complex(0, 0))
        if abs(basis.amplitude - expected) > atol:
            return False
    return True


def verify_unitarity_explicit_dag(
    state,
    operator,
    rtol: float = 1e-6,
    atol: float = 1e-10,
) -> bool:
    """Verify unitarity of a non-self-adjoint operator: U^dagger * U = I.

    Verified using the explicit dag() method.

    Args:
        state: SparseState instance
        operator: operator object with a dag() method
        rtol: relative tolerance (unused, kept for compatibility)
        atol: absolute tolerance

    Returns:
        True if the verification passes

    Example:
        >>> state = ps.SparseState()
        >>> # ... initialize the state ...
        >>> assert verify_unitarity_explicit_dag(state, operator)
    """
    import pysparq as ps

    # Record the initial amplitudes
    initial_amplitudes = {}
    for basis in state.basis_states:
        key = tuple(
            (ps.System.name_of(i), basis.get(i).value)
            for i in range(len(basis.registers))
        )
        initial_amplitudes[key] = basis.amplitude

    # Apply the forward operator, then apply dag
    operator(state)
    operator.dag(state)

    # Verify that the state returns to the initial one
    for basis in state.basis_states:
        key = tuple(
            (ps.System.name_of(i), basis.get(i).value)
            for i in range(len(basis.registers))
        )
        expected = initial_amplitudes.get(key, complex(0, 0))
        if abs(basis.amplitude - expected) > atol:
            return False
    return True


def state_to_amplitude_dict(state, main_reg: str) -> dict:
    """Convert a sparse state to an amplitude dict.

    Args:
        state: SparseState instance
        main_reg: main register name

    Returns:
        Dict mapping register values to amplitudes
    """
    import pysparq as ps

    reg_id = ps.System.get_id(main_reg)
    return {basis.get(reg_id).value: basis.amplitude for basis in state.basis_states}


def assert_amplitude_close(actual: complex, expected: complex, tol: float = 1e-10):
    """Verify that an amplitude is close to the expected value.

    Args:
        actual: actual amplitude
        expected: expected amplitude
        tol: tolerance

    Raises:
        AssertionError: if the amplitudes do not match
    """
    assert abs(actual - expected) < tol, f"Amplitude mismatch: {actual} != {expected}"


def assert_probability_distribution(
    state,
    expected_probs: dict,
    main_reg: str,
    rtol: float = 0.1,
    atol: float = 0.05,
):
    """Verify that the measurement probability distribution is close to the expected one.

    Args:
        state: SparseState instance
        expected_probs: dict mapping register values to expected probabilities
        main_reg: main register name
        rtol: relative tolerance
        atol: absolute tolerance

    Raises:
        AssertionError: if the probabilities do not match
    """
    import pysparq as ps

    reg_id = ps.System.get_id(main_reg)
    measured = {}
    for basis in state.basis_states:
        val = basis.get(reg_id).value
        measured[val] = measured.get(val, 0) + abs(basis.amplitude) ** 2

    for val, expected_prob in expected_probs.items():
        actual_prob = measured.get(val, 0)
        if not np.isclose(actual_prob, expected_prob, rtol=rtol, atol=atol):
            raise AssertionError(
                f"Probability mismatch: value {val} expected {expected_prob:.4f}, actual {actual_prob:.4f}"
            )
