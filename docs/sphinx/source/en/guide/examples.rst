Examples
========

This section demonstrates how to build quantum algorithms with PySparQ from scratch — from creating initial states and using existing operators to defining custom operators, and finally to assembling Block Encoding circuits.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Prerequisites
-------------

PySparQ is installed and the C++ core has been compiled (GPU support is optional).

.. code-block:: bash

   pip install pysparq


Example 1: Creating the Initial State
-------------------------------------

The starting point of a quantum circuit is the quantum state (``SparseState``). In PySparQ, qubits are organized in units of **registers** (Register) rather than as individual qubits.

Create the system and initialize the registers:

.. code-block:: python

   import pysparq as ps

   # Clear the system (recommended before every run)
   ps.System.clear()

   # Add a 4-bit integer register
   ps.System.add_register("q", ps.UnsignedInteger, 4)

   # Create the quantum state
   state = ps.SparseState()
   print(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt4 |
   # 1.000000+0.000000i  q=|0>

The initial state is :math:`|0000\rangle`. Create a superposition with the Hadamard transform:

.. code-block:: python

   # Apply Hadamard to the entire register → equivalent to applying H to every bit
   ps.Hadamard_Int_Full("q")(state)

   # Print the state (sparse format: only non-zero amplitudes are shown)
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt4 |
   # 0.250000+0.000000i  q=|0>
   # 0.250000+0.000000i  q=|1>
   # ...
   # 0.250000+0.000000i  q=|15>
   # A superposition of 16 equal-amplitude states (each 0.25)

Set a particular basis state to a specific value:

.. code-block:: python

   ps.Init_Unsafe("q", 5)(state)   # Initialize the register to |0101⟩
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt4 |
   # 0.250000+0.000000i  q=|5>   ← all 16 amplitudes become |5⟩

Drive state evolution with an addition operator (the core of register-level programming):

.. code-block:: python

   ps.Add_ConstUInt_InPlace("q", 1)(state)  # |q⟩ → |q+1⟩, so |5⟩ → |6⟩
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)q : UInt4 |
   # 0.250000+0.000000i  q=|6>
   # 0.250000+0.000000i  q=|6>
   # ... (16 identical basis states)


Example 2: Using Existing Operators
-----------------------------------

PySparQ provides a rich set of built-in operators covering arithmetic, QRAM, QFT, conditional rotation, and other categories. The following shows how to combine them.

Addition, Multiplication, Shifting
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Arithmetic between two integer registers:

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("a", 3)(state)   # a = 3
   ps.Init_Unsafe("b", 7)(state)   # b = 7

   # a ← a + b (out_reg = a, i.e. in-place)
   ps.Add_UInt_UInt("a", "b", "a")(state)
   ps.pprint(state)           # a = 10
   # Output:
   # StatePrint (mode=Detail)
   # |(0)a : UInt4 | |(1)b : UInt4 |
   # 1.000000+0.000000i  a=|10> b=|7>

   # a ← a × 2
   ps.Mult_UInt_ConstUInt("a", 2)(state)
   ps.pprint(state)           # a = 20
   # Output:
   # StatePrint (mode=Detail)
   # |(0)a : UInt4 | |(1)b : UInt4 |
   # 1.000000+0.000000i  a=|20> b=|7>

   # a ← a << 1 (left shift by 1 bit, equivalent to multiplying by 2)
   ps.ShiftLeft_InPlace("a", 1)(state)
   ps.pprint(state)           # a = 40
   # Output:
   # StatePrint (mode=Detail)
   # |(0)a : UInt4 | |(1)b : UInt4 |
   # 1.000000+0.000000i  a=|40> b=|7>


QRAM Data Loading
~~~~~~~~~~~~~~~~~

Load a classical array into a quantum state, with support for batch queries under an address superposition:

