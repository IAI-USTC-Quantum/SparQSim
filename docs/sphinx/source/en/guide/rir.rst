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
  programming** (Register Level Programming, see
  :doc:`core concepts </guide/core_concepts/index>`): named typed registers,
  native register arithmetic operators, and native QRAM queries.

Executing RIR therefore requires no extra "lowering" stage:
``pysparq.run_rir`` interprets the JSON document directly on a
:class:`SparseState <pysparq.SparseState>`, expands the module graph at interpretation time, and maps
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
     - :doc:`StateStorageType.General / UnsignedInteger / SignedInteger / Rational </guide/core_concepts/register_types>`
   * - ``add_const`` on an entire register
     - :class:`Add_ConstUInt_InPlace <pysparq.Add_ConstUInt_InPlace>` (a single native register arithmetic,
       not a gate-chain decomposition)
   * - uncontrolled ``gphase``
     - :class:`GlobalPhase <pysparq.GlobalPhase>`
   * - controlled ``gphase``
     - :class:`Phase_Bool <pysparq.Phase_Bool>` plus the remaining control bits
   * - gate broadcast over a view ( ``h`` / ``x`` / ``rx`` / ...)
     - :class:`Rot_Bool <pysparq.Rot_Bool>` applied bit by bit
   * - ``xor`` / ``swap`` over views
     - chains of controlled :class:`X_Bool <pysparq.X_Bool>`
   * - QRAM resource + ``Load``
     - :class:`QRAMCircuit_qutrit <pysparq.QRAMCircuit_qutrit>` materialization + :class:`QRAMLoad <pysparq.QRAMLoad>` query
       (see :doc:`QRAM operators </operators/qram_ops>`)
   * - ``Control`` (a multi-bit coherent condition)
     - multi-bit conditioning via :ref:`conditioned_by_bit <conditional-operations>` (bits valued 0
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
     - :doc:`AddRegister / RemoveRegister </guide/core_concepts/register_management>` + uncomputation check on exit

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
  :class:`Add_ConstUInt_InPlace <pysparq.Add_ConstUInt_InPlace>` directly instead of decomposing it into a
  multi-gate chain; the uncontrolled global phase goes through
  :class:`GlobalPhase <pysparq.GlobalPhase>`, and QRAM loading goes through the native :class:`QRAMLoad <pysparq.QRAMLoad>`.
- **View semantics are guaranteed by the fallback path.** When an operand
  is a register slice, the interpreter falls back to bit-by-bit
  decomposition, and modular addition wraps around within the **view
  width**, consistent with the RIR specification.
- **Everything is cleaned up automatically after execution.** ``run_rir``
  guarantees :meth:`System.clear() <pysparq.System.clear>` on exit; if the global register table is
  not empty before the call, an error is raised immediately.

Basic Usage
-----------

The entry API is provided by :mod:`pysparq.rir` and is also exported in the
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

The return value is an :class:`RIRResult <pysparq.rir.RIRResult>`:

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
QRAM resource is materialized as a :class:`QRAMCircuit_qutrit <pysparq.QRAMCircuit_qutrit>`, and ``Load``
copies into a temporary register through reversible XOR, then performs the
native :class:`QRAMLoad <pysparq.QRAMLoad>` query and fully uncomputes it:

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
:class:`RIRError <pysparq.rir.RIRError>` instead of truncating silently:

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

The interpreter in this repository (:mod:`pysparq.rir`)
deliberately consumes only JSON documents and does not import
``pyqecclang``, so the two paths together with the OriginIR-ext export
can serve as three-way cross-checking baselines for one another.
Regression tests live in ``PySparQ/test/test_rir.py``.

API Reference
-------------

For detailed API documentation, see :doc:`../api/rir`.
