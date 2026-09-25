Quantum Algorithm Development Guide
===================================

This guide introduces how to develop new quantum algorithms in SparQ/QRAM-Simulator.

.. contents::
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Core Paradigm
-------------

SparQ adopts the **Register Level Programming** paradigm, in contrast to the traditional composition of gate-level circuits:

+---------------------+-----------------------------------------+----------------------------------------------------+
| Dimension           | Traditional approach                    | Register Level Programming                         |
+=====================+=========================================+====================================================+
| State storage       | Arrays of qubits                        | Register values stored directly as ``uint64_t``    |
+---------------------+-----------------------------------------+----------------------------------------------------+
| Arithmetic          | Compiled into quantum gate sequences    | Arithmetic performed directly on register values   |
+---------------------+-----------------------------------------+----------------------------------------------------+
| Development model   | Bottom-up (build from gate circuits)    | Top-down (high-level modules first, then refine)   |
+---------------------+-----------------------------------------+----------------------------------------------------+

Development Workflow
--------------------

1. **Environment setup** - build the project and install dependencies

2. **Understand the core components** - get familiar with the code structure

3. **Implement the algorithm** - refer to existing experiments

4. **Test and verify** - ensure correctness

5. **Submit the code** - PR to upstream

.. toctree::
   :maxdepth: 2

   workflow
   templates
   verification
   standards
