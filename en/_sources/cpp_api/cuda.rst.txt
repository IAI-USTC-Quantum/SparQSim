CUDA Backend
============

Build-side instructions: :doc:`GPU Build </guide/development/workflow>` in the development workflow.

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
