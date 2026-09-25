RIR Interpretive Execution
==========================

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

RIR (Register-level Intermediate Representation) is the register-level
intermediate representation of the quantum programming language QECC.Lang
(the ``pyqecclang`` package). An RIR program is a versioned JSON document:
it declares typed named registers ( ``bits`` / ``uint`` / ``sint`` /
``rational`` with bit widths), QRAM resources, and a module graph — the
module bodies mix gate-level primitives with structured control nodes
( ``Call`` / ``Repeat`` / ``Control`` / ``Adjoint`` ).

PySparQ is the **natural interpreter** for RIR. This is no coincidence:

- RIR itself is **register-level**: it talks about "registers", "views",
  "integer-word addition" and "QRAM loading", rather than physical qubits
  and gate sequences;
- PySparQ's programming model happens to be precisely **register-level
  programming** (Register Level Programming): named typed registers,
  native register arithmetic operators, and native QRAM queries.

Executing RIR therefore requires no extra "lowering" stage:
``pysparq.run_rir`` interprets the JSON document directly on a
``SparseState``, expands the module graph at interpretation time, and maps
register-level operations onto native PySparQ operators whenever the
operands align with whole registers. The interpreter consumes only the
JSON document and does not depend on the ``pyqecclang`` package itself, so
the two implementations can cross-validate each other.

Concept Correspondence
~~~~~~~~~~~~~~~~~~~~~~

RIR concepts correspond one-to-one to native PySparQ capabilities:

.. list-table::
   :header-rows: 1

   * - RIR concept
     - Native PySparQ counterpart
   * - ``bits`` / ``uint`` / ``sint`` / ``rational`` of ``RegType``
     - ``StateStorageType.General`` / ``UnsignedInteger`` /
       ``SignedInteger`` / ``Rational``
   * - ``add_const`` on an entire register
     - ``Add_ConstUInt_InPlace`` (a single native register arithmetic,
       not a gate-chain decomposition)
   * - uncontrolled ``gphase``
     - ``GlobalPhase``
   * - controlled ``gphase``
     - ``Phase_Bool`` plus the remaining control bits
   * - gate broadcast over a view ( ``h`` / ``x`` / ``rx`` / ...)
     - ``Rot_Bool`` applied bit by bit
   * - ``xor`` / ``swap`` over views
     - chains of controlled ``X_Bool``
   * - QRAM resource + ``Load``
     - ``QRAMCircuit_qutrit`` materialization + ``QRAMLoad`` query
   * - ``Control`` (a multi-bit coherent condition)
     - multi-bit conditioning via ``conditioned_by_bit`` (bits valued 0
       are temporarily flipped with X)
   * - ``Call``
     - inlined at interpretation time: argument views are bound to the
       callee through register renaming
   * - ``Repeat``
     - replayed at interpretation time (the serialized JSON stays
       unexpanded)
   * - ``Adjoint``
     - reverse traversal + per-operation inversion (angles negated,
       modular subtraction, self-inverse gates)
   * - ``Module.locals`` private workspace
     - ``AddRegister`` / ``RemoveRegister`` + uncomputation check on exit

Automatic Processing Pipeline
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Execution of ``run_rir`` is organized into five phases, all of them
carried out automatically for the user:

.. code-block:: text

   RIR JSON document
        │  1. Load and structural validation (version, entry, uniqueness of module names)
        ▼
   Resource assembly: entry registers → native named registers; QRAM resources → qutrit-tree materialization
        │  2. Event-stream expansion (Call inlining / Repeat replay / Adjoint reversal / Control accumulation)
        ▼
   Sparse-state execution: whole-register alignment → native operators; register slices → bit-by-bit fallback decomposition
        │  3. Result readout: amplitudes aggregated by the integer values of the entry registers
        ▼
   RIRResult (registers + amplitudes, optional dense statevector)

A few key points:

