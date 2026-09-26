.. _operator-reference:

Operator Reference
==================

Operators are the building blocks of quantum operations in PySparQ. All operations are implemented as operator objects that take a :doc:`SparseState </guide/core_concepts/sparse_state>` and transform it.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

What Is an Operator?
--------------------

An **operator** is a callable object that transforms a ``SparseState``, implementing quantum operations while guaranteeing unitarity. The :doc:`Operators </guide/core_concepts/operators>` chapter of the core concepts guide explains the underlying design. For hands-on practice, work through the :doc:`operator usage notebook </notebooks/03_operator_examples>`.

Basic Usage
^^^^^^^^^^^

.. code-block:: python

   import pysparq as ps

   ps.System.clear()

   # Create registers
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Initialize the inputs
   ps.Init_Unsafe("a", 3)(state)
   ps.Init_Unsafe("b", 5)(state)

   # Create and apply an operator
   add_op = ps.Add_UInt_UInt("a", "b", "result")
   add_op(state)  # apply the operator

   # For a non-self-adjoint operator, use dag() to undo the operation
   add_op.dag(state)  # undo (restore the original state)

Operator Properties
-------------------

Unitarity
^^^^^^^^^

All quantum operators must satisfy the unitarity condition:

.. math::

   U^\dagger U = I

PySparQ guarantees unitarity through two mechanisms:

.. list-table:: Unitarity mechanisms
   :header-rows: 1

   * - Type
     - Mechanism
     - Example
   * - Out-of-place
     - XOR write: ``result ^= f(inputs)``
     - :class:`Add_UInt_UInt <pysparq.Add_UInt_UInt>`, :class:`Mult_UInt_ConstUInt <pysparq.Mult_UInt_ConstUInt>`
   * - In-place
     - Explicit dagger implementation
     - :class:`Add_UInt_UInt_InPlace <pysparq.Add_UInt_UInt_InPlace>`, :class:`ShiftLeft_InPlace <pysparq.ShiftLeft_InPlace>`

.. _selfadjoint-vs-baseoperator:

SelfAdjointOperator vs BaseOperator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table:: Operator base classes
   :header-rows: 1

   * - Base class
     - Characteristics
     - ``dag()`` behavior
     - Typical operators
   * - :class:`SelfAdjointOperator <pysparq.SelfAdjointOperator>`
     - :math:`U^\dagger = U`
     - :meth:`dag() <pysparq.BaseOperator.dag>` is equivalent to ``operator()``
     - :class:`Add_UInt_UInt <pysparq.Add_UInt_UInt>`, :class:`X_Bool <pysparq.X_Bool>`
   * - :class:`BaseOperator <pysparq.BaseOperator>`
     - General unitary operator
     - Requires an explicit :meth:`dag() <pysparq.BaseOperator.dag>` implementation
     - :class:`Add_UInt_UInt_InPlace <pysparq.Add_UInt_UInt_InPlace>`, :class:`ShiftLeft_InPlace <pysparq.ShiftLeft_InPlace>`

.. code-block:: python

   # SelfAdjointOperator: applying twice restores the original state
   op = ps.Add_UInt_UInt("a", "b", "result")
   op(state)  # apply
   op(state)  # apply again = undo (because XOR is self-inverse)

   # BaseOperator: dag() is required to undo
   op = ps.ShiftLeft_InPlace("reg", 2)
   op(state)      # shift left by 2 bits
   op.dag(state)  # shift right by 2 bits (undo)

Type Constraints
^^^^^^^^^^^^^^^^

Operators impose strict requirements on register types:

.. list-table:: Register types
   :header-rows: 1

   * - Type
     - Description
     - Valid range
   * - :doc:`UnsignedInteger </guide/core_concepts/register_types>`
     - Unsigned integer
     - :math:`[0, 2^n-1]`
   * - :doc:`SignedInteger </guide/core_concepts/register_types>`
     - Signed integer (two's complement)
     - :math:`[-2^{n-1}, 2^{n-1}-1]`
   * - :doc:`Boolean </guide/core_concepts/register_types>`
     - Single qubit
     - {0, 1}
   * - :doc:`Rational </guide/core_concepts/register_types>`
     - Fixed-point fraction
     - :math:`[0, 1)`
   * - :doc:`General </guide/core_concepts/register_types>`
     - Raw bit storage
     - Arbitrary bit patterns

.. code-block:: python

   # Correct: Boolean for single-qubit gates
   ps.System.add_register("qubit", ps.Boolean, 1)
   ps.X_Bool("qubit", 0)(state)

   # Wrong: type mismatch
   # ps.System.add_register("counter", ps.UnsignedInteger, 4)
   # ps.X_Bool("counter", 0)(state)  # raises an exception!

Bit Constraints
^^^^^^^^^^^^^^^

Many operators verify:

- Register sizes match the expected dimensions
- Bit indices are within the register range
- Output registers have sufficient capacity

.. _conditional-operations:

Conditional Operations
----------------------

All operators support conditional execution to implement controlled operations.

Condition Methods
^^^^^^^^^^^^^^^^^

.. list-table:: Condition methods
   :header-rows: 1

   * - Method
     - Condition
     - Example
   * - ``conditioned_by_nonzeros(reg)``
     - Register value ≠ 0
     - Controlled by any nonzero state
   * - ``conditioned_by_all_ones(reg)``
     - All bits are 1
     - Multi-qubit control
   * - ``conditioned_by_bit(reg, pos)``
     - The specified bit is 1
     - Single-qubit control
   * - ``conditioned_by_value(reg, val)``
     - The register equals a specific value
     - Classical control

.. code-block:: python

   op = ps.Add_UInt_UInt("a", "b", "result")

   # Apply when the control register is nonzero
   op.conditioned_by_nonzeros("control")(state)

   # Apply when bit 0 of flag is 1
   op.conditioned_by_bit("flag", 0)(state)

   # Apply when mode equals 1
   op.conditioned_by_value("mode", 1)(state)

   # Multiple conditions
   op.conditioned_by_nonzeros(["ctrl1", "ctrl2"])(state)

Clearing Control Conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   op.clear_control_nonzeros()
   op.clear_control_by_bit()
   op.clear_control_by_value()
   op.clear_control_all_ones()

Inspecting Control Variables
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Get the current control variables
   print(op.condition_variable_nonzeros)
   print(op.condition_variable_by_bit)  # list[tuple[int, int]]
   print(op.condition_variable_by_value)  # list[tuple[int, int]]

API Reference
-------------

.. autoclass:: pysparq.BaseOperator
   :members:
   :undoc-members:

.. autoclass:: pysparq.SelfAdjointOperator
   :members:
   :undoc-members:

Operator Categories in Detail
-----------------------------

.. toctree::
   :maxdepth: 2

   arithmetic
   gates
   hadamard
   qft
   condrot
   phase_ops
   rot_state_prep
   qram_ops
   system_ops
   partial_trace
   measurement
   sort_ops
   dark_magic
   debug
