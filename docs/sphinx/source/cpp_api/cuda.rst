CUDA 后端
=========

.. note::

   CUDA/GPU 后端代码保留在源码树中，但当前 CMake 配置暂时屏蔽 GPU 构建，
   默认只编译 CPU 路径。

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
