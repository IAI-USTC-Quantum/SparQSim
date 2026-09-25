Sorting Operators
=================

Sorting operators sort the basis states in a ``SparseState`` along different dimensions. Sorting itself does not change the quantum state (amplitudes are unchanged), but it affects the display order of tools such as ``StatePrint`` and is used for correct matching in internal conditional operations.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Sorting operators overview
   :header-rows: 1

   * - Operator
     - Sort key
   * - ``SortByKey``
     - Sort by the value of a specified register
   * - ``SortByKey2``
     - Joint sort by the values of two registers
   * - ``SortExceptKey``
     - Sort by all registers except the specified one
   * - ``SortExceptBit``
     - Sort by all bits except the specified bit
   * - ``SortExceptKeyHadamard``
     - Sort by the register after the Hadamard transform
   * - ``SortUnconditional``
     - Unconditional sort (by the full basis-state key)
   * - ``SortByAmplitude``
     - Sort by amplitude magnitude

---

SortByKey (sort by key)
-----------------------

.. autoclass:: pysparq.SortByKey
   :members:
   :undoc-members:

**Operation**: Sorts the basis states in ascending order by the value of the specified register.

**Parameters**: ``key`` — the register to sort by (name or ID).

.. code-block:: python

   import pysparq as ps

   ps.SortByKey("addr")(state)

SortByKey2 (two-key sort)
-------------------------

.. autoclass:: pysparq.SortByKey2
   :members:
   :undoc-members:

**Operation**: Jointly sorts by the values of two registers (first by ``key1``, then by ``key2``).

**Parameters**: ``key1``, ``key2`` — the two sort-key registers.

.. code-block:: python

   ps.SortByKey2("addr", "data")(state)

SortExceptKey (sort excluding a key)
------------------------------------

.. autoclass:: pysparq.SortExceptKey
   :members:
   :undoc-members:

**Operation**: Sorts by the values of all registers except the specified one.

**Parameters**: ``key`` — the register to exclude.

**Purpose**: Use this when the value of a certain register should be ignored during sorting; it is often needed for correct matching in internal conditional operations.

.. code-block:: python

   ps.SortExceptKey("temp")(state)

SortExceptBit (sort excluding a bit)
------------------------------------

.. autoclass:: pysparq.SortExceptBit
   :members:
   :undoc-members:

**Operation**: Sorts by all bits except a specified bit of a specified register.

**Parameters**: ``key`` — the register, ``digit`` — the index of the bit to exclude.

.. code-block:: python

   ps.SortExceptBit("q", 0)(state)

SortExceptKeyHadamard (sort excluding a Hadamard key)
-----------------------------------------------------

.. autoclass:: pysparq.SortExceptKeyHadamard
   :members:
   :undoc-members:

**Operation**: Sorts by excluding the specified qubits after a Hadamard transform.

**Parameters**: ``key`` — the register, ``qubit_ids`` — the set of qubit indices to exclude.

.. code-block:: python

   ps.SortExceptKeyHadamard("q", {0, 2})(state)

SortUnconditional (unconditional sort)
--------------------------------------

.. autoclass:: pysparq.SortUnconditional
   :members:
   :undoc-members:

**Operation**: Unconditionally sorts by the full basis-state key. No parameters are required.

.. code-block:: python

   ps.SortUnconditional()(state)

SortByAmplitude (sort by amplitude)
-----------------------------------

.. autoclass:: pysparq.SortByAmplitude
   :members:
   :undoc-members:

**Operation**: Sorts by the magnitude of the basis-state amplitudes. No parameters are required.

**Purpose**: Quickly see which basis states contribute the most when debugging.

.. code-block:: python

   ps.SortByAmplitude()(state)
