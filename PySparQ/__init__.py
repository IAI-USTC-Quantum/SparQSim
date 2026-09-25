"""
PySparQ - QRAM Simulator Python Interface

This package provides Python bindings for the QRAM sparse state simulator.
"""

# Import core functionality from the pysparq submodule
try:
    from .pysparq import *
    from .pysparq import (
        SparseState,
        System,
        BaseOperator,
        SelfAdjointOperator,
        StateStorage,
        __version__,
    )
except ImportError:
    # pysparq._core may not be compiled yet
    __version__ = "0.0.0.dev0"

# Import the dynamic operator module
try:
    from .dynamic_operator import compile_operator
except ImportError:
    # dynamic_operator may not be installed yet
    pass

# Define the public interface
__all__ = [
    # Core classes
    "SparseState",
    "System",
    "BaseOperator",
    "SelfAdjointOperator",
    "StateStorage",
    # Dynamic operator
    "compile_operator",
    # Version
    "__version__",
]
