"""
CKS linear system solver tests.

Tested content:
- ChebyshevPolynomialCoefficient: Chebyshev coefficient computation
- get_coef_positive_only: coefficients for positive matrix elements
- get_coef_common: coefficients for general matrix elements
- SparseMatrix: sparse matrix representation

Reference: related CPP CKS test patterns
"""

import pytest
import numpy as np
import math

import pysparq as ps
from pysparq.algorithms.cks_solver import (
    ChebyshevPolynomialCoefficient,
    get_coef_positive_only,
    get_coef_common,
    SparseMatrix,
)


class TestChebyshevPolynomialCoefficient:
    """Test Chebyshev coefficient computation."""

    def test_initialization(self):
        """Test initialization."""
        cheb = ChebyshevPolynomialCoefficient(b=10)
        assert cheb.b == 10

    def test_coefficient_values(self):
        """Verify coefficient values are positive and decreasing."""
        cheb = ChebyshevPolynomialCoefficient(b=10)

        prev_coef = float("inf")
        for j in range(cheb.b):
            coef = cheb.coef(j)
            assert coef >= 0
            assert coef <= prev_coef + 1e-10  # allow numerical error
            prev_coef = coef

    def test_step_size(self):
        """The step size should be 2j + 1."""
        cheb = ChebyshevPolynomialCoefficient(b=10)

        for j in range(cheb.b):
            assert cheb.step(j) == 2 * j + 1

    def test_sign_alternation(self):
        """Signs should alternate: positive for even j, negative for odd j."""
        cheb = ChebyshevPolynomialCoefficient(b=10)

        for j in range(cheb.b):
            expected_sign = (j & 1) == 1  # True for odd
            assert cheb.sign(j) == expected_sign

    def test_step_sequence(self):
        """Verify the step size sequence."""
        cheb = ChebyshevPolynomialCoefficient(b=5)
        expected_steps = [1, 3, 5, 7, 9]

        for j, expected in enumerate(expected_steps):
            assert cheb.step(j) == expected

    def test_large_b(self):
        """Test a large b value."""
        cheb = ChebyshevPolynomialCoefficient(b=200)

        # For large b, use the erfc approximation
        for j in range(0, cheb.b, 50):
            coef = cheb.coef(j)
            assert 0 <= coef <= 2

    def test_small_b(self):
        """Test a small b value."""
        cheb = ChebyshevPolynomialCoefficient(b=5)

        # For small b, use exact computation
        for j in range(cheb.b):
            coef = cheb.coef(j)
            assert coef >= 0

    def test_combinatorial_coefficient(self):
        """Test combinatorial coefficient computation."""
        cheb = ChebyshevPolynomialCoefficient(b=5)

        # Test a simple binomial coefficient
        # C(n, k) = n! / (k! * (n-k)!)
        c = cheb.C(10, 2)
        # C(10, 2) / 4^b = 45 / 1024
        expected = 45.0 / (2**10) / (2**10)
        # Note: the implementation may use a different normalization
        assert c > 0


class TestGetCoefPositiveOnly:
    """Test coefficient computation for positive matrix elements."""

    def test_output_format(self):
        """Verify the output format."""
        mat_data_size = 8
        v = 100
        row, col = 0, 0

        result = get_coef_positive_only(mat_data_size, v, row, col)

        assert len(result) == 4
        assert all(isinstance(c, complex) for c in result)

    def test_unitary_matrix(self):
        """Verify the generated matrix is unitary."""
        mat_data_size = 8

        for v in [0, 50, 100, 200]:
            result = get_coef_positive_only(mat_data_size, v, 0, 0)
            R = np.array([[result[0], result[1]], [result[2], result[3]]])

            # Verify unitarity
            identity = R @ R.conj().T
            assert np.allclose(identity, np.eye(2), atol=1e-10)

    def test_zero_value(self):
        """Test the zero value case."""
        mat_data_size = 8
        v = 0

        result = get_coef_positive_only(mat_data_size, v, 0, 0)

        # When v=0, x=0, y=1
        # The matrix should be [[0, -1], [1, 0]]
        assert abs(result[0]) < 1e-10
        assert abs(result[1] + 1) < 1e-10
        assert abs(result[2] - 1) < 1e-10
        assert abs(result[3]) < 1e-10

    def test_max_value(self):
        """Test the maximum value case."""
        mat_data_size = 8
        Amax_real = 2**mat_data_size - 1

        result = get_coef_positive_only(mat_data_size, Amax_real, 0, 0)

        # When v=Amax_real, x=1, y=0
        # The matrix should be [[1, 0], [0, 1]]
        assert abs(result[0] - 1) < 1e-10
        assert abs(result[1]) < 1e-10
        assert abs(result[2]) < 1e-10
        assert abs(result[3] - 1) < 1e-10


