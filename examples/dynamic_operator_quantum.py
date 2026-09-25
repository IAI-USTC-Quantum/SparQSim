#!/usr/bin/env python3
"""
Dynamic Operator Quantum Circuit Example

Demonstrates how to define and compile dynamic operators for quantum computing.

Before running, make sure:
1. QRAM-Simulator has been built correctly
2. The PySparQ module can be imported
3. A g++ compiler is available

Usage:
    python examples/dynamic_operator_quantum.py

Note:
    This example demonstrates operator compilation and instance creation.
    Applying the operators to actual quantum states requires ABI compatibility.
"""

import sys
import os
import math

# Add the project root directory to the path
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(project_root, "PySparQ"))  # Source-tree import; uses site-packages when installed

print("=" * 70)
print("PySparQ Dynamic Operator Quantum Circuit Example")
print("=" * 70)
print()

# Import PySparQ
try:
    import pysparq as ps
    from pysparq.dynamic_operator import compile_operator, get_cache_info, clear_cache
    print("Successfully imported pysparq and compile_operator")
except ImportError as e:
    print(f"Import failed: {e}")
    print("Make sure PySparQ is installed correctly: pip install .")
    sys.exit(1)

print()

# Clear the cache to demonstrate the full workflow
print("Clearing compilation cache...")
clear_cache()
print()

# ============================================================================
# Example 1: Controlled phase gate (CU(1))
# ============================================================================
print("-" * 70)
print("Example 1: Controlled phase gate (CU(1))")
print("-" * 70)
print()
print("The controlled phase gate applies a phase rotation when both the control and target qubits are |1>.")
print("It is one of the commonly used controlled gates in quantum computing.")
print()

controlled_phase_code = """
class ControlledPhase : public BaseOperator {
    size_t control_reg;
    size_t target_reg;
    double phase;
public:
    ControlledPhase(size_t c, size_t t, double theta)
        : control_reg(c), target_reg(t), phase(theta) {}

    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            // Apply the phase when both the control and target qubits are |1>
            if (s.get(control_reg).value && s.get(target_reg).value) {
                s.amplitude *= std::exp(std::complex<double>(0, phase));
            }
        }
    }

    void dag(std::vector<System>& state) const override {
        for (auto& s : state) {
            if (s.get(control_reg).value && s.get(target_reg).value) {
                s.amplitude *= std::exp(std::complex<double>(0, -phase));
            }
        }
    }
};
"""

print("Compiling the controlled phase gate...")
try:
    ControlledPhase = compile_operator(
        name="ControlledPhase",
        cpp_code=controlled_phase_code,
        base_class="BaseOperator",
        constructor_args=[
            ("size_t", "control_reg"),
            ("size_t", "target_reg"),
            ("double", "phase")
        ],
        verbose=True
    )
    print()
    print("Compilation successful!")
    print(f"  Class name: {ControlledPhase.__name__}")
    print(f"  Base class: {ControlledPhase._base_class}")

    # Create an instance
    op = ControlledPhase(control_reg=0, target_reg=1, phase=math.pi/4)
    print(f"  Instance: {repr(op)}")
    print()
except Exception as e:
    print(f"Compilation failed: {e}")
    print()
    ControlledPhase = None

# ============================================================================
# Example 2: Multi-register entanglement gate
# ============================================================================
print("-" * 70)
print("Example 2: Multi-register entanglement gate")
print("-" * 70)
print()
print("A triple-XOR entanglement gate, used to create multi-register entangled states.")
print()

entangle_code = """
class MultiEntangleOp : public SelfAdjointOperator {
    size_t reg_a;
    size_t reg_b;
    size_t reg_c;
public:
    MultiEntangleOp(size_t a, size_t b, size_t c)
        : reg_a(a), reg_b(b), reg_c(c) {}

    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            // Triple XOR: entangle three registers
            uint64_t val = s.get(reg_a).value ^ s.get(reg_b).value ^ s.get(reg_c).value;
            s.get(reg_a).value = val;
        }
    }
};
"""

