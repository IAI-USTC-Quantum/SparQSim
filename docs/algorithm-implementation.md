## Quantum Algorithm Implementations Based on PySparQ

[简体中文](algorithm-implementation_zh-cn.md)

This section documents in detail how PySparQ translates SparQ's quantum algorithm experiment code (C++) into Python implementations one by one, covering the precise correspondence between the code and the quantum algorithm steps, register length management strategies, and the implementation workflow of each submodule.

### 1. Coverage Overview: C++ Experiments vs PySparQ Implementations

| Algorithm | C++ Experiment Code | PySparQ Implementation File | Status |
|------|------------|----------------|--------|
| **Grover** | `Experiments/Grover/GroverTest.cpp` | `PySparQ/pysparq/algorithms/grover.py` | ✅ Complete replication |
| **Shor** | `Experiments/Shor/ShorTest.cpp` | `PySparQ/pysparq/algorithms/shor.py` | ✅ Complete replication |
| **QDA** | `Experiments/QDA/QDATest/QDATest.cpp` | `PySparQ/pysparq/algorithms/qda_solver.py` | ✅ Primitive-level translation; readout post-processing to be completed |
| **CKS** | `Experiments/CKS/HamiltonianSimulationTest.cpp` | `PySparQ/pysparq/algorithms/cks_solver.py` | ✅ Primitive-level translation; LCU readout post-processing to be completed |
| **QFT** | `Experiments/QFT/QFTTest.cpp` | ❌ No standalone module | ❌ Not yet implemented |
| **GHZ** | `Experiments/GHZ/GHZTest.cpp` | ❌ None | ❌ Not yet implemented |
| **QCNN** | `Experiments/QCNN/QCNNTest.cpp` (disabled with `#if false`) | ❌ None | ❌ Not yet implemented |

**Key findings:**
- Grover and Shor achieve a complete one-to-one correspondence and are currently the two most complete implementations in the PySparQ algorithm library
- The Python implementations of QDA and CKS do not call the C++ solvers directly; instead they compose PySparQ low-level primitives to replicate the register-level flow of the C++ code. What is currently unfinished is the measurement, post-selection, and readout of the linear-system solution vector
- The C++ implementations of QFT and GHZ are relatively simple, making them priority targets for completion next
- QCNN has been disabled on the C++ side; it must first be restored on the C++ side before being translated to Python

---

### 2. Common Patterns of Register Level Programming in Quantum Algorithms

Before diving into each algorithm, we first summarize the common register management patterns used when implementing quantum algorithms in PySparQ.

#### 2.1 Division of Register Roles

Every quantum algorithm needs to define the following kinds of registers; understanding their roles is the key to understanding the implementation details:

| Register Type | Naming Pattern | Length (bits) | Purpose |
|-----------|---------|------------|------|
| **Main data register** | `"main"`, `"addr"` | `log₂(N)` (N is the problem size) | Stores the algorithm's main input, such as search-space addresses |
| **Data register** | `"data"` | Depends on the data range, usually 64 | Temporarily holds the data read from the QRAM |
| **Search-value register** | `"target"`, `"search"` | Same as data | Holds the target value to search for / match |
| **Auxiliary ancilla register** | `"anc_1"`, `"anc_2"`, etc. | 1, or the same as the main register | Control flow, flag bits, conditional reflection |
| **Precision register** | `"count"`, `"work_reg"` | 2×n (n is the main register width) | Phase estimation, iteration counting |

#### 2.2 Principles for Choosing Register Lengths

```
Main register width = ceil(log2(problem size N))
Data register width = 64 (default value, large enough to hold the integers obtained after quantizing floating-point values)
Precision register width = 2 × ceil(log2(N))  ← mainly used for phase estimation in Shor's algorithm
```

In `grover.py` (lines 256–283):
```python
n_bits = int(math.log2(n)) + 1 if n > 0 else 1   # address register width
data_reg = ps.AddRegister("data", ps.UnsignedInteger, data_size)(state)  # data register fixed at 64 bits
```

#### 2.3 Common Quantum Operation Pattern

Almost all algorithms follow this operation pattern:

```python
# ① Clear the system
ps.System.clear()

# ② Create the quantum state
state = ps.SparseState()

# ③ Add registers (chained call, returns the register ID)
addr_reg = ps.AddRegister("addr", ps.UnsignedInteger, n_bits)(state)

# ④ Initialize (superposition / specified value)
ps.Hadamard_Int_Full("addr")(state)    # full superposition
ps.Init_Unsafe("target", target)(state)  # specified value

# ⑤ Algorithm main body (Oracle / Walk / quantum gate sequence)
grover_op(state)  # or walk_op(state)

# ⑥ Measurement (partial trace)
measured_results, prob = ps.PartialTrace([...Registers...])(state)
```

