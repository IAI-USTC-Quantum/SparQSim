Installation
============

Requirements
------------

- Python 3.10 or higher
- NumPy

Install from PyPI
-----------------

.. code-block:: bash

   pip install pysparq

Install from Source
-------------------

.. code-block:: bash

   git clone https://github.com/IAI-USTC-Quantum/QRAM-Simulator.git
   cd QRAM-Simulator
   pip install .

Build Requirements
------------------

Building from source requires the following tools:

- CMake 3.15+
- A compiler with C++17 support
- OpenMP (used for parallelization, required)

Optional dependencies:

- The CUDA/GPU backend is currently disabled in CMake; source builds default to the CPU-only path.

Verify the Installation
-----------------------

.. code-block:: python

   import pysparq
   pysparq.test_import()

   # Create a quantum system
   system = pysparq.System()
   state = pysparq.SparseState(system)

   print("PySparQ installed successfully!")

----

中文版
===

安装
====

环境要求
--------

- Python 3.10 或更高版本
- NumPy

从 PyPI 安装
------------

.. code-block:: bash

   pip install pysparq

从源码安装
----------

.. code-block:: bash

   git clone https://github.com/IAI-USTC-Quantum/QRAM-Simulator.git
   cd QRAM-Simulator
   pip install .

编译要求
--------

从源码编译需要以下工具：

- CMake 3.15+
- 支持 C++17 的编译器
- OpenMP（用于并行化，必需）

可选依赖：

- CUDA/GPU 后端当前在 CMake 中临时屏蔽；源码构建默认走 CPU-only 路径。

验证安装
--------

.. code-block:: python

   import pysparq
   pysparq.test_import()

   # 创建量子系统
   system = pysparq.System()
   state = pysparq.SparseState(system)

   print("PySparQ 安装成功！")
