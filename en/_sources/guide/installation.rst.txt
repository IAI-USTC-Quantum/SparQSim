Installation
============

Requirements
------------

- Python 3.10 or higher
- :mod:`numpy`

Install from PyPI
-----------------

.. code-block:: bash

   pip install pysparq

Install from Source
-------------------

.. code-block:: bash

   git clone https://github.com/IAI-USTC-Quantum/QRAM-Simulator.git
   cd QRAM-Simulator
   pip install .

Build Requirements
------------------

Building from source requires the following tools:

- CMake 3.15+
- A compiler with C++17 support
- OpenMP (used for parallelization, required)

Optional dependencies:

- The CUDA/GPU backend is currently disabled in CMake; source builds default to the CPU-only path. See the :doc:`CUDA backend </cpp_api/cuda>` overview and the :doc:`development workflow </guide/development/workflow>` for build details.

Verify the Installation
-----------------------

.. code-block:: python

   import pysparq
   pysparq.test_import()

   # Create a sparse quantum state (|0...0> initial state)
   state = pysparq.SparseState()

   print("PySparQ installed successfully!")

This instantiates a :class:`System <pysparq.System>` and a :class:`SparseState <pysparq.SparseState>` — the two core abstractions introduced in :doc:`Core Concepts </guide/core_concepts/index>`.