---

### 3. Grover Quantum Search Algorithm

#### 3.1 Algorithm Overview and Mathematical Principles

The Grover algorithm searches for M marked items among N unordered entries with complexity O(√(N/M)), a quadratic speedup over the classical O(N).

Core iteration:
```
G = D · O
```
where the Oracle O marks the target states (applying a negative phase), and the diffusion operator D amplifies the amplitudes.

#### 3.2 Exact Correspondence: C++ Implementation → Python

The correspondence between the core logic of the C++ test (`GroverTest.cpp`, lines 272–355) and `grover.py`:

| C++ Code (GroverTest.cpp) | PySparQ Implementation (grover.py) | Notes |
|--------------------------|--------------------------|------|
| `qram_qutrit::QRAMCircuit qram(...)` | `ps.QRAMCircuit_qutrit(n_bits, data_size, memory)` | Quantum random access memory setup |
| `System::add_register("addr", UnsignedInteger, addr_size)` | `ps.AddRegister("addr", ps.UnsignedInteger, n_bits)(state)` | Address register addition |
| `Init_Unsafe("target", search_target)(state)` | `ps.Init_Unsafe("search", target)(state)` | Search target initialization |
| `Hadamard_Int_Full(qram_addr_reg)(state)` | `ps.Hadamard_Int_Full("addr")(state)` | Create an equal-amplitude superposition state |
| `GroverOperator(&qram, ...)(state)` | `GroverOperator(qram, "addr", "data", "search")(state)` | Full Grover iteration |
| `PartialTrace` | `ps.PartialTrace([...])(state)` | Partial trace to obtain the measurement results |

#### 3.3 Oracle Implementation Details (Line-Level Correspondence)

The C++ GroverTest.cpp contains no explicit Oracle comments, but `grover.h` (SparQ_Algorithm) defines the `GroverOracle` structure; the corresponding Python implementation is as follows:

**Lines 82–113 (Oracle workflow):**

```python
def __call__(self, state: ps.SparseState) -> None:
    # Step 1: QRAMLoad → |addr⟩|0⟩ → |addr⟩|memory[addr]⟩
    ps.QRAMLoad(self.qram, self.addr_reg, self.data_reg)(state)

    # Step 2: Create the comparison flag registers (less / equal)
    compare_less  = ps.AddRegister("compare_less",  ps.Boolean, 1)(state)
    compare_equal = ps.AddRegister("compare_equal", ps.Boolean, 1)(state)

    # Step 3: Compare data with target, producing the less/equal flags
    ps.Compare_UInt_UInt(
        self.data_reg, self.search_reg, compare_less, compare_equal
    )(state)

    # Step 4: Apply a negative phase on match (marking)
    phase_flip = ps.ZeroConditionalPhaseFlip([compare_equal])
    phase_flip(state)

    # Step 5: Reverse comparison (inverse operation, cleaning the auxiliary registers)
    ps.Compare_UInt_UInt(
        self.data_reg, self.search_reg, compare_less, compare_equal
    )(state)

    # Step 6: Delete the temporary registers
    ps.RemoveRegister(compare_equal)(state)
    ps.RemoveRegister(compare_less)(state)

    # Step 7: QRAMLoad self-inverse operation (QRAMLoad² = I)
    ps.QRAMLoad(self.qram, self.addr_reg, self.data_reg)(state)
```

**Key design decisions:**
- **Three-step phase marking**: QRAMLoad → Compare → PhaseFlip → Uncompute → QRAMLoad implements a phase oracle free of ancilla pollution
- **Compare_UInt_UInt**: PySparQ directly compares the values of two registers and produces Boolean flag bits, with no need to compile into a quantum gate sequence — this is the core advantage of Register Level Programming
- **Temporary register management**: every Oracle call temporarily creates `compare_less/equal` and immediately RemoveRegisters them after use, avoiding state-space blow-up

#### 3.4 Diffusion Operator Implementation Details (Lines 116–171)

```python
def __call__(self, state: ps.SparseState) -> None:
    ps.Hadamard_Int_Full(self.addr_reg)(state)   # H
    phase_flip = ps.ZeroConditionalPhaseFlip([self.addr_reg])  # P_0
    phase_flip(state)                     # apply phase on |0⟩
    ps.Hadamard_Int_Full(self.addr_reg)(state)   # H
```

Mathematically: `D = H · P_0 · H = 2|s⟩⟨s| - I` (a reflection about the uniform superposition state)

#### 3.5 Complete GroverIterator Flow (Lines 173–222)

