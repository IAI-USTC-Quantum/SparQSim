Algorithm Library (Top Level)
=============================

These headers compose the :doc:`core operators </operators/index>` into complete algorithms (Grover, Shor, Hamiltonian simulation). Python-facing counterparts live in ``pysparq.algorithms`` and are exercised in :doc:`Examples </guide/examples>`.

.. note::

   ``SparQ_Algorithm/include/qcnn.h`` (quantum convolutional neural networks) is currently disabled
   as a whole by ``#if false`` in the source, so it does not appear in this reference.

Grover Search (``SparQ_Algorithm/include/grover.h``)
----------------------------------------------------

.. doxygenfile:: grover.h
   :project: SparQ

Shor Factoring (``SparQ_Algorithm/include/shor.h``)
---------------------------------------------------

.. doxygenfile:: shor.h
   :project: SparQ

State Preparation (``SparQ_Algorithm/include/state_preparation.h``)
-------------------------------------------------------------------

.. doxygenfile:: state_preparation.h
   :project: SparQ

Block Encoding Overview (``SparQ_Algorithm/include/block_encoding.h``)
----------------------------------------------------------------------

.. doxygenfile:: block_encoding.h
   :project: SparQ

Hamiltonian Simulation (``SparQ_Algorithm/include/hamiltonian_simulation.h``)
-----------------------------------------------------------------------------

.. doxygenfile:: hamiltonian_simulation.h
   :project: SparQ

QRAM-Free Tridiagonal CKS Walk (``SparQ_Algorithm/include/hamiltonian_simulation_tridiagonal.h``)
--------------------------------------------------------------------------------------------------

For constant-diagonal tridiagonal matrices the CKS walk's QRAM table oracles are
replaced by direct arithmetic oracles (``TridiagOracle1/2``, ``T_Tridiagonal``,
``ChebyshevInverseCoefficients``); validated end to end at 2^20 dimensions on a
single A800 (``docs/verification/v1e-cks-direct-oracle-linsolver.md``).

.. doxygenfile:: hamiltonian_simulation_tridiagonal.h
   :project: SparQ
