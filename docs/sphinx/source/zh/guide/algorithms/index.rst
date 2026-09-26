算法指南
========

SparQ 的算法栈由彼此镜像的两层组成：

- **C++ 层**（``SparQ_Algorithm/``）：由 :doc:`核心算子 </operators/index>` 组合而成的生产实现，
  按头文件逐个呈现在 :doc:`C++ API 参考 </cpp_api/algorithms>` 中。
- **Python 层**（``pysparq.algorithms``）：用于快速原型开发与交叉验证的纯 Python 对应实现，
  自动收录在下方的 API 树中。

下面的每个算法都列出了它的 Python 入口、所对应的 C++ 头文件以及运行示例的位置。
两层之间的逐行对应关系见 :doc:`算法移植指南 <porting>`。

.. toctree::
   :maxdepth: 1

   porting

Grover 搜索
-----------

QRAM oracle 驱动的 Grover 搜索，含振幅放大与量子计数。

- Python：:class:`GroverOperator <PySparQ.pysparq.algorithms.grover.GroverOperator>`、:class:`GroverOracle <PySparQ.pysparq.algorithms.grover.GroverOracle>`、:func:`grover_search() <PySparQ.pysparq.algorithms.grover.grover_search>`、:func:`grover_count() <PySparQ.pysparq.algorithms.grover.grover_count>`
- C++：:doc:`算法库 </cpp_api/algorithms>` 中的 ``grover.h``
- 构建块：扩散步骤即 :doc:`相位与反射算子 </operators/phase_ops>` 中的反射原语；oracle 按条件标记状态（:ref:`条件执行 <conditional-operations>`）
- 试用：:doc:`动态算子扩展 </guide/dynamic_operators>` 中的自定义 oracle 示例

Shor 因数分解
-------------

基于模乘法的 Shor 因数分解（标准版与半经典版）。

- Python：:class:`Shor <PySparQ.pysparq.algorithms.shor.Shor>`、:class:`ModMul <PySparQ.pysparq.algorithms.shor.ModMul>`
- C++：:doc:`算法库 </cpp_api/algorithms>` 中的 ``shor.h``
- 构建块：:doc:`算术算子 </operators/arithmetic>` （模加/模乘）与 :doc:`QFT </operators/qft>`

量子线性方程组（QDA）
---------------------

基于块编码与态制备的离散绝热量子线性方程组求解器。

- Python：:func:`qda_solve() <PySparQ.pysparq.algorithms.qda_solver.qda_solve>`、:func:`qda_solve_tridiagonal() <PySparQ.pysparq.algorithms.qda_solver.qda_solve_tridiagonal>`、:func:`qda_solve_via_qram() <PySparQ.pysparq.algorithms.qda_solver.qda_solve_via_qram>`，以及编排类 :class:`WalkS <PySparQ.pysparq.algorithms.qda_solver.WalkS>`、:class:`LCU <PySparQ.pysparq.algorithms.qda_solver.LCU>`、:class:`BlockEncoding <PySparQ.pysparq.algorithms.qda_solver.BlockEncoding>` 与 :class:`StatePreparation <PySparQ.pysparq.algorithms.qda_solver.StatePreparation>`
- C++：:doc:`离散绝热（QDA） </cpp_api/qda>`
- 构建块：块编码（见下文）；步长/条件数等超参数在 :doc:`算子 </guide/core_concepts/operators>` 中介绍

哈密顿量模拟（CKS）
-------------------

通过量子行走 / LCU / 稀疏矩阵 oracle 进行哈密顿量模拟，以 CKS 实验为参考实现。

- Python：``pysparq.algorithms.cks_solver`` 中的 :class:`TOperator <PySparQ.pysparq.algorithms.cks_solver.TOperator>` 与 :class:`QuantumWalkNSteps <PySparQ.pysparq.algorithms.cks_solver.QuantumWalkNSteps>`
- C++：:doc:`算法库 </cpp_api/algorithms>` 中的 ``hamiltonian_simulation.h`` （``T`` 态制备算子与 ``SparseMatrixOracle1`` 位于此处）
- 相关：:doc:`动态算子扩展 </guide/dynamic_operators>` 中的量子行走动态算子示例

块编码
------

将经典矩阵编码进更大的酉矩阵中——上述线性方程组与哈密顿量模拟算法的核心子程序。

- Python：``pysparq.algorithms.block_encoding`` 中的 :class:`BlockEncodingTridiagonal <PySparQ.pysparq.algorithms.block_encoding.BlockEncodingTridiagonal>` 与 :class:`BlockEncodingViaQRAM <PySparQ.pysparq.algorithms.block_encoding.BlockEncodingViaQRAM>`
- C++：:doc:`块编码 </cpp_api/block_encoding>`
- 试用：示例指南中的 :doc:`示例 4 </guide/examples>` （三对角矩阵完整走查，含可运行代码），以及 :doc:`算子使用示例 notebook </notebooks/03_operator_examples>` 中的 Block Encoding 一节

态制备
------

基于 QRAM 从 ``|0⟩`` 制备任意目标态。

- Python：:class:`StatePrepViaQRAM <PySparQ.pysparq.algorithms.state_preparation.StatePrepViaQRAM>` 与 :class:`StatePreparation <PySparQ.pysparq.algorithms.state_preparation.StatePreparation>`
- C++：:doc:`算法库 </cpp_api/algorithms>` 中的 ``state_preparation.h``
- 构建块：:doc:`旋转与态制备算子 </operators/rot_state_prep>` 与 :doc:`QRAM 算子 </operators/qram_ops>`