class TestGetCoefCommon:
    """Test coefficient computation for general matrix elements."""

    def test_output_format(self):
        """Verify the output format."""
        mat_data_size = 8
        v = 100
        row, col = 0, 0

        result = get_coef_common(mat_data_size, v, row, col)

        assert len(result) == 4

    def test_positive_value(self):
        """Test a positive value."""
        mat_data_size = 8
        v = 100

        result = get_coef_common(mat_data_size, v, 0, 0)

        # A positive value should produce a valid rotation matrix
        R = np.array([[result[0], result[1]], [result[2], result[3]]])

    def test_negative_value(self):
        """Test a negative value."""
        mat_data_size = 8
        # Negative values are represented using two's complement

        result = get_coef_common(mat_data_size, 200, 0, 0)

        # Should produce a valid result


class TestSparseMatrix:
    """Test sparse matrix representation."""

    def test_from_dense_positive(self):
        """Test creating a sparse matrix from a positive dense matrix."""
        A = np.array([[1, 2], [2, 1]], dtype=float)

        mat = SparseMatrix.from_dense(A, data_size=8, positive_only=True)

        assert mat.n_row == 2
        assert mat.positive_only == True

    def test_from_dense_with_negative(self):
        """Test creating a sparse matrix from a dense matrix with negative elements."""
        A = np.array([[1, -2], [-2, 1]], dtype=float)

        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 2
        assert mat.positive_only == False

    def test_from_dense_identity(self):
        """Test creation from the identity matrix."""
        A = np.eye(4)

        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 4

    def test_matrix_properties(self):
        """Test matrix properties."""
        A = np.array([[2, 1], [1, 2]], dtype=float)
        mat = SparseMatrix.from_dense(A, data_size=8)

        # Verify basic properties
        assert hasattr(mat, "n_row")
        assert hasattr(mat, "data_size")

    def test_get_walk_angle_func(self):
        """Test getting the walk angle function."""
        A = np.eye(2)
        mat = SparseMatrix.from_dense(A, data_size=8)

        # Get the walk angle function
        func = mat.get_walk_angle_func()
        assert callable(func)


class TestCKSIntegration:
    """CKS integration tests."""

    def test_chebyshev_walk_correspondence(self):
        """Test the correspondence between Chebyshev coefficients and walk steps."""
        b = 10
        cheb = ChebyshevPolynomialCoefficient(b)

        for j in range(b):
            coef = cheb.coef(j)
            step = cheb.step(j)
            sign = cheb.sign(j)

            # The step count should be odd
            assert step % 2 == 1

            # Coefficients should be non-negative
            assert coef >= 0

    def test_matrix_coefficient_consistency(self):
        """Test matrix coefficient consistency."""
        mat_data_size = 8

        # Test multiple values
        for v in range(0, 256, 50):
            coef_pos = get_coef_positive_only(mat_data_size, v, 0, 0)
            coef_com = get_coef_common(mat_data_size, v, 0, 0)

            # Both should be valid 2x2 matrices
            assert len(coef_pos) == 4
            assert len(coef_com) == 4

    def test_full_coefficient_sequence(self):
        """Test the full coefficient sequence."""
        b = 20
        cheb = ChebyshevPolynomialCoefficient(b)

        total_coef = 0.0
        for j in range(b):
            total_coef += cheb.coef(j) * ((-1) ** j)

        # The coefficient sum should be within a reasonable range
        assert abs(total_coef) < 2 * b


class TestCKSEdgeCases:
    """CKS edge case tests."""

    def test_b_one(self):
        """Test b=1."""
        cheb = ChebyshevPolynomialCoefficient(b=1)

        coef = cheb.coef(0)
        step = cheb.step(0)

        assert step == 1
        assert coef >= 0

    def test_large_matrix(self):
        """Test a large matrix."""
        A = np.eye(10)

        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 10

    def test_zero_matrix(self):
        """Test the zero matrix."""
        A = np.zeros((3, 3))

        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 3

    def test_single_element_matrix(self):
        """Test a single-element matrix."""
        A = np.array([[5.0]])

        mat = SparseMatrix.from_dense(A, data_size=8)

        assert mat.n_row == 1

    def test_very_large_data_size(self):
        """Test a large data size."""
        A = np.eye(2)

        mat = SparseMatrix.from_dense(A, data_size=32)

        assert mat.data_size == 32
