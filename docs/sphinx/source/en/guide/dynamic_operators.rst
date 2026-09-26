Dynamic Operator Extension
==========================

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

The dynamic operator extension allows you to write custom C++ quantum operators at runtime, compile them into shared libraries, and use them directly from Python. This makes the following scenarios possible:

- **Performance optimization**: C++-compiled operators are faster than pure Python implementations
- **Custom quantum gates**: implement specialized quantum gates that do not exist in the standard library
- **Rapid prototyping**: test new operators without recompiling the entire PySparQ library
- **Algorithm-specific optimization**: create tailored operators for specific algorithms

Applicable Scenarios
~~~~~~~~~~~~~~~~~~~~

Dynamic operators are best suited for the following scenarios:

1. **Performance-critical operations**: complex quantum operations that require C++-level performance
2. **Research prototypes**: quickly test new quantum gate designs
3. **Algorithm customization**: create dedicated operators for specific quantum algorithms
4. **Teaching demonstrations**: show concrete implementations of quantum gates

Prerequisites
~~~~~~~~~~~~~

Before using dynamic operators, make sure that:

- g++ or clang++ compiler is installed
- PySparQ is properly installed (``pip install pysparq``)
- You are familiar with basic C++ and quantum computing concepts

Architecture Overview
---------------------

The workflow of a dynamic operator is as follows:

.. code-block:: text

   Python Layer                        C++ Layer
   +-------------------+              +-------------------+
   | compile_operator  | -----------> | g++ compile       |
   +-------------------+              +-------------------+
            |                                  |
            v                                  v
   +-------------------+              +-------------------+
   | DynamicOpClass    | <----------> | .so shared        |
   | (via type())      |   ctypes     | library (factory) |
   +-------------------+              +-------------------+
            |                                  |
            v                                  v
   +-------------------+              +-------------------+
   | SparseState       | <----------> | BaseOperator      |
   | (Python object)   |              | (C++ instance)    |
   +-------------------+              +-------------------+

Factory Functions
~~~~~~~~~~~~~~~~~

The compiled shared library exports the following C-style factory functions:

.. code-block:: cpp

   // Create an operator instance
   extern "C" BaseOperator* create_operator(...);

   // Destroy an operator instance
   extern "C" void destroy_operator(BaseOperator* op);

   // Get the operator name
   extern "C" const char* get_operator_name();

   // Get the base class name
   extern "C" const char* get_base_class();

   // Helper functions invoked from Python
   extern "C" void apply_operator(BaseOperator* op, SparseState* state);
   extern "C" void apply_operator_dag(BaseOperator* op, SparseState* state);

Base Class Selection
--------------------

Dynamic operators support two base classes (see :ref:`SelfAdjointOperator vs BaseOperator <selfadjoint-vs-baseoperator>`): ``BaseOperator`` and ``SelfAdjointOperator``.

Comparison
~~~~~~~~~~

.. list-table:: Base class comparison
   :header-rows: 1

   * - Feature
     - :class:`SelfAdjointOperator <pysparq.SelfAdjointOperator>`
     - :class:`BaseOperator <pysparq.BaseOperator>`
   * - Dagger behavior
     - Automatically equal to itself
     - Must be implemented manually
   * - Applicable scenarios
     - Hermitian operators (X, Z, Hadamard)
     - Non-Hermitian operators (phase rotations, general unitary gates)
   * - Implementation complexity
     - Simple (only operator() is needed)
     - More complex (dag() must be implemented)
   * - Typical examples
     - Pauli gates, controlled-NOT
     - Phase gates, controlled phase gates

SelfAdjointOperator
~~~~~~~~~~~~~~~~~~~

Use ``SelfAdjointOperator`` when the operator meets the following conditions:

- The operator is Hermitian (equal to its own conjugate transpose)
- The dagger operation should be the operator itself (e.g. the X gate)
- A simpler implementation is desired

**Example: flip operator**

