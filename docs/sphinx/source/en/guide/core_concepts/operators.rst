Operators
=========

The operator (Operator) is the core abstraction for quantum operations in SparQ. Unlike traditional quantum circuit simulators, which describe quantum algorithms as sequences of gates, SparQ abstracts every transformation as a **callable object** (a C++ functor), separating the definition of an operator from the objects it acts upon.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

The Two-Phase Model
-------------------

SparQ operators follow a two-phase model of **construction → application**:

1. **Construction phase** — specify which registers the operator acts on, together with the relevant parameters. No quantum state is involved in this phase.

2. **Application phase** — apply the constructed operator to a ``SparseState``, completing the quantum state transformation.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Phase 1: construction — specify registers and parameters
   add_op = ps.Add_UInt_UInt("a", "b", "result")

   # Phase 2: application — act on the quantum state
   add_op(state)

The benefits of this separation:

- **Reuse**: the same operator object can be applied to different quantum states repeatedly, without reconstructing it
- **Composition**: operators can carry conditions (conditioned_by), so the control logic can be adjusted flexibly after construction
- **Reversibility**: a constructed operator supports the ``dag()`` method to perform the inverse operation

Construction Parameters
-----------------------

Construction parameters determine the behavior of an operator and usually fall into the following categories:

Register Identifiers
^^^^^^^^^^^^^^^^^^^^

Almost every operator needs to specify its target registers. This can be done with a name string or an integer index:

.. code-block:: python

   # By name (recommended)
   op = ps.Add_UInt_UInt("a", "b", "result")

   # By index
   op = ps.Add_UInt_UInt(0, 1, 2)

Constant Parameters
^^^^^^^^^^^^^^^^^^^

Some operators accept classical constants as parameters, for example the addend in an addition or the multiplier in a multiplication:

.. code-block:: python

   # Constant addition: specify the constant 7
   ps.Add_ConstUInt_InPlace("counter", 7)

   # Constant multiplication: specify the multiplier 3
   ps.Mult_UInt_ConstUInt("input", 3, "result")

Hyperparameters
^^^^^^^^^^^^^^^

Some advanced operators require algorithm-level hyperparameters, such as :math:`\kappa` (condition number) and :math:`\epsilon` (precision) in Hamiltonian simulation, or the step-size parameter :math:`s` in QDA.

Bit Indices and Angles
^^^^^^^^^^^^^^^^^^^^^^

Single-qubit gates need to specify which bit of the register they act on, and rotation gates need the rotation angle:

.. code-block:: python

   # Apply an X gate to bit 0 of register "q"
   ps.X_Bool("q", 0)

   # Rotate by π/4 around the X axis
   ps.RX_Bool("q", 0, np.pi / 4)

Applying Operators
------------------

Direct Application
^^^^^^^^^^^^^^^^^^

After construction, the function-call syntax ``op(state)`` applies the operator to a ``SparseState``:

.. code-block:: python

   op = ps.Hadamard_Int("reg", 4)
   op(state)

Inverse Application
^^^^^^^^^^^^^^^^^^^

For operators derived from ``BaseOperator``, use ``dag()`` to perform the inverse transformation:

.. code-block:: python

   op = ps.ShiftLeft_InPlace("reg", 2)
   op(state)       # shift left by 2 bits
   op.dag(state)   # shift right by 2 bits (undo)

For ``SelfAdjointOperator``, calling ``op(state)`` again is the inverse operation (:math:`U^\dagger = U`).

Conditional Application
^^^^^^^^^^^^^^^^^^^^^^^

All operators support chained conditional methods to implement controlled operations:

.. code-block:: python

   op = ps.Add_UInt_UInt("a", "b", "result")

   # Execute only when "ctrl" is non-zero
   op.conditioned_by_nonzeros("ctrl")(state)

   # Stacking multiple conditions
   op.conditioned_by_nonzeros(["ctrl1", "ctrl2"])(state)

Operator Categories
-------------------

SparQ's operators are divided into the following major categories by function:

.. list-table:: Operator category overview
   :header-rows: 1

   * - Category
     - Description
     - Header file
   * - Quantum arithmetic
     - Register-level operations such as addition, multiplication, shifting, and comparison
     - ``quantum_arithmetic.h``
   * - Basic quantum gates
     - Pauli gates, phase gates, rotation gates, general-purpose gates
     - ``basic_gates.h``
   * - Hadamard operations
     - Hadamard transforms over integers / booleans / partial qubits
     - ``hadamard.h``
   * - QFT
     - Quantum Fourier transform and its inverse
     - ``qft.h``
   * - QRAM operators
     - Quantum random access memory load operations
     - ``qram.h``
   * - Conditional rotation
     - Rotating qubits conditioned on register values
     - ``condrot.h``
   * - Phase and reflection
     - Conditional phase flips, global phase, Grover reflection
     - ``parallel_phase_operations.h``
   * - Rotation and state preparation
     - Unitary rotations of arbitrary dimension, quantum state preparation
     - ``rot.h``
   * - System operations
     - Register stack management, zero-amplitude cleanup, system split/merge
     - ``system_operations.h``
   * - Partial trace
     - Measurement, selective collapse, range collapse
     - ``partial_trace.h``
   * - Sorting
     - Sorting basis states by key value, amplitude, and other dimensions
     - ``sort_state.h``
   * - Dark magic operations
     - Unsafe operations that directly modify the quantum state
     - ``dark_magic.h``
   * - Debugging tools
     - Normalization checks, NaN detection, state printing
     - ``debugger.h``

For the detailed API and usage of each category of operators, see the :ref:`Operator Reference <operator-reference>` section.

Concept: Operators vs. Flow-Control Classes
-------------------------------------------

SparQ's codebase distinguishes between two concepts at different levels:

**Operators** inherit from ``BaseOperator`` or ``SelfAdjointOperator`` and directly manipulate the quantum state (``SparseState``); they are closely tied to quantum algorithms and quantum circuits. Examples include ``CondRot_Fixed_Bool``, ``T`` (the state-preparation operator in CKS), and ``SparseMatrixOracle1``.

**Flow-control classes (Flow-Control / Algorithm Classes)** do not manipulate the quantum state directly; instead they hold registers, orchestrate operator execution, and manage state initialization and iteration. Their responsibility is to **test and verify** whether a quantum procedure is correct, not to implement quantum gates. For example:

- ``QuantumWalkNSteps`` (CKS): manages multi-step quantum walks, register creation, and environment initialization
- ``LCU_Container`` (CKS): manages the Chebyshev LCU iteration loop
- ``WalkS`` (QDA): coordinates block encoding and state preparation in the QDA algorithm

The point of this distinction: operators are the minimal composable, testable units, while flow-control classes are the glue that chains operators together for a particular algorithmic workflow. Flow-control classes should not inherit from the ``Operator`` base class, because their responsibility is **orchestration** rather than **application**.
