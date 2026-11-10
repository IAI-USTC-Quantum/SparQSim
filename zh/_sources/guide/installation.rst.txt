安装
====

环境要求
--------

- Python 3.10 或更高版本
- :mod:`numpy`

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

- CUDA/GPU 后端当前在 CMake 中临时屏蔽；源码构建默认走 CPU-only 路径；GPU 代码布局见 :doc:`CUDA 后端 </cpp_api/cuda>`，构建细节见 :doc:`开发工作流 </guide/development/workflow>`。

验证安装
--------

.. code-block:: python

   import pysparq
   pysparq.test_import()

   # 创建稀疏量子态（|0...0> 初态）
   state = pysparq.SparseState()

   print("PySparQ 安装成功！")

这创建了 :class:`System <pysparq.System>` 与 :class:`SparseState <pysparq.SparseState>` ——两个核心抽象，
参见 :doc:`核心概念 </guide/core_concepts/index>`。