```python
def __call__(self, state: ps.SparseState) -> None:
    # Each iteration: Oracle → Diffusion
    self.oracle(state)       # marking
    self.diffusion(state)    # amplitude amplification
```

Corresponds to the loop in the C++ test (lines 319–324):
```cpp
for (size_t i = 0; i < n_repeats * 4; ++i) {
    auto qram_data_reg = AddRegister("data", UnsignedInteger, data_size)(state);
    GroverOperator(&qram, qram_addr_reg, qram_data_reg, search_data_reg)(state);
    (RemoveRegister(qram_data_reg))(state);   // clean up the temporary data register after each iteration
}
```

#### 3.6 Quantum Counting: GroverCount Implementation (Lines 314–384)

Corresponds to the C++ `grover_count_success_test` (lines 449–503):

| C++ Code | Python Implementation |
|---------|-----------|
| `System::add_register("count", UnsignedInteger, count_precision)` | `ps.AddRegister("count", ps.UnsignedInteger, precision_bits)(state)` |
| `Hadamard_Int_Full(addr/count)` | `ps.Hadamard_Int_Full("count/addr")(state)` |
| `GroverCount(&qram, count_reg, ...)` | `GroverOperator(...).conditioned_by_bit("count", i)(state)` |
| Inverse QFT `InverseQFT` | `ps.InverseQFT("count")(state)` |

---

### 4. Shor Quantum Factoring Algorithm

#### 4.1 Algorithm Overview

Shor's algorithm uses quantum phase estimation to find the period r of modular exponentiation, thereby reducing the factoring of large numbers to computing a gcd; it has important applications in areas such as RSA encryption. Its complexity is O((log N)³), an exponential speedup over classical sub-exponential factoring.

Core flow:
```
1. Choose a random number a (coprime with N)
2. Quantum phase estimation: find the smallest r such that a^r ≡ 1 (mod N)
3. Compute gcd(a^(r/2)±1, N) to obtain the factors
```

#### 4.2 Exact Correspondence: C++ Implementation → Python

C++ `ShorTest.cpp` (lines 7–26):
```cpp
semi_classical_shor(N);  // core call, iterated 100 times
```

Corresponds to the `SemiClassicalShor` class in Python `shor.py` (lines 241–331):

| C++ Step | Python Implementation (shor.py) | Code Location |
|---------|----------------------|---------|
| Semi-classical iterative measurement | `SemiClassicalShor.run()` | Lines 282–330 |
| Create the working qubit | `ps.AddRegisterWithHadamard("work_reg", ps.UnsignedInteger, 1)` | Lines 300–302 |
| Controlled modular exponentiation | `ModMul("anc_reg", a, size-1-x, N).conditioned_by_all_ones("work_reg")` | Lines 306–307 |
| Phase correction | `ps.Phase_Bool("work_reg", phase)` | Lines 311–313 |
| Hadamard + measurement | `ps.Hadamard_Bool("work_reg")` + `ps.PartialTrace` | Lines 316–320 |
| Inverse QFT (implicit in the iterative measurement)| `shor_postprocess(meas, size, a, N)` | Line 329 |
| Classical post-processing | `shor_postprocess()` | Lines 166–196 |

#### 4.3 Modular Exponentiation Implementation (Lines 39–65 and 198–239)

The C++ Shor implementation uses a custom quantum arithmetic module. The corresponding Python implementation:

```python
# Modular exponentiation function (pure math, lines 39–65)
def general_expmod(a: int, x: int, N: int) -> int:
    """Classical square-and-multiply algorithm: a^x mod N"""
    if x == 0: return 1
    if x & 1:  # odd
        return (general_expmod(a, x-1, N) * a) % N
    else:       # even
        half = general_expmod(a, x // 2, N)
        return (half * half) % N

# Controlled modular multiplication operator (lines 198–239)
class ModMul:
    def __call__(self, state: ps.SparseState) -> None:
        def modmul_func(val: int) -> int:
            return (val * self.opnum) % self.N
        op = ps.CustomArithmetic([self.reg], 64, 64, modmul_func)
        op.conditioned_by_all_ones(cond_reg)(state)
```

**Key design:**
- `CustomArithmetic` is the ultimate embodiment of Register Level Programming — you directly write a Python function describing an integer-to-integer mapping, and SparQ automatically compiles it into a quantum arithmetic circuit
- No need to manually construct controlled-NOT and single-qubit gate combinations

#### 4.4 Quantum Circuit Flow of Semi-Classical Shor (Lines 282–330)

