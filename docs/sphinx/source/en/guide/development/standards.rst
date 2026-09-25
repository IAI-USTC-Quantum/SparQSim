Coding Standards
================

C++ Standards
-------------

- Format with ``clang-format -i``
- 4-space indentation, 120-column limit
- Follow the LLVM coding style
- Add the ``COMPOSITE_OPERATION`` macro to support dagger operations

Naming Conventions
------------------

+-------------------------+-----------------------------+---------------------------+
| Type                    | Convention                  | Example                   |
+=========================+=============================+===========================+
| Class names             | UpperCamelCase              | ``GroverOperator``        |
+-------------------------+-----------------------------+---------------------------+
| Functions/methods       | snake_case                  | ``add_register``          |
+-------------------------+-----------------------------+---------------------------+
| Register names          | short and meaningful        | ``"addr"``, ``"data"``    |
+-------------------------+-----------------------------+---------------------------+
| Namespaces              | lowercase                   | ``qram_simulator``        |
+-------------------------+-----------------------------+---------------------------+

Performance Considerations
--------------------------

- Prefer the sparse state representation
- Clean up temporary registers promptly: ``RemoveRegister(temp_reg)(state)``
- Use the ``profiler`` to track performance bottlenecks

Performance optimization example:

.. code-block:: cpp

   // Clean up registers that are no longer in use promptly
   auto temp_reg = AddRegister("temp", UnsignedInteger, size)(state);
   // ... use temp_reg ...
   RemoveRegister(temp_reg)(state);  // release the memory

   // Use the profiler to locate bottlenecks
   {
       profiler _("MySlowOperation");
       ComplexOperation(params)(state);
   }

Comment Guidelines
------------------

Add comments only in the following cases:

- When a constraint is not obvious
- When there is a hidden invariant
- For a workaround targeting a specific bug

.. code-block:: cpp

   // Do not write:
   // Add a to b
   Add_UInt_UInt("a", "b")(state);

   // Instead, write:
   // Workaround for known issue #123
   Add_UInt_UInt("a", "b")(state)