- **The module graph is expanded at interpretation time.** ``Call``
  executes in place by binding the argument views onto the callee's formal
  registers through register renaming; ``Repeat`` replays its body when
  executed; ``Adjoint`` traverses the body in reverse and inverts every
  operation; the conditions of nested ``Control`` nodes accumulate by
  logical conjunction into a multi-bit condition.
- **Native operators are preferred over decomposition.** For example, when
  the operand of ``add_const`` fully covers a register (start 0 and width
  equal to the declared width), the interpreter calls
  ``Add_ConstUInt_InPlace`` directly instead of decomposing it into a
  multi-gate chain; the uncontrolled global phase goes through
  ``GlobalPhase``, and QRAM loading goes through the native ``QRAMLoad``.
- **View semantics are guaranteed by the fallback path.** When an operand
  is a register slice, the interpreter falls back to bit-by-bit
  decomposition, and modular addition wraps around within the **view
  width**, consistent with the RIR specification.
- **Everything is cleaned up automatically after execution.** ``run_rir``
  guarantees ``System.clear()`` on exit; if the global register table is
  not empty before the call, an error is raised immediately.

Basic Usage
-----------

The entry API is provided by ``pysparq.rir`` and is also exported in the
top-level namespace:

.. code-block:: python

   import pysparq as ps

   result = ps.run_rir(document, memory)          # use the default budgets
   result = ps.run_rir(document, memory, max_steps=500_000, max_states=4_096)
   result = ps.run_rir_file("program.rir.json", memory={"rom": [1, 2, 4, 7]})
   document = ps.load_rir("program.rir.json")   # dict / JSON string / file path

``document`` may be an already-decoded dict, a JSON string, or a file
path; ``memory`` binds contents to the QRAM resource names declared by the
entry module one by one, and each resource accepts either a full word
sequence or a sparse ``{address: word}`` mapping (cells left out are
zero).

The return value is an ``RIRResult``:

.. code-block:: python

   result.registers      # (("a", 1), ("b", 1)) —— entry registers (name, width), in declaration order
   result.amplitudes     # {(0, 0): 0.707..., (1, 1): 0.707...}
                         # the keys are tuples of the registers' integer values (in declaration order)
   result.statevector()  # dense little-endian state vector, index = value(r0) + (value(r1) << width(r0)) + ...

In RIR's JSON encoding every record is an object carrying a ``tag``
field, and **all fields must be written out** (including ``null`` and
empty lists). For example, an H-broadcast instruction acting on a
two-bit integer register:

.. code-block:: json

   {
     "tag": "Primitive",
     "op": "h",
     "operands": [{
       "tag": "Ref",
       "parts": [{"tag": "Span", "register": "address", "start": 0, "width": 2}],
       "type": {"tag": "RegType", "kind": "uint", "width": 2}
     }],
     "angle": null,
     "value": null
   }

Examples
--------

The examples below share a set of helper functions that build RIR nodes
(written the same way as in ``PySparQ/test/test_rir.py``); they are plain
dicts and depend on no extra packages:

.. code-block:: python

   def ref(register, start, width, kind="uint"):
       return {
           "tag": "Ref",
           "parts": [{"tag": "Span", "register": register, "start": start, "width": width}],
           "type": {"tag": "RegType", "kind": kind, "width": width},
       }

   def prim(op, operands, angle=None, value=None):
       return {"tag": "Primitive", "op": op, "operands": operands, "angle": angle, "value": value}

   def register(name, width, kind="uint"):
       return {"tag": "Register", "name": name, "type": {"tag": "RegType", "kind": kind, "width": width}}

   def module(name, registers, body, locals_=None, resources=None):
       return {
           "tag": "Module", "name": name, "registers": registers,
           "locals": locals_ or [], "resources": resources or [],
           "body": body, "attributes": [],
       }

   def program(entry, modules):
       return {"tag": "Program", "entry": entry, "modules": modules, "version": "0.3"}

   def call(module_name, arguments, resources=()):
       return {"tag": "Call", "module": module_name, "arguments": arguments, "resources": list(resources)}

