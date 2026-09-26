The SparseState Class
=====================

The ``SparseState`` class is the core data structure of PySparQ; it represents a sparse quantum state. It internally manages a ``std::vector<System>`` (each element is a :doc:`System </guide/core_concepts/system>`) and stores only basis states with non-zero amplitudes.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Sparse Representation Principles
--------------------------------

A traditional full state-vector simulator stores :math:`2^n` amplitudes (:math:`n` being the number of qubits), regardless of whether most of them are zero. PySparQ uses a sparse representation:

- Only basis states with ``amplitude ≠ 0`` are stored
- Each ``System`` represents one computational basis state
- When the number of superposed states is finite, the storage complexity is polynomial

Uniqueness Rule
---------------

A core invariant of ``SparseState`` is: **the register-value combination of every ``System`` must be unique**.

If an operation on the basis states of a ``SparseState`` produces two ``System`` objects with identical register values, this means quantum interference has occurred — in that case the amplitudes of the two ``System`` objects should be added and they should be merged into a single ``System``. This process is usually performed automatically by the ``sort-merge-unique`` mechanism inside :doc:`operators </guide/core_concepts/operators>`.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   # SparseState() default construction: creates a single |q=0⟩ basis state
   state = ps.SparseState()
   print(f"basis-state count: {state.size()}")  # 1

   # Apply Hadamard to create 2^2 = 4 basis states
   ps.Hadamard_Int_Full("q")(state)
   print(f"basis-state count: {state.size()}")  # 4

Basic Usage
-----------

Creating a SparseState
^^^^^^^^^^^^^^^^^^^^^^

.. important::

   The default constructor of ``SparseState()`` automatically creates a ``|0...0⟩`` initial state (i.e. a single ``System`` whose register values are all 0 and whose amplitude is 1). You usually do **not** need to manually construct ``System`` objects to create a ``SparseState``.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 10)

   # Default construction: creates a single |q=0⟩ basis state (amplitude 1)
   state = ps.SparseState()

Accessing Basis States
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Get the list of basis states
   for system in state.basis_states:
       print(f"amplitude: {system.amplitude}")

   # Access by index
   first = state[0]
   last = state.basis_states[-1]

   # Get the number of basis states
   n = state.size()

   # Check whether the state is empty
   if state.empty():
       print("state is empty")

State Evolution Example
-----------------------

The following example shows how a ``SparseState`` evolves under :ref:`operator actions <operator-reference>`:

.. code-block:: python
   :caption: Example: Hadamard creates a superposition

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)

   # SparseState creates the |q=0⟩ initial state by default
   state = ps.SparseState()

   print("initial state:")
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 1.000000+0.000000i  q=|0>

   # Apply Hadamard (partial superposition: apply H to bit 0)
   ps.Hadamard_Int("q", 1)(state)

   print("\nafter Hadamard:")
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 0.707107+0.000000i  q=|0>
   # 0.707107+0.000000i  q=|2>

   # Full Hadamard (all output states)
   ps.Hadamard_Int_Full("q")(state)

   print("\nafter full Hadamard:")
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt2 |
   # 0.500000+0.000000i  q=|0>
   # 0.500000+0.000000i  q=|1>
   # 0.500000+0.000000i  q=|2>
   # 0.500000+0.000000i  q=|3>

State Printing Modes
--------------------

``ps.StatePrint(state, mode)`` and ``ps.pprint(state, mode)`` (see :doc:`debugging tools </operators/debug>`) support several display modes:

.. list-table:: The StatePrintDisplay enum
   :header-rows: 1

   * - Mode
     - Value
     - Description
     - Example output
   * - ``Default``
     - 0
     - Default mode, decimal values
     - ``0.5+0.000000i |5>``
   * - ``Detail``
     - 1
     - Detailed mode, with register header and amplitudes
     - ``0.500000+0.000000i  q=|5>``
   * - ``Binary``
     - 2
     - Binary representation
     - ``0.5+0.000000i |0101>``
   * - ``Prob``
     - 4
     - Probability view
     - ``0.5+0.000000i (p = 0.25) |5>``

.. code-block:: python

   # Different display modes
   ps.pprint(state)                                                    # Detail (default)
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default))      # Default
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Binary))       # Binary
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Prob))         # Prob

   # Specify the precision
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default, precision=15))
   print(ps.StatePrint(state, mode=ps.StatePrintDisplay.Default, precision=4))

Clearing Near-Zero Amplitudes
-----------------------------

Use :class:`ClearZero <pysparq.ClearZero>` from :doc:`system operations </operators/system_ops>` and :class:`Normalize <pysparq.Normalize>` from :doc:`dark magic operations </operators/dark_magic>`:

.. code-block:: python

   # Remove basis states with |amplitude|² < epsilon
   ps.ClearZero(epsilon=1e-10)(state)

   # Normalize the state
   ps.Normalize()(state)

Merging Duplicate Basis States
------------------------------

When multiple basis states have identical register values, their amplitudes are merged automatically:

.. code-block:: python

   # The merge_system function: merges the amplitudes of two identical basis states
   # Usually called automatically inside operators

Iterator Support
----------------

``SparseState`` supports the standard Python iterator protocol:

.. code-block:: python

   # Forward iteration
   for system in state.basis_states:
       print(system.amplitude)

   # Reverse iteration
   for system in reversed(state.basis_states):
       print(system.amplitude)

   # Index access
   first = state.basis_states[0]
   last = state.basis_states[-1]

   # Slicing
   first_three = state.basis_states[:3]

API Reference
-------------

.. autoclass:: pysparq.SparseState
   :members:
   :undoc-members:

.. autofunction:: pysparq.merge_system

.. autofunction:: pysparq.remove_system

The ``split_systems`` and ``combine_systems`` helpers are documented under :doc:`system operations </operators/system_ops>`.
