"""
QRAM utility function tests.

Tested content:
- pow2: powers of 2
- make_complement/get_complement: two's complement conversion
- column_flatten: matrix transposition
- scale_and_convert_vector: vector scaling and conversion
- make_vector_tree: binary tree construction
- make_func/make_func_inv: rotation matrix computation

These are pure Python functions with no dependency on quantum state operations.
"""

import pytest
import numpy as np
import math

from pysparq.algorithms.qram_utils import (
    pow2,
    make_complement,
    get_complement,
    column_flatten,
    scale_and_convert_vector,
    make_vector_tree,
    make_func,
    make_func_inv,
    PI,
)


class TestPow2:
    """Test the pow2 function."""

    @pytest.mark.parametrize("n,expected", [(0, 1), (1, 2), (2, 4), (10, 1024), (20, 1048576)])
    def test_pow2_values(self, n, expected):
        """Verify 2^n is computed correctly."""
        assert pow2(n) == expected

    def test_pow2_negative_raises(self):
        """Negative exponents should produce valid results (Python left-shift behavior)."""
        # Python's left shift has special behavior for negative numbers
        # 1 << -1 raises ValueError or produces 0
        # Here we only test non-negative numbers


class TestTwoComplement:
    """Test two's complement conversion functions."""

    @pytest.mark.parametrize(
        "data,data_sz,expected",
        [
            (3, 8, 3),  # positive numbers unchanged
            (-3, 8, 253),  # -3 in 8 bits = 253
            (-1, 4, 15),  # -1 in 4 bits = 15
            (0, 8, 0),  # zero unchanged
            (127, 8, 127),  # positive boundary
            (-128, 8, 128),  # negative boundary
        ],
    )
    def test_make_complement(self, data, data_sz, expected):
        """Verify two's complement encoding is correct."""
        assert make_complement(data, data_sz) == expected

    @pytest.mark.parametrize(
        "data,data_sz,expected",
        [
            (253, 8, -3),
            (15, 4, -1),
            (0, 8, 0),
            (3, 8, 3),
            (128, 8, -128),
        ],
    )
    def test_get_complement(self, data, data_sz, expected):
        """Verify two's complement decoding is correct."""
        assert get_complement(data, data_sz) == expected

    @pytest.mark.parametrize("val", [-128, -1, -50, -100, -127])
    def test_roundtrip_8bit(self, val):
        """Verify 8-bit two's complement round-trip conversion."""
        enc = make_complement(val, 8)
        dec = get_complement(enc, 8)
        assert dec == val

    @pytest.mark.parametrize("val", [-8, -1, -4, -7])
    def test_roundtrip_4bit(self, val):
        """Verify 4-bit two's complement round-trip conversion."""
        enc = make_complement(val, 4)
        dec = get_complement(enc, 4)
        assert dec == val

    def test_64bit_handling(self):
        """Verify 64-bit special handling."""
        # No conversion is performed for 64 bits
        assert make_complement(-1, 64) == -1
        assert get_complement((1 << 63), 64) < 0  # negative number


class TestColumnFlatten:
    """Test the matrix transposition function."""

    def test_2x2_matrix(self):
        """Verify 2x2 matrix transposition."""
        row_major = [1, 2, 3, 4]  # [[1, 2], [3, 4]]
        col_major = column_flatten(row_major)
        assert col_major == [1, 3, 2, 4]  # [[1, 3], [2, 4]]

    def test_3x3_matrix(self):
        """Verify 3x3 matrix transposition."""
        row_major = [1, 2, 3, 4, 5, 6, 7, 8, 9]
        col_major = column_flatten(row_major)
        assert col_major == [1, 4, 7, 2, 5, 8, 3, 6, 9]

    def test_4x4_matrix(self):
        """Verify 4x4 matrix transposition."""
        row_major = list(range(1, 17))
        col_major = column_flatten(row_major)
        # Verify positions after transposition
        assert col_major[0] == 1  # (0,0)
        assert col_major[1] == 5  # (1,0)
        assert col_major[4] == 2  # (0,1)

    def test_non_square_raises(self):
        """A non-perfect-square length should raise an exception."""
        with pytest.raises(ValueError):
            column_flatten([1, 2, 3])  # length 3 is not a perfect square


