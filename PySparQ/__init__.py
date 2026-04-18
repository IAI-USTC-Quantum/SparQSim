"""
PySparQ - QRAM Simulator Python Interface

This package provides Python bindings for the QRAM sparse state simulator.
"""

# 从 pysparq 子模块导入核心功能
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
    # pysparq._core 可能尚未编译
    __version__ = "0.0.0.dev0"

# 导入动态算子模块
try:
    from .dynamic_operator import compile_operator
except ImportError:
    # dynamic_operator 可能尚未安装
    pass

# 定义公开接口
__all__ = [
    # 核心类
    "SparseState",
    "System", 
    "BaseOperator",
    "SelfAdjointOperator",
    "StateStorage",
    # 动态算子
    "compile_operator",
    # 版本
    "__version__",
]
