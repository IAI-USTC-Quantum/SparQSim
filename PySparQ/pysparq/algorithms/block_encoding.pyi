"""Matrix block encoding algorithms (tridiagonal and QRAM-based)."""

from __future__ import annotations

from typing import Union

import numpy as np


def get_tridiagonal_matrix(alpha: float, beta: float, dim: int) -> np.ndarray:
    """Return the dim x dim tridiagonal matrix alpha*I + beta*T."""
    ...


def get_u_plus(size: int) -> np.ndarray:
    """Return the size x size down-shift (subdiagonal) matrix."""
    ...


def get_u_minus(size: int) -> np.ndarray:
    """Return the size x size up-shift (superdiagonal) matrix."""
    ...


class PlusOneAndOverflow:
    """Add 1 to a register and track overflow."""

    main_reg: str
    overflow: str

    def __init__(self, main_reg: str, overflow: str) -> None: ...
    def conditioned_by_nonzeros(
        self, conds: Union[str, list[str]]
    ) -> "PlusOneAndOverflow": ...
    def conditioned_by_all_ones(
        self, conds: Union[str, list[str]]
    ) -> "PlusOneAndOverflow": ...
    def conditioned_by_bit(
        self, reg: Union[str, int], pos: int
    ) -> "PlusOneAndOverflow": ...
    def conditioned_by_value(
        self, reg: Union[str, int], value: int
    ) -> "PlusOneAndOverflow": ...
    def clear_conditions(self) -> None: ...
    def __call__(self, state: "SparseState") -> None: ...
    def dag(self, state: "SparseState") -> None: ...


class BlockEncodingTridiagonal:
    """Block encoding of the tridiagonal matrix alpha*I + beta*T."""

    main_reg: str
    anc_UA: str
    alpha: float
    beta: float
    prep_state: list[complex]

    def __init__(
        self, main_reg: str, anc_UA: str, alpha: float, beta: float
    ) -> None: ...
    def conditioned_by_nonzeros(
        self, conds: Union[str, list[str]]
    ) -> "BlockEncodingTridiagonal": ...
    def conditioned_by_all_ones(
        self, conds: Union[str, list[str]]
    ) -> "BlockEncodingTridiagonal": ...
    def conditioned_by_bit(
        self, reg: Union[str, int], pos: int
    ) -> "BlockEncodingTridiagonal": ...
    def clear_conditions(self) -> None: ...
    def __call__(self, state: "SparseState") -> None: ...
    def dag(self, state: "SparseState") -> None: ...


class UR:
    """Right-multiplication operator based on QRAM block encoding."""

    qram: "QRAMCircuit_qutrit"
    column_index: str
    data_size: int
    rational_size: int
    addr_size: int

    def __init__(
        self,
        qram: "QRAMCircuit_qutrit",
        column_index: str,
        data_size: int,
        rational_size: int,
    ) -> None: ...
    def conditioned_by_nonzeros(
        self, conds: Union[str, list[str]]
    ) -> "UR": ...
    def conditioned_by_all_ones(
        self, conds: Union[str, list[str]]
    ) -> "UR": ...
    def conditioned_by_bit(
        self, reg: Union[str, int], pos: int
    ) -> "UR": ...
    def clear_conditions(self) -> None: ...
    def __call__(self, state: "SparseState") -> None: ...
    def dag(self, state: "SparseState") -> None: ...


class UL:
    """Left-multiplication operator based on QRAM block encoding."""

    qram: "QRAMCircuit_qutrit"
    row_index: str
    column_index: str
    data_size: int
    rational_size: int
    addr_size: int

    def __init__(
        self,
        qram: "QRAMCircuit_qutrit",
        row_index: str,
        column_index: str,
        data_size: int,
        rational_size: int,
    ) -> None: ...
    def conditioned_by_nonzeros(
        self, conds: Union[str, list[str]]
    ) -> "UL": ...
    def conditioned_by_all_ones(
        self, conds: Union[str, list[str]]
    ) -> "UL": ...
    def conditioned_by_bit(
        self, reg: Union[str, int], pos: int
    ) -> "UL": ...
    def clear_conditions(self) -> None: ...
    def __call__(self, state: "SparseState") -> None: ...
    def dag(self, state: "SparseState") -> None: ...


class BlockEncodingViaQRAM:
    """Block encoding of an arbitrary matrix via QRAM."""

    qram: "QRAMCircuit_qutrit"
    column_index: str
    row_index: str
    data_size: int
    rational_size: int

    def __init__(
        self,
        qram: "QRAMCircuit_qutrit",
        column_index: str,
        row_index: str,
        data_size: int,
        rational_size: int,
    ) -> None: ...
    def conditioned_by_nonzeros(
        self, conds: Union[str, list[str]]
    ) -> "BlockEncodingViaQRAM": ...
    def conditioned_by_all_ones(
        self, conds: Union[str, list[str]]
    ) -> "BlockEncodingViaQRAM": ...
    def conditioned_by_bit(
        self, reg: Union[str, int], pos: int
    ) -> "BlockEncodingViaQRAM": ...
    def clear_conditions(self) -> None: ...
    def __call__(self, state: "SparseState") -> None: ...
    def dag(self, state: "SparseState") -> None: ...


def create_block_encoding_demo() -> str:
    """Return a demo script string showing block encoding usage."""
    ...