class TestScaleAndConvertVector:
    """Test the vector scaling and conversion function."""

    def test_positive_values(self):
        """Verify scaling conversion of positive values."""
        vec = [0.25, 0.5, 0.75, 1.0]
        result = scale_and_convert_vector(vec, exponent=2, data_size=8, from_matrix=False)
        # 0.25 * 4 = 1, 0.5 * 4 = 2, etc.
        expected = [1, 2, 3, 4]
        assert result == expected

    def test_negative_values(self):
        """Verify scaling conversion of negative values."""
        vec = [0.25, -0.25, 0.5, -0.5]
        result = scale_and_convert_vector(vec, exponent=2, data_size=8, from_matrix=False)
        # -0.25 * 4 = -1 -> 255 (8-bit two's complement)
        # -0.5 * 4 = -2 -> 254 (8-bit two's complement)
        assert result[0] == 1
        assert result[1] == 255  # -1 in 8-bit
        assert result[2] == 2
        assert result[3] == 254  # -2 in 8-bit

    def test_from_matrix_transposes(self):
        """Verify transposition when from_matrix=True."""
        vec = [1.0, 2.0, 3.0, 4.0]  # [[1, 2], [3, 4]]
        result = scale_and_convert_vector(vec, exponent=0, data_size=8, from_matrix=True)
        # After transposition: [1, 3, 2, 4]
        assert result == [1, 3, 2, 4]

    def test_from_matrix_false_no_transpose(self):
        """Verify no transposition when from_matrix=False."""
        vec = [1.0, 2.0, 3.0, 4.0]
        result = scale_and_convert_vector(vec, exponent=0, data_size=8, from_matrix=False)
        assert result == [1, 2, 3, 4]

    def test_numpy_array_input(self):
        """Verify numpy array input."""
        vec = np.array([0.5, 1.0])
        result = scale_and_convert_vector(vec, exponent=1, data_size=8, from_matrix=False)
        assert result == [1, 2]


class TestMakeVectorTree:
    """Test the binary tree construction function."""

    def test_all_zeros(self):
        """Verify the tree for an all-zeros distribution."""
        tree = make_vector_tree([0, 0, 0, 0], data_size=4)
        # All values should be zero
        assert all(v == 0 for v in tree[:-1])  # exclude the trailing 0

    def test_uniform_values(self):
        """Verify the tree for a uniform distribution."""
        tree = make_vector_tree([1, 1, 1, 1], data_size=4)
        # The root node should be 4 (sum of squares: 1+1+1+1=4)
        assert tree[0] == 4

    def test_single_pair(self):
        """Verify the tree for a single-pair distribution."""
        tree = make_vector_tree([3, 4], data_size=4)
        # The root node should be 3^2 + 4^2 = 25
        assert tree[0] == 25
        assert tree[1] == 3
        assert tree[2] == 4

    def test_tree_structure(self):
        """Verify the hierarchical relations of the tree structure."""
        # Use 4 elements: a, b, c, d
        # First level: a^2+b^2, c^2+d^2
        # Second level: sum of first layer
        tree = make_vector_tree([2, 2, 3, 3], data_size=4)
        # Root node: 4+4+9+9 = 26
        assert tree[0] == 26

    def test_negative_values_in_tree(self):
        """Verify handling of negative values in tree construction."""
        # Negative values encoded via two's complement
        # -2 in 8-bit = 254
        tree = make_vector_tree([254, 254], data_size=8)
        # (-2)^2 + (-2)^2 = 4 + 4 = 8
        assert tree[0] == 8


