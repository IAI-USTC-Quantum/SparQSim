Development Workflow
====================

Environment Setup
-----------------

CPU Build
^^^^^^^^^

.. code-block:: bash

   cd QRAM-Simulator
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)

GPU Build
^^^^^^^^^

The CUDA/GPU backend is off by default; enabling it requires a local CUDA toolchain (CUDA 13 / CCCL 3 tested).

.. code-block:: bash

   cmake .. -DCMAKE_BUILD_TYPE=Release -DSPARQ_ENABLE_CUDA=ON
   make -j$(nproc)

Python Bindings
^^^^^^^^^^^^^^^

.. code-block:: bash

   pip install .

Core Code Structure
-------------------

+----------------------------+-----------------------------------------------+------------------------------------------+
| Component                  | Path                                          | Purpose                                  |
+============================+===============================================+==========================================+
| Sparse state simulator     | ``SparQ/include/sparse_state_simulator.h``    | Core state representation                |
+----------------------------+-----------------------------------------------+------------------------------------------+
| Register management        | ``SparQ/include/system_operations.h``         | Creation, lifetime, storage types        |
+----------------------------+-----------------------------------------------+------------------------------------------+
| Arithmetic operations      | ``SparQ/include/quantum_arithmetic.h``        | Add, Mult, Shift, etc.                   |
+----------------------------+-----------------------------------------------+------------------------------------------+
| Basic gates                | ``SparQ/include/basic_gates.h``               | H, X, Y, Z, CNOT, etc.                   |
+----------------------------+-----------------------------------------------+------------------------------------------+
| QRAM                       | ``SparQ/include/qram.h``                      | QRAM load operations                     |
+----------------------------+-----------------------------------------------+------------------------------------------+
| High-level algorithms      | ``SparQ_Algorithm/``                          | State preparation, block encoding, etc.  |
+----------------------------+-----------------------------------------------+------------------------------------------+

Adding a New Experiment
-----------------------

1. Create the experiment directory structure:

   .. code-block:: text

      Experiments/
      └── MyAlgorithm/
          ├── MyAlgorithmTest.cpp
          └── CMakeLists.txt

2. Write ``CMakeLists.txt``:

   .. code-block:: cmake

      add_executable(MyAlgorithmTest MyAlgorithmTest.cpp)
      target_link_libraries(MyAlgorithmTest PRIVATE SparQ SparQ_Algorithm Common)

3. Register it in ``Experiments/CMakeLists.txt``:

   .. code-block:: cmake

      add_subdirectory(MyAlgorithm)

Git Workflow
------------

Create a branch and develop:

.. code-block:: bash

   # Create a feature branch
   git checkout -b feat/my-algorithm origin/main

   # Develop, test...

   # Push to your fork
   git push origin feat/my-algorithm

Submit a PR to upstream after CI passes.

Running Tests
-------------

.. code-block:: bash

   # Run all tests
   cd build && ctest --output-on-failure

   # Run a specific test
   ./build/bin/MyAlgorithmTest
