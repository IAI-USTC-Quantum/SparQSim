算法库（顶层）
==============

这些头文件把 :doc:`核心算子 </operators/index>` 组合成完整算法（Grover、Shor、哈密顿量模拟）。Python 对应物位于 ``pysparq.algorithms``，并在 :doc:`示例 </guide/examples>` 中演练。

.. note::

   ``SparQ_Algorithm/include/qcnn.h``（量子卷积网络）当前在源码中被
   ``#if false`` 整体禁用，故不出现在本参考中。

Grover 搜索（``SparQ_Algorithm/include/grover.h``）
---------------------------------------------------

.. doxygenfile:: grover.h
   :project: SparQ

Shor 因数分解（``SparQ_Algorithm/include/shor.h``）
----------------------------------------------------

.. doxygenfile:: shor.h
   :project: SparQ

态制备（``SparQ_Algorithm/include/state_preparation.h``）
---------------------------------------------------------

.. doxygenfile:: state_preparation.h
   :project: SparQ

块编码总览（``SparQ_Algorithm/include/block_encoding.h``）
----------------------------------------------------------

.. doxygenfile:: block_encoding.h
   :project: SparQ

哈密顿量模拟（``SparQ_Algorithm/include/hamiltonian_simulation.h``）
---------------------------------------------------------------------

.. doxygenfile:: hamiltonian_simulation.h
   :project: SparQ

无 QRAM 三对角 CKS 行走（``SparQ_Algorithm/include/hamiltonian_simulation_tridiagonal.h``）
------------------------------------------------------------------------------------------

对常对角三对角矩阵，CKS 行走的 QRAM 表 Oracle 被直接算术 Oracle 取代
（``TridiagOracle1/2``、``T_Tridiagonal``、``ChebyshevInverseCoefficients``）；
已在单张 A800 上完成 2^20 维端到端验证
（``docs/verification/v1e-cks-direct-oracle-linsolver.md``）。

.. doxygenfile:: hamiltonian_simulation_tridiagonal.h
   :project: SparQ
