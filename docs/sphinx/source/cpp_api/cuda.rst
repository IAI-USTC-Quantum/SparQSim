CUDA Backend
============

.. note::

   The CUDA/GPU backend is controlled by the ``SPARQ_ENABLE_CUDA`` switch (OFF by default; only the CPU
   path is compiled). Enabling it requires a local CUDA toolchain (CUDA 13 / CCCL 3 tested). GPU kernels
   for the production CondRot primitives are not yet available; the relevant operators automatically
   fall back to the CPU path on GPU states.

GPU Basic Components (``SparQ/include/cuda/basic_components.cuh``)
------------------------------------------------------------------

.. doxygenfile:: basic_components.cuh
   :project: SparQ

GPU Conditional Rotations (``SparQ/include/cuda/condrot.cuh``)
--------------------------------------------------------------

.. doxygenfile:: condrot.cuh
   :project: SparQ

GPU Interference Basic Components (``SparQ/include/cuda/quantum_interfere_basic.cuh``)
--------------------------------------------------------------------------------------

.. doxygenfile:: quantum_interfere_basic.cuh
   :project: SparQ

GPU Sparse State Simulator Entry (``SparQ/include/cuda/sparse_state_simulator.cuh``)
------------------------------------------------------------------------------------

.. doxygenfile:: sparse_state_simulator.cuh
   :project: SparQ

----

中文版
===

CUDA 后端
=========

.. note::

   CUDA/GPU 后端由 ``SPARQ_ENABLE_CUDA`` 开关控制（默认 OFF，仅编译 CPU 路径；
   开启需本机 CUDA 工具链，CUDA 13 / CCCL 3 实测通过）。生产 CondRot 原语的
   GPU 内核尚未提供，相关算子在 GPU 态自动回退 CPU 路径执行。

GPU 基础组件（``SparQ/include/cuda/basic_components.cuh``）
------------------------------------------------------------

.. doxygenfile:: basic_components.cuh
   :project: SparQ

GPU 条件旋转（``SparQ/include/cuda/condrot.cuh``）
---------------------------------------------------

.. doxygenfile:: condrot.cuh
   :project: SparQ

GPU 干涉基组件（``SparQ/include/cuda/quantum_interfere_basic.cuh``）
--------------------------------------------------------------------

.. doxygenfile:: quantum_interfere_basic.cuh
   :project: SparQ

GPU 稀疏态模拟器入口（``SparQ/include/cuda/sparse_state_simulator.cuh``）
--------------------------------------------------------------------------

.. doxygenfile:: sparse_state_simulator.cuh
   :project: SparQ
