"""
Algorithm-specific fixtures and helper functions.

Provides:
- tridiagonal_matrix: creates a tridiagonal matrix
- random_unitary: creates a random unitary matrix
- assert_probability_distribution: validates a probability distribution
"""

import pytest
import numpy as np


@pytest.fixture
def tridiagonal_matrix():
    """Create a tridiagonal matrix for block encoding tests.

    Returns:
        Callable: takes (alpha, beta, dim) parameters and returns a normalized tridiagonal matrix
    """

    def _create(alpha: float, beta: float, dim: int) -> np.ndarray:
        mat = np.zeros((dim, dim))
        for i in range(dim):
            mat[i, i] = alpha
            if i > 0:
                mat[i - 1, i] = beta
            if i < dim - 1:
                mat[i + 1, i] = beta
        # Normalize
        norm = np.linalg.norm(mat, "fro")
        if norm > 0:
            mat = mat / norm
        return mat

    return _create


@pytest.fixture
def random_unitary():
    """Generate a random unitary matrix for testing.

    Returns:
        Callable: takes (dim, seed) parameters and returns a unitary matrix
    """

    def _create(dim: int, seed: int = 42) -> np.ndarray:
        np.random.seed(seed)
        z = np.random.randn(dim, dim) + 1j * np.random.randn(dim, dim)
        q, r = np.linalg.qr(z)
        return q

    return _create


@pytest.fixture
def simple_linear_system():
    """Create a simple linear system for solver tests.

    Returns:
        Callable: takes (n, kappa) parameters and returns (A, b, x_expected)
    """

    def _create(n: int = 2, kappa: float = 2.0) -> tuple:
        # Create a symmetric positive definite matrix with condition number kappa
        # A = I + (kappa-1)/n * ones
        # This gives a condition number of approximately kappa
        A = np.eye(n) + (kappa - 1) / n * np.ones((n, n))
        b = np.ones(n)
        x_expected = np.linalg.solve(A, b)
        return A, b, x_expected

    return _create


@pytest.fixture
def fixed_seed_system(fresh_system):
    """Fresh system with fixed random seed for deterministic regression tests.

    Mirrors the C++ ``random_engine::set_seed(seed)`` pattern from
    test/CPUTest/CommonTest/CorrectnessTest_Common.inl.

    All quantum simulation in the returned context uses np.random.seed(42),
    making tests reproducible across runs.  Resets the C++ SparseState system
    between tests via the ``fresh_system`` fixture from the root conftest.py.
    """
    import pysparq as ps

    np.random.seed(42)
    yield fresh_system
    ps.System.clear()
    np.random.seed(None)  # Restore random state
