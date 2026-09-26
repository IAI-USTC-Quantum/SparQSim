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

CUDA/GPU 后端默认关闭；开启需本机 CUDA 工具链（CUDA 13 / CCCL 3 实测通过）。GPU 代码布局见 :doc:`CUDA 后端参考 </cpp_api/cuda>`。

.. code-block:: bash

   cmake .. -DCMAKE_BUILD_TYPE=Release -DSPARQ_ENABLE_CUDA=ON
   make -j$(nproc)

Python 绑定
^^^^^^^^^^^

.. code-block:: bash

   pip install .

核心代码结构
------------

下述头文件的渲染参考：:doc:`核心组件 </cpp_api/core>`、:doc:`寄存器/系统操作 </cpp_api/system_ops>`、
:doc:`量子算术 </cpp_api/arithmetic>`、:doc:`QRAM </cpp_api/qram>` 与 :doc:`算法库 </cpp_api/algorithms>`。

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
