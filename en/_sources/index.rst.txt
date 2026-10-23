SparQ Documentation
===================

SparQ is a :doc:`sparse-state </guide/core_concepts/sparse_state>` quantum circuit simulator framework with native
:doc:`QRAM </operators/qram_ops>` support, a :doc:`register-level programming </guide/core_concepts/index>` paradigm,
a quantum :doc:`algorithm library </cpp_api/algorithms>` (Grover, Shor, block encoding, Hamiltonian simulation,
discrete adiabatic, etc.), and complete Python bindings (`pysparq <https://pypi.org/project/pysparq/>`_).

.. raw:: html

   <div class="badges" style="display: flex; gap: 10px; flex-wrap: wrap; margin-bottom: 20px;">
     <a href="https://arxiv.org/abs/2503.15118"><img src="https://img.shields.io/badge/SparQ-arXiv%3A2503%2E15118-6f42c1.svg" alt="arXiv"></a>
     <a href="https://arxiv.org/abs/2503.13832"><img src="https://img.shields.io/badge/QRAM_Simulator-arXiv%3A2503%2E13832-b31b1b.svg" alt="arXiv"></a>
     <a href="https://pypi.org/project/pysparq/"><img src="https://img.shields.io/pypi/v/pysparq.svg" alt="PyPI"></a>
     <a href="https://github.com/IAI-USTC-Quantum/SparQSim"><img src="https://img.shields.io/badge/GitHub-Repo-181717?logo=github" alt="GitHub"></a>
     <a href="https://iai-ustc-quantum.github.io/SparQSim/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-4D6AE4" alt="Documentation"></a>
   </div>

Quick Links
-----------

* `GitHub repository <https://github.com/IAI-USTC-Quantum/SparQSim>`_ - source code and issue tracking
* `QRAM-Simulator repository <https://github.com/IAI-USTC-Quantum/QRAM-Simulator>`_ - the QRAM foundation (referenced by this repository as a submodule)
* `PyPI <https://pypi.org/project/pysparq/>`_ - ``pip install pysparq``

.. toctree::
   :maxdepth: 3
   :caption: User Guide

   guide/installation
   guide/quickstart
   guide/architecture
   guide/examples
   guide/algorithms/index
   guide/dynamic_operators
   guide/rir
   guide/core_concepts/index
   guide/development/index

.. toctree::
   :maxdepth: 2
   :caption: Operator Reference

   operators/index

.. toctree::
   :maxdepth: 2
   :caption: C++ API Reference

   cpp_api/index

.. toctree::
   :maxdepth: 2
   :caption: Interactive Tutorials

   notebooks/01_quickstart
   notebooks/02_sparse_state_evolution
   notebooks/03_operator_examples

.. toctree::
   :maxdepth: 2
   :caption: Python API Reference

   api/index

Getting Started
---------------

Install PySparQ via pip (see :doc:`installation </guide/installation>` for building from source):

.. code-block:: bash

   pip install pysparq

A quick example:

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 4)
   state = ps.SparseState()
   ps.Hadamard_Int("q", 4)(state)
   print(state)

This declares a register, creates a :class:`SparseState <pysparq.SparseState>` in the ``|0⟩`` state, and applies the :doc:`Hadamard </operators/hadamard>` operator. Continue with the :doc:`Quick Start </guide/quickstart>`.

Indices and Tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
