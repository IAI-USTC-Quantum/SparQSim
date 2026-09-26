Arithmetic Operators
====================

Arithmetic operators implement quantum integer arithmetic, including addition, multiplication, shifting, comparison, and other operations. See :doc:`register types </guide/core_concepts/register_types>` for the accepted register types and :doc:`Examples </guide/examples>` for end-to-end usage.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Arithmetic operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Type
     - Unitarity class
   * - ``Add_UInt_UInt``
     - ``result ^= lhs + rhs``
     - Out-of-place
     - SelfAdjoint
   * - ``Add_UInt_UInt_InPlace``
     - ``rhs += lhs``
     - In-place
     - BaseOperator
   * - ``Add_UInt_ConstUInt``
     - ``result ^= lhs + const``
     - Out-of-place
     - SelfAdjoint
   * - ``Add_ConstUInt_InPlace``
     - ``reg += const``
     - In-place
     - BaseOperator
   * - ``Mult_UInt_ConstUInt``
     - ``result ^= input * const``
     - Out-of-place
     - SelfAdjoint
   * - ``Add_Mult_UInt_ConstUInt_InPlace``
     - ``res += lhs * const``
     - In-place
     - BaseOperator
   * - ``Mod_Mult_UInt_ConstUInt_InPlace``
     - ``y = y * a^(2^x) mod N``
     - In-place
     - BaseOperator
   * - ``ShiftLeft_InPlace``
     - Circular left shift
     - In-place
     - BaseOperator
   * - ``ShiftRight_InPlace``
     - Circular right shift
     - In-place
     - BaseOperator
   * - ``Compare_UInt_UInt``
     - Comparison flags
     - Out-of-place
     - SelfAdjoint
   * - ``Less_UInt_UInt``
     - Less-than flag
     - Out-of-place
     - SelfAdjoint
   * - ``Assign``
     - ``dst ^= src``
     - Out-of-place
     - SelfAdjoint
   * - ``FlipBools``
     - Bitwise NOT
     - In-place
     - SelfAdjoint
   * - ``Swap_General_General``
     - Swap two registers
     - In-place
     - SelfAdjoint
   * - ``GetMid_UInt_UInt``
     - Compute the midpoint
     - Out-of-place
     - SelfAdjoint

---

Addition Operators
------------------

Add_UInt_UInt (out-of-place addition)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Add_UInt_UInt
   :members:
   :undoc-members:

**Operation**: ``result ^= lhs + rhs``

**Unitarity guarantee**: XOR mechanism — applying twice restores the original value.

**Type constraints**: All registers must be ``UnsignedInteger``.

