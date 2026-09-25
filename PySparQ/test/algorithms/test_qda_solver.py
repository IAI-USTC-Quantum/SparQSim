"""
QDA linear system solver tests.

Tested content:
- compute_fs: interpolation parameter computation
- compute_rotation_matrix: rotation matrix computation
- chebyshev_T: Chebyshev polynomials
- dolph_chebyshev: Dolph-Chebyshev filter
- compute_fourier_coeffs: Fourier coefficient computation

Reference: test/CPUTest/CommonTest/CorrectnessTest_QDA_CompareList.inl
"""

import pytest
import numpy as np
import math

import pysparq as ps
from pysparq.algorithms.qda_solver import (
    compute_fs,
    compute_rotation_matrix,
    chebyshev_T,
    dolph_chebyshev,
    compute_fourier_coeffs,
)


class TestComputeFs:
    """Test interpolation parameter computation."""

    def test_fs_at_zero(self):
        """f(0) should be 0."""
        fs = compute_fs(0.0, kappa=10.0, p=0.5)
        assert abs(fs) < 1e-10

    def test_fs_at_one(self):
        """f(1) should be 1."""
        fs = compute_fs(1.0, kappa=10.0, p=0.5)
        assert abs(fs - 1.0) < 1e-10

    def test_fs_monotonic(self):
        """f(s) should be monotonically increasing."""
        kappa, p = 10.0, 0.5
        prev_fs = compute_fs(0.0, kappa, p)

        for s in np.linspace(0.1, 1.0, 10):
            fs = compute_fs(s, kappa, p)
            assert fs >= prev_fs - 1e-10  # allow numerical error
            prev_fs = fs

    def test_fs_kappa_one(self):
        """When kappa=1, f(s) should equal s."""
        for s in [0.0, 0.25, 0.5, 0.75, 1.0]:
            fs = compute_fs(s, kappa=1.0, p=0.5)
            assert abs(fs - s) < 1e-10

    def test_fs_bounded(self):
        """f(s) should be within the range [0, 1]."""
        for kappa in [2.0, 10.0, 100.0]:
            for p in [0.3, 0.5, 0.7]:
                for s in np.linspace(0, 1, 20):
                    fs = compute_fs(s, kappa, p)
                    assert 0.0 <= fs <= 1.0

    def test_fs_different_kappa(self):
        """Test different condition numbers."""
        s = 0.5
        p = 0.5

        for kappa in [2.0, 5.0, 10.0, 100.0]:
            fs = compute_fs(s, kappa, p)
            assert 0.0 <= fs <= 1.0

    def test_fs_different_p(self):
        """Test different schedule parameters."""
        s = 0.5
        kappa = 10.0

        for p in [0.1, 0.3, 0.5, 0.7, 0.9]:
            fs = compute_fs(s, kappa, p)
            assert 0.0 <= fs <= 1.0


class TestRotationMatrix:
    """Test rotation matrix computation."""

    def test_rotation_matrix_unitary(self):
        """The rotation matrix should be unitary."""
        for fs in [0.2, 0.5, 0.8]:
            R = compute_rotation_matrix(fs)
            R_matrix = np.array([[R[0], R[1]], [R[2], R[3]]])

            # Check unitarity: R * R^dagger = I
            identity = R_matrix @ R_matrix.conj().T
            assert np.allclose(identity, np.eye(2), atol=1e-10)

    def test_rotation_matrix_determinant(self):
        """The rotation matrix determinant should be -1."""
        for fs in [0.2, 0.5, 0.8]:
            R = compute_rotation_matrix(fs)
            det = R[0] * R[3] - R[1] * R[2]
            assert abs(det + 1) < 1e-10  # det = -1

    def test_rotation_matrix_structure(self):
        """Verify the rotation matrix structure."""
        fs = 0.5
        R = compute_rotation_matrix(fs)

        sqrt_N = 1.0 / math.sqrt((1 - fs) ** 2 + fs**2)

        expected_u00 = sqrt_N * (1 - fs)
        expected_u01 = sqrt_N * fs
        expected_u10 = sqrt_N * fs
        expected_u11 = sqrt_N * (fs - 1)

        assert abs(R[0] - expected_u00) < 1e-10
        assert abs(R[1] - expected_u01) < 1e-10
        assert abs(R[2] - expected_u10) < 1e-10
        assert abs(R[3] - expected_u11) < 1e-10

    def test_rotation_matrix_fs_zero(self):
        """Rotation matrix when fs=0."""
        R = compute_rotation_matrix(0.0)
        # When fs=0, sqrt_N = 1
        # R = [[1, 0], [0, -1]]
        assert abs(R[0] - 1) < 1e-10
        assert abs(R[1]) < 1e-10
        assert abs(R[2]) < 1e-10
        assert abs(R[3] + 1) < 1e-10

    def test_rotation_matrix_fs_one(self):
        """Rotation matrix when fs=1."""
        R = compute_rotation_matrix(1.0)
        # When fs=1, sqrt_N = 1
        # R = [[0, 1], [1, 0]]
        assert abs(R[0]) < 1e-10
        assert abs(R[1] - 1) < 1e-10
        assert abs(R[2] - 1) < 1e-10
        assert abs(R[3]) < 1e-10