Example 1: Gate-level program and coherent control (Bell state)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An ``h`` broadcast followed by a ``Control`` block: the interpreter maps
``h`` onto ``Rot_Bool`` bit by bit, and turns the ``Control`` (a quantum
condition on the value 1) into a multi-bit coherent condition rather than
a classical branch.

.. code-block:: python

   import pysparq as ps

   bell = program("main", [module(
       "main",
       [register("a", 1, "bits"), register("b", 1, "bits")],
       [
           prim("h", [ref("a", 0, 1, "bits")]),
           {"tag": "Control", "register": ref("a", 0, 1, "bits"), "value": 1,
            "body": [prim("x", [ref("b", 0, 1, "bits")])]},
       ],
   )])

   result = ps.run_rir(bell)
   print(result.registers)   # (('a', 1), ('b', 1))
   print(result.amplitudes)  # {(0, 0): (0.7071067811865476+0j), (1, 1): (0.7071067811865476+0j)}

The condition value of ``Control`` may be any unsigned bit pattern; bits
whose value is 0 are temporarily flipped with an X gate by the interpreter
and flipped back once the body has executed, staying coherent throughout.

Example 2: The module graph (Call / Repeat / Adjoint) and native arithmetic mapping
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Define a module ``inc`` that adds 1 to an integer register; the entry
module calls it three times and then applies one adjoint. ``Call`` is
inlined automatically, ``Repeat`` is replayed automatically, and
``Adjoint`` reverses automatically, turning ``add_const 1`` into a modular
subtraction of 1:

.. code-block:: python

   inc = module("inc", [register("x", 4)], [prim("add_const", [ref("x", 0, 4)], value=1)])
   main = module(
       "main",
       [register("x", 4)],
       [
           {"tag": "Repeat", "count": 3, "body": [call("inc", [ref("x", 0, 4)])]},
           {"tag": "Adjoint", "body": [call("inc", [ref("x", 0, 4)])]},
       ],
   )

   result = ps.run_rir(program("main", [main, inc]))
   print(result.amplitudes)  # {(2,): (1+0j)}   —— add 1 three times, then the adjoint subtracts 1

This is where the core benefit of the "natural interpreter" shows: the
operand of ``add_const`` fully covers register ``x``, so the interpreter
applies a single native ``Add_ConstUInt_InPlace`` and skips the gate-level
decomposition; for the adjoint, the constant is negated modulo
(``-1 mod 2^4 = 15``), which is again a single native arithmetic.

Example 3: Register views and module arguments
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An RIR ``Ref`` may refer to register slices. The formal registers of the
callee are bound to whatever view the caller passes in; the interpreter
does not assume that the arguments are contiguous physical qubits:

.. code-block:: python

   bump = module("bump", [register("lo", 2)], [prim("add_const", [ref("lo", 0, 2)], value=1)])
   main = module(
       "main",
       [register("x", 4)],
       [
           prim("x", [ref("x", 2, 2)]),              # set the upper two bits to 1 → x = 0b1100
           call("bump", [ref("x", 0, 2)]),           # pass only the lower two bits to the callee
       ],
   )

   result = ps.run_rir(program("main", [main, bump]))
   print(result.amplitudes)  # {(13,): (1+0j)}   —— lower two bits 00 + 1 → x = 0b1101

When ``add_const`` acts on a slice, the interpreter falls back to
bit-by-bit decomposition, and the addition wraps around within the
**view width**. For example, adding 1 to the lower two bits of
``x = 0b0011`` wraps the view value 3 + 1 around to 0, yielding
``x = 0``:

.. code-block:: python

   wrap = module(
       "main",
       [register("x", 4)],
       [
           prim("x", [ref("x", 0, 1)]),
           prim("x", [ref("x", 1, 1)]),              # x = 0b0011
           prim("add_const", [ref("x", 0, 2)], value=1),
       ],
   )

   result = ps.run_rir(program("main", [wrap]))
   print(result.amplitudes)  # {(0,): (1+0j)}

Example 4: QRAM loading
~~~~~~~~~~~~~~~~~~~~~~~

The entry module declares a QRAM resource (address width 2, data width 3)
and performs the load after a Hadamard broadcast over the address
register. The memory contents are not part of the program JSON; they are
bound at execution time. The interpreter does it all automatically: the
QRAM resource is materialized as a ``QRAMCircuit_qutrit``, and ``Load``
copies into a temporary register through reversible XOR, then performs the
native ``QRAMLoad`` query and fully uncomputes it:

.. code-block:: python

   values = {
       "tag": "Resource", "name": "values",
       "type": {"tag": "QRAM", "address_width": 2, "data_width": 3},
   }
   main = module(
       "main",
       [register("address", 2), register("data", 3)],
       [
           prim("h", [ref("address", 0, 2)]),
           {"tag": "Load", "resource": "values",
            "address": ref("address", 0, 2), "data": ref("data", 0, 3)},
       ],
       resources=[values],
   )

   result = ps.run_rir(program("main", [main]), memory={"values": [1, 2, 4, 7]})
   print(result.amplitudes)
   # the amplitude of each branch ≈ 0.5 (up to floating-point tail differences):
   # {(0, 1): (0.5+0j), (1, 2): (0.5+0j), (2, 4): (0.5+0j), (3, 7): (0.5+0j)}

Each of the four address branches picks up the corresponding word with
probability 1/2. ``memory`` also accepts sparse mappings; for example,
``{"values": {2: 5}}`` writes only a single cell and leaves the others
zero.

When a dense state vector is needed, ``statevector()`` exports it
following RIR's index convention (little-endian concatenation in
declaration order):

.. code-block:: python

   vector = result.statevector()          # 32-dimensional: address(2 bits) + data(3 bits)
   print(vector[(2 << 2) | 1])            # ≈ 0.5 —— address=1, data=2
   print(vector[(7 << 2) | 3])            # ≈ 0.5 —— address=3, data=7

Execution Budgets and Safety Properties
---------------------------------------

The interpreter enforces two kinds of budgets; exceeding one raises an
``RIRError`` instead of truncating silently:

- ``max_steps`` (default ``1_000_000``): the module-graph expansion
  budget. A ``Repeat`` is charged its body cost multiplied by the repeat
  count, and a ``Call`` is charged the callee's cost; the budget is
  checked **before** taking over the global register table.
- ``max_states`` (default ``65_536``): a budget on the number of
  sparse-state basis states, checked after every operator application.

There are also structural checks:

- materialization is refused when a single QRAM's ``address_width``
  exceeds 20 (i.e. more than 2^20 cells);
- ``memory`` must correspond one-to-one with the QRAM resources declared
  by the entry module, and both addresses and words are range-checked;
- a ``Module.locals`` private workspace must be clean again when the
  module exits, otherwise "local register not uncomputed" is raised —
  this is the runtime obligation that the RIR specification delegates to
  the simulator;
- the global register table must be empty before the call (``run_rir``
  performs ``System.clear()`` automatically on exit).

Cross-validation with pyqecclang
--------------------------------

On the ``pyqecclang`` side there are two independent execution paths:

- ``run_pysparq``: an event adapter that interprets the Python-level IR
  objects directly;
- ``run_pysparq_rir``: serializes the program to JSON and hands it over
  to ``pysparq.run_rir``.

The interpreter in this repository (``PySparQ/pysparq/rir.py``)
deliberately consumes only JSON documents and does not import
``pyqecclang``, so the two paths together with the OriginIR-ext export
can serve as three-way cross-checking baselines for one another.
Regression tests live in ``PySparQ/test/test_rir.py``.

