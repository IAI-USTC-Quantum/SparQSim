System Operations
=================

System operations manage register lifecycles, splitting and merging of basis states, and cleanup of zero amplitudes. Although these operations do not correspond directly to quantum gates, they are indispensable in algorithm workflows; the underlying concepts are introduced in :doc:`register management </guide/core_concepts/register_management>`.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: System operations overview
   :header-rows: 1

   * - Operator / Function
     - Operation
     - Type
   * - ``Push``
     - Push a register onto the stack, saving its current value
     - BaseOperator
   * - ``Pop``
     - Restore a register's value from the stack
     - SelfAdjointOperator
   * - ``ClearZero``
     - Remove basis states whose amplitude is close to zero
     - SelfAdjointOperator
   * - ``split_systems``
     - Split the set of basis states by condition
     - Free function
   * - ``combine_systems``
     - Merge sets of basis states
     - Free function

---

Register Stack Management
-------------------------

Push (push onto the stack)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Push
   :members:
   :undoc-members:

**Operation**: Saves the register's current value to an internal stack and resets the register to ``|0⟩``.

**Parameters**:

- ``reg`` — target register (name or ID)
- ``garbage_name`` — temporary name used when saving it on the stack (optional)

**Purpose**: When a register needs to be used temporarily as an ancilla, first save its value, then restore it with ``Pop`` once it is no longer needed.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("data", ps.UnsignedInteger, 4)
   ps.System.add_register("temp", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("data", 7)(state)
   ps.Init_Unsafe("temp", 3)(state)

   # Save the current value of temp and reset it to 0
   ps.Push("temp")(state)

   # temp can now be used freely as an ancilla register
   ps.Add_UInt_UInt("data", "temp", "temp")(state)

Pop (pop from the stack)
^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Pop
   :members:
   :undoc-members:

**Operation**: Restores the register's value from the stack.

**Parameters**: ``reg`` — the register to restore (name or ID)

.. warning::

   ``Pop`` requires that the value saved on the stack is not entangled with the other registers; otherwise the behavior is undefined. It should normally be used in pairs with ``Push``.

.. code-block:: python

   # Restore temp to its value from before the Push
   ps.Pop("temp")(state)

The Push/Pop Pattern
""""""""""""""""""""

A typical workflow for ``Push`` / ``Pop``:

.. code-block:: python

   # 1. Save the ancilla register
   ps.Push("ancilla")(state)

   # 2. Compute with the ancilla register
   ps.Add_UInt_UInt("a", "ancilla", "ancilla")(state)
   # ... more operations ...
   ps.Add_UInt_UInt("a", "ancilla", "ancilla")(state)  # undo (SelfAdjoint)

   # 3. Restore the ancilla register
   ps.Pop("ancilla")(state)

---

Basis-State Cleanup
-------------------

ClearZero (clear near-zero amplitudes)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.ClearZero
   :members:
   :undoc-members:

**Operation**: Iterates over all basis states and removes the entries whose amplitude magnitude is below the threshold.

**Parameters**: ``eps`` (optional) — truncation threshold, ``1e-12`` by default.

**Purpose**: After many computations, the amplitudes of some basis states may become extremely small but nonzero due to floating-point error. ``ClearZero`` removes this numerical noise and keeps the sparse state compact.

.. code-block:: python

   ps.ClearZero()(state)       # use the default threshold
   ps.ClearZero(1e-8)(state)   # custom threshold

---

Splitting and Merging Basis States
----------------------------------

split_systems (split basis states)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autofunction:: pysparq.split_systems

**Operation**: Splits the basis states of a ``SparseState`` into two groups — those that satisfy and those that do not satisfy the given conditioned_by condition.

**Parameters**:

- ``state`` — the source ``SparseState``
- ``nonzeros`` / ``all_ones`` / ``by_bit`` / ``by_value`` — condition parameters

**Returns**: ``list[System]`` — the list of basis states that satisfy the condition.

.. code-block:: python

   # Split out the basis states where the ctrl register is nonzero
   matching = ps.split_systems(state, nonzeros=["ctrl"])

combine_systems (merge basis states)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autofunction:: pysparq.combine_systems

**Operation**: Merges the split-out basis states back into the ``SparseState``.

**Parameters**:

- ``to`` — the target ``SparseState``
- ``from`` — the list of basis states to merge

.. code-block:: python

   # Merge back into the main state
   ps.combine_systems(state, matching)

The Split-Transform-Merge Pattern
"""""""""""""""""""""""""""""""""

``split_systems`` / ``combine_systems`` can be used to implement manual conditional operations:

.. code-block:: python

   # 1. Split
   matching = ps.split_systems(state, nonzeros=["ctrl"])

   # 2. Apply operations to the matching basis states
   # ...

   # 3. Merge
   ps.combine_systems(state, matching)
