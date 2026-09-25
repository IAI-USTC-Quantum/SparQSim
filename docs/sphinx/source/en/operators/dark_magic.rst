Dark Magic Operations
=====================

.. raw:: html

   <div class="admonition warning">
   <p class="admonition-title">Warning</p>
   <p>The operations in this section bypass the normal constraints of quantum mechanics and are <strong>not guaranteed to be unitary</strong>. Use them only for debugging, initialization, or when you clearly understand the consequences.</p>
   </div>

Dark magic operations provide the ability to directly modify the internal representation of a quantum state without going through normal quantum gate transformations. These operations do not correspond to any physical quantum operations.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Dark magic operations overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity
   * - ``Normalize``
     - Normalize the quantum state
     - Non-unitary
   * - ``Init_Unsafe``
     - Set a register value directly
     - Non-unitary

---

Normalize
------------------

.. autoclass:: pysparq.Normalize
   :members:
   :undoc-members:

**Operation**: Normalizes the amplitudes of the entire ``SparseState`` so that :math:`\sum_i |\alpha_i|^2 = 1`.

**Parameters**: None.

**Purpose**: Use this operator to repair a quantum state that has been left unnormalized by manual manipulation (e.g. ``Init_Unsafe``) or by numerical error.

.. code-block:: python

   import pysparq as ps

   # Suppose some operations have left the quantum state unnormalized
   ps.Normalize()(state)

   # You can check it with ViewNormalization
   ps.ViewNormalization()(state)

---

Init_Unsafe (unsafe initialization)
-----------------------------------

.. autoclass:: pysparq.Init_Unsafe
   :members:
   :undoc-members:

**Operation**: Directly sets the value of the specified register to a given constant, **without going through any quantum gate**.

**Parameters**:

- ``reg`` — target register (name or ID)
- ``value`` — the value to set (integer)

**Behavior**: Iterates over all basis states and forcibly sets the specified register's value to ``value``. If multiple basis states end up with duplicate keys after the assignment, their amplitudes are summed.

.. warning::

   This operation is **not guaranteed to be unitary**. It destroys superposition information — if the register was in a superposition, all components other than ``value`` are overwritten.

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Set the initial values directly
   ps.Init_Unsafe("a", 3)(state)
   ps.Init_Unsafe("b", 5)(state)

   ps.pprint(state)
   # |a=3,b=5⟩ : (1+0j)

.. note::

   The most common use of ``Init_Unsafe`` is to set the initial values of the input registers at the start of an algorithm. Be especially careful when using it on a superposed state.