```
for x in range(size):           # size = 2 * n_bits(N)
    ① work_reg = |0⟩ + |1⟩           (via Hadamard)
    ② Controlled modular multiplication a^(2^(size-1-x)) mod N (conditioned_by_all_ones on work_reg)
    ③ Phase correction from prior results
    ④ Hadamard on work_reg
    ⑤ Measure work_reg → result_bit
    ⑥ Remove work_reg
    ↓
    Merge bits → meas_result (integer)
    ↓
    shor_postprocess → period r → factors p, q
```

#### 4.5 Classical Post-Processing (Lines 166–196)

```python
def shor_postprocess(meas: int, size: int, a: int, N: int) -> Tuple[int, int]:
    # ① Find the best fraction approximation of y/Q
    r, c = find_best_fraction(y, Q, N)
    
    # ② Validate r (even, ≠ -1 mod N)
    check_period(r, a, N)
    
    # ③ Extract the factors
    a_exp_r_half = general_expmod(a, r // 2, N)
    p = math.gcd(a_exp_r_half + 1, N)
    q = math.gcd(a_exp_r_half - 1, N)
```

---

### 5. QDA Quantum Discrete Adiabatic Linear System Solver

#### 5.1 Algorithm Overview

QDA (Quantum Discrete Adiabatic) uses the discrete adiabatic theorem to solve the linear system Ax = b, achieving the optimal scale O(κ log(κ/ε)) (κ is the condition number).

The PySparQ version follows the register-level structure of the C++ `Walk_s_Tridiagonal` and `Walk_s_via_QRAM`: matrix block encoding, `StatePrepViaQRAM`, `Hadamard_Int_Full`, reflections, controlled rotations, and global phases are all composed on the Python side by calling low-level primitives. It does not wrap the entire QDA solver as a C++ binding to bypass the Python implementation. Currently `qda_solve()` executes the quantum walk sequence; since the measurement and post-selection readout are not yet wired up, it explicitly raises a `RuntimeError` when the run completes, avoiding the use of classical `np.linalg.solve` to mask errors in the algorithm path.

Core idea:
```
H(s) = (1-f(s))·H₀ + f(s)·H₁   (interpolated Hamiltonian)
W_s = R · H_s                   (quantum walk operator)
By discretizing s and executing the sequence of W_s, the system evolves from |b⟩ to |x⟩
```

#### 5.2 Implementation of the Interpolation Function f(s) (qda_solver.py Lines 41–72)

```python
def compute_fs(s: float, kappa: float, p: float) -> float:
    """
    From the PRX Quantum paper Eq.(69):
    f(s) = κ/(κ-1) * (1 - (1 + s(κ^(p-1) - 1))^(1/(1-p)))
    """
    if kappa == 1:
        return s
    kappa_p_minus_1 = kappa ** (p - 1)
    inner = 1 + s * (kappa_p_minus_1 - 1)
    exponent = 1 / (1 - p)
    result = kappa / (kappa - 1) * (1 - inner ** exponent)
    return max(0.0, min(1.0, result))
```

Corresponds to the call in C++ QDATest.cpp lines 410–413:
```cpp
WalkSequence_via_QRAM_Debug(&qram_A, &qram_b, mat, b, ...,
    steps, kappa, p, data_size, rational_size, ...)(state);
```

#### 5.3 Implementation of the Block Encoding of H(s) (qda_solver.py Lines 196–314)

The `BlockEncodingHs` class implements the block encoding of the interpolated Hamiltonian, corresponding to the C++ `Walk_s_via_QRAM` operation. Core operation sequence (lines 248–308):

```python
def __call__(self, state: ps.SparseState) -> None:
    ps.Hadamard_Bool(self.anc_3)(state)         # H
    self.enc_b.dag(state)                        # inverse of the state preparation
    ps.X_Bool(self.anc_1, 0)(state)          # X
    ps.Reflection_Bool(self.main_reg, True)      # reflection operator (about |0⟩)
          .conditioned_by_all_ones([self.anc_1, self.anc_3, self.anc_4])(state)
    ps.X_Bool(self.anc_1, 0)(state)
    self.enc_b(state)                            # state preparation
    
    # Rotation sequence: R_s(f(s))
    ps.X_Bool(self.anc_4, 0)(state)
    ps.Rot_Bool(self.anc_2, self.R_s).conditioned_by_all_ones(self.anc_4)(state)
    ...
    self.enc_A.conditioned_by_all_ones([self.anc_1, self.anc_2])(state)  # block encoding of A
```

#### 5.4 Complete QRAM Data Preparation Flow (Corresponding to QDATest.cpp Lines 356–391)