API Reference
-------------

For detailed API documentation, see :doc:`../api/rir`.

----

中文版
===

RIR 解释执行
============

.. contents:: 目录
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

概述
----

RIR（Register-level Intermediate Representation）是量子编程语言
QECC.Lang（ ``pyqecclang`` 包）的寄存器级中间表示。一个 RIR 程序是带版本的
JSON 文档：它声明带类型的命名寄存器（ ``bits`` / ``uint`` / ``sint`` /
``rational`` 与位宽）、QRAM 资源，以及一个模块图——模块体由门级基元与结构化
控制节点（ ``Call`` / ``Repeat`` / ``Control`` / ``Adjoint`` ）混合组成。

PySparQ 是 RIR 的**天然解释器**。这不是巧合：

- RIR 本身就是**寄存器级**的：它谈论的是"寄存器"、"视图"、"整数字加法"和
  "QRAM 加载"，而不是物理量子位与门序列；
- PySparQ 的编程模型恰好是**寄存器级编程**（Register Level Programming）：
  命名类型化寄存器、原生寄存器算术算子与原生 QRAM 查询。

因此 RIR 的执行不需要额外的"降低"阶段： ``pysparq.run_rir`` 把 JSON 文档
直接解释在 ``SparseState`` 上，在解释期展开模块图，并在操作数与整个寄存器
对齐时把寄存器级操作映射为 PySparQ 原生算子。解释器只消费 JSON 文档，不依赖
``pyqecclang`` 包本身，因此两套实现可以互相交叉验证。

概念对应
~~~~~~~~

RIR 概念与 PySparQ 原生能力一一对应：

.. list-table::
   :header-rows: 1

   * - RIR 概念
     - PySparQ 原生对应
   * - ``RegType`` 的 ``bits`` / ``uint`` / ``sint`` / ``rational``
     - ``StateStorageType.General`` / ``UnsignedInteger`` /
       ``SignedInteger`` / ``Rational``
   * - 整个寄存器上的 ``add_const``
     - ``Add_ConstUInt_InPlace`` （一条原生寄存器算术，而非门链分解）
   * - 非受控 ``gphase``
     - ``GlobalPhase``
   * - 受控 ``gphase``
     - ``Phase_Bool`` 加剩余控制位
   * - 视图上的门广播（ ``h`` / ``x`` / ``rx`` / ...）
     - ``Rot_Bool`` 逐位施加
   * - ``xor`` / ``swap`` 视图
     - 受控 ``X_Bool`` 链
   * - QRAM 资源 + ``Load``
     - ``QRAMCircuit_qutrit`` 物化 + ``QRAMLoad`` 查询
   * - ``Control`` （多比特相干条件）
     - ``conditioned_by_bit`` 多位条件（值为 0 的位临时用 X 翻转）
   * - ``Call``
     - 解释期内联：实参视图经寄存器重命名绑定到被调模块
   * - ``Repeat``
     - 解释期重放执行（序列化 JSON 不展开）
   * - ``Adjoint``
     - 反向遍历 + 逐操作取逆（角度取负、模减法、自逆门）
   * - ``Module.locals`` 私有工作区
     - ``AddRegister`` / ``RemoveRegister`` + 退出时的复净检查

自动处理流程
~~~~~~~~~~~~

``run_rir`` 的执行分为五个阶段，全部对用户自动完成：

.. code-block:: text

   RIR JSON 文档
        │  1. 载入与结构校验（版本、入口、模块名唯一性）
        ▼
   资源装配：入口寄存器 → 原生命名寄存器；QRAM 资源 → qutrit 树物化
        │  2. 事件流展开（Call 内联 / Repeat 重放 / Adjoint 反向 / Control 累积）
        ▼
   稀疏态执行：整寄存器对齐 → 原生算子；寄存器切片 → 逐位回退分解
        │  3. 结果读出：按入口寄存器的整数值聚合振幅
        ▼
   RIRResult（registers + amplitudes，可选稠密 statevector）

