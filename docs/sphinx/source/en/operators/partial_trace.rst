Partial Trace
=============

The partial trace operation performs a measurement or a selective collapse on the specified register(s). It is the key means of extracting classical information in quantum algorithms.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Partial trace operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Return value
   * - ``PartialTrace``
     - Randomly measure the specified register(s)
     - ``(measured_values, probability)``
   * - ``PartialTraceSelect``
     - Collapse to specified values
     - ``probability``
   * - ``PartialTraceSelectRange``
     - Collapse to within a specified range
     - ``probability``

Physically, a partial trace "measures away" a register from the quantum state — after the measurement that register collapses to a definite value, while the remaining registers enter the corresponding conditional state according to the measurement outcome.

---

PartialTrace (random measurement)
---------------------------------

.. autoclass:: pysparq.PartialTrace
   :members:
   :undoc-members:

**Operation**: Performs a random measurement on the specified register(s) (sampling from the probability distribution), returning the measurement outcome and the corresponding probability.

**Parameters**: Register identifier(s) (a name string, a list of names, an ID, or a list of IDs).

**Returns**: ``(measured_values, probability)``

- ``measured_values`` — the list of register values obtained from the measurement
- ``probability`` — the probability of this measurement outcome

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("addr", ps.UnsignedInteger, 3)
   ps.System.add_register("data", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Hadamard_Int("addr", 3)(state)

   # Randomly measure the addr register
   values, prob = ps.PartialTrace("addr")(state)
   print(f"Measurement result: {values}, probability: {prob:.4f}")
   # Example: Measurement result: [3], probability: 0.1250

   # After the measurement addr has collapsed; only the corresponding basis state remains in state

When measuring multiple registers, the returned value list is ordered according to the parameter order:

.. code-block:: python

   values, prob = ps.PartialTrace(["addr", "data"])(state)

---

PartialTraceSelect (selective collapse)
---------------------------------------

.. autoclass:: pysparq.PartialTraceSelect
   :members:
   :undoc-members:

**Operation**: Collapses the specified register(s) to the given values instead of measuring randomly.

**Parameters**: A register-to-value mapping (dictionary or lists).

**Returns**: ``probability`` — the probability of this collapse outcome.

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 2)
   ps.System.add_register("b", ps.UnsignedInteger, 2)

   state = ps.SparseState()
   ps.Hadamard_Int("a", 2)(state)

   # Collapse a to the value 1
   prob = ps.PartialTraceSelect({"a": 1})(state)
   print(f"Collapse probability: {prob:.4f}")
   # Collapse probability: 0.2500

   # A list form also works
   prob = ps.PartialTraceSelect(["a"], [1])(state)

---

PartialTraceSelectRange (range collapse)
----------------------------------------

.. autoclass:: pysparq.PartialTraceSelectRange
   :members:
   :undoc-members:

**Operation**: Collapses the specified register to within a given value range, keeping the basis states that satisfy the condition and renormalizing.

**Parameters**: Register identifier and value range ``(min, max)``.

**Returns**: ``probability`` — the total probability within the range.

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("x", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Hadamard_Int("x", 4)(state)

   # Keep only the basis states with x ∈ [2, 5]
   prob = ps.PartialTraceSelectRange("x", (2, 5))(state)
   print(f"Probability within range: {prob:.4f}")
   # Probability within range: 0.2500 (4 out of 16 basis states)