```python
# Step 1: Quantize the floating-point matrix/vector into integers
conv_A = scaleAndConvertVector(mat, exponent=15, data_size=50)
conv_b = scaleAndConvertVector(b, exponent=15, data_size=50)

# Step 2: Build the QRAM tree structure
data_tree_A = make_vector_tree(conv_A, data_size)
data_tree_b = make_vector_tree(conv_b, data_size)

# Step 3: Set up the QRAM circuit
addr_size = log_column_size * 2 + 1   # 1 bit more than data (used for normalization)
qram_A = ps.QRAMCircuit_qutrit(addr_size, data_size)
qram_A.set_memory(data_tree_A)
qram_b = ps.QRAMCircuit_qutrit(log_column_size + 1, data_size)
qram_b.set_memory(data_tree_b)

# Step 4: Add registers
main_reg = ps.AddRegister("main_reg", ps.UnsignedInteger, log_column_size)(state)
anc_UA   = ps.AddRegister("anc_UA",   ps.UnsignedInteger, log_column_size)(state)
anc_4    = ps.AddRegister("anc_4",    ps.Boolean, 1)(state)
anc_3    = ps.AddRegister("anc_3",    ps.Boolean, 1)(state)
anc_2    = ps.AddRegister("anc_2",    ps.Boolean, 1)(state)
anc_1    = ps.AddRegister("anc_1",    ps.Boolean, 1)(state)
```

#### 5.5 Construction of the Rotation Matrix (qda_solver.py Lines 74–95)

```python
def compute_rotation_matrix(fs: float) -> List[complex]:
    """
    Rotation matrix R_s:
    R_s = 1/√((1-f_s)² + f_s²) * [[1-f_s, f_s], [f_s, f_s-1]]
    """
    sqrt_N = 1.0 / math.sqrt((1 - fs) ** 2 + fs**2)
    return [
        complex(sqrt_N * (1 - fs), 0),
        complex(sqrt_N * fs,       0),
        complex(sqrt_N * fs,       0),
        complex(sqrt_N * (fs - 1), 0),
    ]
```

#### 5.6 Dolph-Chebyshev Filtering (qda_solver.py Lines 102–189)

Used to improve the accuracy of the QDA solution:

```python
def dolph_chebyshev(epsilon: float, l: int, phi: float) -> float:
    beta = math.cosh(math.acosh(1.0 / epsilon) / l)
    return epsilon * chebyshev_T(l, beta * math.cos(phi))
```

Corresponds to the filtering steps in C++ QDATest.cpp lines 550–566:
```cpp
int l_ = static_cast<int>(floor(kappa * log(2.0 / epsilon_)));
vector<double> weights = ComputeFourierCoeffs(epsilon_, l_);
// Build the weight QRAM → apply the LCU
```

---

### 6. CKS Quantum Linear System Solver (Childs-Kothari-Somma)

#### 6.1 Algorithm Overview

The CKS algorithm uses Chebyshev polynomial approximation and quantum walks to achieve O(κ log(κ/ε)) complexity for sparse matrices, with better constant factors than HHL-type algorithms.

The PySparQ version likewise replicates only the quantum walk path of the C++ `HamiltonianSimulationTest.cpp` through low-level primitives. The Python `SparseMatrix` uses the same compact QRAM layout as the C++ one; steps such as `TOperator`, `QuantumBinarySearch_Fast`, `GetRowAddr`, `GetDataAddr`, `GetQWRotateAngle_Int_Int_Int`, `CondRot_Fixed_Bool`, `QRAMLoad`, and register swaps are composed in the C++ order. It does not bind the complete C++ CKS solver, nor does it expose the old generalized CondRot API that lets users pass in Python functions.

Core flow:
```
1. Chebyshev polynomial coefficients c_j = erfc((j+0.5)/√b) * 2
2. Construct the quantum walk operator W = T† · P_0 · T · Swap
3. LCU: Σ_j c_j W^(2j+1) approximates 1/A
```

#### 6.2 Implementation of the Chebyshev Coefficients (cks_solver.py Lines 41–127)

```python
class ChebyshevPolynomialCoefficient:
    def __init__(self, b: int):
        # b = κ² log(κ/ε), computed by LCU_Container at initialization
        self.b = b

    def coef(self, j: int) -> float:
        """Chebyshev coefficient of step j"""
        if self.b > 100:
            # Large b: use the asymptotic approximation (erfc)
            return math.erfc((j + 0.5) / math.sqrt(self.b)) * 2
        else:
            # Small b: exact computation
            ret = 0.0
            for i in range(j + 1, self.b + 1):
                ret += self.C(2 * self.b, self.b + i)
            return ret * 4

    def sign(self, j: int) -> bool:
        """Odd steps are negative (apply a minus sign)"""
        return (j & 1) == 1

    def step(self, j: int) -> int:
        """Number of walk steps at step j = 2j + 1"""
        return 2 * j + 1
```

