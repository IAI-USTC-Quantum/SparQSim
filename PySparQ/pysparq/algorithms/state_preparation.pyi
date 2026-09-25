"""Quantum state preparation based on QRAM binary-tree decomposition."""

from __future__ import annotations

from typing import Union


class StatePrepViaQRAM:
    """Quantum state-preparation operator based on QRAM binary-tree decomposition."""

    qram: object
    work_qubit: str
    addr_size: int
    data_size: int
    rational_size: int

    def __init__(
        self,
        qram: object,
        work_qubit: str,
        data_size: int,
        rational_size: int,
    ) -> None: ...
    def conditioned_by_nonzeros(
        self, cond: Union[str, int, list[Union[str, int]]]
    ) -> "StatePrepViaQRAM": ...
    def conditioned_by_all_ones(
        self, cond: Union[str, int, list[Union[str, int]]]
    ) -> "StatePrepViaQRAM": ...
    def conditioned_by_bit(
        self, reg: Union[str, int], bit: int
    ) -> "StatePrepViaQRAM": ...
    def clear_conditions(self) -> "StatePrepViaQRAM": ...
    def __call__(self, state: "SparseState") -> None: ...
    def dag(self, state: "SparseState") -> None: ...


class StatePreparation:
    """High-level wrapper managing the full state-preparation pipeline."""

    qubit_number: int
    data_size: int
    data_range: int
    rational_size: int
    dist: list[int]
    tree: list[int]
    qram: Union[object, None]

    def __init__(self, qubit_number: int, data_size: int, data_range: int) -> None: ...
    def random_distribution(self) -> None:
        """Generate a random amplitude distribution."""
        ...
    def show_distribution(self) -> None:
        """Print the raw values and normalized amplitudes of the distribution."""
        ...
    def get_real_dist(self) -> list[float]:
        """Return the normalized amplitude distribution as a list of floats."""
        ...
    def make_tree(self) -> None:
        """Build the binary tree from the current distribution."""
        ...
    def show_tree(self) -> None:
        """Print the binary tree level by level."""
        ...
    def make_qram(self) -> None:
        """Create a QRAM circuit sized for the tree data."""
        ...
    def set_qram(self) -> None:
        """Load the binary-tree data into the QRAM circuit."""
        ...
    def get_fidelity(self) -> float:
        """Compute the fidelity between the prepared state and the target state."""
        ...
    def run(self) -> None:
        """Run the full state-preparation pipeline."""
        ...


def create_state_preparation_demo() -> str:
    """Return a demo script string for state preparation."""
    ...
