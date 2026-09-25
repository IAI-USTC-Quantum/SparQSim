Development Workflow
====================

Environment Setup
-----------------

CPU Build
^^^^^^^^^

.. code-block:: bash

   cd QRAM-Simulator
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)

GPU Build
^^^^^^^^^

The CUDA/GPU backend is off by default; enabling it requires a local CUDA toolchain (CUDA 13 / CCCL 3 tested).

.. code-block:: bash

   cmake .. -DCMAKE_BUILD_TYPE=Release -DSPARQ_ENABLE_CUDA=ON
   make -j$(nproc)

Python Bindings
^^^^^^^^^^^^^^^

.. code-block:: bash

   pip install .

Core Code Structure
-------------------

+----------------------------+-----------------------------------------------+------------------------------------------+
| Component                  | Path                                          | Purpose                                  |
+============================+===============================================+==========================================+
| Sparse state simulator     | ``SparQ/include/sparse_state_simulator.h``    | Core state representation                |
+----------------------------+-----------------------------------------------+------------------------------------------+
| Register management        | ``SparQ/include/system_operations.h``         | Creation, lifetime, storage types        |
+----------------------------+-----------------------------------------------+------------------------------------------+
| Arithmetic operations      | ``SparQ/include/quantum_arithmetic.h``        | Add, Mult, Shift, etc.                   |
+----------------------------+-----------------------------------------------+------------------------------------------+
| Basic gates                | ``SparQ/include/basic_gates.h``               | H, X, Y, Z, CNOT, etc.                   |
+----------------------------+-----------------------------------------------+------------------------------------------+
| QRAM                       | ``SparQ/include/qram.h``                      | QRAM load operations                     |
+----------------------------+-----------------------------------------------+------------------------------------------+
| High-level algorithms      | ``SparQ_Algorithm/``                          | State preparation, block encoding, etc.  |
+----------------------------+-----------------------------------------------+------------------------------------------+

Adding a New Experiment
-----------------------

1. Create the experiment directory structure:

   .. code-block:: text

      Experiments/
      └── MyAlgorithm/
          ├── MyAlgorithmTest.cpp
          └── CMakeLists.txt

2. Write ``CMakeLists.txt``:

   .. code-block:: cmake

      add_executable(MyAlgorithmTest MyAlgorithmTest.cpp)
      target_link_libraries(MyAlgorithmTest PRIVATE SparQ SparQ_Algorithm Common)

3. Register it in ``Experiments/CMakeLists.txt``:

   .. code-block:: cmake

      add_subdirectory(MyAlgorithm)

Git Workflow
------------

Create a branch and develop:

.. code-block:: bash

   # Create a feature branch
   git checkout -b feat/my-algorithm origin/main

   # Develop, test...

   # Push to your fork
   git push origin feat/my-algorithm

Submit a PR to upstream after CI passes.

Running Tests
-------------

.. code-block:: bash

   # Run all tests
   cd build && ctest --output-on-failure

   # Run a specific test
   ./build/bin/MyAlgorithmTest

----

中文版
===

开发工作流
==========

环境准备
--------

CPU 构建
^^^^^^^^

.. code-block:: bash

   cd QRAM-Simulator
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)

GPU 构建
^^^^^^^^

CUDA/GPU 后端默认关闭；开启需本机 CUDA 工具链（CUDA 13 / CCCL 3 实测通过）。

.. code-block:: bash

   cmake .. -DCMAKE_BUILD_TYPE=Release -DSPARQ_ENABLE_CUDA=ON
   make -j$(nproc)

Python 绑定
^^^^^^^^^^^

.. code-block:: bash

   pip install .

核心代码结构
------------

+--------------------------+---------------------------------------------+--------------------------------------+
| 组件              | 路径                                   | 用途                   |
+==========================+=============================================+======================================+
| 稀疏态模拟器      | ``SparQ/include/sparse_state_simulator.h`` | 核心状态表示            |
+--------------------------+---------------------------------------------+--------------------------------------+
| 寄存器管理        | ``SparQ/include/system_operations.h`` | 创建、生命周期、存储类型|
+--------------------------+---------------------------------------------+--------------------------------------+
| 算术运算          | ``SparQ/include/quantum_arithmetic.h`` | Add, Mult, Shift 等     |
+--------------------------+---------------------------------------------+--------------------------------------+
| 基础门            | ``SparQ/include/basic_gates.h``       | H, X, Y, Z, CNOT 等     |
+--------------------------+---------------------------------------------+--------------------------------------+
| QRAM              | ``SparQ/include/qram.h``               | QRAM 加载操作            |
+--------------------------+---------------------------------------------+--------------------------------------+
| 高层算法          | ``SparQ_Algorithm/``                   | 状态制备、块编码等       |
+--------------------------+---------------------------------------------+--------------------------------------+

添加新实验
---------

1. 创建实验目录结构：

   .. code-block:: text

      Experiments/
      └── MyAlgorithm/
          ├── MyAlgorithmTest.cpp
          └── CMakeLists.txt

2. 编写 ``CMakeLists.txt``：

   .. code-block:: cmake

      add_executable(MyAlgorithmTest MyAlgorithmTest.cpp)
      target_link_libraries(MyAlgorithmTest PRIVATE SparQ SparQ_Algorithm Common)

3. 在 ``Experiments/CMakeLists.txt`` 中注册：

   .. code-block:: cmake

      add_subdirectory(MyAlgorithm)

Git 工作流
---------

创建分支并开发：

.. code-block:: bash

   # 创建功能分支
   git checkout -b feat/my-algorithm origin/main

   # 开发、测试...

   # 推送到 fork
   git push origin feat/my-algorithm

CI 验证后提交 PR 到 upstream。

运行测试
--------

.. code-block:: bash

   # 运行所有测试
   cd build && ctest --output-on-failure

   # 运行特定测试
   ./build/bin/MyAlgorithmTest