**Bit constraints**: No special requirements; the result is truncated to the output register size.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()

   ps.System.add_register("lhs", ps.UnsignedInteger, 4)
   ps.System.add_register("rhs", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("lhs", 3)(state)
   ps.Init_Unsafe("rhs", 5)(state)

   # result = 0 ^ (3 + 5) = 8
   ps.Add_UInt_UInt("lhs", "rhs", "result")(state)

   ps.pprint(state)
   # Output: |lhs=3,rhs=5,result=8⟩ : (1+0j)

   # Apply again = undo
   ps.Add_UInt_UInt("lhs", "rhs", "result")(state)
   # result = 8 ^ 8 = 0

Add_UInt_UInt_InPlace (in-place addition)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Add_UInt_UInt_InPlace
   :members:
   :undoc-members:

**Operation**: ``rhs = (rhs + lhs) mod 2^n``

**Dagger implementation**: ``rhs = (rhs + 2^n - lhs) mod 2^n``

**Type constraints**: Both registers must be ``UnsignedInteger``.

**Bit constraints**: Registers of the same size are recommended; otherwise the smaller value is truncated.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("lhs", ps.UnsignedInteger, 4)
   ps.System.add_register("rhs", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("lhs", 7)(state)
   ps.Init_Unsafe("rhs", 10)(state)

   # rhs = (10 + 7) % 16 = 1 (overflow wraps around)
   op = ps.Add_UInt_UInt_InPlace("lhs", "rhs")
   op(state)

   # Undo: rhs = (1 + 16 - 7) % 16 = 10
   op.dag(state)

Add_UInt_ConstUInt (constant out-of-place addition)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Add_UInt_ConstUInt
   :members:
   :undoc-members:

**Operation**: ``result ^= lhs + const``

**Type constraints**: All registers must be ``UnsignedInteger``.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("lhs", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("lhs", 3)(state)

   # result = 0 ^ (3 + 5) = 8
   ps.Add_UInt_ConstUInt("lhs", 5, "result")(state)

Add_ConstUInt_InPlace (constant in-place addition)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Add_ConstUInt_InPlace
   :members:
   :undoc-members:

**Operation**: ``reg = (reg + const) mod 2^n``

**Dagger implementation**: ``reg = (reg + 2^n - const) mod 2^n``

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("counter", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("counter", 10)(state)

   # counter = (10 + 7) % 16 = 1
   op = ps.Add_ConstUInt_InPlace("counter", 7)
   op(state)

   # Undo
   op.dag(state)  # counter = 10

---

Multiplication Operators
------------------------

Mult_UInt_ConstUInt (constant out-of-place multiplication)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Mult_UInt_ConstUInt
   :members:
   :undoc-members:

**Operation**: ``result ^= input * const``

**Unitarity guarantee**: XOR mechanism.

.. warning::

   **The multiplier should be odd** in order to guarantee bijectivity. An even multiplier loses the information of the lowest bit.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("input", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("input", 3)(state)

   # Good: odd multiplier
   ps.Mult_UInt_ConstUInt("input", 3, "result")(state)
   # result = 0 ^ (3 * 3) = 9

   # Avoid: even multipliers are not bijective
   # ps.Mult_UInt_ConstUInt("input", 2, "result")(state)  # loses the LSB

Add_Mult_UInt_ConstUInt_InPlace (multiply-accumulate)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Add_Mult_UInt_ConstUInt_InPlace
   :members:
   :undoc-members:

**Operation**: ``result += input * const``

**Dagger implementation**: ``result += (2^n - input * const) mod 2^n``

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("input", ps.UnsignedInteger, 4)
   ps.System.add_register("result", ps.UnsignedInteger, 8)  # larger to avoid overflow

   state = ps.SparseState()
   ps.Init_Unsafe("input", 3)(state)
   ps.Init_Unsafe("result", 5)(state)

   # result = 5 + (3 * 4) = 17
   op = ps.Add_Mult_UInt_ConstUInt_InPlace("input", 4, "result")
   op(state)

   # Undo
   op.dag(state)

---

Modular Multiplication Operator
-------------------------------

Mod_Mult_UInt_ConstUInt_InPlace (modular multiplication operator)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Mod_Mult_UInt_ConstUInt_InPlace
   :members:
   :undoc-members:

**Operation**: ``y → y * a^(2^x) mod N`` (in-place modular multiplication)

**Dagger**: ``y → y * a^(-2^x) mod N`` (the modular inverse is computed with the extended Euclidean algorithm)

**Type constraints**: ``UnsignedInteger``; the register size must be ≥ ⌈log₂(N)⌉.

**Condition**: ``a`` and ``N`` must be coprime (gcd(a, N) = 1); otherwise an exception is raised at construction.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   reg = ps.System.add_register("y", ps.UnsignedInteger, 4)
   state = ps.SparseState()
   ps.Init_Unsafe("y", 3)(state)

   # y = 3 * 7 mod 15 = 6
   op = ps.Mod_Mult_UInt_ConstUInt_InPlace("y", 7, 0, 15)
   op(state)

   # Undo: y = 6 * 13 mod 15 = 3
   op.dag(state)

---

Shift Operators
---------------

ShiftLeft_InPlace (circular left shift)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.ShiftLeft_InPlace
   :members:
   :undoc-members:

**Operation**: Circularly shifts left by ``digit`` bits

**Dagger**: ``.dag()`` is implemented and has the same effect as ``ShiftRight_InPlace(reg, digit)``; the two are daggers of each other.

**Type constraints**: ``UnsignedInteger`` or ``SignedInteger``.

**Bit constraints**: ``digit <= register size``.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("reg", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("reg", 0b1010)(state)  # 10

   ps.ShiftLeft_InPlace("reg", 1)(state)
   # reg = 0b0101 = 5

   # Undo
   op.dag(state)
   # reg = 0b1010 = 10

ShiftRight_InPlace (circular right shift)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.ShiftRight_InPlace
   :members:
   :undoc-members:

**Operation**: Circularly shifts right by ``digit`` bits

**Dagger**: ``ShiftLeft_InPlace(reg, digit)``

.. code-block:: python

   ps.Init_Unsafe("reg", 0b1010)(state)  # 10

   ps.ShiftRight_InPlace("reg", 1)(state)
   # reg = 0b0101 = 5

   # ShiftLeft_InPlace and ShiftRight_InPlace are inverses of each other
   ps.ShiftLeft_InPlace("reg", 1)(state)
   # reg = 0b1010 = 10

---

Comparison Operators
--------------------

Compare_UInt_UInt (comparison)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Compare_UInt_UInt
   :members:
   :undoc-members:

**Operation**: Sets ``less_flag`` and ``equal_flag`` based on ``lhs < rhs`` and ``lhs == rhs``.

**Type constraints**: Inputs ``UnsignedInteger``, outputs ``Boolean``.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("lhs", ps.UnsignedInteger, 4)
   ps.System.add_register("rhs", ps.UnsignedInteger, 4)
   ps.System.add_register("less", ps.Boolean, 1)
   ps.System.add_register("equal", ps.Boolean, 1)

   state = ps.SparseState()
   ps.Init_Unsafe("lhs", 3)(state)
   ps.Init_Unsafe("rhs", 5)(state)

   ps.Compare_UInt_UInt("lhs", "rhs", "less", "equal")(state)
   # less = 1 (3 < 5), equal = 0

Less_UInt_UInt (less-than comparison)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Less_UInt_UInt
   :members:
   :undoc-members:

**Operation**: Sets only ``less_flag``.

---

Other Operators
---------------

Assign (assignment)
^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Assign
   :members:
   :undoc-members:

**Operation**: ``dst ^= src``

**Unitarity guarantee**: XOR mechanism.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("src", ps.UnsignedInteger, 4)
   ps.System.add_register("dst", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("src", 5)(state)

   # dst = 0 ^ 5 = 5
   ps.Assign("src", "dst")(state)

FlipBools (bitwise NOT)
^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.FlipBools
   :members:
   :undoc-members:

**Operation**: ``reg = ~reg``

**Type constraints**: Any integer type.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("reg", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("reg", 0b1010)(state)  # 10

   ps.FlipBools("reg")(state)
   # reg = 0b0101 = 5

Swap_General_General (swap)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.Swap_General_General
   :members:
   :undoc-members:

**Operation**: Swaps the values of two registers.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("a", 3)(state)
   ps.Init_Unsafe("b", 5)(state)

   ps.Swap_General_General("a", "b")(state)
   # a = 5, b = 3

GetMid_UInt_UInt (midpoint computation)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: pysparq.GetMid_UInt_UInt
   :members:
   :undoc-members:

**Operation**: ``mid ^= (left + right) // 2``

**Purpose**: Binary search algorithms.

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("left", ps.UnsignedInteger, 4)
   ps.System.add_register("right", ps.UnsignedInteger, 4)
   ps.System.add_register("mid", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("left", 2)(state)
   ps.Init_Unsafe("right", 8)(state)

   ps.GetMid_UInt_UInt("left", "right", "mid")(state)
   # mid = (2 + 8) // 2 = 5
