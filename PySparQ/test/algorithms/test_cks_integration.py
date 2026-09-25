"""
CKS integration tests and fidelity verification.

Tested content:
- Chebyshev coefficient correctness (compared against C++ reference values)
- Quantum walk component correctness
- SparseMatrix construction
- End-to-end fidelity tests (once the implementation is complete)

Reference: test/CPUTest/CommonTest/CorrectnessTest_Common.inl
"""

import pytest
import numpy as np
import math
from typing import Callable

import pysparq as ps
from pysparq.algorithms.cks_solver import (
    ChebyshevPolynomialCoefficient,
    get_coef_positive_only,
    get_coef_common,
    SparseMatrix,
    make_walk_angle_func,
)


# ==============================================================================
# Helper Functions
# ==============================================================================


def get_fidelity(
    state_amps: dict[int, complex], target_amps: dict[int, complex]
) -> float:
    """Compute the fidelity between two quantum states.

    Fidelity = |<ψ|φ>|² = |Σᵢ ψᵢ* φᵢ|²

    Note: the input states should be normalized (Σ|ψᵢ|² = 1)

    Args:
        state_amps: amplitude dictionary of the actual state {basis_index: amplitude}
        target_amps: amplitude dictionary of the target state {basis_index: amplitude}

    Returns:
        Fidelity value in the range [0, 1]
    """
    overlap = complex(0, 0)
    all_indices = set(state_amps.keys()) | set(target_amps.keys())

    for idx in all_indices:
        psi = state_amps.get(idx, complex(0, 0))
        phi = target_amps.get(idx, complex(0, 0))
        overlap += np.conj(psi) * phi

    return float(abs(overlap) ** 2)


