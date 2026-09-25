Installation
============

Requirements
------------

- Python 3.10 or higher
- NumPy

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

- The CUDA/GPU backend is currently disabled in CMake; source builds default to the CPU-only path.

Verify the Installation
-----------------------

.. code-block:: python

   import pysparq
   pysparq.test_import()

   # Create a quantum system
   system = pysparq.System()
   state = pysparq.SparseState(system)

   print("PySparQ installed successfully!")
