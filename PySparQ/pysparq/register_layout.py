"""Logical registers backed by multiple native words, without truncating high bits.

The C++ machine-word ABI stays intact. Every bit operation still executes on
the real sparse-state backend; logical values are reconstructed as Python ints.
"""

from __future__ import annotations

from typing import Any

from . import _core

WORD_BITS = 64


def register_bits(name: str, width: int) -> list[tuple[str, int]]:
    """Resolve one logical register to little-endian native bit coordinates."""
    if type(width) is not int or width < 0:
        raise ValueError("logical register width must be a nonnegative integer")
    if width <= WORD_BITS:
        return [(name, bit) for bit in range(width)]
    return [(f"{name}__word_{bit // WORD_BITS}", bit % WORD_BITS) for bit in range(width)]


def _active_native_names() -> set[str]:
    # Native AddRegister silently permits duplicate names, which would make
    # name-based gate addressing ambiguous; screen candidates against the
    # live register table first. The 4th tuple element is the active flag.
    return {entry[0] for entry in _core.System.name_register_map if entry[3]}


class RegisterLayout:
    """Own logical register allocations in one native SparseState.

    ``add`` returns bit coordinates accepted by native gate operators. ``value``
    reads a complete logical word from a basis state, including every high word.
    Names must be distinct within the process-global native register namespace.
    """

    def __init__(self, state: Any):
        self.state = state
        self.widths: dict[str, int] = {}
        self.words: dict[str, tuple[tuple[str, int], ...]] = {}

    def add(self, name: str, kind: Any, width: int) -> list[tuple[str, int]]:
        bits = register_bits(name, width)
        if name in self.words:
            raise ValueError(f"logical register already exists: {name}")
        words = ((name, width),) if 0 < width <= WORD_BITS else tuple(
            (f"{name}__word_{offset // WORD_BITS}", min(WORD_BITS, width-offset))
            for offset in range(0, width, WORD_BITS)
        )
        clash = {native for native, _ in words} & _active_native_names()
        if clash:
            raise ValueError(
                f"native word register name already in use: {', '.join(sorted(clash))}"
            )
        allocated = []
        try:
            for native, size in words:
                native_kind = kind if width <= WORD_BITS else _core.StateStorageType.General
                _core.AddRegister(native, native_kind, size)(self.state)
                allocated.append(native)
        except Exception:
            for native in reversed(allocated):
                _core.RemoveRegister(native)(self.state)
            raise
        self.widths[name] = width
        self.words[name] = words
        return bits

    def bits(self, name: str) -> list[tuple[str, int]]:
        return register_bits(name, self.widths[name])

    def value(self, name: str, basis: Any) -> int:
        return sum((int(basis.get(_core.System.get_id(native)).value) & ((1 << width)-1))
                   << (WORD_BITS*index)
                   for index, (native, width) in enumerate(self.words[name]))

    def remove(self, name: str, *, require_zero: bool = True) -> None:
        if require_zero and any(self.value(name, basis) for basis in self.state.basis_states):
            raise ValueError(f"logical register not uncomputed: {name}")
        for native, _ in reversed(self.words[name]):
            _core.RemoveRegister(native)(self.state)
        del self.words[name]
        del self.widths[name]
