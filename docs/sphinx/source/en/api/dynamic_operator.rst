Dynamic Operator API Reference
==============================

This module provides runtime compilation and loading of custom C++ quantum operators.

.. automodule:: pysparq.dynamic_operator
   :members:
   :show-inheritance:
   :member-order: groupwise
   :no-index:

Main Functions
--------------

compile_operator
~~~~~~~~~~~~~~~~

.. autofunction:: pysparq.dynamic_operator.compile_operator
   :no-index:

Cache Management
----------------

get_cache_info
~~~~~~~~~~~~~~

.. autofunction:: pysparq.dynamic_operator.get_cache_info
   :no-index:

clear_cache
~~~~~~~~~~~

.. autofunction:: pysparq.dynamic_operator.clear_cache
   :no-index:

Configuration Classes
---------------------

CompilerConfig
~~~~~~~~~~~~~~

.. autoclass:: pysparq.dynamic_operator.CompilerConfig
   :members:
   :show-inheritance:
   :no-index:

Exception Classes
-----------------

CompilationError
~~~~~~~~~~~~~~~~

.. autoclass:: pysparq.dynamic_operator.CompilationError
   :members:
   :show-inheritance:
   :no-index:

DynamicOperatorError
~~~~~~~~~~~~~~~~~~~~

.. autoclass:: pysparq.dynamic_operator.DynamicOperatorError
   :members:
   :show-inheritance:
   :no-index:

DynamicOperatorLoadError
~~~~~~~~~~~~~~~~~~~~~~~~

.. autoclass:: pysparq.dynamic_operator.DynamicOperatorLoadError
   :members:
   :show-inheritance:
   :no-index:

DynamicOperatorFactoryError
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. autoclass:: pysparq.dynamic_operator.DynamicOperatorFactoryError
   :members:
   :show-inheritance:
   :no-index:

Low-Level Functions
-------------------

compile_cpp_code
~~~~~~~~~~~~~~~~

.. autofunction:: pysparq.dynamic_operator.compile_cpp_code
   :no-index:

compute_code_hash
~~~~~~~~~~~~~~~~~

.. autofunction:: pysparq.dynamic_operator.compute_code_hash
   :no-index:

generate_cpp_source
~~~~~~~~~~~~~~~~~~~

.. autofunction:: pysparq.dynamic_operator.generate_cpp_source
   :no-index:

create_operator_class
~~~~~~~~~~~~~~~~~~~~~

.. autofunction:: pysparq.dynamic_operator.create_operator_class
   :no-index:
