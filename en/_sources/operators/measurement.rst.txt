Mid-Circuit Measurement and Seeding
===================================

The measurement operators perform **non-unitary** operations on a :class:`SparseState <pysparq.SparseState>`: Born-rule sampling, active reset, and read-only probability queries. They are the building blocks for mid-circuit measurement, dynamic branching, and classical readout in quantum algorithms. Unlike the :doc:`partial trace operators </operators/partial_trace>`, which collapse registers to extract readout values, ``MeasureZ`` samples an outcome, ``Reset`` prepares a definite classical value, and ``Probability`` queries the outcome distribution without modifying the state.

All sampling operators draw from a **seedable global random engine** (see `Seeding Functions`_), so runs can be made reproducible for testing.

.. contents:: Contents
   :local:
   :class: this-will-duplicate-information-and-it-is-still-useful-here

Overview
--------

.. list-table:: Measurement operators overview
   :header-rows: 1

   * - Operator
     - Operation
     - Modifies the state?
   * - :class:`MeasureZ <pysparq.MeasureZ>`
     - Projective Z-basis measurement (Born-rule sample + collapse)
     - Yes (collapses and renormalizes)
   * - :class:`Reset <pysparq.Reset>`
     - Active reset: measure, then force a definite classical value
     - Yes
   * - :class:`Probability <pysparq.Probability>`
     - Read-only probability of specific register values
     - No

Basic Usage
-----------

.. code-block:: python

   import pysparq as ps

   ps.System.clear()
   ps.System.add_register("q", ps.UnsignedInteger, 2)
   state = ps.SparseState()
   ps.Hadamard_Int("q", 2)(state)   # uniform superposition over |0..3>

   # Reproducible sampling
   ps.set_seed(0)
   outcome, prob = ps.MeasureZ("q")(state)      # sample + collapse
   print(outcome, prob)

   # Active reset to a definite value (defaults to 0)
   measured = ps.Reset("q", 3)(state)           # force q = 3

   # Read-only probability query
   p = ps.Probability("q", 3)(state)            # probability that q == 3
   dist = ps.Probability.distribution(state, "q")  # full outcome distribution

Because these operations are non-unitary, they provide no ``dag()`` and cannot be used inside :ref:`conditional operations <conditional-operations>` blocks that require reversibility.

API Reference
-------------

.. autoclass:: pysparq.MeasureZ
   :members:
   :undoc-members:

.. autoclass:: pysparq.Reset
   :members:
   :undoc-members:

.. autoclass:: pysparq.Probability
   :members:
   :undoc-members:

Seeding Functions
-----------------

The sampling operators (:class:`MeasureZ <pysparq.MeasureZ>`, :class:`Reset <pysparq.Reset>`) and the :doc:`partial trace operators </operators/partial_trace>` share one global random engine. Seed it before sampling to make outcomes reproducible — this is required for deterministic replay and testing of dynamic executors.

.. autofunction:: pysparq.set_seed

.. autofunction:: pysparq.get_seed

.. autofunction:: pysparq.reseed

.. autofunction:: pysparq.time_seed