几个关键点：

- **模块图在解释期展开**。 ``Call`` 通过寄存器重命名把实参视图绑定到被调模块
  的形式寄存器上原地执行； ``Repeat`` 在执行时重放其主体； ``Adjoint`` 反向
  遍历主体并对每条操作取逆；嵌套 ``Control`` 的条件按逻辑合取累积成多比特
  条件。
- **能用原生算子就不分解**。例如 ``add_const`` 的操作数完整覆盖一个寄存器
  （起点 0 且宽度等于声明宽度）时，解释器直接调用
  ``Add_ConstUInt_InPlace`` ，而不是把它分解成多门链；非受控全局相位走
  ``GlobalPhase`` ，QRAM 加载走原生 ``QRAMLoad`` 。
- **视图语义由回退路径保证**。当操作数是寄存器切片时，解释器回退到逐位
  分解，模加法在**视图宽度**内回绕，与 RIR 规范一致。
- **执行完自动清理**。 ``run_rir`` 在退出时保证 ``System.clear()`` ；调用前
  若全局寄存器表非空则直接报错。

基本用法
--------

入口 API 由 ``pysparq.rir`` 提供，并已在顶层命名空间导出：

.. code-block:: python

   import pysparq as ps

   result = ps.run_rir(document, memory)          # 使用默认预算
   result = ps.run_rir(document, memory, max_steps=500_000, max_states=4_096)
   result = ps.run_rir_file("program.rir.json", memory={"rom": [1, 2, 4, 7]})
   document = ps.load_rir("program.rir.json")   # dict / JSON 字符串 / 文件路径

``document`` 可以是已解码的 dict、JSON 字符串或文件路径； ``memory`` 按
入口模块声明的 QRAM 资源名逐一绑定内容，每个资源接受完整字序列或稀疏的
``{地址: 字}`` 映射（未给出的单元为零）。

返回值是 ``RIRResult`` ：

.. code-block:: python

   result.registers      # (("a", 1), ("b", 1)) —— 入口寄存器 (名字, 位宽)，按声明序
   result.amplitudes     # {(0, 0): 0.707..., (1, 1): 0.707...}
                         # 键是各寄存器整数值组成的元组（按声明序）
   result.statevector()  # 稠密小端状态向量，索引 = value(r0) + (value(r1) << width(r0)) + ...

RIR 的 JSON 编码里每条记录都是带 ``tag`` 字段的对象，且**所有字段都要写出**
（包括 ``null`` 与空列表）。例如一条作用于两位整数寄存器的 H 广播指令：

.. code-block:: json

   {
     "tag": "Primitive",
     "op": "h",
     "operands": [{
       "tag": "Ref",
       "parts": [{"tag": "Span", "register": "address", "start": 0, "width": 2}],
       "type": {"tag": "RegType", "kind": "uint", "width": 2}
     }],
     "angle": null,
     "value": null
   }

案例
----

下面的案例共用一组构造 RIR 节点的辅助函数（与 ``PySparQ/test/test_rir.py``
中的写法一致），它们只是普通 dict，不依赖任何额外包：

.. code-block:: python

   def ref(register, start, width, kind="uint"):
       return {
           "tag": "Ref",
           "parts": [{"tag": "Span", "register": register, "start": start, "width": width}],
           "type": {"tag": "RegType", "kind": kind, "width": width},
       }

   def prim(op, operands, angle=None, value=None):
       return {"tag": "Primitive", "op": op, "operands": operands, "angle": angle, "value": value}

   def register(name, width, kind="uint"):
       return {"tag": "Register", "name": name, "type": {"tag": "RegType", "kind": kind, "width": width}}

   def module(name, registers, body, locals_=None, resources=None):
       return {
           "tag": "Module", "name": name, "registers": registers,
           "locals": locals_ or [], "resources": resources or [],
           "body": body, "attributes": [],
       }

   def program(entry, modules):
       return {"tag": "Program", "entry": entry, "modules": modules, "version": "0.3"}

   def call(module_name, arguments, resources=()):
       return {"tag": "Call", "module": module_name, "arguments": arguments, "resources": list(resources)}