Corresponds to C++ HamiltonianSimulationTest.cpp lines 139–149:
```cpp
auto test_chebyshev_polynomial_coef() {
    ChebyshevPolynomialCoefficient chebyshev_obj(b);
    for (size_t j = 0; j <= 64; ++j)
        fmt::print("j={}, coef={}\n", j,
            chebyshev_obj.coef(j) * (chebyshev_obj.sign(j) ? -1 : 1));
}
```

#### 6.3 Sparse Matrix Representation (cks_solver.py Lines 225–340)

```python
@dataclass
class SparseMatrixData:
    n_row: int           # number of rows
    nnz_col: int         # maximum number of non-zero elements per column
    data: List[int]      # flattened matrix data (quantized integers)
    data_size: int       # quantization bit width (usually 32)
    positive_only: bool  # whether the matrix is all-positive (determines the rotation matrix form)
    sparsity_offset: int  # QRAM sparse addressing offset

class SparseMatrix:
    @classmethod
    def from_dense(cls, matrix: np.ndarray, data_size: int = 32,
                   positive_only: bool = None) -> "SparseMatrix":
        # ① Detect positivity
        # ② Normalize + quantize
        Amax = 2 ** (data_size - 1) - 1
        scaled = matrix / max_val * Amax
        # ③ Convert to two's complement (negative numbers)
        # ④ Compute nnz_col
```

Corresponds to the C++ `SparseMatrix` class (`hamiltonian_simulation.h`) and `generate_simplest_sparse_matrix_unsigned_2()`.

#### 6.4 Quantum Walk Implementation (cks_solver.py Lines 637–710)

```python
class QuantumWalk:
    """
    W = T† · P_0 · T · Swap

    T: state preparation operator (|j⟩|0⟩ → Σ_k √(A_jk/‖A_j‖)|j⟩|k⟩)
    P_0: phase reflection (about the |0⟩ state)
    Swap: row-column swap
    """
    def __call__(self, state: ps.SparseState) -> None:
        # Tdagger
        t_op.dag(state)
        
        # Phase reflection
        ps.ZeroConditionalPhaseFlip(
            [self.b1_reg, self.k_reg, self.b2_reg, self.k_comp_reg]
        )(state)
        
        # T
        t_op(state)
        
        # Swap rows and columns
        ps.Swap_General_General(self.j_reg, self.k_reg)(state)
        ps.Swap_General_General(self.b1_reg, self.b2_reg)(state)
        ps.Swap_General_General(self.j_comp_reg, self.k_comp_reg)(state)
```

Corresponds to C++ HamiltonianSimulationTest.cpp lines 116–118:
```cpp
for (int i = 0; i < 99; ++i)
    QuantumWalk(qram, j, b1, k, b2, j_comp, k_comp, ...)(system_states);
```

When aligning the CKS fidelity test with the C++ `automatic_Chebyshev_test`, one must follow the C++ logic and compare only the target register after post-selection, and use the dense matrix representation actually encoded by `SparseMatrix` (including the `nnz_col` normalization); the raw input matrix `A / ||A||` cannot be used directly as the target state.

#### 6.5 LCU Container (cks_solver.py Lines 843–930)

```python
class LCUContainer:
    def __init__(self, mat, kappa, eps, qram=None):
        # b = κ² log(κ/ε) — determines the number of Chebyshev expansion terms
        self.b = int(kappa * kappa * (math.log(kappa) - math.log(eps)))
        # j0 = √(b log(4b/ε)) — truncation point
        self.j0 = int(math.sqrt(self.b * (math.log(4 * self.b) - math.log(eps))))
        
    def iterate(self):
        j, a = 0, 0.0
        while j <= self.j0:
            if j != 0:
                self.walk.step(self.step_state)
            coef = self.chebyshev.coef(j)
            sign = self.chebyshev.sign(j)
            a += coef
            self._add_state(self.step_state, coef, sign)
            j += 1
```

Corresponds to C++ `LCU_Container_NoiseFree` (HamiltonianSimulationTest.cpp lines 275–326):
```cpp
obj.ExternalInput<Hadamard_Int>(addr_size);  // Hadamard initialization
while (obj.Step()) {
    auto [state, success_rate] = obj.PartialTrace_Nondestructive();
    double fidelity = get_fidelity(m, target_result);
    // Record the success rate and fidelity at each step
}
```

---

### 7. GHZ State Generation (To Be Implemented)

#### 7.1 C++ Implementation Analysis