.. code-block:: python

   cpp_code = """
   class FlipOp : public SelfAdjointOperator {
       size_t reg_id;
   public:
       FlipOp(size_t r) : reg_id(r) {}
       void operator()(std::vector<System>& state) const override {
           for (auto& s : state) {
               s.get(reg_id).value ^= 1;  // flip the bit
           }
       }
   };
   """

BaseOperator
~~~~~~~~~~~~

Use ``BaseOperator`` when the operator meets the following conditions:

- The operator is not Hermitian
- Custom dagger behavior is required
- Phase rotations or general unitary gates

**Example: phase rotation gate**

.. code-block:: python

   cpp_code = """
   class PhaseGate : public BaseOperator {
       size_t reg_id;
       double phase;
   public:
       PhaseGate(size_t r, double p) : reg_id(r), phase(p) {}
       void operator()(std::vector<System>& state) const override {
           for (auto& s : state) {
               s.amplitude *= std::exp(std::complex<double>(0, phase));
           }
       }
       void dag(std::vector<System>& state) const override {
           for (auto& s : state) {
               s.amplitude *= std::exp(std::complex<double>(0, -phase));
           }
       }
   };
   """

Basic Usage
-----------

Step 1: Write the C++ Operator Code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Write a C++ class that inherits from ``BaseOperator`` or ``SelfAdjointOperator``:

.. code-block:: python

   cpp_code = """
   class MyFlipOp : public SelfAdjointOperator {
       size_t reg_id;
   public:
       MyFlipOp(size_t r) : reg_id(r) {}
       void operator()(std::vector<System>& state) const override {
           for (auto& s : state) {
               s.get(reg_id).value ^= 1;
           }
       }
   };
   """

Step 2: Compile the Operator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use :func:`compile_operator() <pysparq.dynamic_operator.compile_operator>` to compile the code and create a Python class:

.. code-block:: python

   from pysparq.dynamic_operator import compile_operator

   MyFlipOp = compile_operator(
       name="MyFlipOp",
       cpp_code=cpp_code,
       base_class="SelfAdjointOperator",
       constructor_args=[("size_t", "reg_id")]
   )

Step 3: Use the Operator
~~~~~~~~~~~~~~~~~~~~~~~~

Use the dynamic operator just like a native PySparQ operator:

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   state = ps.SparseState()
   q = ps.System.add_register("q", ps.Boolean, 1)

   op = MyFlipOp(reg_id=q)
   op(state)  # apply the operator

Quantum Computing Examples
--------------------------

Example 1: Controlled Phase Gate
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Implement a controlled phase gate that applies a phase when both the control bit and the target bit are ``|1⟩``:

.. code-block:: python

   import pysparq as ps
   from pysparq.dynamic_operator import compile_operator
   import math

   # Define the controlled phase gate
   controlled_phase_code = """
   class ControlledPhase : public BaseOperator {
       size_t control_reg;
       size_t target_reg;
       double phase;
   public:
       ControlledPhase(size_t c, size_t t, double theta)
           : control_reg(c), target_reg(t), phase(theta) {}

       void operator()(std::vector<System>& state) const override {
           for (auto& s : state) {
               if (s.get(control_reg).value && s.get(target_reg).value) {
                   s.amplitude *= std::exp(std::complex<double>(0, phase));
               }
           }
       }

       void dag(std::vector<System>& state) const override {
           for (auto& s : state) {
               if (s.get(control_reg).value && s.get(target_reg).value) {
                   s.amplitude *= std::exp(std::complex<double>(0, -phase));
               }
           }
       }
   };
   """

   ControlledPhase = compile_operator(
       name="ControlledPhase",
       cpp_code=controlled_phase_code,
       base_class="BaseOperator",
       constructor_args=[
           ("size_t", "control_reg"),
           ("size_t", "target_reg"),
           ("double", "phase")
       ]
   )

   # Create the quantum state
   ps.System.clear()
   state = ps.SparseState()
   c = ps.System.add_register("control", ps.Boolean, 1)
   t = ps.System.add_register("target", ps.Boolean, 1)

   # Initialize to |11⟩
   ps.Init_Unsafe("control", 1)(state)
   ps.Init_Unsafe("target", 1)(state)

   # Apply the controlled phase gate (π/4 phase)
   op = ControlledPhase(control_reg=c, target_reg=t, phase=math.pi/4)
   op(state)

   # Inspect the result
   ps.pprint(state)