案例 1：门级程序与相干控制（Bell 态）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

一条 ``h`` 广播加一段 ``Control`` ：解释器把 ``h`` 逐位映射为 ``Rot_Bool`` ，
把 ``Control`` （值为 1 的量子条件）转化为多比特相干条件，而不是经典分支。

.. code-block:: python

   import pysparq as ps

   bell = program("main", [module(
       "main",
       [register("a", 1, "bits"), register("b", 1, "bits")],
       [
           prim("h", [ref("a", 0, 1, "bits")]),
           {"tag": "Control", "register": ref("a", 0, 1, "bits"), "value": 1,
            "body": [prim("x", [ref("b", 0, 1, "bits")])]},
       ],
   )])

   result = ps.run_rir(bell)
   print(result.registers)   # (('a', 1), ('b', 1))
   print(result.amplitudes)  # {(0, 0): (0.7071067811865476+0j), (1, 1): (0.7071067811865476+0j)}

``Control`` 的条件值可以是任意无符号位模式；值为 0 的位会被解释器用 X 门
临时翻转、执行完主体再翻回，全程保持相干。

案例 2：模块图（Call / Repeat / Adjoint）与原生算术映射
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

定义一个把整数字寄存器加 1 的模块 ``inc`` ，入口模块调用它三次再做一次伴随。
``Call`` 被自动内联， ``Repeat`` 被自动重放， ``Adjoint`` 自动反向并把
``add_const 1`` 变成模减 1：

.. code-block:: python

   inc = module("inc", [register("x", 4)], [prim("add_const", [ref("x", 0, 4)], value=1)])
   main = module(
       "main",
       [register("x", 4)],
       [
           {"tag": "Repeat", "count": 3, "body": [call("inc", [ref("x", 0, 4)])]},
           {"tag": "Adjoint", "body": [call("inc", [ref("x", 0, 4)])]},
       ],
   )

   result = ps.run_rir(program("main", [main, inc]))
   print(result.amplitudes)  # {(2,): (1+0j)}   —— 3 次加 1 后再伴随减 1

这里体现了"天然解释器"的核心收益： ``add_const`` 的操作数完整覆盖寄存器
``x`` ，解释器直接施加一条原生 ``Add_ConstUInt_InPlace`` ，跳过门级分解；
伴随时把常数取模取负（ ``-1 mod 2^4 = 15`` ），仍是一条原生算术。

案例 3：寄存器视图与模块传参
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

RIR 的 ``Ref`` 可以引用寄存器切片。被调模块的形式寄存器绑定到调用方给出的
任意视图上，解释器不假定实参是连续的物理量子位：

.. code-block:: python

   bump = module("bump", [register("lo", 2)], [prim("add_const", [ref("lo", 0, 2)], value=1)])
   main = module(
       "main",
       [register("x", 4)],
       [
           prim("x", [ref("x", 2, 2)]),              # 高两位置 1 → x = 0b1100
           call("bump", [ref("x", 0, 2)]),           # 只把低两位传给被调模块
       ],
   )

   result = ps.run_rir(program("main", [main, bump]))
   print(result.amplitudes)  # {(13,): (1+0j)}   —— 低两位 00 + 1 → x = 0b1101

当 ``add_const`` 作用在切片上时，解释器回退到逐位分解，加法在**视图宽度**
内回绕。例如对 ``x = 0b0011`` 的低两位加 1，视图值 3 + 1 回绕为 0，得到
``x = 0`` ：

