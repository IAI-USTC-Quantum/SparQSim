CUDA 后端
=========

构建侧说明见开发工作流中的 :doc:`GPU 构建 </guide/development/workflow>`。

.. note::

   CUDA/GPU 后端由 ``SPARQ_ENABLE_CUDA`` 开关控制（默认 OFF，仅编译 CPU 路径；
   开启需本机 CUDA 工具链，CUDA 13 / CCCL 3 与 8×A800 Linux 服务器上的
   CUDA 12.8 / sm_80 均实测通过，验证记录见仓库内
   ``docs/verification/v1c-gpu-cuda-a800.md``）。生产 CondRot 原语的
   GPU 内核尚未提供，相关算子在 GPU 态自动回退 CPU 路径执行。

   大规模稀疏态运行可用 ``-DCACHED_REGISTER_SIZE=<n>`` 调低每振幅负载
   （默认 64，不得小于电路活跃寄存器数，CUDA 侧断言 <= 64）。2^20 维 QDA
   线性求解器生产基准以 8 编译，配合元素级并行的 ``Hadamard_Int_Full``
   内核，40568 步行走峰值显存约 9.6 GB、约 1.3 秒/步（单卡 A800，
   见 ``docs/verification/v1d-qda-linsolver-a800.md``）。

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