Example 2: Quantum Walk Operator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Implement the shift operator of a quantum walk:

.. code-block:: python

   # Quantum walk shift operator
   quantum_walk_code = """
   class QuantumWalkStep : public SelfAdjointOperator {
       size_t position_reg;
       size_t coin_reg;
       size_t n_positions;
   public:
       QuantumWalkStep(size_t pos, size_t coin, size_t n)
           : position_reg(pos), coin_reg(coin), n_positions(n) {}

       void operator()(std::vector<System>& state) const override {
           for (auto& s : state) {
               size_t coin_val = s.get(coin_reg).value;
               size_t pos = s.get(position_reg).value;

               // Coin value 0 moves right, coin value 1 moves left
               if (coin_val == 0 && pos < n_positions - 1) {
                   s.get(position_reg).value = pos + 1;
               } else if (coin_val == 1 && pos > 0) {
                   s.get(position_reg).value = pos - 1;
               }
           }
       }
   };
   """

   QuantumWalkStep = compile_operator(
       name="QuantumWalkStep",
       cpp_code=quantum_walk_code,
       base_class="SelfAdjointOperator",
       constructor_args=[
           ("size_t", "position_reg"),
           ("size_t", "coin_reg"),
           ("size_t", "n_positions")
       ]
   )

Example 3: Grover Search Oracle
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Implement a custom Grover search oracle (the built-in version lives in the :doc:`algorithm library </cpp_api/algorithms>`):

.. code-block:: python

   # Custom marking oracle
   oracle_code = """
   class MarkOracle : public SelfAdjointOperator {
       size_t data_reg;
       uint64_t target_value;
   public:
       MarkOracle(size_t d, uint64_t t) : data_reg(d), target_value(t) {}

       void operator()(std::vector<System>& state) const override {
           for (auto& s : state) {
               if (s.get(data_reg).value == target_value) {
                   s.amplitude *= -1.0;  // mark the target state
               }
           }
       }
   };
   """

   MarkOracle = compile_operator(
       name="MarkOracle",
       cpp_code=oracle_code,
       base_class="SelfAdjointOperator",
       constructor_args=[
           ("size_t", "data_reg"),
           ("uint64_t", "target_value")
       ]
   )

Example 4: Hamiltonian Evolution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Implement a time-evolution operator (related: :doc:`Hamiltonian simulation </cpp_api/algorithms>` in the algorithm library):

.. code-block:: python

   # Hamiltonian evolution operator
   hamiltonian_code = """
   class HamiltonianEvolution : public BaseOperator {
       size_t reg_id;
       double coupling_strength;
       double time;
   public:
       HamiltonianEvolution(size_t r, double g, double t)
           : reg_id(r), coupling_strength(g), time(t) {}

       void operator()(std::vector<System>& state) const override {
           double phase = coupling_strength * time;
           for (auto& s : state) {
               double value_phase = phase * s.get(reg_id).value;
               s.amplitude *= std::exp(std::complex<double>(0, value_phase));
           }
       }

       void dag(std::vector<System>& state) const override {
           double phase = -coupling_strength * time;
           for (auto& s : state) {
               double value_phase = phase * s.get(reg_id).value;
               s.amplitude *= std::exp(std::complex<double>(0, value_phase));
           }
       }
   };
   """

   HamiltonianEvolution = compile_operator(
       name="HamiltonianEvolution",
       cpp_code=hamiltonian_code,
       base_class="BaseOperator",
       constructor_args=[
           ("size_t", "reg_id"),
           ("double", "coupling_strength"),
           ("double", "time")
       ]
   )