The GHZ state generation logic in C++ `GHZTest.cpp` (lines 26–72) is extremely concise:

```cpp
// Register layout: ctrl (1 bit) + main registers (chunked, each chunk ≤ 64 bits)
// Let nqubit = 65, then: quotient = 1, remainder = 0
// Actual layout: ctrl(1) + main(64)

FlipBools("ctrl")(state);       // ctrl → |1⟩
Hadamard_Bool("ctrl")(state);   // H → (|0⟩+|1⟩)/√2

// Apply a controlled X to each main qubit (flip when ctrl=1)
for (int i = 1; i < remainder + 1; i++)
    X_Bool("main", i-1).conditioned_by_all_ones("ctrl")(state);
for (auto reg : reg_names)
    FlipBools(System::get(reg)).conditioned_by_all_ones("ctrl")(state);
```

**Generated state: `(|0...0⟩ + |1...1⟩)/√2`** (a GHZ state of nqubit qubits)

#### 7.2 Python Translation Plan

```python
def ghz_state(nqubit: int) -> ps.SparseState:
    """Generate a GHZ state: (|0...0⟩ + |1...1⟩)/√2"""
    ps.System.clear()
    state = ps.SparseState()
    
    # Handle the > 64-bit case (register chunking)
    quotient, remainder = divmod(nqubit - 1, 64)
    
    ctrl = ps.AddRegister("ctrl", ps.UnsignedInteger, 1)(state)
    # Add the main registers (at most 64 bits per chunk)
    main_regs = []
    for k in range(1, quotient + 1):
        reg = ps.AddRegister(f"main{k}", ps.UnsignedInteger, remainder or 64)(state)
        main_regs.append(reg)
    if remainder != 0:
        main_reg = ps.AddRegister("main", ps.UnsignedInteger, remainder)(state)
        main_regs.append(main_reg)
    
    # GHZ circuit
    ps.X_Bool("ctrl", 0)(state)       # |0⟩ → |1⟩
    ps.Hadamard_Bool("ctrl")(state)        # (|0⟩+|1⟩)/√2
    
    for reg in main_regs:
        for pos in range(ps.System.size_of(reg)):
            ps.X_Bool(reg, pos).conditioned_by_nonzeros("ctrl")(state)
    
    return state
```

---

### 8. Summary of Implementation Principles

#### 8.1 Seven Principles for Translating Quantum Algorithms

**Principle 1: Registers first, state follows**
```python
# Wrong: the state is created before the registers
state = ps.SparseState()
addr = ps.AddRegister("addr", ...)  # ← wrong order

# Correct: define the register system first, then create the state
ps.System.add_register("addr", ps.UnsignedInteger, n_bits)
state = ps.SparseState()  # or ps.SparseState() automatically inherits the registered registers
```

**Principle 2: Delete temporary registers as soon as they are used**
```python
# GroverOracle creates compare_less/equal each time and deletes them before the Oracle returns
compare_less = ps.AddRegister("compare_less", ps.Boolean, 1)(state)
# ... use compare_less ...
ps.RemoveRegister(compare_less)(state)   # prevent exponential state-space blow-up
```

**Principle 3: Chained conditional operations**
```python
# All conditional operations return self, supporting chaining
op = ps.ZeroConditionalPhaseFlip([cond_reg])
op.conditioned_by_nonzeros(other_reg)(state)  # add conditions multiple times

# Corresponds in C++ to:
# X_Bool("ctrl", 0).conditioned_by_all_ones("ctrl")(state);
```

**Principle 4: Use `.dag()` for the inverse of self-adjoint operators**
```python
# QRAMLoad, Hadamard, Swap — self-adjoint (U = U†)
ps.QRAMLoad(qram, addr, data)(state)   # forward
ps.QRAMLoad(qram, addr, data)(state)   # inverse operation (the same operation)

# General operators
t_op.dag(state)   # inverse operation
```

**Principle 5: Floating-point quantization is the key to the quantum-classical interface**
```python
# QDA/QDA: floating-point matrix → fixed-point integers
# Quantization factor: 2^exponent, exponent is usually 15 (to guarantee precision)
Amax = 2 ** (data_size - 1) - 1
scaled = matrix / max_val * Amax  # normalize to [-Amax, Amax]
int_data = scaled.astype(int)      # quantize
```

**Principle 6: Top-down — algorithm first, implementation second**
```
Correct order:
① Understand the algorithm's mathematical formulas (Grover iteration / Shor phase estimation / Chebyshev expansion)
② Define the register roles (main register, auxiliary registers, precision register)
③ Write out the algorithm's main flow (Python functions or classes)
④ Fill in the PySparQ operations corresponding to each step

Wrong order:
① Translate the C++ code line by line directly
② Ignore the register length design, causing overflow
```

