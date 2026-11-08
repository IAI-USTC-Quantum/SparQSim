"""Independently reconstruct the native gate matrix, including complex phases."""
import numpy as np
import pytest
import pysparq as ps


def _matrix(operator, width):
    register = ps.System.get_id("data")
    matrix = np.zeros((2**width, 2**width), dtype=complex)
    for column in range(2**width):
        state = ps.SparseState()
        ps.Init_Unsafe("data", column)(state)
        operator(state)
        for basis in state.basis_states:
            matrix[basis.get(register).value, column] += basis.amplitude
    return matrix


@pytest.mark.parametrize("target", [
    [0, 1], [0, 0, 1, 0], [0, 1, 1j, -1], [1, 1j],
    [1j, 0], [-1, 0], [1, 1e-14j], [1e-300, 2e-300j],
    [1e300, -2e300j], [1+2j, -2+1j, 3-1j, 1j],
])
def test_preparation_full_matrix_is_unitary_and_has_exact_target_phase(target):
    width = (len(target)-1).bit_length()
    ps.System.add_register("data", ps.UnsignedInteger, width)
    operator = ps.Rot_GeneralStatePrep("data", target)
    matrix = _matrix(operator, width)
    scaled = np.asarray(target, dtype=complex)/max(abs(complex(x)) for x in target)
    expected = scaled/np.linalg.norm(scaled)
    np.testing.assert_allclose(matrix[:, 0], expected, atol=2e-12, rtol=0)
    np.testing.assert_allclose(matrix.conj().T@matrix, np.eye(len(target)), atol=2e-12, rtol=0)
    # Dagger is checked on arbitrary basis inputs, not only on prepared |0>.
    inverse = _matrix(operator.dag, width)
    np.testing.assert_allclose(inverse, matrix.conj().T, atol=2e-12, rtol=0)


@pytest.mark.parametrize("width", [1, 2, 3, 4])
def test_complex_random_state_preparation_unitarity(width):
    rng = np.random.default_rng(20261109+width)
    target = rng.normal(size=2**width)+1j*rng.normal(size=2**width)
    ps.System.add_register("data", ps.UnsignedInteger, width)
    operator = ps.Rot_GeneralStatePrep("data", target.tolist())
    matrix = _matrix(operator, width)
    np.testing.assert_allclose(matrix[:, 0], target/np.linalg.norm(target), atol=2e-12, rtol=0)
    np.testing.assert_allclose(matrix.conj().T@matrix, np.eye(len(target)), atol=2e-12, rtol=0)


def test_historical_public_builder_remains_available_and_matches_gate():
    target = [0, 1, 1j, -2]
    ps.System.add_register("data", ps.UnsignedInteger, 2)
    helper = ps.stateprep_unitary_build_schmidt(target)
    matrix = _matrix(ps.Rot_GeneralUnitary("data", helper), 2)
    prepared = _matrix(ps.Rot_GeneralStatePrep("data", target), 2)
    np.testing.assert_allclose(matrix, prepared, atol=2e-12, rtol=0)


@pytest.mark.parametrize("target", [[0, 0], [float("nan"), 1],
    [float("inf"), 1], [1, complex(0, float("inf"))], []])
def test_invalid_preparation_is_rejected(target):
    ps.System.add_register("data", ps.UnsignedInteger, 1)
    with pytest.raises((ValueError, RuntimeError)):
        ps.Rot_GeneralStatePrep("data", target)
