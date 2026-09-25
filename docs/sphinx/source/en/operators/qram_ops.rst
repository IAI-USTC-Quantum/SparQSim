QRAM Operators
==============

QRAM (Quantum Random Access Memory) operators implement quantum parallel data access.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: QRAM operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity class
   * - ``QRAMLoad``
     - Standard QRAM load
     - SelfAdjoint
   * - ``QRAMLoadFast``
     - Optimized QRAM load
     - SelfAdjoint

Quantum Parallel Data Access
----------------------------

QRAM implements quantum parallel memory access:

.. math::

   \sum_x \alpha_x |x\rangle |0\rangle \xrightarrow{QRAM} \sum_x \alpha_x |x\rangle |f(x)\rangle

This allows quantum algorithms to access multiple data items simultaneously.

---

QRAMLoad
--------

.. autoclass:: pysparq.QRAMLoad
   :members:
   :undoc-members:

**Operation**: Loads data from the QRAM circuit into a quantum register

**Type constraints**:
- Address register: ``UnsignedInteger``
- Data register: ``UnsignedInteger`` or ``General``

**Bit constraints**:
- The address size must match the QRAM configuration
- The data size must match the QRAM data width

.. code-block:: python

   import pysparq as ps

   ps.System.clear()

   # Create a QRAM circuit (qutrit version)
   addr_size = 4  # 4-bit address → 16 memory cells
   data_size = 8  # 8-bit data

   # Set the memory contents (memory must be passed in at construction)
   memory = [i * 10 for i in range(16)]  # 0, 10, 20, ..., 150
   qram = ps.QRAMCircuit_qutrit(addr_size, data_size, memory)

   # Create registers
   ps.System.add_register("addr", ps.UnsignedInteger, addr_size)
   ps.System.add_register("data", ps.UnsignedInteger, data_size)

   state = ps.SparseState()

   # Uniform superposition on the address register
   ps.Hadamard_Int_Full("addr")(state)

   # QRAM load: access all addresses in parallel
   ps.QRAMLoad(qram, "addr", "data")(state)

   ps.pprint(state)
   # The output contains 16 states:
   # |addr=0,data=0⟩, |addr=1,data=10⟩, ...

   # QRAMLoad is self-adjoint; applying it again undoes the load
   ps.QRAMLoad(qram, "addr", "data")(state)
   # The data register is zeroed

QRAMLoadFast
------------

.. autoclass:: pysparq.QRAMLoadFast
   :members:
   :undoc-members:

**Operation**: Optimized QRAM load

**Characteristics**: Optimized for specific memory patterns, with higher performance.

.. code-block:: python

   # Fast load (same interface)
   ps.QRAMLoadFast(qram, "addr", "data")(state)

---

QRAMCircuit_qutrit
------------------

PySparQ provides the ``QRAMCircuit_qutrit`` class for configuring a QRAM:

.. autoclass:: pysparq.QRAMCircuit_qutrit
   :members:
   :undoc-members:

.. note::

   ``QRAMCircuit_qubit`` (the qubit-version QRAM in the C++ API) is **not available** in PySparQ. PySparQ provides only the qutrit version.

Creating and Configuring a QRAM Circuit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   import pysparq as ps

   # Create a QRAM circuit
   addr_bits = 4   # number of address bits
   data_bits = 8   # number of data bits

   # Set the memory contents (memory must be passed in at construction)
   memory_list = [0, 10, 20, 30, 40, 50, 60, 70,
                  80, 90, 100, 110, 120, 130, 140, 150]
   qram = ps.QRAMCircuit_qutrit(addr_bits, data_bits, memory_list)

   # Note: the set_memory() method is not available in PySparQ
   # memory must be passed in when constructing QRAMCircuit_qutrit

   # Query information
   print(f"Address size: {qram.addr_size}")
   print(f"Data size: {qram.data_size}")

Conditional Loading
-------------------

QRAM loading supports conditional execution:

.. code-block:: python

   op = ps.QRAMLoad(qram, "addr", "data")

   # Load only when control is nonzero
   op.conditioned_by_nonzeros("control")(state)

   # Load only when bit 0 of flag is 1
   op.conditioned_by_bit("flag", 0)(state)

Use Cases
---------

Quantum Database Search
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Database contents
   database = [42, 17, 99, 5, ...]

   # Create the QRAM (memory is passed in at construction)
   qram = ps.QRAMCircuit_qutrit(addr_bits, data_bits, database)

   # Address register in superposition
   ps.Hadamard_Int_Full("addr")(state)

   # Load all data items in parallel
   ps.QRAMLoad(qram, "addr", "data")(state)

   # Now you can search for a specific value...

Quantum Machine Learning
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Load training data (memory is passed in at construction)
   qram = ps.QRAMCircuit_qutrit(addr_bits, data_bits, training_data)
   ps.Hadamard_Int_Full("sample_id")(state)
   ps.QRAMLoad(qram, "sample_id", "sample_data")(state)

   # All training samples can now be processed in parallel
