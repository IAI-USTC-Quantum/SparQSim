Quantum Algorithm Development Guide
===================================

This guide introduces how to develop new quantum algorithms in SparQ/QRAM-Simulator.

.. contents::
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Core Paradigm
-------------

SparQ adopts the **Register Level Programming** paradigm, in contrast to the traditional composition of gate-level circuits:

+---------------------+-----------------------------------------+----------------------------------------------------+
| Dimension           | Traditional approach                    | Register Level Programming                         |
+=====================+=========================================+====================================================+
| State storage       | Arrays of qubits                        | Register values stored directly as ``uint64_t``    |
+---------------------+-----------------------------------------+----------------------------------------------------+
| Arithmetic          | Compiled into quantum gate sequences    | Arithmetic performed directly on register values   |
+---------------------+-----------------------------------------+----------------------------------------------------+
| Development model   | Bottom-up (build from gate circuits)    | Top-down (high-level modules first, then refine)   |
+---------------------+-----------------------------------------+----------------------------------------------------+

Development Workflow
--------------------

1. **Environment setup** - build the project and install dependencies

2. **Understand the core components** - get familiar with the code structure

3. **Implement the algorithm** - refer to existing experiments

4. **Test and verify** - ensure correctness

5. **Submit the code** - PR to upstream

.. toctree::
   :maxdepth: 2

   workflow
   templates
   verification
   standards

----

中文版
===

量子算法开发指南

================



本指南介绍如何在 SparQ/QRAM-Simulator 中开发新的量子算法。



.. contents::
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here



核心范式

--------



SparQ 采用 **Register Level Programming**（寄存器级编程）范式，区别于传统的门级电路组合：



+---------------+------------------------------------------------+---------------------------------------------------+

| 维度   | 传统方式                          | Register Level Programming        |

+---------------+------------------------------------------------+---------------------------------------------------+

| 状态存储 | 量子比特数组                      | ``uint64_t`` 直接存储寄存器值    |

+---------------+------------------------------------------------+---------------------------------------------------+

| 算术运算 | 编译成量子门序列                  | 直接对寄存器值进行算术操作        |

+---------------+------------------------------------------------+---------------------------------------------------+

| 开发模式 | 自底向上（从门电路构建）          | 自顶向下（先写高层模块再细化）    |

+---------------+------------------------------------------------+---------------------------------------------------+



开发工作流

---------



1. **环境准备** - 构建项目，安装依赖

2. **理解核心组件** - 熟悉代码结构

3. **实现算法** - 参考现有实验

4. **测试验证** - 确保正确性

5. **提交代码** - PR 到 upstream



.. toctree::

   :maxdepth: 2



   workflow

   templates

   verification

   standards