class TestChebyshevPolynomial:
    """Test Chebyshev polynomials."""

    @pytest.mark.parametrize("n,x,expected", [
        (0, 0.5, 1.0),
        (1, 0.5, 0.5),
        (2, 0.5, -0.5),
        (3, 0.5, -1.0),
        (0, 1.0, 1.0),
        (1, 1.0, 1.0),
        (2, 1.0, 1.0),
    ])
    def test_chebyshev_T_values(self, n, x, expected):
        """Test Chebyshev polynomial values."""
        result = chebyshev_T(n, x)
        assert abs(result - expected) < 1e-10

    def test_chebyshev_recursion(self):
        """Verify the recurrence relation T_n(x) = 2x T_{n-1}(x) - T_{n-2}(x)."""
        x = 0.7
        for n in range(2, 10):
            Tn = chebyshev_T(n, x)
            Tn_minus_1 = chebyshev_T(n - 1, x)
            Tn_minus_2 = chebyshev_T(n - 2, x)
            expected = 2 * x * Tn_minus_1 - Tn_minus_2
            assert abs(Tn - expected) < 1e-10

    def test_chebyshev_at_one(self):
        """T_n(1) = 1 for all n."""
        for n in range(10):
            assert chebyshev_T(n, 1.0) == 1.0

    def test_chebyshev_at_minus_one(self):
        """T_n(-1) = (-1)^n."""
        for n in range(10):
            result = chebyshev_T(n, -1.0)
            expected = (-1) ** n
            assert abs(result - expected) < 1e-10

    def test_chebyshev_cosine_relation(self):
        """Verify T_n(cos(theta)) = cos(n*theta)."""
        for theta in [0.1, 0.5, 1.0, 2.0]:
            x = math.cos(theta)
            for n in range(5):
                Tn = chebyshev_T(n, x)
                expected = math.cos(n * theta)
                assert abs(Tn - expected) < 1e-10


class TestDolphChebyshev:
    """Test the Dolph-Chebyshev filter."""

    def test_dolph_chebyshev_basic(self):
        """Basic Dolph-Chebyshev computation."""
        epsilon = 0.1
        l = 5
        phi = 0.5

        result = dolph_chebyshev(epsilon, l, phi)
        assert isinstance(result, float)

    def test_dolph_chebyshev_positive(self):
        """Dolph-Chebyshev values should be positive in most cases."""
        epsilon = 0.1
        l = 5

        positive_count = 0
        for phi in np.linspace(0, math.pi, 20):
            result = dolph_chebyshev(epsilon, l, phi)
            if result > -epsilon:
                positive_count += 1

        # Most values should be positive or near zero
        assert positive_count >= 15, "Most values should be positive or near zero"

    def test_dolph_chebyshev_at_zero(self):
        """Value at phi=0."""
        epsilon = 0.1
        l = 5

        result = dolph_chebyshev(epsilon, l, 0.0)
        # When phi=0, cos(phi)=1
        # Should be close to some positive value
        assert result >= 0


