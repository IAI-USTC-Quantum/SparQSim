Algorithm Guide
===============

The SparQ algorithm stack has two layers that mirror each other:

- **C++ layer** (``SparQ_Algorithm/``): production implementations composed from the :doc:`core operators </operators/index>`, rendered per header in the :doc:`C++ API Reference </cpp_api/algorithms>`.
- **Python layer** (``pysparq.algorithms``): pure-Python counterparts used for rapid prototyping and cross-validation, auto-documented in the API tree below.

Each algorithm below lists its Python entry points, the C++ header it replicates, and where to see it running. The line-level correspondence between the two layers is documented in the :doc:`algorithm porting guide <porting>`.

.. toctree::
   :maxdepth: 1

   porting

Grover Search
-------------

QRAM-oracle-driven Grover search with amplitude amplification and quantum counting.

- Python: :class:`GroverOperator <PySparQ.pysparq.algorithms.grover.GroverOperator>`, :class:`GroverOracle <PySparQ.pysparq.algorithms.grover.GroverOracle>`, :func:`grover_search() <PySparQ.pysparq.algorithms.grover.grover_search>`, :func:`grover_count() <PySparQ.pysparq.algorithms.grover.grover_count>`
- C++: ``grover.h`` in :doc:`the algorithm library </cpp_api/algorithms>`
- Building blocks: the diffusion step is the reflection primitive from :doc:`phase and reflection operators </operators/phase_ops>`; the oracle marks states conditionally (:ref:`conditional operations <conditional-operations>`)
- Try it: the custom-oracle example in :doc:`Dynamic Operator Extension </guide/dynamic_operators>`

Shor's Factoring
----------------

Shor factoring (standard and semiclassical variants) built on modular multiplication.

- Python: :class:`Shor <PySparQ.pysparq.algorithms.shor.Shor>`, :class:`ModMul <PySparQ.pysparq.algorithms.shor.ModMul>`
- C++: ``shor.h`` in :doc:`the algorithm library </cpp_api/algorithms>`
- Building blocks: :doc:`arithmetic operators </operators/arithmetic>` (modular add/mul) and :doc:`QFT </operators/qft>`

Quantum Linear Systems (QDA)
----------------------------

Discrete-adiabatic quantum linear-system solvers built on block encoding and state preparation.

- Python: :func:`qda_solve() <PySparQ.pysparq.algorithms.qda_solver.qda_solve>`, :func:`qda_solve_tridiagonal() <PySparQ.pysparq.algorithms.qda_solver.qda_solve_tridiagonal>`, :func:`qda_solve_via_qram() <PySparQ.pysparq.algorithms.qda_solver.qda_solve_via_qram>`, plus the orchestration classes :class:`WalkS <PySparQ.pysparq.algorithms.qda_solver.WalkS>`, :class:`LCU <PySparQ.pysparq.algorithms.qda_solver.LCU>`, :class:`BlockEncoding <PySparQ.pysparq.algorithms.qda_solver.BlockEncoding>`, and :class:`StatePreparation <PySparQ.pysparq.algorithms.qda_solver.StatePreparation>`
- C++: :doc:`Discrete Adiabatic (QDA) </cpp_api/qda>`
- Building blocks: block encoding (below); the step-size/condition-number hyperparameters are introduced in :doc:`Operators </guide/core_concepts/operators>`

Hamiltonian Simulation (CKS)
----------------------------

Hamiltonian simulation via quantum walk / LCU / sparse-matrix oracle, with the CKS experiment as the reference implementation.

- Python: :class:`TOperator <PySparQ.pysparq.algorithms.cks_solver.TOperator>`, :class:`QuantumWalkNSteps <PySparQ.pysparq.algorithms.cks_solver.QuantumWalkNSteps>` in ``pysparq.algorithms.cks_solver``
- C++: ``hamiltonian_simulation.h`` in :doc:`the algorithm library </cpp_api/algorithms>` (the ``T`` state-preparation operator and ``SparseMatrixOracle1`` live here)
- Related: the quantum-walk dynamic operator example in :doc:`Dynamic Operator Extension </guide/dynamic_operators>`

Block Encoding
--------------

Encodes a classical matrix into a larger unitary — the core subroutine of the linear-system and Hamiltonian-simulation algorithms above.

- Python: :class:`BlockEncodingTridiagonal <PySparQ.pysparq.algorithms.block_encoding.BlockEncodingTridiagonal>`, :class:`BlockEncodingViaQRAM <PySparQ.pysparq.algorithms.block_encoding.BlockEncodingViaQRAM>` in ``pysparq.algorithms.block_encoding``
- C++: :doc:`Block Encoding </cpp_api/block_encoding>`
- Try it: :doc:`Example 4 </guide/examples>` in the Examples guide (tridiagonal walkthrough with runnable code) and the Block Encoding section of the :doc:`operator usage notebook </notebooks/03_operator_examples>`

State Preparation
-----------------

QRAM-based preparation of arbitrary target states from ``|0⟩``.

- Python: :class:`StatePrepViaQRAM <PySparQ.pysparq.algorithms.state_preparation.StatePrepViaQRAM>`, :class:`StatePreparation <PySparQ.pysparq.algorithms.state_preparation.StatePreparation>`
- C++: ``state_preparation.h`` in :doc:`the algorithm library </cpp_api/algorithms>`
- Building blocks: :doc:`rotation and state preparation operators </operators/rot_state_prep>` and :doc:`QRAM operators </operators/qram_ops>`