.. code-block:: python

   import numpy as np

   ps.System.clear()
   n_addr, n_data = 3, 4
   ps.System.add_register("addr", ps.UnsignedInteger, n_addr)
   ps.System.add_register("data", ps.UnsignedInteger, n_data)

   state = ps.SparseState()

   # Classical data (8 memory locations, 4 bits each)
   memory = np.array([1, 3, 5, 7, 2, 4, 6, 8], dtype=np.uint64)

   # Superpose the address register → query all addresses at once
   ps.Hadamard_Int_Full("addr")(state)

   # QRAM load: data = memory[addr]
   qram = ps.QRAMCircuit_qutrit(n_addr, n_data, memory)
   ps.QRAMLoad(qram, "addr", "data")(state)

   # The state contains amplitudes for all (addr, memory[addr]) pairs
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)addr : UInt3 | |(1)data : UInt4 |
   # 0.353553+0.000000i  addr=|0> data=|1>
   # 0.353553+0.000000i  addr=|1> data=|3>
   # 0.353553+0.000000i  addr=|2> data=|5>
   # 0.353553+0.000000i  addr=|3> data=|7>
   # 0.353553+0.000000i  addr=|4> data=|2>
   # 0.353553+0.000000i  addr=|5> data=|4>
   # 0.353553+0.000000i  addr=|6> data=|6>
   # 0.353553+0.000000i  addr=|7> data=|8>
   # 8 superposed states, each with amplitude 1/√8 ≈ 0.354


QFT and Inverse QFT
~~~~~~~~~~~~~~~~~~~

The quantum Fourier transform and its inverse:

.. code-block:: python

   ps.System.clear()
   ps.System.add_register("reg", ps.UnsignedInteger, 4)
   state = ps.SparseState()

   ps.Init_Unsafe("reg", 5)(state)
   ps.QFT("reg")(state)        # Apply the QFT
   ps.InverseQFT("reg")(state)  # Apply the inverse QFT, restoring |5⟩
   ps.pprint(state)
   # Output:
   # StatePrint (mode=Detail)
   # |(0)reg : UInt4 |
   # 1.000000+0.000000i  reg=|5>   ← InverseQFT after QFT restores |5⟩


Example 3: Custom Operators on the Python Side
----------------------------------------------

When the built-in operators do not meet your needs, you can directly **compose existing operators** on the Python side and wrap them in a new class:

.. code-block:: python

   class MyDoubleAdder:
       """Multiply the target register value by 2: left-shift then add to itself."""

       def __init__(self, target: str, control: str):
           self.target = target
           self.control = control

       def __call__(self, state: ps.SparseState):
           # Make a copy of target
           ps.SplitRegister(self.target, "tmp", 1)(state)
           ps.CombineRegister(self.target, "tmp")(state)
           # tmp = target << 1
           ps.ShiftLeft_InPlace(self.target, 1)(state)
           # target = target + tmp
           ps.Add_UInt_UInt(self.target, "tmp")(state)
           ps.RemoveRegister("tmp")(state)

       def dag(self, state: ps.SparseState):
           # For a self-adjoint operator, dag = itself
           self(state)


For cases that require new primitives, ``pysparq.dynamic_operator.compile_operator`` supports compiling user-provided C++ code into a dynamically linked library and wrapping it directly as a Python class:

.. code-block:: python

   from pysparq.dynamic_operator import compile_operator

   cpp_code = '''
   class FlipOp : public SelfAdjointOperator {
       size_t reg_id;
   public:
       FlipOp(size_t r) : reg_id(r) {}
       void operator()(std::vector<System>& state) const override {
           for (auto& s : state) {
               s.get(reg_id).value ^= 1;
           }
       }
   };
   '''

   FlipOp = compile_operator(
       name="FlipOp",
       cpp_code=cpp_code,
       base_class="SelfAdjointOperator",
       constructor_args=[("size_t", "reg_id")],
       verbose=True,   # view the compilation output
   )

   # Usage is exactly the same as for built-in operators
   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 4)
   state = ps.SparseState()
   ps.Init_Unsafe("q", 1)(state)

   flip = FlipOp(reg_id=0)
   flip(state)          # Flip bit 0
   flip.dag(state)      # Inverse operation (FlipOp is self-adjoint, dag = itself)

Compilation results are cached based on a hash of the code, so repeated calls do not trigger recompilation.

For finer-grained compiler control (such as adding custom header files or linking extra libraries), see :doc:`dynamic_operators`.

Example 4: Block Encoding Concepts and Implementation
-----------------------------------------------------

Block Encoding is one of the most important circuit construction paradigms in quantum algorithms — it encodes a :math:`d \times d` classical (or quantum) matrix :math:`A` into a much larger unitary matrix :math:`U_A` such that:

.. math::

   \langle 0| \langle i| U_A |0\rangle |j\rangle = A_{ij} / \alpha

where :math:`\alpha` is a normalization factor.

Block Encoding of Tridiagonal Matrices
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``pysparq.algorithms.block_encoding.BlockEncodingTridiagonal`` encodes the symmetric tridiagonal matrix :math:`A = \alpha I + \beta T` (:math:`T` is the shift matrix) as a quantum circuit. Implementation logic:

1. Prepare a 4-element superposition on an ancillary register
2. Apply controlled add/subtract 1 to the main register (an overflow bit records the carry)
3. Insert a reflection gate to correct the sign when :math:`\beta < 0`
4. The inverse operator is used to release the ancillary register

Full example:

.. code-block:: python

   import numpy as np
   import pysparq as ps
   from pysparq.algorithms.block_encoding import (
       get_tridiagonal_matrix,
       BlockEncodingTridiagonal,
   )

   alpha, beta, dim = 0.5, 0.3, 4
   A = get_tridiagonal_matrix(alpha, beta, dim)
   print(f"Tridiagonal matrix A:\n{A}")

   # --- Build the Block Encoding circuit ---
   ps.System.clear()
   ps.System.add_register("main_reg", ps.UnsignedInteger, 2)  # dim = 2^2 = 4
   ps.System.add_register("anc_UA", ps.UnsignedInteger, 4)

   state = ps.SparseState()
   ps.Init_Unsafe("main_reg", 0)(state)
   ps.Init_Unsafe("anc_UA", 0)(state)

   block_enc = BlockEncodingTridiagonal("main_reg", "anc_UA", alpha, beta)
   block_enc(state)
   print(f"Block encoding applied successfully, current basis-state count: {state.size()}")

   # Inverse Block Encoding (releases the ancillary register)
   block_enc.dag(state)
   print(f"Basis-state count after inverse block encoding: {state.size()}")


QRAM-Based Block Encoding
~~~~~~~~~~~~~~~~~~~~~~~~~

For arbitrary sparse matrices (stored in QRAM), ``BlockEncodingViaQRAM`` combines :math:`U_L` (row-direction rotation), :math:`U_R^\dagger` (column-direction rotation), and a SWAP operation to realize complete Block Encoding:

.. math::

   U_A = \text{SWAP}(\text{row}, \text{col}) \cdot U_R^\dagger(\text{col}) \cdot U_L(\text{row}, \text{col})

Each operation loads parent/child node data via QRAM, computes the rotation angles, and applies conditional rotations. See the source code of the
``pysparq.algorithms.block_encoding`` module for implementation details.

These two Block Encoding building blocks are the core of advanced algorithms such as QDA (quantum linear system solvers) and Hamiltonian simulation, forming the bridge from operators to complete quantum algorithms.


Example 5: Custom Operators on the C++ Side (Advanced)
------------------------------------------------------

If performance or expressive power on the Python side is insufficient, you can implement new operators directly in the C++ core; the steps are as follows:

1. **Create a header file in** ``SparQ/include/`` that inherits from ``BaseOperator`` or ``SelfAdjointOperator`` and implements the ``apply()`` method:

   .. code-block:: cpp

      // SparQ/include/MyCustomOp.h
      #pragma once
      #include "base_operator.h"

      class MyCustomOp : public SelfAdjointOperator {
      public:
          MyCustomOp(size_t reg_id, double phase)
              : reg_id_(reg_id), phase_(phase) {}

          void apply(std::vector<System>& state) const override {
              for (auto& s : state) {
                  // Read the register value
                  auto val = s.get(reg_id_).value;
                  // Modify the amplitude or the register value
                  s.amplitude *= std::exp(complex(0, phase_ * val));
              }
          }

      private:
          size_t reg_id_;
          double phase_;
      };

2. **Implement the** ``apply()`` **method in** ``SparQ/src/`` (if a separate cpp file is needed)

3. **Add pybind11 bindings in** ``PySparQ/src/pybind_wrapper.cpp``

4. **Rebuild PySparQ** (``pip install .``)

See :doc:`dynamic_operators` for the complete development workflow.