.. code-block:: python

   wrap = module(
       "main",
       [register("x", 4)],
       [
           prim("x", [ref("x", 0, 1)]),
           prim("x", [ref("x", 1, 1)]),              # x = 0b0011
           prim("add_const", [ref("x", 0, 2)], value=1),
       ],
   )

   result = ps.run_rir(program("main", [wrap]))
   print(result.amplitudes)  # {(0,): (1+0j)}

案例 4：QRAM 加载
~~~~~~~~~~~~~~~~~

入口模块声明 QRAM 资源（地址宽 2、数据宽 3），对地址寄存器做 Hadamard 广播
后加载。内存内容不属于程序 JSON，而是执行时绑定。解释器自动完成：QRAM 资源
物化为 ``QRAMCircuit_qutrit`` ， ``Load`` 通过可逆 XOR 复制到临时寄存器后执行
原生 ``QRAMLoad`` 查询并完整反算：

.. code-block:: python

   values = {
       "tag": "Resource", "name": "values",
       "type": {"tag": "QRAM", "address_width": 2, "data_width": 3},
   }
   main = module(
       "main",
       [register("address", 2), register("data", 3)],
       [
           prim("h", [ref("address", 0, 2)]),
           {"tag": "Load", "resource": "values",
            "address": ref("address", 0, 2), "data": ref("data", 0, 3)},
       ],
       resources=[values],
   )

   result = ps.run_rir(program("main", [main]), memory={"values": [1, 2, 4, 7]})
   print(result.amplitudes)
   # 各分支振幅 ≈ 0.5（浮点尾差略）：
   # {(0, 1): (0.5+0j), (1, 2): (0.5+0j), (2, 4): (0.5+0j), (3, 7): (0.5+0j)}

四个地址分支各以 1/2 概率取到对应的字。 ``memory`` 也接受稀疏映射，例如
``{"values": {2: 5}}`` 只写一个单元，其余单元为零。

需要稠密状态向量时， ``statevector()`` 按 RIR 的索引约定（声明序小端拼接）
导出：

.. code-block:: python

   vector = result.statevector()          # 32 维：address(2 位) + data(3 位)
   print(vector[(2 << 2) | 1])            # ≈ 0.5 —— address=1, data=2
   print(vector[(7 << 2) | 3])            # ≈ 0.5 —— address=3, data=7

执行预算与安全属性
------------------

解释器施加两类预算，超限抛出 ``RIRError`` 而不是静默截断：

- ``max_steps`` （默认 ``1_000_000`` ）：模块图展开预算。 ``Repeat`` 的代价按
  次数乘主体代价计， ``Call`` 按被调模块代价计；预算在接管全局寄存器表**之前**
  检查。
- ``max_states`` （默认 ``65_536`` ）：稀疏态基矢数量预算，每次施加算子后检查。

另有结构性检查：

- 单个 QRAM 的 ``address_width`` 超过 20（即超过 2^20 个单元）时拒绝物化；
- ``memory`` 必须与入口模块声明的 QRAM 资源一一对应，地址与字都做范围检查；
- ``Module.locals`` 私有工作区在模块退出时必须复净，否则报
  "local register not uncomputed"——这是 RIR 规范交给模拟器的运行期义务；
- 调用前全局寄存器表必须为空（ ``run_rir`` 退出时自动 ``System.clear()`` ）。

与 pyqecclang 的交叉验证
------------------------

``pyqecclang`` 侧有两条独立的执行路径：

- ``run_pysparq`` ：事件适配器，直接解释 Python 层的 IR 对象；
- ``run_pysparq_rir`` ：把程序序列化为 JSON 后交给 ``pysparq.run_rir`` 。

本仓库的解释器（ ``PySparQ/pysparq/rir.py`` ）刻意只消费 JSON 文档、不导入
``pyqecclang`` ，因此两条路径加上 OriginIR-ext 导出可以三方互为对拍基准。
回归测试见 ``PySparQ/test/test_rir.py`` 。

API 参考
--------

详细 API 文档请参考 :doc:`../api/rir`。