**Principle 7: Probability normalization of the measurement results**
```python
# The prob returned by PartialTrace is unnormalized and needs to be processed
prob_inv0 = PartialTraceSelect({ anc_UA, anc_2, anc_3 }, {0, 0, 0})(state)
prob0 = (1.0 / prob_inv0) ** 2   # normalize to obtain the success probability

# Corresponds to C++ QDATest.cpp lines 415–416:
# double prob_inv0 = PartialTraceSelect({anc_UA, anc_2, anc_3}, {0, 0, 0})(state);
# double prob0 = (1.0 / prob_inv0) * (1.0 / prob_inv0);
```

#### 8.2 Overview of Common Register Configurations

| Algorithm | Main Register Width | Data Register Width | Number of Ancillas | Special Registers |
|------|------------|-------------|------------|-----------|
| Grover | ceil(log2(N)) | 64 | 2 (compare flags) | search(target) |
| Shor | 2×n_bits(N) | — | 1 (work qubit) | ancilla (modular exponentiation result) |
| QDA | ceil(log2(n)) | 50 | 4 (anc_1~anc_4) | anc_UA (block encoding) |
| CKS | ceil(log2(n_row)) | 32 | 4 (b1,b2,j_comp,k_comp) | sparse_offset |
| GHZ | ceil(log2(n)) | — | 0 | ctrl (1 bit) |

---

### 9. Implementation Roadmap for the Missing Algorithms

#### 9.1 QFT (Priority Implementation)

C++ has complete `QFT_Full()` and `QFT()` implementations (`SparQ/include/qft.h`); the translation is simple:

```python
def qft(state: ps.SparseState, reg: str) -> None:
    """Corresponds to the C++ QFT("main")(state)"""
    n = ps.System.size_of(reg)
    for i in range(n):
        ps.Hadamard_Bool(reg, i)(state)
        for j in range(i + 1, n):
            ps.CPhase_Bool(reg, j, i, math.pi / (2 ** (j - i)))(state)

def inverse_qft(state: ps.SparseState, reg: str) -> None:
    """Inverse QFT: iterate the controlled phases in reverse"""
    n = ps.System.size_of(reg)
    for i in reversed(range(n)):
        for j in reversed(range(i + 1, n)):
            ps.CPhase_Bool(reg, j, i, -math.pi / (2 ** (j - i)))(state)
        ps.Hadamard_Bool(reg, i)(state)
```

#### 9.2 GHZ (Second Priority)

See the translation plan in Section 7.2 — the implementation workload is small and the logic is clear.

#### 9.3 QCNN (C++ Restoration Required First)

`Experiments/QCNN/QCNNTest.cpp` is currently disabled with `#if false`; one must first:
1. Fix the C++ QCNN implementation
2. Translate it in `PySparQ/pysparq/algorithms/qcnn.py`

---

### 10. Lessons for Future Developers

1. **Going from C++ to Python is not a line-by-line translation**: understand the mathematical essence of the algorithm (Grover = amplitude amplification, Shor = phase estimation, CKS = Chebyshev approximation) and rewrite it with a Python mindset. Interfaces such as PySparQ's `CustomArithmetic` are more concise than the C++ versions — you can directly pass in Python lambdas.

2. **Register chunking is the key to very large qubit systems**: beyond 64 bits, registers must be split into multiple `UnsignedInteger` chunks (each ≤ 64 bits), as in the C++ GHZTest. The Python layer achieves this through naming conventions (`"main1"`, `"main2"`) and `FlipBools(System::get(reg))`.

3. **The core of sparse-state optimization is zero-amplitude pruning**: PySparQ's `SparseState` stores only the non-zero amplitudes, and the `ClearZero()` operation is called after each walk/Chebyshev iteration step to prevent an explosion in the number of states. This is the key to why Grover (O(√N) state growth) and CKS (O(κ) state growth) can handle large problems.

4. **Tests before implementation**: following the patterns in `test_doc_examples.py`, use pytest to verify the correctness of each submodule before assembling the complete algorithm. Unit tests should cover: the Oracle (correctness of the amplitude flip), diffusion (reflection about the superposition state), and the quantum walk (Chebyshev approximation accuracy).

5. **Composability of noise models**: the `set_noise_models()` call in the C++ tests is equivalent in PySparQ to:
```python
qram = ps.QRAMCircuit_qutrit(...)
qram.set_noise_model("depolarizing", 1e-4)  # corresponds to the C++ qram->set_noise_models(...)
```
This is very important for evaluating the behavior of algorithms on real hardware.
