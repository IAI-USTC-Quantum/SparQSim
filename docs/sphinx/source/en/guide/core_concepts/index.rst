Core Concepts
=============

This section introduces the fundamental building blocks of sparse-state quantum simulation in PySparQ. Understanding these concepts is a prerequisite for using the :doc:`Register Level Programming </guide/quickstart>` paradigm.

.. toctree::
   :maxdepth: 2

   system
   sparse_state
   register_management
   register_types
   operators

Core Abstractions
-----------------

PySparQ is built on three core abstractions:

1. **System** — a single computational basis state, containing one complex amplitude ``amplitude`` and the values of all registers ``registers`` (see :doc:`The System Class <system>`)
2. **SparseState** — a sparse quantum state managing a ``std::vector<System>``, storing only basis states with non-zero amplitudes (see :doc:`The SparseState Class <sparse_state>`)
3. **Registers** — named quantum variables with typed storage, using ``uint64_t`` as the storage unit, allowing multi-register encodings of the form ``|a⟩|b⟩|c⟩`` (see :doc:`Register Management <register_management>` and :doc:`Register Types <register_types>`)

Core Data Relationships
-----------------------

- The default constructor of ``SparseState`` creates a ``|0...0⟩`` initial state (internally containing a single ``System`` whose register values are all 0 and whose amplitude is 1)
- The register-value combination of each ``System`` instance must be unique within a ``SparseState`` — if a duplicate appears, quantum interference has occurred, and the two amplitudes should be added and de-duplicated
- The operational level of quantum programming rises from qubits up to quantum registers; almost all operations take registers as their unit — see the :ref:`operator reference <operator-reference>` for the built-in catalogue.

Unlike traditional full state-vector simulators, which store :math:`2^n` amplitudes, PySparQ stores only the non-zero basis states, so quantum algorithms with a finite number of superposed states can be simulated with polynomial resources.
