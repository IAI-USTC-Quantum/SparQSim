The System Class
================

The ``System`` class is the basic unit of the sparse state simulator; it represents a single computational basis state. Each ``System`` stores a complex amplitude ``amplitude`` and the values of all registers ``registers``.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Core Concepts
-------------

In PySparQ, quantum states use a sparse representation: only basis states with non-zero amplitudes are stored. Each basis state is represented by a ``System`` object:

.. math::

   |\psi\rangle = \sum_{i \in \text{non-zero}} \alpha_i |i\rangle

where each :math:`|i\rangle` corresponds to one ``System`` instance.

A ``System`` contains two core pieces of data:

- **amplitude** (``complex``): the complex amplitude of this basis state
- **registers** (``list[StateStorage]``): the values of all registers, indexed by register ID. Each register value is stored as a ``uint64_t`` — the storage cell is described in :doc:`register types </guide/core_concepts/register_types>`

.. important::

   ``System`` is not an object used standalone. It is always managed by a :class:`SparseState <pysparq.SparseState>`. ``SparseState`` guarantees that the register-value combinations of all its ``System`` objects are unique — if two ``System`` objects have identical register values, quantum interference has occurred, and their amplitudes should be added and merged into a single ``System``.

Static Variables: Global Register Tracking
------------------------------------------

The ``System`` class uses static (class-level) variables to track the metadata of all registers. This means:

- All ``System`` instances share the same set of register metadata
- Register IDs are allocated globally
- Static state is not cleared automatically when the program ends

.. warning::

   **Important**: Before every new program run, you must call :meth:`System.clear() <pysparq.System.clear>` to clean up the static state; otherwise, information from the previous run will remain!

Static Members
^^^^^^^^^^^^^^

.. list-table:: System static variables
   :header-rows: 1

   * - Variable name
     - Type
     - Description
   * - ``name_register_map``
     - ``list``
     - Register metadata list; each element is a ``(name, type, size, status)`` tuple
   * - ``max_register_count``
     - ``int``
     - Historical maximum number of registers (for statistics)
   * - ``max_system_size``
     - ``int``
     - Historical maximum number of basis states (for statistics)
   * - ``reusable_registers``
     - ``list``
     - List of reusable register IDs
   * - ``temporal_registers``
     - ``list``
     - Stack of temporary registers

Instance Members
----------------

.. list-table:: System instance members
   :header-rows: 1

   * - Member
     - Type
     - Description
   * - ``amplitude``
     - ``complex``
     - Complex amplitude of this basis state
   * - ``registers``
     - ``list[StateStorage]``
     - Values of all registers (indexed by register ID), stored internally as ``uint64_t``

Register Storage Principles
---------------------------

The registers in the system provide storage for ``n`` ``uint64_t`` values. This allows us to encode quantum states in a multi-register form such as :math:`|a\rangle|b\rangle|c\rangle` without having to manage how individual qubits are encoded.

For example, a QRAM access :math:`|i\rangle|0\rangle \to |i\rangle|d[i]\rangle` can be encoded easily — only an address register and a data register are needed (see :doc:`QRAM operators </operators/qram_ops>`). The management level of the whole project rises from qubits to quantum registers, and almost all operations take quantum registers as their unit.

Query Methods
-------------

Getting Register Information
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # Get the register ID
   reg_id = ps.System.get_id("counter")

   # Get the register size (number of bits)
   size = ps.System.size_of("counter")  # or ps.System.size_of(reg_id)

   # Get the register type
   type_ = ps.System.type_of("counter")  # Returns a StateStorageType enum

   # Get the register activation status
   active = ps.System.status_of("counter")  # True means activated

   # Get the full metadata
   info = ps.System.get_register_info("counter")
   # Returns an (name, type, size, status) tuple

   # Get the name from the ID
   name = ps.System.name_of(reg_id)

The returned :class:`StateStorageType <pysparq.StateStorageType>` enum and the value interpretation of each storage type are described in :doc:`register types </guide/core_concepts/register_types>`.

Statistics
^^^^^^^^^^

.. code-block:: python

   # Get the total number of qubits
   n_qubits = ps.System.get_qubit_count()

   # Get the number of activated registers
   n_regs = ps.System.get_activated_register_size()

Accessing Register Values in Basis States
-----------------------------------------

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("x", ps.UnsignedInteger, 4)
   state = ps.SparseState()

   # Iterate over all basis states
   for system in state.basis_states:
       # Access the amplitude
       amp = system.amplitude

       # Access a register value (by ID)
       x_id = ps.System.get_id("x")
       value = system.get(x_id).value

       # String representation
       print(system)

   # Access the last activated register
   last_val = system.last_register()

Comparison and Sorting
----------------------

The ``System`` class supports comparison operators, used for sorting and de-duplication:

.. code-block:: python

   # Equality comparison (all register values and the amplitude are identical)
   if s1 == s2:
       print("same basis state")

   # Less-than comparison (used for sorting)
   if s1 < s2:
       print("s1 sorts before s2")

   # String representation
   print(str(system))  # e.g.: "|x=3, y=5⟩ : (0.5+0j)"

Best Practices
--------------

1. **Always call ``System.clear()`` at the start of your program**

   .. code-block:: python

      import pysparq as ps

      ps.System.clear()  # First step!

      # Then create the registers...
      ps.System.add_register("a", ps.UnsignedInteger, 4)

      # SparseState() creates the |0...0⟩ initial state by default
      state = ps.SparseState()

2. **Do not manually construct a list of Systems to create a SparseState**

   The default constructor of ``SparseState`` already creates an initial state in which all register values are 0. Manually constructing ``System`` objects is usually unnecessary.

3. **Use meaningful register names**

   Names are used for debugging and :doc:`StatePrint </operators/debug>` output, so they should be descriptive.

API Reference
-------------

.. autoclass:: pysparq.System
   :members:
   :undoc-members:
