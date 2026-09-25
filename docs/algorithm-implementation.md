## Quantum Algorithm Implementations Based on PySparQ

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

---

### 中文版

## 基于 PySparQ 的量子算法实现

本节详细记录 PySparQ 如何将 SparQ 的量子算法实验代码（C++）逐一转译为 Python 实现，涵盖代码与量子算法步骤的精确对应关系、寄存器长度管理策略，以及各子模块的实现流程。

### 一、覆盖率总览：C++ 实验 vs PySparQ 实现

| 算法 | C++ 实验代码 | PySparQ 实现文件 | 对应状态 |
|------|------------|----------------|--------|
| **Grover** | `Experiments/Grover/GroverTest.cpp` | `PySparQ/pysparq/algorithms/grover.py` | ✅ 完整复刻 |
| **Shor** | `Experiments/Shor/ShorTest.cpp` | `PySparQ/pysparq/algorithms/shor.py` | ✅ 完整复刻 |
| **QDA** | `Experiments/QDA/QDATest/QDATest.cpp` | `PySparQ/pysparq/algorithms/qda_solver.py` | ✅ primitive 级转译，解读出后处理待补 |
| **CKS** | `Experiments/CKS/HamiltonianSimulationTest.cpp` | `PySparQ/pysparq/algorithms/cks_solver.py` | ✅ primitive 级转译，LCU 解读出后处理待补 |
| **QFT** | `Experiments/QFT/QFTTest.cpp` | ❌ 无独立模块 | ❌ 尚未实现 |
| **GHZ** | `Experiments/GHZ/GHZTest.cpp` | ❌ 无 | ❌ 尚未实现 |
| **QCNN** | `Experiments/QCNN/QCNNTest.cpp`（已被 `#if false` 禁用） | ❌ 无 | ❌ 尚未实现 |

**关键发现：**
- Grover 和 Shor 实现了完整的一一对应，是目前 PySparQ 算法库中最完整的两个实现
- QDA 和 CKS 的 Python 实现不直接调用 C++ solver，而是组合 PySparQ 底层 primitive 来复刻 C++ 的寄存器级流程；当前未完成的是测量、post-selection 和线性系统解向量读出
- QFT、GHZ 的 C++ 实现较简单，可作为接下来补全的优先目标
- QCNN 在 C++ 侧已被禁用，需要先在 C++ 侧恢复再转译到 Python

---

### 二、Register Level Programming 在量子算法中的通用模式

在深入各算法之前，先总结 PySparQ 实现量子算法时的通用寄存器管理模式。

#### 2.1 寄存器角色分工

每个量子算法均需定义以下几类寄存器，理解它们的角色是理解实现细节的关键：

| 寄存器类型 | 命名模式 | 长度（比特） | 作用 |
|-----------|---------|------------|------|
| **主数据寄存器** | `"main"`, `"addr"` | `log₂(N)`（N 为问题规模） | 存储算法主输入，如搜索空间地址 |
| **数据寄存器** | `"data"` | 视数据范围而定，通常 64 | 临时存放 QRAM 读取的数据 |
| **搜索值寄存器** | `"target"`, `"search"` | 与 data 相同 | 存放要搜索/匹配的目标值 |
| **辅助 Ancilla 寄存器** | `"anc_1"`, `"anc_2"` 等 | 1 或与主寄存器相同 | 控制流、标记位、条件反射 |
| **精度寄存器** | `"count"`, `"work_reg"` | 2×n（n 为主寄存器宽度） | 相位估计、迭代计数 |

#### 2.2 寄存器长度的选取原则

```
主寄存器宽度 = ceil(log2(问题规模 N))
数据寄存器宽度 = 64（默认值，足够容纳浮点数量化后的整数）
精度寄存器宽度 = 2 × ceil(log2(N))  ← 主要用于 Shor 算法的相位估计
```

在 `grover.py` 中（第 256–283 行）：
```python
n_bits = int(math.log2(n)) + 1 if n > 0 else 1   # 地址寄存器宽度
data_reg = ps.AddRegister("data", ps.UnsignedInteger, data_size)(state)  # 数据寄存器固定64位
```

#### 2.3 通用量子操作模式

几乎所有算法都遵循以下操作模式：

```python
# ① 清理系统
ps.System.clear()

# ② 创建量子态
state = ps.SparseState()

# ③ 添加寄存器（链式调用，返回寄存器 ID）
addr_reg = ps.AddRegister("addr", ps.UnsignedInteger, n_bits)(state)

# ④ 初始化（叠加态 / 指定值）
ps.Hadamard_Int_Full("addr")(state)    # 全叠加
ps.Init_Unsafe("target", target)(state)  # 指定值

# ⑤ 算法主体（Oracle / Walk / 量子门序列）
grover_op(state)  # 或 walk_op(state)

# ⑥ 测量（偏迹）
measured_results, prob = ps.PartialTrace([...Registers...])(state)
```

---

### 三 Grover 量子搜索算法

#### 3.1 算法概述与数学原理

Grover 算法用于在 N 个无序条目中搜索 M 个标记项，复杂度为 O(√(N/M))，相比经典 O(N) 有平方加速。

核心迭代：
```
G = D · O
```
其中 Oracle O 标记目标态（加负相位），扩散算子 D 放大振幅。

#### 3.2 C++ 实现 → Python 的精确对应

C++ 测试（`GroverTest.cpp`，第 272–355 行）的核心逻辑与 `grover.py` 的对应关系：

| C++ 代码（GroverTest.cpp） | PySparQ 实现（grover.py） | 说明 |
|--------------------------|--------------------------|------|
| `qram_qutrit::QRAMCircuit qram(...)` | `ps.QRAMCircuit_qutrit(n_bits, data_size, memory)` | 量子随机存取存储器建立 |
| `System::add_register("addr", UnsignedInteger, addr_size)` | `ps.AddRegister("addr", ps.UnsignedInteger, n_bits)(state)` | 地址寄存器添加 |
| `Init_Unsafe("target", search_target)(state)` | `ps.Init_Unsafe("search", target)(state)` | 搜索目标初始化 |
| `Hadamard_Int_Full(qram_addr_reg)(state)` | `ps.Hadamard_Int_Full("addr")(state)` | 创建等幅叠加态 |
| `GroverOperator(&qram, ...)(state)` | `GroverOperator(qram, "addr", "data", "search")(state)` | 完整 Grover 迭代 |
| `PartialTrace` | `ps.PartialTrace([...])(state)` | 偏迹获取测量结果 |

#### 3.3 Oracle 的实现细节（代码行级对应）

C++ GroverTest.cpp 中无显式 Oracle 注释，但 `grover.h`（SparQ_Algorithm）定义了 `GroverOracle` 结构，对应 Python 实现如下：

**第 82–113 行（Oracle 工作流程）：**

```python
def __call__(self, state: ps.SparseState) -> None:
    # Step 1: QRAMLoad → |addr⟩|0⟩ → |addr⟩|memory[addr]⟩
    ps.QRAMLoad(self.qram, self.addr_reg, self.data_reg)(state)

    # Step 2: 创建比较标志寄存器（less / equal）
    compare_less  = ps.AddRegister("compare_less",  ps.Boolean, 1)(state)
    compare_equal = ps.AddRegister("compare_equal", ps.Boolean, 1)(state)

    # Step 3: 比较 data 与 target，产生 less/equal 标志
    ps.Compare_UInt_UInt(
        self.data_reg, self.search_reg, compare_less, compare_equal
    )(state)

    # Step 4: 匹配时加负相位（标记）
    phase_flip = ps.ZeroConditionalPhaseFlip([compare_equal])
    phase_flip(state)

    # Step 5: 反比较（反操作，清理辅助寄存器）
    ps.Compare_UInt_UInt(
        self.data_reg, self.search_reg, compare_less, compare_equal
    )(state)

    # Step 6: 删除临时寄存器
    ps.RemoveRegister(compare_equal)(state)
    ps.RemoveRegister(compare_less)(state)

    # Step 7: QRAMLoad 自反操作（QRAMLoad² = I）
    ps.QRAMLoad(self.qram, self.addr_reg, self.data_reg)(state)
```

**关键设计决策：**
- **三步式相位标记**：QRAMLoad → Compare → PhaseFlip → Uncompute → QRAMLoad，实现了无 ancilla 污染的相位 oracle
- **Compare_UInt_UInt**：PySparQ 直接对两个寄存器值做比较，生成布尔标志位，无需编译成量子门序列——这是 Register Level Programming 的核心优势
- **临时寄存器管理**：每次 Oracle 调用都会临时创建 `compare_less/equal`，使用后立即 RemoveRegister，避免状态空间膨胀

#### 3.4 扩散算子的实现细节（第 116–171 行）

```python
def __call__(self, state: ps.SparseState) -> None:
    ps.Hadamard_Int_Full(self.addr_reg)(state)   # H
    phase_flip = ps.ZeroConditionalPhaseFlip([self.addr_reg])  # P_0
    phase_flip(state)                     # 对 |0⟩ 加相位
    ps.Hadamard_Int_Full(self.addr_reg)(state)   # H
```

数学上：`D = H · P_0 · H = 2|s⟩⟨s| - I`（关于均匀叠加态的反射）

#### 3.5 GroverIterator 的完整流程（第 173–222 行）

```python
def __call__(self, state: ps.SparseState) -> None:
    # 每次迭代：Oracle → Diffusion
    self.oracle(state)       # 标记
    self.diffusion(state)    # 振幅放大
```

对应 C++ 测试中的循环（第 319–324 行）：
```cpp
for (size_t i = 0; i < n_repeats * 4; ++i) {
    auto qram_data_reg = AddRegister("data", UnsignedInteger, data_size)(state);
    GroverOperator(&qram, qram_addr_reg, qram_data_reg, search_data_reg)(state);
    (RemoveRegister(qram_data_reg))(state);   // 每次迭代后清理临时 data 寄存器
}
```

#### 3.6 量子计数 GroverCount 的实现（第 314–384 行）

对应 C++ `grover_count_success_test`（第 449–503 行）：

| C++ 代码 | Python 实现 |
|---------|-----------|
| `System::add_register("count", UnsignedInteger, count_precision)` | `ps.AddRegister("count", ps.UnsignedInteger, precision_bits)(state)` |
| `Hadamard_Int_Full(addr/count)` | `ps.Hadamard_Int_Full("count/addr")(state)` |
| `GroverCount(&qram, count_reg, ...)` | `GroverOperator(...).conditioned_by_bit("count", i)(state)` |
| 逆向 QFT `InverseQFT` | `ps.InverseQFT("count")(state)` |

---

### 四、Shor 量子因数分解算法

#### 4.1 算法概述

Shor 算法通过量子相位估计找到模幂运算的周期 r，从而将大数分解转化为求 gcd，在 RSA 加密等领域有重要应用。复杂度 O((log N)³)，相对经典的亚指数分解有指数加速。

核心流程：
```
1. 选择随机数 a (与 N 互素)
2. 量子相位估计：找最小 r 使得 a^r ≡ 1 (mod N)
3. 计算 gcd(a^(r/2)±1, N) 得到因子
```

#### 4.2 C++ 实现 → Python 的精确对应

C++ `ShorTest.cpp`（第 7–26 行）：
```cpp
semi_classical_shor(N);  // 核心调用，迭代 100 次
```

对应 Python `shor.py` 中的 `SemiClassicalShor` 类（第 241–331 行）：

| C++ 步骤 | Python 实现（shor.py） | 代码位置 |
|---------|----------------------|---------|
| 半经典迭代测量 | `SemiClassicalShor.run()` | 第 282–330 行 |
| 创建工作量子比特 | `ps.AddRegisterWithHadamard("work_reg", ps.UnsignedInteger, 1)` | 第 300–302 行 |
| 受控模幂运算 | `ModMul("anc_reg", a, size-1-x, N).conditioned_by_all_ones("work_reg")` | 第 306–307 行 |
| 相位校正 | `ps.Phase_Bool("work_reg", phase)` | 第 311–313 行 |
| Hadamard + 测量 | `ps.Hadamard_Bool("work_reg")` + `ps.PartialTrace` | 第 316–320 行 |
| 逆向 QFT（隐式在迭代测量中）| `shor_postprocess(meas, size, a, N)` | 第 329 行 |
| 经典后处理 | `shor_postprocess()` | 第 166–196 行 |

#### 4.3 模幂运算的实现（第 39–65 行、198–239 行）

C++ Shor 实现使用自定义量子算术模块。Python 对应实现：

```python
# 模幂函数（纯数学，第 39–65 行）
def general_expmod(a: int, x: int, N: int) -> int:
    """经典平方-乘算法：a^x mod N"""
    if x == 0: return 1
    if x & 1:  # 奇数
        return (general_expmod(a, x-1, N) * a) % N
    else:       # 偶数
        half = general_expmod(a, x // 2, N)
        return (half * half) % N

# 受控模乘算子（第 198–239 行）
class ModMul:
    def __call__(self, state: ps.SparseState) -> None:
        def modmul_func(val: int) -> int:
            return (val * self.opnum) % self.N
        op = ps.CustomArithmetic([self.reg], 64, 64, modmul_func)
        op.conditioned_by_all_ones(cond_reg)(state)
```

**关键设计：**
- `CustomArithmetic` 是 Register Level Programming 的极致体现——直接写 Python 函数描述整数到整数的映射，由 SparQ 自动编译为量子算术电路
- 无需手动构造受控-NOT 和单比特门组合

#### 4.4 半经典 Shor 的量子电路流程（第 282–330 行）

```
for x in range(size):           # size = 2 * n_bits(N)
    ① work_reg = |0⟩ + |1⟩           (通过 Hadamard)
    ② 受控模乘 a^(2^(size-1-x)) mod N (conditioned_by_all_ones on work_reg)
    ③ 相位校正 from prior results
    ④ Hadamard on work_reg
    ⑤ 测量 work_reg → result_bit
    ⑥ 移除 work_reg
    ↓
    合并 bits → meas_result (整数)
    ↓
    shor_postprocess → period r → factors p, q
```

#### 4.5 经典后处理（第 166–196 行）

```python
def shor_postprocess(meas: int, size: int, a: int, N: int) -> Tuple[int, int]:
    # ① 找最佳分数近似 y/Q
    r, c = find_best_fraction(y, Q, N)
    
    # ② 验证 r 的有效性（偶数、≠ -1 mod N）
    check_period(r, a, N)
    
    # ③ 提取因子
    a_exp_r_half = general_expmod(a, r // 2, N)
    p = math.gcd(a_exp_r_half + 1, N)
    q = math.gcd(a_exp_r_half - 1, N)
```

---

### 五、QDA 量子离散绝热线性系统求解器

#### 5.1 算法概述

QDA（Quantum Discrete Adiabatic）利用离散绝热定理求解线性系统 Ax = b，达到 O(κ log(κ/ε)) 的最优规模（κ 为条件数）。

PySparQ 版本按 C++ `Walk_s_Tridiagonal` 和 `Walk_s_via_QRAM` 的寄存器级结构实现：矩阵块编码、`StatePrepViaQRAM`、`Hadamard_Int_Full`、反射、受控旋转和全局相位都由 Python 侧调用底层 primitive 组合完成。它不会把整个 QDA solver 作为一个 C++ 绑定来绕过 Python 实现。当前 `qda_solve()` 会执行量子 walk 序列；由于测量和 post-selection 读出尚未接上，运行完成后会显式抛出 `RuntimeError`，避免用经典 `np.linalg.solve` 掩盖算法路径中的错误。

核心思路：
```
H(s) = (1-f(s))·H₀ + f(s)·H₁   （插值哈密顿量）
W_s = R · H_s                   （量子游走算子）
通过对 s 离散化执行 W_s 序列，使系统从 |b⟩ 演化到 |x⟩
```

#### 5.2 插值函数 f(s) 的实现（qda_solver.py 第 41–72 行）

```python
def compute_fs(s: float, kappa: float, p: float) -> float:
    """
    来自 PRX Quantum 论文 Eq.(69):
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

对应 C++ QDATest.cpp 第 410–413 行中的调用：
```cpp
WalkSequence_via_QRAM_Debug(&qram_A, &qram_b, mat, b, ...,
    steps, kappa, p, data_size, rational_size, ...)(state);
```

#### 5.3 块编码 H(s) 的实现（qda_solver.py 第 196–314 行）

`BlockEncodingHs` 类实现插值哈密顿量的块编码，对应 C++ 中的 `Walk_s_via_QRAM` 操作。核心操作序列（第 248–308 行）：

```python
def __call__(self, state: ps.SparseState) -> None:
    ps.Hadamard_Bool(self.anc_3)(state)         # H
    self.enc_b.dag(state)                        # 状态准备逆操作
    ps.X_Bool(self.anc_1, 0)(state)          # X
    ps.Reflection_Bool(self.main_reg, True)      # 反射算子（关于 |0⟩）
          .conditioned_by_all_ones([self.anc_1, self.anc_3, self.anc_4])(state)
    ps.X_Bool(self.anc_1, 0)(state)
    self.enc_b(state)                            # 状态准备
    
    # 旋转序列：R_s(f(s))
    ps.X_Bool(self.anc_4, 0)(state)
    ps.Rot_Bool(self.anc_2, self.R_s).conditioned_by_all_ones(self.anc_4)(state)
    ...
    self.enc_A.conditioned_by_all_ones([self.anc_1, self.anc_2])(state)  # 块编码 A
```

#### 5.4 QRAM 数据准备的完整流程（对应 QDATest.cpp 第 356–391 行）

```python
# Step 1: 将浮点矩阵/向量量化为整数
conv_A = scaleAndConvertVector(mat, exponent=15, data_size=50)
conv_b = scaleAndConvertVector(b, exponent=15, data_size=50)

# Step 2: 构建 QRAM 树结构
data_tree_A = make_vector_tree(conv_A, data_size)
data_tree_b = make_vector_tree(conv_b, data_size)

# Step 3: 建立 QRAM 电路
addr_size = log_column_size * 2 + 1   # 比 data 多 1 bit（用于归一化）
qram_A = ps.QRAMCircuit_qutrit(addr_size, data_size)
qram_A.set_memory(data_tree_A)
qram_b = ps.QRAMCircuit_qutrit(log_column_size + 1, data_size)
qram_b.set_memory(data_tree_b)

# Step 4: 添加寄存器
main_reg = ps.AddRegister("main_reg", ps.UnsignedInteger, log_column_size)(state)
anc_UA   = ps.AddRegister("anc_UA",   ps.UnsignedInteger, log_column_size)(state)
anc_4    = ps.AddRegister("anc_4",    ps.Boolean, 1)(state)
anc_3    = ps.AddRegister("anc_3",    ps.Boolean, 1)(state)
anc_2    = ps.AddRegister("anc_2",    ps.Boolean, 1)(state)
anc_1    = ps.AddRegister("anc_1",    ps.Boolean, 1)(state)
```

#### 5.5 旋转矩阵的构造（qda_solver.py 第 74–95 行）

```python
def compute_rotation_matrix(fs: float) -> List[complex]:
    """
    旋转矩阵 R_s:
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

#### 5.6 Dolph-Chebyshev 过滤（qda_solver.py 第 102–189 行）

用于提高 QDA 解的精度：

```python
def dolph_chebyshev(epsilon: float, l: int, phi: float) -> float:
    beta = math.cosh(math.acosh(1.0 / epsilon) / l)
    return epsilon * chebyshev_T(l, beta * math.cos(phi))
```

对应 C++ QDATest.cpp 第 550–566 行的滤波步骤：
```cpp
int l_ = static_cast<int>(floor(kappa * log(2.0 / epsilon_)));
vector<double> weights = ComputeFourierCoeffs(epsilon_, l_);
// 构建权重 QRAM → 应用 LCU
```

---

### 六、CKS 量子线性系统求解器（Childs-Kothari-Somma）

#### 6.1 算法概述

CKS 算法利用 Chebyshev 多项式逼近和量子游走，在稀疏矩阵条件下达到 O(κ log(κ/ε)) 的复杂度，比 HHL 类算法有更好的常数因子。

PySparQ 版本同样只通过底层 primitive 复刻 C++ `HamiltonianSimulationTest.cpp` 的量子游走路径。Python 的 `SparseMatrix` 使用 C++ 同款紧凑 QRAM 布局，`TOperator`、`QuantumBinarySearch_Fast`、`GetRowAddr`、`GetDataAddr`、`GetQWRotateAngle_Int_Int_Int`、`CondRot_Fixed_Bool`、`QRAMLoad` 和寄存器交换等步骤按 C++ 顺序组合；没有绑定完整 C++ CKS solver，也不暴露用户传入 Python function 的旧泛化 CondRot API。

核心流程：
```
1. Chebyshev 多项式系数 c_j = erfc((j+0.5)/√b) * 2
2. 构造量子游走算子 W = T† · P_0 · T · Swap
3. LCU: Σ_j c_j W^(2j+1) 逼近 1/A
```

#### 6.2 Chebyshev 系数的实现（cks_solver.py 第 41–127 行）

```python
class ChebyshevPolynomialCoefficient:
    def __init__(self, b: int):
        # b = κ² log(κ/ε)，由 LCU_Container 在初始化时计算
        self.b = b

    def coef(self, j: int) -> float:
        """第 j 步的 Chebyshev 系数"""
        if self.b > 100:
            # 大 b 值：使用渐近近似（erfc）
            return math.erfc((j + 0.5) / math.sqrt(self.b)) * 2
        else:
            # 小 b 值：精确计算
            ret = 0.0
            for i in range(j + 1, self.b + 1):
                ret += self.C(2 * self.b, self.b + i)
            return ret * 4

    def sign(self, j: int) -> bool:
        """奇数步为负（加负号）"""
        return (j & 1) == 1

    def step(self, j: int) -> int:
        """第 j 步的游走步数 = 2j + 1"""
        return 2 * j + 1
```

对应 C++ HamiltonianSimulationTest.cpp 第 139–149 行：
```cpp
auto test_chebyshev_polynomial_coef() {
    ChebyshevPolynomialCoefficient chebyshev_obj(b);
    for (size_t j = 0; j <= 64; ++j)
        fmt::print("j={}, coef={}\n", j,
            chebyshev_obj.coef(j) * (chebyshev_obj.sign(j) ? -1 : 1));
}
```

#### 6.3 稀疏矩阵表示（cks_solver.py 第 225–340 行）

```python
@dataclass
class SparseMatrixData:
    n_row: int           # 行数
    nnz_col: int         # 每列最大非零元素数
    data: List[int]      # 扁平化矩阵数据（量化后整数）
    data_size: int       # 量化位数（通常 32）
    positive_only: bool  # 矩阵是否全正（决定旋转矩阵形式）
    sparsity_offset: int  # QRAM 稀疏寻址偏移

class SparseMatrix:
    @classmethod
    def from_dense(cls, matrix: np.ndarray, data_size: int = 32,
                   positive_only: bool = None) -> "SparseMatrix":
        # ① 检测正性
        # ② 归一化 + 量化
        Amax = 2 ** (data_size - 1) - 1
        scaled = matrix / max_val * Amax
        # ③ 转两补码（负数）
        # ④ 计算 nnz_col
```

对应 C++ `SparseMatrix` 类（`hamiltonian_simulation.h`）和 `generate_simplest_sparse_matrix_unsigned_2()`。

#### 6.4 量子游走的实现（cks_solver.py 第 637–710 行）

```python
class QuantumWalk:
    """
    W = T† · P_0 · T · Swap

    T: 状态准备算子（|j⟩|0⟩ → Σ_k √(A_jk/‖A_j‖)|j⟩|k⟩）
    P_0: 相位反射（关于 |0⟩ 态）
    Swap: 行列交换
    """
    def __call__(self, state: ps.SparseState) -> None:
        # Tdagger
        t_op.dag(state)
        
        # 相位反射
        ps.ZeroConditionalPhaseFlip(
            [self.b1_reg, self.k_reg, self.b2_reg, self.k_comp_reg]
        )(state)
        
        # T
        t_op(state)
        
        # Swap 行列
        ps.Swap_General_General(self.j_reg, self.k_reg)(state)
        ps.Swap_General_General(self.b1_reg, self.b2_reg)(state)
        ps.Swap_General_General(self.j_comp_reg, self.k_comp_reg)(state)
```

对应 C++ HamiltonianSimulationTest.cpp 第 116–118 行：
```cpp
for (int i = 0; i < 99; ++i)
    QuantumWalk(qram, j, b1, k, b2, j_comp, k_comp, ...)(system_states);
```

CKS 的 fidelity 测试与 C++ `automatic_Chebyshev_test` 对齐时，需要按 C++ 逻辑只比较 post-selection 后的目标寄存器，并使用 `SparseMatrix` 实际编码出的稠密矩阵表示（包含 `nnz_col` 归一化），不能直接拿原始输入矩阵 `A / ||A||` 做目标态。

#### 6.5 LCU 容器（cks_solver.py 第 843–930 行）

```python
class LCUContainer:
    def __init__(self, mat, kappa, eps, qram=None):
        # b = κ² log(κ/ε) — 决定 Chebyshev 展开项数
        self.b = int(kappa * kappa * (math.log(kappa) - math.log(eps)))
        # j0 = √(b log(4b/ε)) — 截断点
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

对应 C++ `LCU_Container_NoiseFree`（HamiltonianSimulationTest.cpp 第 275–326 行）：
```cpp
obj.ExternalInput<Hadamard_Int>(addr_size);  // Hadamard 初始化
while (obj.Step()) {
    auto [state, success_rate] = obj.PartialTrace_Nondestructive();
    double fidelity = get_fidelity(m, target_result);
    // 记录每步成功率和保真度
}
```

---

### 七、GHZ 态生成（待实现）

#### 7.1 C++ 实现解析

C++ `GHZTest.cpp`（第 26–72 行）的 GHZ 态生成逻辑极其简洁：

```cpp
// 寄存器布局：ctrl (1 bit) + main registers (分块，每块 ≤ 64 bits)
// 设 nqubit = 65，则：quotient = 1, remainder = 0
// 实际布局：ctrl(1) + main(64)

FlipBools("ctrl")(state);       // ctrl → |1⟩
Hadamard_Bool("ctrl")(state);   // H → (|0⟩+|1⟩)/√2

// 对每个主量子比特应用受控 X（ctrl=1 时翻转）
for (int i = 1; i < remainder + 1; i++)
    X_Bool("main", i-1).conditioned_by_all_ones("ctrl")(state);
for (auto reg : reg_names)
    FlipBools(System::get(reg)).conditioned_by_all_ones("ctrl")(state);
```

**生成的态：`(|0...0⟩ + |1...1⟩)/√2`**（nqubit 个量子比特的 GHZ 态）

#### 7.2 Python 转译方案

```python
def ghz_state(nqubit: int) -> ps.SparseState:
    """生成 GHZ 态：(|0...0⟩ + |1...1⟩)/√2"""
    ps.System.clear()
    state = ps.SparseState()
    
    # 处理 > 64 比特的情况（寄存器分块）
    quotient, remainder = divmod(nqubit - 1, 64)
    
    ctrl = ps.AddRegister("ctrl", ps.UnsignedInteger, 1)(state)
    # 添加主寄存器（每块最多 64 位）
    main_regs = []
    for k in range(1, quotient + 1):
        reg = ps.AddRegister(f"main{k}", ps.UnsignedInteger, remainder or 64)(state)
        main_regs.append(reg)
    if remainder != 0:
        main_reg = ps.AddRegister("main", ps.UnsignedInteger, remainder)(state)
        main_regs.append(main_reg)
    
    # GHZ 电路
    ps.X_Bool("ctrl", 0)(state)       # |0⟩ → |1⟩
    ps.Hadamard_Bool("ctrl")(state)        # (|0⟩+|1⟩)/√2
    
    for reg in main_regs:
        for pos in range(ps.System.size_of(reg)):
            ps.X_Bool(reg, pos).conditioned_by_nonzeros("ctrl")(state)
    
    return state
```

---

### 八、实现原则总结

#### 8.1 量子算法转译的七大原则

**原则一：寄存器先行，状态跟随**
```python
# 错误：状态在寄存器之前创建
state = ps.SparseState()
addr = ps.AddRegister("addr", ...)  # ← 错误顺序

# 正确：先定义寄存器体系，再创建状态
ps.System.add_register("addr", ps.UnsignedInteger, n_bits)
state = ps.SparseState()  # 或 ps.SparseState() 自动继承已注册的寄存器
```

**原则二：临时寄存器用完即删**
```python
# GroverOracle 每次创建 compare_less/equal，Oracle 返回前删除
compare_less = ps.AddRegister("compare_less", ps.Boolean, 1)(state)
# ... 使用 compare_less ...
ps.RemoveRegister(compare_less)(state)   # 防止状态空间指数膨胀
```

**原则三：条件操作链式调用**
```python
# 所有条件操作均返回 self，支持链式
op = ps.ZeroConditionalPhaseFlip([cond_reg])
op.conditioned_by_nonzeros(other_reg)(state)  # 多次加条件

# 对应 C++ 中：
# X_Bool("ctrl", 0).conditioned_by_all_ones("ctrl")(state);
```

**原则四：自伴算子用 `.dag()` 逆操作**
```python
# QRAMLoad, Hadamard, Swap — 自伴（U = U†）
ps.QRAMLoad(qram, addr, data)(state)   # 前向
ps.QRAMLoad(qram, addr, data)(state)   # 逆操作（同一操作）

# 一般算子
t_op.dag(state)   # 逆操作
```

**原则五：浮点量化是量子-经典接口的关键**
```python
# QDA/QDA: 浮点矩阵 → 定点整数
# 量化因子：2^exponent，exponent 通常取 15（保证精度）
Amax = 2 ** (data_size - 1) - 1
scaled = matrix / max_val * Amax  # 归一化到 [-Amax, Amax]
int_data = scaled.astype(int)      # 量化
```

**原则六：自顶向下，先算法后实现**
```
正确顺序：
① 理解算法数学公式（Grover 迭代 / Shor 相位估计 / Chebyshev 展开）
② 定义寄存器角色（主寄存器、辅助寄存器、精度寄存器）
③ 写出算法主体流程（Python 函数或类）
④ 填入各步对应的 PySparQ 操作

错误顺序：
① 直接从 C++ 代码逐行翻译
② 忽略寄存器长度设计导致溢出
```

**原则七：测量结果的概率归一化**
```python
# PartialTrace 返回的 prob 是未归一化的，需要处理
prob_inv0 = PartialTraceSelect({ anc_UA, anc_2, anc_3 }, {0, 0, 0})(state)
prob0 = (1.0 / prob_inv0) ** 2   # 归一化得到成功概率

# 对应 C++ QDATest.cpp 第 415–416 行：
# double prob_inv0 = PartialTraceSelect({anc_UA, anc_2, anc_3}, {0, 0, 0})(state);
# double prob0 = (1.0 / prob_inv0) * (1.0 / prob_inv0);
```

#### 8.2 常见寄存器配置一览

| 算法 | 主寄存器宽度 | 数据寄存器宽度 | Ancilla 数量 | 特殊寄存器 |
|------|------------|-------------|------------|-----------|
| Grover | ceil(log2(N)) | 64 | 2 (compare flags) | search(target) |
| Shor | 2×n_bits(N) | — | 1 (work qubit) | ancilla (模幂结果) |
| QDA | ceil(log2(n)) | 50 | 4 (anc_1~anc_4) | anc_UA (块编码) |
| CKS | ceil(log2(n_row)) | 32 | 4 (b1,b2,j_comp,k_comp) | sparse_offset |
| GHZ | ceil(log2(n)) | — | 0 | ctrl (1 bit) |

---

### 九、缺失算法的实现路线图

#### 9.1 QFT（优先实现）

C++ 有完整的 `QFT_Full()` 和 `QFT()` 实现（`SparQ/include/qft.h`），转译简单：

```python
def qft(state: ps.SparseState, reg: str) -> None:
    """对应 C++ QFT("main")(state)"""
    n = ps.System.size_of(reg)
    for i in range(n):
        ps.Hadamard_Bool(reg, i)(state)
        for j in range(i + 1, n):
            ps.CPhase_Bool(reg, j, i, math.pi / (2 ** (j - i)))(state)

def inverse_qft(state: ps.SparseState, reg: str) -> None:
    """逆 QFT：反向迭代受控相位"""
    n = ps.System.size_of(reg)
    for i in reversed(range(n)):
        for j in reversed(range(i + 1, n)):
            ps.CPhase_Bool(reg, j, i, -math.pi / (2 ** (j - i)))(state)
        ps.Hadamard_Bool(reg, i)(state)
```

#### 9.2 GHZ（次优先）

参见第 7.2 节的转译方案，实现工作量小且逻辑清晰。

#### 9.3 QCNN（需先恢复 C++）

`Experiments/QCNN/QCNNTest.cpp` 目前被 `#if false` 禁用，需先：
1. 修复 C++ QCNN 实现
2. 在 `PySparQ/pysparq/algorithms/qcnn.py` 中转译

---

### 十、给后续开发者的经验总结

1. **从 C++ 到 Python 不是逐行翻译**：理解算法的数学本质（Grover = 振幅放大、Shor = 相位估计、CKS = Chebyshev 逼近），用 Python 的思维重写。PySparQ 的 `CustomArithmetic` 等接口比 C++ 版本更简洁，直接传入 Python lambda 即可。

2. **寄存器分块是超大量子比特系统的关键**：超过 64 比特时，需要像 C++ GHZTest 那样将寄存器分成多个 `UnsignedInteger` 子块（每块 ≤ 64 位）。Python 层通过命名约定（`"main1"`, `"main2"`）和 `FlipBools(System::get(reg))` 实现。

3. **稀疏态优化的核心是零振幅剪枝**：PySparQ 的 `SparseState` 仅存储非零振幅，`ClearZero()` 操作在每步游走/Chebyshev 迭代后调用，防止状态数爆炸。这是 Grover（O(√N) 状态增长）和 CKS（O(κ) 状态增长）能处理大问题的关键。

4. **测试先于实现**：参考 `test_doc_examples.py` 中的模式，用 pytest 验证各子模块的正确性，再组装完整算法。单元测试覆盖：Oracle（振幅翻转正确性）、扩散（关于叠加态的反射）、量子游走（Chebyshev 逼近精度）。

5. **噪声模型的可组合性**：C++ 测试中的 `set_noise_models()` 调用在 PySparQ 中等价于：
```python
qram = ps.QRAMCircuit_qutrit(...)
qram.set_noise_model("depolarizing", 1e-4)  # 对应 C++ qram->set_noise_models(...)
```
这对评估真实硬件上的算法行为非常重要。