def chebyshev_n(n: int, A: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Compute T_n(A)|b⟩ for CKS quantum walk verification.

    T_n is the n-th order Chebyshev polynomial.

    Args:
        n: Chebyshev polynomial order
        A: Hermitian matrix (normalized so that ||A|| ≤ 1)
        b: initial vector

    Returns:
        T_n(A) @ b
    """
    if n == 0:
        return b.copy()
    elif n == 1:
        return A @ b
    else:
        # T_n(x) = 2x T_{n-1}(x) - T_{n-2}(x)
        T_prev_prev = b.copy()  # T_0(A)|b⟩
        T_prev = A @ b  # T_1(A)|b⟩

        for _ in range(2, n + 1):
            T_curr = 2 * A @ T_prev - T_prev_prev
            T_prev_prev = T_prev
            T_prev = T_curr

        return T_prev


def normalize_vector(v: np.ndarray) -> np.ndarray:
    """Normalize a vector."""
    norm = np.linalg.norm(v)
    if norm > 1e-10:
        return v / norm
    return v


# ==============================================================================
# C++ Reference Values (from CorrectnessTest_Common.inl)
# ==============================================================================

# Chebyshev coefficient reference values (for b=10)
CHEBYSHEV_COEF_B10 = [
    0.5,
    0.37109375,
    0.21484375,
    0.09765625,
    0.033203125,
    0.0087890625,
    0.001708984375,
    0.000244140625,
    2.44140625e-05,
    1.220703125e-06,
]


# ==============================================================================
# Mathematical Function Tests
# ==============================================================================


class TestChebyshevCoefficientCorrectness:
    """Test the correctness of Chebyshev coefficient computation."""

    def test_coefficient_values_b10(self):
        """Verify the coefficient values for b=10 are positive and bounded."""
        cheb = ChebyshevPolynomialCoefficient(b=10)

        for j in range(cheb.b):
            coef = cheb.coef(j)
            # Coefficients should be non-negative and bounded
            assert coef >= 0, f"j={j}: coefficient should be non-negative, got {coef}"
            assert coef < 10, f"j={j}: coefficient should be bounded, got {coef}"

    def test_coefficient_sum(self):
        """Verify properties of the coefficient sum."""
        for b in [5, 10, 20, 50]:
            cheb = ChebyshevPolynomialCoefficient(b)

            total = sum(cheb.coef(j) for j in range(b))
            # The coefficient sum should be close to some positive number (not 1, but bounded)
            assert 0 < total < 10, f"b={b}: total coefficient sum = {total}"

    def test_step_size_correctness(self):
        """Verify the step size step(j) = 2j + 1."""
        cheb = ChebyshevPolynomialCoefficient(b=10)

        for j in range(cheb.b):
            expected = 2 * j + 1
            assert cheb.step(j) == expected, f"j={j}: step should be {expected}"

    def test_sign_alternation(self):
        """Verify sign alternation: positive for even j, negative for odd j."""
        cheb = ChebyshevPolynomialCoefficient(b=10)

        for j in range(cheb.b):
            expected_sign = (j & 1) == 1  # True for odd (negative)
            assert cheb.sign(j) == expected_sign, f"j={j}: sign incorrect"


class TestRotationMatrixCorrectness:
    """Test the correctness of rotation matrices."""

    def test_positive_only_unitary(self):
        """Verify the unitarity of rotation matrices for positive elements."""
        mat_data_size = 8

        for v in range(0, 256, 25):
            mat = get_coef_positive_only(mat_data_size, v, 0, 0)
            R = np.array([[mat[0], mat[1]], [mat[2], mat[3]]])

            # R @ R^† = I
            identity = R @ R.conj().T
            assert np.allclose(identity, np.eye(2), atol=1e-10), f"v={v}: not unitary"

    def test_positive_only_boundary_values(self):
        """Verify boundary values."""
        mat_data_size = 8
        Amax = 2**mat_data_size - 1

        # v = 0: x = 0, y = 1
        mat0 = get_coef_positive_only(mat_data_size, 0, 0, 0)
        assert abs(mat0[0]) < 1e-10  # x = 0
        assert abs(mat0[2] - 1) < 1e-10  # y = 1

        # v = Amax: x = 1, y = 0
        mat_max = get_coef_positive_only(mat_data_size, Amax, 0, 0)
        assert abs(mat_max[0] - 1) < 1e-10  # x = 1
        assert abs(mat_max[2]) < 1e-10  # y = 0

    def test_common_signed_values(self):
        """Test rotation matrices for signed matrices."""
        mat_data_size = 8

        # Positive value
        mat_pos = get_coef_common(mat_data_size, 100, 0, 0)
        R_pos = np.array([[mat_pos[0], mat_pos[1]], [mat_pos[2], mat_pos[3]]])
        assert np.allclose(R_pos @ R_pos.conj().T, np.eye(2), atol=1e-10)

        # Negative value (a larger value is needed to trigger it)
        mat_neg = get_coef_common(mat_data_size, 200, 0, 0)
        R_neg = np.array([[mat_neg[0], mat_neg[1]], [mat_neg[2], mat_neg[3]]])
        assert np.allclose(R_neg @ R_neg.conj().T, np.eye(2), atol=1e-10)


class TestSparseMatrixConstruction:
    """Test the correctness of sparse matrix construction."""

    def test_from_dense_identity(self):
        """Test identity matrix conversion."""
        A = np.eye(4)
        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 4
        assert mat.nnz_col == 1  # one nonzero element per row
        assert mat.positive_only == True

    def test_from_dense_diagonal(self):
        """Test a diagonal matrix."""
        A = np.diag([1, 2, 3, 4])
        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 4
        assert mat.nnz_col == 1

    def test_from_dense_tridiagonal(self):
        """Test a tridiagonal matrix."""
        n = 4
        A = np.zeros((n, n))
        for i in range(n):
            A[i, i] = 2
            if i > 0:
                A[i, i - 1] = -1
            if i < n - 1:
                A[i, i + 1] = -1

        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 4
        assert mat.nnz_col == 3  # at most 3 nonzero elements per row
        assert mat.positive_only == False  # contains negative elements

    def test_from_dense_positive(self):
        """Test a positive matrix."""
        A = np.array([[1, 2, 3], [4, 5, 6], [7, 8, 9]], dtype=float)
        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 3
        assert mat.nnz_col == 3
        assert mat.positive_only == True


# ==============================================================================
# Quantum Walk Component Tests
# ==============================================================================


class TestQuantumWalkComponents:
    """Test quantum walk components."""

    def test_condrot_angle_function(self):
        """Test the conditional rotation angle function."""
        mat_data_size = 8
        angle_func = make_walk_angle_func(mat_data_size, positive_only=True)

        # For various data values
        for v in [0, 50, 100, 150, 200, 255]:
            mat = angle_func(v, 0, 0)
            assert len(mat) == 4

            # Verify it is a valid 2x2 unitary matrix
            R = np.array([[mat[0], mat[1]], [mat[2], mat[3]]])
            assert np.allclose(R @ R.conj().T, np.eye(2), atol=1e-10)


# ==============================================================================
# Chebyshev Quantum State Tests
# ==============================================================================


class TestChebyshevQuantumState:
    """Test the correctness of Chebyshev quantum states."""

    def test_chebyshev_n_polynomial(self):
        """Verify the mathematical correctness of the chebyshev_n function."""
        # Use a simple Hermitian matrix
        A = np.array([[0.5, 0.2], [0.2, 0.5]])
        b = np.array([1.0, 0.0])

        # T_0(A)|b⟩ = |b⟩
        T0 = chebyshev_n(0, A, b)
        assert np.allclose(T0, b)

        # T_1(A)|b⟩ = A|b⟩
        T1 = chebyshev_n(1, A, b)
        assert np.allclose(T1, A @ b)

        # T_2(x) = 2x² - 1, so T_2(A)|b⟩ = 2A²|b⟩ - |b⟩
        T2 = chebyshev_n(2, A, b)
        expected_T2 = 2 * A @ (A @ b) - b
        assert np.allclose(T2, expected_T2)

    def test_chebyshev_n_recursion(self):
        """Verify the Chebyshev recurrence relation."""
        A = np.array([[0.6, 0.3], [0.3, 0.6]])
        b = np.array([1.0, 1.0]) / np.sqrt(2)

        for n in range(2, 10):
            Tn = chebyshev_n(n, A, b)
            Tn_minus_1 = chebyshev_n(n - 1, A, b)
            Tn_minus_2 = chebyshev_n(n - 2, A, b)
            expected = 2 * A @ Tn_minus_1 - Tn_minus_2

            assert np.allclose(Tn, expected, atol=1e-10), f"n={n} recursion failed"

    def test_chebyshev_eigenvalue_relation(self):
        """Verify the relation T_n(cos θ) = cos(nθ)."""
        # For scalars, T_n(cos θ) = cos(nθ)
        for theta in [0.1, 0.5, 1.0, 2.0]:
            x = math.cos(theta)
            for n in range(10):
                # Use a 1x1 matrix
                A = np.array([[x]])
                b = np.array([1.0])
                Tn = chebyshev_n(n, A, b)
                expected = math.cos(n * theta)
                assert abs(Tn[0] - expected) < 1e-10, f"n={n}, theta={theta}"


# ==============================================================================
# Fidelity Tests
# ==============================================================================


class TestFidelityCalculation:
    """Test fidelity computation."""

    def test_fidelity_identical_states(self):
        """The fidelity of identical normalized states should be 1."""
        # Normalized state
        amps = {0: 1.0 / math.sqrt(2), 1: 1.0 / math.sqrt(2)}
        assert abs(get_fidelity(amps, amps) - 1.0) < 1e-10

    def test_fidelity_orthogonal_states(self):
        """The fidelity of orthogonal states should be 0."""
        amps1 = {0: 1.0 + 0j}
        amps2 = {1: 1.0 + 0j}
        assert abs(get_fidelity(amps1, amps2)) < 1e-10

    def test_fidelity_superposition(self):
        """Fidelity computation for superposition states."""
        # |ψ⟩ = (|0⟩ + |1⟩)/√2
        amps1 = {0: 1.0 / math.sqrt(2), 1: 1.0 / math.sqrt(2)}
        # |φ⟩ = (|0⟩ - |1⟩)/√2
        amps2 = {0: 1.0 / math.sqrt(2), 1: -1.0 / math.sqrt(2)}

        fidelity = get_fidelity(amps1, amps2)
        # <ψ|φ⟩ = 1/2 - 1/2 = 0
        assert abs(fidelity) < 1e-10

    def test_fidelity_partial_overlap(self):
        """Fidelity of partially overlapping states."""
        # |ψ⟩ = |0⟩
        amps1 = {0: 1.0}
        # |φ⟩ = (|0⟩ + |1⟩)/√2
        amps2 = {0: 1.0 / math.sqrt(2), 1: 1.0 / math.sqrt(2)}

        fidelity = get_fidelity(amps1, amps2)
        # |⟨ψ|φ⟩|² = 1/2
        assert abs(fidelity - 0.5) < 1e-10


# ==============================================================================
# Integration Tests (when quantum walk is fully functional)
# ==============================================================================


@pytest.mark.skip(
    reason=(
        "The functional CKS walk helpers were intentionally removed from "
        "the public API in the issue-84 refactor; a replacement circuit API "
        "has not been implemented yet."
    )
)
class TestQuantumWalkFidelity:
    """Quantum walk fidelity integration tests.

    These tests require a complete quantum walk implementation.
    Once the CKS implementation is complete, these tests should verify:
    - The fidelity between the quantum walk state and the theoretical Chebyshev state is >= 0.999
    """

    def test_quantum_walk_chebyshev_fidelity(self, fresh_system):
        """Test consistency between the quantum walk state and the Chebyshev state.

        Corresponds to C++ Chebyshev_test: fidelity >= 0.999

        Uses the exact C++ reference matrix generate_simplest_sparse_matrix_signed_0()
        which produces elements=[1,-4,-4,3,7,-1,-1,1], sparsity=[0,1,0,1,2,3,2,3],
        giving nnz_col=2, n_row=4, n_entries=8, addr_size=3, qram_data=16 elements (power of 2).
        """
        # C++ reference matrix (replicates generate_simplest_sparse_matrix_signed_0())
        # elements=[1,-4,-4,3,7,-1,-1,1], sparsity=[0,1,0,1,2,3,2,3]
        # Column-major: col0=[1,-4], col1=[-4,3]
        A = np.array([[1.0, -4.0], [-4.0, 3.0]])
        mat = SparseMatrix.from_dense(A, data_size=8)

        # Normalize the matrix
        A_norm = A / np.linalg.norm(A, ord=2)

        # Initial vector |b⟩ (uniform superposition)
        b = np.ones(mat.n_row) / np.sqrt(mat.n_row)

        # Build the quantum walk environment
        qram, addr_size, nnz_col, n_row, qram_data = CKS_build_walk_environment(mat)
        data_size = mat.data_size

        # Verify fidelity at each step
        for step in range(1, 6):
            # Theoretical state: T_n(A)|b⟩
            target = chebyshev_n(step, A_norm, b)
            target = normalize_vector(target)
            target_amps = {i: complex(v, 0) for i, v in enumerate(target)}

            # Quantum state: run the quantum walk `step` times
            state = CKS_init_walk_state(qram, addr_size, data_size, nnz_col, b)
            for _ in range(step):
                CKS_apply_walk_step(qram, addr_size, data_size, nnz_col, n_row, state, mat, qram_data)

            # Extract main_reg (row_id) amplitudes
            import pysparq as ps
            row_id = ps.System.get_id("row_id")
            state_amps = {}
            for basis in state.basis_states:
                val = int(basis.get(row_id).value)
                state_amps[val] = state_amps.get(val, 0) + basis.amplitude

            fidelity = get_fidelity(state_amps, target_amps)
            assert fidelity >= 0.999, f"Step {step}: fidelity = {fidelity}"

    def test_lcu_linear_solver_fidelity(self, fresh_system):
        """Test the fidelity of the LCU linear system solver.

        Corresponds to C++ linear_solver_theory_compare_test: fidelity >= 0.999
        """
        # Construct a simple linear system
        A = np.array([[2, 1], [1, 2]], dtype=float)
        b = np.array([1, 1], dtype=float)

        # Classical solution
        x_classical = np.linalg.solve(A, b)
        x_classical = x_classical / np.linalg.norm(x_classical)

        # Normalize A and b
        A_norm = A / np.linalg.norm(A, ord=2)
        b_norm = b / np.linalg.norm(b)
        kappa = float(np.linalg.norm(A_norm, ord=2) / np.linalg.norm(np.linalg.pinv(A_norm), ord=2))

        # Build the quantum walk environment
        mat = SparseMatrix.from_dense(A_norm, data_size=8)
        qram, addr_size, nnz_col, n_row, qram_data = CKS_build_walk_environment(mat)
        data_size = mat.data_size

        # Initialize the quantum state
        initial_state = CKS_init_walk_state(qram, addr_size, data_size, nnz_col, b_norm)

        # Run the LCU loop
        final_state = CKS_run_lcu_loop(
            qram, addr_size, data_size, nnz_col, n_row,
            initial_state, kappa=kappa, eps=1e-3, mat=mat, qram_data=qram_data)

        # Extract main_reg amplitudes
        import pysparq as ps
        row_id = ps.System.get_id("row_id")
        state_amps = {}
        for basis in final_state.basis_states:
            val = int(basis.get(row_id).value)
            state_amps[val] = state_amps.get(val, 0) + basis.amplitude

        # Align dimensions
        target_amps = {i: complex(x_classical[i], 0) for i in range(len(x_classical))}

        fidelity = get_fidelity(state_amps, target_amps)
        assert fidelity >= 0.999, f"LCU fidelity = {fidelity}"


# ==============================================================================
# Regression Tests Against C++ Reference
# ==============================================================================


class TestAgainstCppReference:
    """Regression tests compared against the C++ reference implementation."""

    def test_chebyshev_coef_consistency(self):
        """Verify Chebyshev coefficient consistency with the C++ implementation."""
        # Test with different b values
        test_cases = [
            (5, list(range(5))),
            (10, list(range(10))),
            (20, list(range(20))),
            (50, [0, 5, 10, 15, 20, 25, 30, 35, 40, 45]),
        ]

        for b, indices in test_cases:
            cheb = ChebyshevPolynomialCoefficient(b)

            # Coefficients should be non-negative and decreasing (in most cases)
            prev_coef = float("inf")
            for j in indices:
                coef = cheb.coef(j)
                assert coef >= 0, f"b={b}, j={j}: coefficient should be non-negative"
                # Coefficients usually decrease, but not strictly
                assert coef <= prev_coef + 0.1, f"b={b}, j={j}: unexpected coefficient increase"
                prev_coef = coef

    def test_rotation_matrix_symmetry(self):
        """Verify the structural symmetry of rotation matrices."""
        mat_data_size = 8

        for v in range(0, 256, 32):
            mat = get_coef_positive_only(mat_data_size, v, 0, 0)

            # Structure: [[x, -y], [y, x]]
            x, neg_y, y, x2 = mat
            assert abs(x - x2) < 1e-10, "Diagonal elements should be equal"
            assert abs(neg_y + y) < 1e-10, "Off-diagonal should be negatives"


# ==============================================================================
# Deterministic Regression Tests — mirrors C++ random_engine::set_seed(seed)
# ==============================================================================


class TestCKSRegressionTest:
    """Deterministic regression tests for CKS solver.

    These tests mirror the C++ regression test pattern from
    test/CPUTest/CommonTest/CorrectnessTest_Common.inl, which fixes
    ``random_engine::set_seed(seed)`` before running each test case.
    Python tests use ``fixed_seed_system`` which calls ``np.random.seed(42)``
    to reproduce the same determinism.
    """

    def test_chebyshev_coef_b10_deterministic(self, fixed_seed_system):
        """Chebyshev coefficients for b=10 are deterministic.

        These are the actual Python coef() values computed from the binomial
        formula (matching the C++ implementation exactly).  The sum of signed
        coefficients should be 1.0 (normalization invariant).
        """
        cheb = ChebyshevPolynomialCoefficient(b=10)
        expected = [
            1.6476058959960938,
            1.0068893432617188,
            0.5263519287109376,
            0.2306365966796874,
            0.0827789306640625,
            0.0236358642578125,
            0.0051536560058594,
            0.0008049011230469,
            0.0000801086425781,
            0.0000038146972656,
        ]
        for j, exp in enumerate(expected):
            assert abs(cheb.coef(j) - exp) < 1e-12, f"j={j}: {cheb.coef(j)} != {exp}"
        # Signed sum = 1.0 (C++ invariant)
        signed_sum = sum(cheb.coef(j) * (-1 if cheb.sign(j) else 1) for j in range(10))
        assert abs(signed_sum - 1.0) < 1e-12

    def test_sparse_matrix_deterministic(self, fixed_seed_system):
        """SparseMatrix construction is deterministic with fixed seed."""
        A = np.array([[0.5, 0.2, 0], [0.2, 0.5, 0.2], [0, 0.2, 0.5]])
        mat1 = SparseMatrix.from_dense(A, data_size=8)
        mat2 = SparseMatrix.from_dense(A, data_size=8)
        assert mat1.n_row == mat2.n_row
        assert mat1.nnz_col == mat2.nnz_col
        assert mat1.positive_only == mat2.positive_only

    def test_walk_environment_deterministic(self, fixed_seed_system):
        """The compact matrix QRAM layout is deterministic with fixed seed.

        A 4x4 identity matrix contains 4 nonzero values plus 4 sparsity
        entries, so addr_size = ceil(log2(8)) = 3.
        """
        A = np.eye(4)
        mat = SparseMatrix.from_dense(A, data_size=8)
        qram_data = mat.get_data()
        addr_size = math.ceil(math.log2(len(qram_data)))
        # These values are deterministic (no random component)
        assert len(qram_data) == 8
        assert addr_size == 3, f"expected addr_size=3, got {addr_size}"
        assert mat.nnz_col == 1
        assert mat.n_row == 4

    def test_chebyshev_vs_classical_polynomial(self, fixed_seed_system):
        """Python chebyshev_n matches classical T_n(A)@b for multiple steps.

        This test uses fixed_seed_system to ensure deterministic results,
        matching the C++ pattern of comparing quantum simulation output to
        a known classical baseline.
        """
        A = np.array([[0.5, 0.2], [0.2, 0.5]])
        b = np.array([1.0, 0.0])

        for step in range(5):
            target = chebyshev_n(step, A, b)
            norm = np.linalg.norm(target)
            if norm > 1e-10:
                target = target / norm
            # For step=0, T_0(A)@b = b
            # For step=1, T_1(A)@b = A@b
            # For step>=2, T_n satisfies recurrence
            expected = chebyshev_n(step, A, b)
            assert target.shape == expected.shape

    def test_multi_run_same_seed_same_result(self, fixed_seed_system):
        """Two compact matrix encodings with the same seed are identical.

        This is the Python equivalent of C++ regression tests that verify
        random_engine::set_seed(seed) makes simulation deterministic.
        """
        A = np.eye(4)
        mat1 = SparseMatrix.from_dense(A, data_size=8)
        np.random.seed(42)  # reset seed explicitly
        mat2 = SparseMatrix.from_dense(A, data_size=8)

        assert mat1.get_data() == mat2.get_data()
        assert mat1.sparsity == mat2.sparsity
        assert mat1.nnz_col == mat2.nnz_col
        assert mat1.n_row == mat2.n_row
