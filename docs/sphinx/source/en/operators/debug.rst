Debugging Tools
===============

Debugging tools provide quantum-state inspection, normalization checking, NaN detection, state printing, and similar utilities. These tools do not modify the quantum state; they are intended for the development and debugging stages. The :class:`TestRemovable <pysparq.TestRemovable>` check documented here also guards :doc:`RemoveRegister </guide/core_concepts/register_management>` against removing entangled registers.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Debugging tools overview
   :header-rows: 1

   * - Operator
     - Operation
     - Modifies the state?
   * - ``StatePrint``
     - Print the quantum state
     - No
   * - ``CheckNormalization``
     - Check normalization
     - No
   * - ``CheckNan``
     - Detect NaN
     - No
   * - ``ViewNormalization``
     - Display the normalization value
     - No
   * - ``TestRemovable``
     - Test register removability
     - No
   * - ``CheckDuplicateKey``
     - Detect duplicate keys
     - No

---

State Printing
--------------

StatePrint (state printing)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autofunction:: pysparq.StatePrint
.. autofunction:: pysparq.pprint
.. autofunction:: pysparq.to_string
.. autoclass:: pysparq.StatePrinter
   :members:

**Operation**: Returns an information string for all basis states in the ``SparseState`` in the specified format.

**Parameters**:

- ``state`` — a SparseState instance
- ``mode`` — display mode (optional, ``Detail`` by default)
- ``precision`` — floating-point precision (optional)

**Display modes**:

.. list-table:: StatePrint display modes
   :header-rows: 1

   * - Mode
     - Description
   * - ``Default``
     - Standard format ``|reg=val⟩ : (α+βi)``
   * - ``Detail``
     - Detailed information, including register metadata
   * - ``Binary``
     - Display register values in binary
   * - ``Prob``
     - Display probabilities instead of amplitudes

.. code-block:: python

   import pysparq as ps

   # Print to stdout (Detail mode)
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)addr : UInt4 | |(1)data : UInt8 |
   # 0.250000+0.000000i  addr=|0> data=|0>
   # 0.250000+0.000000i  addr=|1> data=|2>
   # ...

   # Return a string (Detail mode)
   ps.StatePrint(state)
   # Returns the same Detail-format string

   # Probability format (returns a string)
   ps.StatePrint(state, mode=ps.StatePrintDisplay.Prob)
   # Output:
   # StatePrint (mode=Prob)
   # 0.250000+0.000000i (p = 0.0625) |0>|0>
   # ...

   # High precision
   ps.StatePrint(state, mode=ps.StatePrintDisplay.Default, precision=15)

   # Binary format
   ps.StatePrint(state, mode=ps.StatePrintDisplay.Binary)
   # Register values displayed in binary

---

Normalization Checking
----------------------

CheckNormalization (normalization check)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.CheckNormalization
   :members:
   :undoc-members:

**Operation**: Asserts that the normalization value :math:`\sum_i |\alpha_i|^2` of the ``SparseState`` is close to 1. Raises an exception if the deviation exceeds the threshold.

**Parameters**: ``threshold`` — allowed deviation threshold (optional, ``1e-6`` by default).

.. code-block:: python

   # Strict check
   ps.CheckNormalization(1e-10)(state)

   # Lenient check
   ps.CheckNormalization(1e-3)(state)

ViewNormalization (display the normalization value)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.ViewNormalization
   :members:
   :undoc-members:

**Operation**: Computes and prints the normalization value :math:`\sum_i |\alpha_i|^2` of the ``SparseState`` without raising an exception.

**Parameters**: None.

.. code-block:: python

   ps.ViewNormalization()(state)
   # Output: Norm = 0.9999999...

---

Data Integrity Checks
---------------------

CheckNan (NaN detection)
^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.CheckNan
   :members:
   :undoc-members:

**Operation**: Iterates over all basis states and checks whether any amplitude contains NaN values. Raises an exception if NaN is found.

**Parameters**: None.

**Purpose**: After extensive arithmetic, some amplitudes may become NaN due to numerical overflow or division by zero. This operator detects such problems early.

.. code-block:: python

   ps.CheckNan()(state)

TestRemovable (register removability test)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.TestRemovable
   :members:
   :undoc-members:

**Operation**: Tests whether the specified register can be safely removed from the ``SparseState`` (i.e. whether the register is entangled with the other registers).

**Parameters**: ``reg`` — target register (name or ID).

**Returns**: The result is reported through an internal assertion.

.. code-block:: python

   # Test whether "ancilla" can be safely removed
   ps.TestRemovable("ancilla")(state)

CheckDuplicateKey (duplicate key detection)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.CheckDuplicateKey
   :members:
   :undoc-members:

**Operation**: Checks whether the ``SparseState`` contains basis states with duplicate keys. Normally no duplicates should exist.

**Parameters**: None.

**Purpose**: If you suspect that a non-unitary operation introduced duplicate keys, use this operator to detect them.

.. code-block:: python

   ps.CheckDuplicateKey()(state)
