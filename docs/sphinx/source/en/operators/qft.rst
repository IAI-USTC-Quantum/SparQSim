Quantum Fourier Transform (QFT)
===============================

The quantum Fourier transform is a core component of many quantum algorithms — such as phase estimation and Shor's factoring — provided ready-made in the :doc:`algorithm library </cpp_api/algorithms>`.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: QFT operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Unitarity class
   * - ``QFT``
     - Quantum Fourier transform
     - BaseOperator
   * - ``InverseQFT``
     - Inverse quantum Fourier transform
     - BaseOperator

Mathematical Definition
-----------------------

The quantum Fourier transform is defined as:

.. math::

   QFT|x\rangle = \frac{1}{\sqrt{2^n}} \sum_{y=0}^{2^n-1} e^{2\pi i xy / 2^n} |y\rangle

The inverse quantum Fourier transform:

.. math::

   QFT^\dagger|y\rangle = \frac{1}{\sqrt{2^n}} \sum_{x=0}^{2^n-1} e^{-2\pi i xy / 2^n} |x\rangle

---

QFT
---

.. autoclass:: pysparq.QFT
   :members:
   :undoc-members:

**Operation**: Performs the quantum Fourier transform on an integer register

**Type constraints**: :doc:`UnsignedInteger </guide/core_concepts/register_types>` or :doc:`SignedInteger </guide/core_concepts/register_types>`

**Bit constraints**: The register size determines the transform dimension

.. code-block:: python

   import pysparq as ps

   ps.System.clear()

   # 4-bit register
   ps.System.add_register("q", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Initialize to the basis state |1⟩
   ps.Init_Unsafe("q", 1)(state)

   print("Initial state:")
   ps.pprint(state)
   # |q=1⟩ : (1+0j)

   # Apply the QFT
   op = ps.QFT("q")
   op(state)

   print("\nAfter QFT:")
   ps.pprint(state)
   # Produces a superposition with uniformly distributed phases

   # Undo (inverse transform)
   op.dag(state)

InverseQFT
----------

.. autoclass:: pysparq.InverseQFT
   :members:
   :undoc-members:

**Operation**: Performs the inverse quantum Fourier transform on an integer register

**Purpose**: Undo a QFT or extract phase information

.. code-block:: python

   ps.System.clear()

   ps.System.add_register("q", ps.UnsignedInteger, 4)

   state = ps.SparseState()

   # Create a superposition
   ps.Hadamard_Int_Full("q")(state)

   # Apply the inverse QFT
   ps.InverseQFT("q")(state)

   # The inverse QFT and the QFT are inverses of each other
   ps.QFT("q")(state)

---

Use Cases
---------

Period Finding (Shor's Algorithm)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   import pysparq as ps
   import numpy as np

   ps.System.clear()

   # Phase register
   n_bits = 8
   ps.System.add_register("phase", ps.UnsignedInteger, n_bits)

   state = ps.SparseState()

   # Simulate a phase estimation result (assume period r = 5)
   # |phase⟩ contains periodic phase information
   ps.Hadamard_Int_Full("phase")(state)

   # Apply the inverse QFT to extract the period
   ps.InverseQFT("phase")(state)

   # Measure the phase register to determine the period
   # ...

Phase Estimation
^^^^^^^^^^^^^^^^

.. code-block:: python

   # Assume a phase estimation circuit already exists
   # The inverse QFT converts phase information into the computational basis

   # 1. Prepare the initial state
   ps.Hadamard_Int_Full("estimate")(state)

   # 2. Apply the controlled-unitary operation
   # controlled_U(...)(state)

   # 3. Extract the phase with the inverse QFT
   ps.InverseQFT("estimate")(state)

   # 4. Measure to obtain the phase estimate
   # ...

Quantum Signal Processing
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   # The QFT is used for frequency-domain analysis
   ps.System.add_register("signal", ps.UnsignedInteger, 16)

   state = ps.SparseState()
   # Load the signal data...

   # Transform to the frequency domain
   ps.QFT("signal")(state)

   # Frequency-domain processing...

   # Transform back to the time domain
   ps.InverseQFT("signal")(state)

Performance Considerations
--------------------------

- **Number of basis states**: When the QFT acts on a superposition, it produces :math:`2^n` basis states
- **Memory limits**: For large n (e.g. n > 20), memory problems may arise
- **Sparse optimization**: PySparQ stores only the basis states with nonzero amplitudes, which can be efficient for certain inputs

.. note::

   For superpositions that are not complete, the sparse implementation of the QFT can be more efficient than full-state simulation.

Practical Examples
------------------

Full QFT + InverseQFT Cycle
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   import pysparq as ps
   import numpy as np

   ps.System.clear()

   n = 3
   ps.System.add_register("q", ps.UnsignedInteger, n)

   state = ps.SparseState()

   # Initial value
   initial_value = 5
   ps.Init_Unsafe("q", initial_value)(state)

   print("Initial state:")
   ps.pprint(state)

   # QFT
   ps.QFT("q")(state)
   print("\nAfter QFT:")
   ps.pprint(state)

   # InverseQFT
   ps.InverseQFT("q")(state)
   print("\nAfter InverseQFT:")
   ps.pprint(state)
   # Restored to |q=5⟩