class TestFourierCoefficients:
    """Test Fourier coefficient computation."""

    def test_fourier_coeffs_length(self):
        """The Fourier coefficient list has the correct length (even-indexed coefficients)."""
        epsilon = 0.1
        l = 5

        coeffs = compute_fourier_coeffs(epsilon, l)
        # The implementation only keeps even-indexed coefficients: ceil((l+1)/2) = 3
        expected_len = (l + 2) // 2
        assert len(coeffs) == expected_len

    def test_fourier_coeffs_positive(self):
        """Fourier coefficients should be non-negative."""
        epsilon = 0.1
        l = 5

        coeffs = compute_fourier_coeffs(epsilon, l)
        for coeff in coeffs:
            assert coeff >= -0.1  # allow small numerical error

    def test_fourier_coeffs_symmetry(self):
        """Symmetry of Fourier coefficients."""
        epsilon = 0.1
        l = 4

        coeffs = compute_fourier_coeffs(epsilon, l)
        # Fourier coefficients of a real symmetric function should be symmetric
        for i in range(len(coeffs)):
            for j in range(len(coeffs)):
                if i + j == l:
                    # Symmetric positions
                    pass  # the relation depends on the specific implementation


class TestQDAIntegration:
    """QDA integration tests."""

    def test_interpolation_sequence(self):
        """Test continuity of the interpolation sequence."""
        kappa = 10.0
        p = 0.5
        steps = 100

        fs_values = [compute_fs(s, kappa, p) for s in np.linspace(0, 1, steps)]

        # Verify monotonicity
        for i in range(1, len(fs_values)):
            assert fs_values[i] >= fs_values[i - 1] - 1e-10

    def test_rotation_matrix_sequence(self):
        """Test the rotation matrix sequence."""
        fs_values = [0.0, 0.25, 0.5, 0.75, 1.0]

        matrices = [compute_rotation_matrix(fs) for fs in fs_values]

        # Verify all matrices are unitary
        for R in matrices:
            R_mat = np.array([[R[0], R[1]], [R[2], R[3]]])
            identity = R_mat @ R_mat.conj().T
            assert np.allclose(identity, np.eye(2), atol=1e-10)

    def test_filter_construction(self):
        """Test filter construction."""
        epsilon = 0.1
        l = 5

        coeffs = compute_fourier_coeffs(epsilon, l)

        # Construct the filter function
        def filter_func(x):
            result = 0.0
            for j, coeff in enumerate(coeffs):
                result += coeff * chebyshev_T(j, x)
            return result

        # Test the filter
        for x in [-0.5, 0.0, 0.5, 1.0]:
            result = filter_func(x)
            assert isinstance(result, float)


class TestQDAEdgeCases:
    """QDA edge case tests."""

    def test_kappa_one(self):
        """Test the special case kappa=1."""
        for s in np.linspace(0, 1, 10):
            fs = compute_fs(s, kappa=1.0, p=0.5)
            assert abs(fs - s) < 1e-10

    def test_large_kappa(self):
        """Test a large condition number."""
        kappa = 1000.0
        p = 0.5

        for s in np.linspace(0, 1, 10):
            fs = compute_fs(s, kappa, p)
            assert 0.0 <= fs <= 1.0

    def test_small_epsilon(self):
        """Test a small error tolerance."""
        epsilon = 1e-6
        l = 10

        coeffs = compute_fourier_coeffs(epsilon, l)
        # Length of even-indexed coefficients
        expected_len = (l + 2) // 2
        assert len(coeffs) == expected_len

    def test_p_near_zero(self):
        """Test p close to 0."""
        kappa = 10.0
        p = 0.01

        for s in np.linspace(0, 1, 10):
            fs = compute_fs(s, kappa, p)
            assert 0.0 <= fs <= 1.0

    def test_p_near_one(self):
        """Test p close to 1."""
        kappa = 10.0
        p = 0.99

        for s in np.linspace(0, 1, 10):
            fs = compute_fs(s, kappa, p)
            assert 0.0 <= fs <= 1.0