Advanced Features
-----------------

Constructor Arguments
~~~~~~~~~~~~~~~~~~~~~

``constructor_args`` supports the following types:

.. list-table:: Supported argument types
   :header-rows: 1

   * - C++ type
     - Python type
     - Purpose
   * - ``size_t``
     - ``int``
     - Register IDs, sizes
   * - ``int``, ``long``
     - ``int``
     - Integer arguments
   * - ``double``, ``float``
     - ``float``
     - Angles, phases, constants
   * - ``bool``
     - ``bool``
     - Toggle flags
   * - ``uint64_t``
     - ``int``
     - Large integers

Example with multiple arguments:

.. code-block:: python

   constructor_args=[
       ("size_t", "control_reg"),
       ("size_t", "target_reg"),
       ("double", "angle"),
       ("int", "iterations")
   ]

Compilation Cache
~~~~~~~~~~~~~~~~~

Dynamic operators use a cache mechanism based on a hash of the code to avoid repeated compilation. Inspect it with :func:`get_cache_info() <pysparq.dynamic_operator.get_cache_info>` and clear it with :func:`clear_cache() <pysparq.dynamic_operator.clear_cache>`:

.. code-block:: python

   from pysparq.dynamic_operator import get_cache_info, clear_cache

   # Inspect the cache status
   info = get_cache_info()
   print(f"Cache directory: {info['cache_dir']}")
   print(f"Cache file count: {info['so_count']}")
   print(f"Cache size: {info['total_size_mb']} MB")

   # Clear the cache
   cleared_count = clear_cache()
   print(f"Cleared {cleared_count} cache files")

Performance Optimization
~~~~~~~~~~~~~~~~~~~~~~~~

Tips for improving operator performance:

1. **Choose the right base class**: for Hermitian operators, using ``SelfAdjointOperator`` simplifies the implementation
2. **Reduce memory allocations**: avoid dynamic memory allocation inside ``operator()``
3. **Use const references**: use ``const`` references wherever possible
4. **Avoid heavy computation at apply time**: move complex computations into the constructor as precomputation

Troubleshooting
---------------

Common Compilation Errors
~~~~~~~~~~~~~~~~~~~~~~~~~

**Header file not found**

.. code-block:: text

   error: 'basic_components.h' file not found

Solution: make sure PySparQ is properly installed and that the header files (see the :doc:`C++ API reference </cpp_api/core>`) are located in the include/ directory.

**Undefined symbols**

.. code-block:: text

   error: undefined reference to 'qram_simulator::System::get'

Solution: check that the namespace is correct (use ``qram_simulator``, see the :doc:`architecture overview </guide/architecture>`) and that the types match.

**Windows ABI compatibility issues**

Solution: when the main library is compiled with MSVC on Windows, dynamic operators may run into ABI compatibility problems. Using a consistent compiler is recommended.

Library Loading Errors
~~~~~~~~~~~~~~~~~~~~~~

**Library file does not exist**

.. code-block:: python

   DynamicOperatorLoadError: Dynamic library does not exist: /path/to/lib.so

Solution: check that compilation succeeded and verify the permissions of the cache directory.

**Factory functions not found**

.. code-block:: python

   DynamicOperatorLoadError: Cannot find required factory functions

Solution: make sure the code defines the factory functions exported with ``extern "C"``.

Argument Errors
~~~~~~~~~~~~~~~

**Invalid base class**

.. code-block:: python

   ValueError: base_class must be one of ['BaseOperator', 'SelfAdjointOperator']

Solution: make sure the ``base_class`` argument is ``"BaseOperator"`` or ``"SelfAdjointOperator"``.

**Empty name or code**

.. code-block:: python

   ValueError: name must be a valid string

Solution: make sure the ``name`` and ``cpp_code`` arguments are not empty.

API Reference
-------------

For detailed API documentation, see :doc:`../api/dynamic_operator`.
