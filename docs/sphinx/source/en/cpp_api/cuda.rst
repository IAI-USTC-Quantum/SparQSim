CUDA Backend
============

Build-side instructions: :doc:`GPU Build </guide/development/workflow>` in the development workflow.

.. note::

   The CUDA/GPU backend is controlled by the ``SPARQ_ENABLE_CUDA`` switch (OFF by default; only the CPU
   path is compiled). Enabling it requires a local CUDA toolchain (CUDA 13 / CCCL 3 and CUDA 12.8 /
   sm_80 on an 8×A800 Linux server tested; verification record: ``docs/verification/v1c-gpu-cuda-a800.md``
   in the repository). GPU kernels for the production CondRot primitives are not yet available; the
   relevant operators automatically fall back to the CPU path on GPU states.

   For large sparse-state runs the per-amplitude payload can be tuned with
   ``-DCACHED_REGISTER_SIZE=<n>`` (default 64; must cover the circuit's live
   register count, asserted <= 64 on CUDA). The 2^20-dimensional QDA
   linear-solver production benchmark runs with 8; combined with the
   element-parallel ``Hadamard_Int_Full`` kernels this brings the
   40568-step walk to ~9.6 GB peak memory and ~1.3 s/step on one A800
   (``docs/verification/v1d-qda-linsolver-a800.md``).

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