print("Compiling the multi-register entanglement gate...")
try:
    MultiEntangleOp = compile_operator(
        name="MultiEntangleOp",
        cpp_code=entangle_code,
        base_class="SelfAdjointOperator",
        constructor_args=[
            ("size_t", "reg_a"),
            ("size_t", "reg_b"),
            ("size_t", "reg_c")
        ],
        verbose=True
    )
    print()
    print("Compilation successful!")
    print(f"  Class name: {MultiEntangleOp.__name__}")
    print(f"  Base class: {MultiEntangleOp._base_class}")

    op = MultiEntangleOp(reg_a=0, reg_b=1, reg_c=2)
    print(f"  Instance: {repr(op)}")
    print()
except Exception as e:
    print(f"Compilation failed: {e}")
    print()
    MultiEntangleOp = None

# ============================================================================
# Example 3: Grover search oracle
# ============================================================================
print("-" * 70)
print("Example 3: Grover search oracle")
print("-" * 70)
print()
print("The marking oracle is a core component of Grover's search algorithm.")
print("It marks the target state by flipping its phase.")
print()

oracle_code = """
class MarkOracle : public SelfAdjointOperator {
    size_t data_reg;
    uint64_t target_value;
public:
    MarkOracle(size_t d, uint64_t t) : data_reg(d), target_value(t) {}

    void operator()(std::vector<System>& state) const override {
        // Mark the target state: when the data register equals the target value, multiply the amplitude by -1
        for (auto& s : state) {
            if (s.get(data_reg).value == target_value) {
                s.amplitude *= -1.0;
            }
        }
    }
};
"""

print("Compiling the marking oracle...")
try:
    MarkOracle = compile_operator(
        name="MarkOracle",
        cpp_code=oracle_code,
        base_class="SelfAdjointOperator",
        constructor_args=[
            ("size_t", "data_reg"),
            ("uint64_t", "target_value")
        ],
        verbose=True
    )
    print()
    print("Compilation successful!")
    print(f"  Class name: {MarkOracle.__name__}")
    print(f"  Base class: {MarkOracle._base_class}")

    op = MarkOracle(data_reg=0, target_value=5)
    print(f"  Instance: {repr(op)}")
    print()
except Exception as e:
    print(f"Compilation failed: {e}")
    print()
    MarkOracle = None

# ============================================================================
# Example 4: Hamiltonian evolution
# ============================================================================
print("-" * 70)
print("Example 4: Hamiltonian evolution")
print("-" * 70)
print()
print("The Hamiltonian evolution operator is used to simulate the time evolution of quantum systems.")
print()

hamiltonian_code = """
class HamiltonianEvolution : public BaseOperator {
    size_t reg_id;
    double coupling_strength;
    double time;
public:
    HamiltonianEvolution(size_t r, double g, double t)
        : reg_id(r), coupling_strength(g), time(t) {}

    void operator()(std::vector<System>& state) const override {
        double phase = coupling_strength * time;
        for (auto& s : state) {
            // Apply a phase rotation based on the register value
            double value_phase = phase * s.get(reg_id).value;
            s.amplitude *= std::exp(std::complex<double>(0, value_phase));
        }
    }

    void dag(std::vector<System>& state) const override {
        double phase = -coupling_strength * time;
        for (auto& s : state) {
            double value_phase = phase * s.get(reg_id).value;
            s.amplitude *= std::exp(std::complex<double>(0, value_phase));
        }
    }
};
"""

print("Compiling the Hamiltonian evolution operator...")
try:
    HamiltonianEvolution = compile_operator(
        name="HamiltonianEvolution",
        cpp_code=hamiltonian_code,
        base_class="BaseOperator",
        constructor_args=[
            ("size_t", "reg_id"),
            ("double", "coupling_strength"),
            ("double", "time")
        ],
        verbose=True
    )
    print()
    print("Compilation successful!")
    print(f"  Class name: {HamiltonianEvolution.__name__}")
    print(f"  Base class: {HamiltonianEvolution._base_class}")

    op = HamiltonianEvolution(reg_id=0, coupling_strength=0.5, time=1.0)
    print(f"  Instance: {repr(op)}")
    print()
