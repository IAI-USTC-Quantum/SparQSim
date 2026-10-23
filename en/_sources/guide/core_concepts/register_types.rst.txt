Register Types
==============

Every quantum register has a specific storage type, which determines how its value is interpreted and its valid range.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

The StateStorageType Enum
-------------------------

.. autoclass:: pysparq.StateStorageType
   :members:
   :undoc-members:

Type Details
------------

UnsignedInteger (unsigned integer)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Value range**: :math:`[0, 2^n - 1]`
- **Storage**: stores the binary value directly
- **Uses**: counters, addresses, array indices

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("addr", ps.UnsignedInteger, 4)  # addresses in the range 0-15

   # Typical usage
   state = ps.SparseState()
   ps.Init_Unsafe("addr", 5)(state)  # Initialize to 5

SignedInteger (signed integer)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Value range**: :math:`[-2^{n-1},\ 2^{n-1}-1]` (two's complement representation)
- **Storage**: two's complement encoding
- **Uses**: signed arithmetic

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("temp", ps.SignedInteger, 8)  # -128 to 127

   # Supports arithmetic operators such as Add_UInt_UInt

Boolean (boolean / single qubit)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Value range**: {0, 1}
- **Size**: must be exactly 1 bit
- **Uses**: flags, control bits, single-qubit gate operations
- **Display format**: when printing states it is shown as ``|true>`` / ``|false>`` instead of ``|1>`` / ``|0>``

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("flag", ps.Boolean, 1)  # Must be exactly 1 bit

   # Apply single-qubit gates
   state = ps.SparseState()
   ps.X_Bool("flag", 0)(state)  # Flip
   ps.Hadamard_Bool("flag")(state)   # Hadamard

Rational (rational / fixed-point)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Value range**: :math:`[0, 1)`
- **Storage**: fixed-point representation, interpreted as :math:`value / 2^n`
- **Uses**: angle encoding, conditional rotation

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("angle", ps.Rational, 16)  # High-precision angle

   # Used for conditional rotation
   ps.CondRot_Rational_Bool("angle", "target")(state)

The conditional-rotation operators built on ``Rational`` registers are described in :doc:`conditional rotation operators </operators/condrot>`.

General (general-purpose storage)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Value range**: any bit pattern
- **Uses**: raw bit storage, special purposes

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("raw", ps.General, 32)

The StateStorage Class
----------------------

Each register value is stored in a ``StateStorage`` object:

.. autoclass:: pysparq.StateStorage
   :members:
   :undoc-members:

Type Constraints in Operators
-----------------------------

Operators check whether register types match their requirements:

.. code-block:: python

   # Correct: a Boolean register is used for single-qubit gates
   ps.X_Bool("flag", 0)(state)

   # Wrong: UnsignedInteger cannot be used with a Boolean operator
   # ps.X_Bool("counter", 0)(state)  # Type mismatch!

Each :ref:`operator <operator-reference>` documents the register types it accepts; the type-constraints rationale is summarized in the operator reference overview.

Type Selection Guide
--------------------

.. list-table:: Type selection guide
   :header-rows: 1

   * - Scenario
     - Recommended type
     - Example
   * - Address / index
     - ``UnsignedInteger``
     - QRAM address
   * - Counter
     - ``UnsignedInteger``
     - Loop variable
   * - Control flag
     - ``Boolean``
     - Comparison result
   * - Angle parameter
     - ``Rational``
     - Rotation angle
   * - Temperature / physical quantity
     - ``SignedInteger``
     - Signed measurement
