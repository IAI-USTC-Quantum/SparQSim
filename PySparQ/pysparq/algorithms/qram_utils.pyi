"""Classical helper functions for building QRAM circuits."""

from __future__ import annotations

from typing import Union

import numpy as np

PI: float
"""Pi constant matching the C++ ``constexpr double pi``."""


def pow2(n: int) -> int:
    """Return ``2**n`` via a left shift, matching ``basic.h`` semantics."""
    ...


def make_complement(data: int, data_sz: int) -> int:
    """Convert a signed integer to its two's-complement representation."""
    ...


def get_complement(data: int, data_sz: int) -> int:
    """Inverse two's complement: sign-extend an unsigned value to a signed integer."""
    ...


def column_flatten(row_vec: list[float]) -> list[float]:
    """Transpose a row-major square matrix into its column-major representation."""
    ...


def scale_and_convert_vector(
    input_vec: Union[list[float], np.ndarray],
    exponent: int,
    data_size: int,
    from_matrix: bool = ...,
) -> list[int]:
    """Scale floating-point values and convert to two's-complement integers."""
    ...


def make_vector_tree(dist: list[int], data_size: int) -> list[int]:
    """Build a binary tree for QRAM circuits from leaf-distribution data."""
    ...


def make_func(value: int, n_digit: int) -> list[complex]:
    """Compute a 2x2 rotation matrix from a rational register value."""
    ...


def make_func_inv(value: int, n_digit: int) -> list[complex]:
    """Compute the inverse 2x2 rotation matrix from a rational register value."""
    ...


def create_qram_utils_demo() -> str:
    """Return a demo script string showing typical usage of this module."""
    ...