except Exception as e:
    print(f"Compilation failed: {e}")
    print()
    HamiltonianEvolution = None

# ============================================================================
# Example 5: Quantum walk operator
# ============================================================================
print("-" * 70)
print("Example 5: Quantum walk step operator")
print("-" * 70)
print()
print("A quantum walk is the quantum analogue of a classical random walk.")
print("The step operator moves the position according to the coin state.")
print()

quantum_walk_code = """
class QuantumWalkStep : public SelfAdjointOperator {
    size_t position_reg;
    size_t coin_reg;
    size_t n_positions;
public:
    QuantumWalkStep(size_t pos, size_t coin, size_t n)
        : position_reg(pos), coin_reg(coin), n_positions(n) {}

    void operator()(std::vector<System>& state) const override {
        for (auto& s : state) {
            size_t coin_val = s.get(coin_reg).value;
            size_t pos = s.get(position_reg).value;

            // Coin 0 moves right, coin 1 moves left
            if (coin_val == 0 && pos < n_positions - 1) {
                s.get(position_reg).value = pos + 1;
            } else if (coin_val == 1 && pos > 0) {
                s.get(position_reg).value = pos - 1;
            }
        }
    }
};
"""

print("Compiling the quantum walk step operator...")
try:
    QuantumWalkStep = compile_operator(
        name="QuantumWalkStep",
        cpp_code=quantum_walk_code,
        base_class="SelfAdjointOperator",
        constructor_args=[
            ("size_t", "position_reg"),
            ("size_t", "coin_reg"),
            ("size_t", "n_positions")
        ],
        verbose=True
    )
    print()
    print("Compilation successful!")
    print(f"  Class name: {QuantumWalkStep.__name__}")
    print(f"  Base class: {QuantumWalkStep._base_class}")

    op = QuantumWalkStep(position_reg=0, coin_reg=1, n_positions=8)
    print(f"  Instance: {repr(op)}")
    print()
except Exception as e:
    print(f"Compilation failed: {e}")
    print()
    QuantumWalkStep = None

# ============================================================================
# Cache information
# ============================================================================
print("-" * 70)
print("Cache information")
print("-" * 70)
print()

info = get_cache_info()
print(f"Cache directory: {info['cache_dir']}")
print(f"Number of cache files: {info['so_count']}")
print(f"Cache size: {info['total_size_mb']:.2f} MB")
print()

# ============================================================================
# Summary
# ============================================================================
print("=" * 70)
print("Example completed")
print("=" * 70)
print()

print("""
Summary:
-----
This example demonstrated the following quantum-computing-related dynamic operators:

1. Controlled phase gate (ControlledPhase):
   - Uses BaseOperator to implement dagger
   - Applies a phase when both the control and target qubits are |1>
   - Can be used in controlled unitary gate sequences

2. Multi-register entanglement gate (MultiEntangleOp):
   - Uses SelfAdjointOperator
   - Creates entanglement via a triple XOR
   - dagger automatically equals the operator itself

3. Grover search oracle (MarkOracle):
   - Uses SelfAdjointOperator
   - Marks the target state by flipping its phase
   - Is a core component of Grover's algorithm

4. Hamiltonian evolution (HamiltonianEvolution):
   - Uses BaseOperator to implement reversible evolution
   - Supports custom coupling strength and time
   - Can be used for quantum simulation

5. Quantum walk step (QuantumWalkStep):
   - Uses SelfAdjointOperator
   - Moves the position according to the coin state
   - Is the foundation of quantum walk algorithms

For more information, refer to:
- docs/sphinx/source/guide/dynamic_operators.rst (full user guide)
- docs/sphinx/source/api/dynamic_operator.rst (API reference)
- PySparQ/test/test_dynamic_operator.py (unit tests)
""")