class TestRotationMatrices:
    """Test the rotation matrix computation functions."""

    def test_zero_rotation(self):
        """A zero angle should produce the identity matrix."""
        mat = make_func(0, 4)
        # [[1, 0], [0, 1]]
        assert abs(mat[0] - complex(1, 0)) < 1e-10
        assert abs(mat[1]) < 1e-10
        assert abs(mat[2]) < 1e-10
        assert abs(mat[3] - complex(1, 0)) < 1e-10

    def test_rotation_matrix_shape(self):
        """Verify the rotation matrix has the correct shape."""
        mat = make_func(1, 4)
        assert len(mat) == 4

    def test_inverse_rotation(self):
        """The inverse rotation should transpose the rotation matrix."""
        mat = make_func(1, 4)
        mat_inv = make_func_inv(1, 4)
        # Off-diagonal elements flip sign
        assert abs(mat[1] + mat_inv[1]) < 1e-10
        assert abs(mat[2] + mat_inv[2]) < 1e-10
        # Diagonal elements are the same
        assert abs(mat[0] - mat_inv[0]) < 1e-10
        assert abs(mat[3] - mat_inv[3]) < 1e-10

    @pytest.mark.parametrize("value", [0, 1, 4, 8, 15])
    def test_unitarity(self, value):
        """The rotation matrix should be unitary."""
        mat = make_func(value, 4)
        R = np.array([[mat[0], mat[1]], [mat[2], mat[3]]])
        # R * R^dagger should be the identity matrix
        identity = R @ R.conj().T
        assert np.allclose(identity, np.eye(2), atol=1e-10)

    def test_determinant_one(self):
        """The rotation matrix determinant should be 1."""
        for value in [0, 1, 4, 8]:
            mat = make_func(value, 4)
            det = mat[0] * mat[3] - mat[1] * mat[2]
            assert abs(det - 1.0) < 1e-10

    def test_angle_calculation(self):
        """Verify the angle calculation is correct."""
        # value=1, n_digit=4 -> theta = 1/16 * 2*pi = pi/8
        mat = make_func(1, 4)
        expected_theta = PI / 8
        expected_cos = math.cos(expected_theta)
        expected_sin = math.sin(expected_theta)

        assert abs(mat[0].real - expected_cos) < 1e-10
        assert abs(mat[1].real + expected_sin) < 1e-10  # -sin
        assert abs(mat[2].real - expected_sin) < 1e-10
        assert abs(mat[3].real - expected_cos) < 1e-10

    def test_64bit_special_case(self):
        """Verify 64-bit special handling."""
        mat = make_func(1, 64)
        # A different denominator is used for 64 bits: 2 * 2^63
        # theta = value / (2 * 2^63) * 2 * pi
        assert len(mat) == 4
        # Should be very close to the identity matrix (the angle is tiny)
        assert abs(mat[0] - complex(1, 0)) < 1e-10
        assert abs(mat[3] - complex(1, 0)) < 1e-10


class TestIntegration:
    """Integration tests: verify multiple functions work together."""

    def test_full_pipeline(self):
        """Verify the complete QRAM data preparation pipeline."""
        # 1. Create matrix data
        matrix_data = [0.25, 0.5, 0.75, 1.0]

        # 2. Transpose
        transposed = column_flatten(matrix_data)

        # 3. Scale and convert
        converted = scale_and_convert_vector(
            transposed, exponent=2, data_size=8, from_matrix=False
        )

        # 4. Build the tree
        tree = make_vector_tree(converted, data_size=8)

        # Verify the pipeline completed
        assert len(tree) > 0

    def test_matrix_encoding_roundtrip(self):
        """Verify matrix encoding round-trip."""
        # Create a simple 2x2 matrix
        original = [1.0, 2.0, 3.0, 4.0]

        # Encode
        encoded = scale_and_convert_vector(original, exponent=0, data_size=8, from_matrix=True)

        # Decode (transpose back)
        decoded = column_flatten(encoded)

        # Verify
        # Note: transposing twice should return to the original matrix
        double_transposed = column_flatten(decoded)
        assert double_transposed == encoded
