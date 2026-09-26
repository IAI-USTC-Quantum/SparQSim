SparQ 文档
----------

SparQ 是一个 :doc:`稀疏态 </guide/core_concepts/sparse_state>` 量子电路模拟器框架，具有原生
:doc:`QRAM </operators/qram_ops>` 支持、:doc:`寄存器级编程 </guide/core_concepts/index>` 范式、
量子 :doc:`算法库 </cpp_api/algorithms>`（Grover、Shor、块编码、哈密顿量模拟、离散绝热等）与完整的
Python 绑定（`pysparq <https://pypi.org/project/pysparq/>`_）。

.. raw:: html

   <div class="badges" style="display: flex; gap: 10px; flex-wrap: wrap; margin-bottom: 20px;">
     <a href="https://arxiv.org/abs/2503.15118"><img src="https://img.shields.io/badge/SparQ-arXiv%3A2503%2E15118-6f42c1.svg" alt="arXiv"></a>
     <a href="https://arxiv.org/abs/2503.13832"><img src="https://img.shields.io/badge/QRAM_Simulator-arXiv%3A2503%2E13832-b31b1b.svg" alt="arXiv"></a>
     <a href="https://pypi.org/project/pysparq/"><img src="https://img.shields.io/pypi/v/pysparq.svg" alt="PyPI"></a>
     <a href="https://github.com/IAI-USTC-Quantum/SparQSim"><img src="https://img.shields.io/badge/GitHub-Repo-181717?logo=github" alt="GitHub"></a>
     <a href="https://iai-ustc-quantum.github.io/SparQSim/"><img src="https://img.shields.io/badge/docs-GitHub%20Pages-4D6AE4" alt="Documentation"></a>
   </div>

快速链接
--------

* `GitHub 仓库 <https://github.com/IAI-USTC-Quantum/SparQSim>`_ - 源码和问题反馈
* `QRAM-Simulator 仓库 <https://github.com/IAI-USTC-Quantum/QRAM-Simulator>`_ - QRAM 基座（本仓库以 submodule 引用）
* `PyPI <https://pypi.org/project/pysparq/>`_ - ``pip install pysparq``

.. toctree::
   :maxdepth: 3
   :caption: 用户指南

   guide/installation
   guide/quickstart
   guide/architecture
   guide/examples
   guide/dynamic_operators
   guide/rir
   guide/core_concepts/index
   guide/development/index

.. toctree::
   :maxdepth: 2
   :caption: 算子参考

   operators/index

.. toctree::
   :maxdepth: 2
   :caption: C++ API 参考

   cpp_api/index

.. toctree::
   :maxdepth: 2
   :caption: 交互教程

   notebooks/01_quickstart
   notebooks/02_sparse_state_evolution
   notebooks/03_operator_examples

.. toctree::
   :maxdepth: 2
   :caption: Python API 参考

   api/index

快速开始
--------

通过 pip 安装 PySparQ（从源码构建参见 :doc:`安装 </guide/installation>`）：

.. code-block:: bash

   pip install pysparq

快速示例：

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 4)
   state = ps.SparseState()
   ps.Hadamard_Int("q", 4)(state)
   print(state)

该示例声明了一个寄存器，创建了处于 ``|0⟩`` 态的 :class:`SparseState <pysparq.SparseState>` 初态，
并施加了 :doc:`Hadamard </operators/hadamard>` 算子。继续阅读 :doc:`快速入门 </guide/quickstart>`。

索引和表格
==========

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
