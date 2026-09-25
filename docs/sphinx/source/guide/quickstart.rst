Quick Start
===========

Register-Level Programming
--------------------------

PySparQ adopts the "register-level programming" paradigm. Instead of composing circuits from individual gates, you operate directly on named quantum registers. The level of abstraction rises from qubits to quantum registers, and almost all operations take registers as their unit.

Basic Workflow
--------------

1. Call ``System.clear()`` to clean up static state
2. Declare registers (name, type, number of bits)
3. Create ``SparseState()`` — the default constructor automatically creates the ``|0...0⟩`` initial state
4. Apply quantum operations
5. Read out the measurement results

Example: Quantum Addition
-------------------------

.. code-block:: python

   import pysparq as ps

   # Step 1: clean up static state
   ps.System.clear()

   # Step 2: declare registers
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)

   # Step 3: create a sparse quantum state (automatically creates the |a=0, b=0⟩ initial state)
   state = ps.SparseState()

   # Step 4: put the registers into superposition
   ps.Hadamard_Int("a")(state)
   ps.Hadamard_Int("b")(state)

   # Quantum addition: a += b
   ps.Add_UInt_UInt("b", "a")(state)

   # The state now holds a superposition of all possible sums
   ps.pprint(state)

Conditional Operations
----------------------

Operations can be conditioned on the values of other registers:

.. code-block:: python

   # Add a control register
   ps.AddRegister("control", ps.Boolean, 1)(state)

   # Apply the operation only when control is |1>
   ps.Add_UInt_UInt("a", "b").conditioned_by_nonzeros("control")(state)

Control Types
^^^^^^^^^^^^^

- :meth:`conditioned_by_nonzeros(reg)` - execute when the register is non-zero
- :meth:`conditioned_by_all_ones(reg)` - execute when the register is all ones
- :meth:`conditioned_by_bit(reg, pos)` - execute when a specific bit is 1
- :meth:`conditioned_by_value(reg, pos)` - execute when the value at the specified position equals a specific value

----

中文版
===

快速入门
========

寄存器级编程
------------

PySparQ 采用"寄存器级编程"范式。与从单个门组合电路不同，您直接对命名的量子寄存器进行操作。操作层面从量子比特上升到了量子寄存器，几乎所有操作都以寄存器为单位。

基本工作流程
------------

1. 调用 ``System.clear()`` 清理静态状态
2. 声明寄存器（名称、类型、比特数）
3. 创建 ``SparseState()``——默认构造函数自动创建 ``|0...0⟩`` 初态
4. 应用量子操作
5. 读取测量结果

示例：量子加法
--------------

.. code-block:: python

   import pysparq as ps

   # 第一步：清理静态状态
   ps.System.clear()

   # 第二步：声明寄存器
   ps.System.add_register("a", ps.UnsignedInteger, 4)
   ps.System.add_register("b", ps.UnsignedInteger, 4)

   # 第三步：创建稀疏量子态（自动创建 |a=0, b=0⟩ 初态）
   state = ps.SparseState()

   # 第四步：将寄存器置于叠加态
   ps.Hadamard_Int("a")(state)
   ps.Hadamard_Int("b")(state)

   # 量子加法：a += b
   ps.Add_UInt_UInt("b", "a")(state)

   # 状态现在包含所有可能求和结果的叠加态
   ps.pprint(state)

条件操作
--------

操作可以根据其他寄存器的值进行条件控制：

.. code-block:: python

   # 添加控制寄存器
   ps.AddRegister("control", ps.Boolean, 1)(state)

   # 仅当 control 为 |1> 时应用操作
   ps.Add_UInt_UInt("a", "b").conditioned_by_nonzeros("control")(state)

控制类型
^^^^^^^^

- :meth:`conditioned_by_nonzeros(reg)` - 当寄存器非零时执行
- :meth:`conditioned_by_all_ones(reg)` - 当寄存器全为1时执行
- :meth:`conditioned_by_bit(reg, pos)` - 当特定位为1时执行
- :meth:`conditioned_by_value(reg, pos)` - 当指定位置的值为特定值时执行
