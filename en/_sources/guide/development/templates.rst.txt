Code Templates
==============

Register Types
--------------

.. code-block:: cpp

   UnsignedInteger  // unsigned integer
   SignedInteger     // signed integer
   Boolean          // single bit
   Rational         // rational number (used for angle computations)

These names map to the storage types described in :doc:`register types <../core_concepts/register_types>`.

C++ Development Template
------------------------

.. code-block:: cpp

   #include "sparse_state_simulator.h"
   #include "system_operations.h"
   #include "quantum_arithmetic.h"

   using namespace qram_simulator;

   // 1. Declare registers in System
   auto addr_reg = System::add_register("addr", UnsignedInteger, addr_size);
   auto data_reg = System::add_register("data", UnsignedInteger, data_size);

   // 2. Create a sparse state (automatically initialized to |0...0>)
   std::vector<System> state;
   state.emplace_back();

   // 3. Apply quantum operations
   Hadamard_Int(addr_reg)(state);           // superposition
   QRAMLoad(qram, addr_reg, data_reg)(state); // QRAM load
   Add_UInt_UInt(data_reg, result_reg)(state); // arithmetic

   // 4. Measure / output
   StatePrint(Detail)(state);

Python Development Template
---------------------------

.. code-block:: python

   import pysparq as ps

   # 1. Clean up static state
   ps.System.clear()

   # 2. Declare registers
   ps.System.add_register("addr", ps.UnsignedInteger, 4)
   ps.System.add_register("data", ps.UnsignedInteger, 4)

   # 3. Create a sparse state
   state = ps.SparseState()

   # 4. Apply operations
   ps.Hadamard_Int("addr", 4)(state)
   ps.QRAMLoad(qram, "addr", "data")(state)

   # 5. Read out the results
   ps.pprint(state)

Grover Search Template
----------------------

See :doc:`Grover search (C++ reference) </cpp_api/algorithms>` for the production implementation.

.. code-block:: cpp

   // Create a QRAM
   qram_qutrit::QRAMCircuit qram(addr_size, data_size);
   qram.set_memory_random();

   // Declare registers
   auto addr_reg = System::add_register("addr", UnsignedInteger, addr_size);
   auto data_reg = System::add_register("data", UnsignedInteger, data_size);
   auto target_reg = System::add_register("target", UnsignedInteger, data_size);

   // Create the state
   std::vector<System> state;
   state.emplace_back();
   Init_Unsafe("target", search_target)(state);

   // Apply the Grover operator
   for (size_t i = 0; i < n_repeats; ++i) {
       auto temp_reg = AddRegister("temp", UnsignedInteger, data_size)(state);
       GroverOperator(&qram, addr_reg, temp_reg, target_reg)(state);
       RemoveRegister(temp_reg)(state);
   }

Block Encoding Template
-----------------------

See :doc:`block encoding (C++ reference) </cpp_api/block_encoding>` (``SparQ_Algorithm/include/block_encoding.h``) for implementing block encodings of unitary matrices; a Python walkthrough is available in :doc:`Examples </guide/examples>`.

Hamiltonian Simulation
----------------------

See the CKS (Carnegie-Kellam-Schulten) algorithm implementation under ``Experiments/CKS/``; the underlying Hamiltonian-simulation primitives are documented under :doc:`the algorithm library </cpp_api/algorithms>`.
