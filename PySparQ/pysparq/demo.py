"""Standalone native simulator experiments: ``pysparq info|run|gui``.

These CPU experiments expose complex amplitudes, phase-sensitive reference
errors, sparse support, and actual classical construction/execution times.
They do not submit quantum hardware jobs or estimate quantum execution time.
"""
from __future__ import annotations

import argparse
import cmath
import hashlib
import json
import math
import platform
import random
import time
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class Experiment:
    case: str = "ghz"
    qubits: int = 4
    seed: int = 20261109
    basis: int = 3

    def validate(self) -> None:
        if self.case not in {"ghz", "qft", "stateprep"}:
            raise ValueError("case must be ghz, qft or stateprep")
        limit = 32 if self.case == "ghz" else 10
        if type(self.qubits) is not int or not 2 <= self.qubits <= limit:
            raise ValueError(f"{self.case} requires 2..{limit} qubits")
        if type(self.seed) is not int or not 0 <= self.seed < 2**32:
            raise ValueError("seed must be an integer in [0,2**32)")
        if type(self.basis) is not int or not 0 <= self.basis < 2**self.qubits:
            raise ValueError("basis must be an integer in [0,2**qubits)")


def information() -> dict:
    import pysparq as ps

    return {"software": "SparQSim / PySparQ", "version": ps.__version__,
            "backend": "native CPU sparse-state simulator", "gpu_execution": False,
            "hardware_submission": False, "python": platform.python_version(),
            "platform": platform.platform(),
            "native_sha256": hashlib.sha256(Path(ps._core.__file__).read_bytes()).hexdigest()}


def run(config: Experiment) -> dict:
    """Run a native experiment and its inverse against independent references.

    System register state is process-global. Use a separate process for each
    experiment when other code shares the simulator (the GUI does this).
    """
    config.validate()
    import pysparq as ps

    started = time.perf_counter()
    ps.System.clear()
    try:
        ps.System.add_register("q", ps.UnsignedInteger, config.qubits)
        state = ps.SparseState()
        size = 2**config.qubits
        reference: dict[int, complex]
        tick = time.perf_counter()
        initial_basis = 0
        if config.case == "ghz":
            reference = {0: complex(2**-0.5), size-1: complex(2**-0.5)}
            target = None
        elif config.case == "qft":
            initial_basis = config.basis
            ps.Init_Unsafe("q", initial_basis)(state)
            reference = {index: cmath.exp(2j*math.pi*initial_basis*index/size)/math.sqrt(size)
                         for index in range(size)}
            target = None
        else:
            rng = random.Random(config.seed)
            target = [0j] + [complex(rng.uniform(-1, 1), rng.uniform(-1, 1))
                              for _ in range(size-1)]
            norm = math.sqrt(sum(abs(value)**2 for value in target))
            reference = {index: value/norm for index, value in enumerate(target)}
        input_reference_s = time.perf_counter()-tick
        tick = time.perf_counter()
        if config.case == "ghz":
            operations = [ps.Hadamard_Partial("q", {0})] + [
                ps.X_Bool("q", bit).conditioned_by_bit("q", 0)
                for bit in range(1, config.qubits)]
        elif config.case == "qft":
            operations = [ps.QFT("q")]
        else:
            operations = [ps.Rot_GeneralStatePrep("q", target)]
        construct_s = time.perf_counter()-tick
        tick = time.perf_counter()
        for operation in operations:
            operation(state)
        execution_s = time.perf_counter()-tick
        register = ps.System.get_id("q")

        def snapshot() -> dict[int, complex]:
            result: dict[int, complex] = {}
            for branch in state.basis_states:
                key = branch.get(register).value
                result[key] = result.get(key, 0j)+branch.amplitude
            return {key: value for key, value in result.items() if value != 0}

        actual = snapshot()
        keys = set(actual) | set(reference)
        trace = sum(abs(value)**2 for value in actual.values())
        overlap = sum(reference.get(key, 0j).conjugate()*value for key, value in actual.items())
        error = math.sqrt(sum(abs(actual.get(key, 0j)-reference.get(key, 0j))**2 for key in keys))
        tick = time.perf_counter()
        for operation in reversed(operations):
            operation.dag(state)
        inverse_s = time.perf_counter()-tick
        returned = snapshot()
        inverse_error = math.sqrt(sum(abs(value-(1 if key == initial_basis else 0))**2
                                      for key, value in returned.items()) +
                                  (0 if initial_basis in returned else 1))
        payload = None if target is None else [[value.real.hex(), value.imag.hex()] for value in target]
        return {"schema": "pysparq.demo.v1", "software": information(),
                "configuration": asdict(config),
                "resources": {"logical_qubits": config.qubits, "hilbert_dimension": size,
                              "nonzero_amplitudes": len(actual),
                              "explicit_stateprep_matrix_entries": size*size if target is not None else 0},
                "validation": {"normalization": trace, "trace_error": abs(trace-1),
                               "reference_fidelity": abs(overlap)**2, "phase_sensitive_l2_error": error,
                               "inverse_l2_error": inverse_error},
                "timing": {"input_reference_s": input_reference_s,
                           "operator_construction_s": construct_s, "native_execution_s": execution_s,
                           "inverse_execution_s": inverse_s, "total_s": time.perf_counter()-started},
                "amplitudes": [{"basis": key, "real": value.real, "imag": value.imag,
                                "probability": abs(value)**2} for key, value in sorted(actual.items())],
                "input": {"target_sha256": None if payload is None else
                          hashlib.sha256(json.dumps(payload, separators=(",", ":")).encode()).hexdigest(),
                          "stateprep_first_amplitude": None if target is None else [0, 0]},
                "scope": {"timing_kind": "classical CPU simulation", "quantum_runtime_estimate": False,
                          "application_speedup_certificate": False}}
    finally:
        ps.System.clear()


def run_rir_document(program_source: str, memory_source: str | None) -> dict:
    """Execute one RIR JSON document natively (wide registers and QRAM included).

    ``program_source`` and ``memory_source`` are filesystem paths or inline
    JSON strings; ``run_rir`` performs the actual native sparse-state
    execution, so widths beyond 64 bits and wide sparse QRAM queries run on
    the real backend. The exported record carries provenance, the program
    digest and every nonzero amplitude keyed by full-width register values.
    """
    import pysparq as ps

    document = ps.load_rir(program_source)
    memory = None
    if memory_source:
        text = memory_source.lstrip()
        if text.startswith("{"):
            memory = json.loads(memory_source)
        else:
            memory = json.loads(Path(memory_source).read_text(encoding="utf-8"))
        # JSON object keys are strings; run_rir addresses are integers.
        memory = {name: {int(address): word for address, word in cells.items()}
                  if isinstance(cells, dict) else cells
                  for name, cells in memory.items()}
    started = time.perf_counter()
    result = ps.run_rir(document, memory)
    elapsed = time.perf_counter()-started
    program_bytes = json.dumps(document, sort_keys=True, separators=(",", ":")).encode()
    return {"schema": "pysparq.rir-run.v1", "software": information(),
            "program_sha256": hashlib.sha256(program_bytes).hexdigest(),
            "registers": [{"name": name, "width": width} for name, width in result.registers],
            "amplitudes": [
                {"values": list(key), "real": value.real, "imag": value.imag,
                 "probability": abs(value)**2}
                for key, value in sorted(result.amplitudes.items())],
            "timing": {"native_execution_s": elapsed, "total_s": elapsed},
            "scope": {"timing_kind": "classical CPU simulation", "quantum_runtime_estimate": False,
                      "application_speedup_certificate": False}}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("info", help="show the installed native backend and provenance")
    experiment = commands.add_parser("run", help="run GHZ, Fourier or complex state preparation")
    experiment.add_argument("--case", choices=["ghz", "qft", "stateprep"], default="ghz")
    experiment.add_argument("--qubits", type=int, default=4)
    experiment.add_argument("--seed", type=int, default=20261109)
    experiment.add_argument("--basis", type=int, default=3)
    experiment.add_argument("--out", type=Path)
    rir = commands.add_parser(
        "rir", help="execute an RIR JSON program natively (wide registers and QRAM)")
    rir.add_argument("--program", required=True,
                     help="path to the RIR JSON document (or inline JSON text)")
    rir.add_argument("--memory",
                     help="optional path to a JSON object of QRAM cells keyed by resource name")
    rir.add_argument("--out", type=Path)
    gui = commands.add_parser("gui", help="start the packaged local simulator workbench")
    gui.add_argument("--port", type=int, default=0)
    gui.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()
    try:
        if args.command == "gui":
            from .demo_server import serve

            serve(port=args.port, open_browser=not args.no_browser)
            return
        if args.command == "rir":
            result = run_rir_document(args.program, args.memory)
        else:
            result = information() if args.command == "info" else run(
                Experiment(args.case, args.qubits, args.seed, args.basis))
        text = json.dumps(result, indent=2, allow_nan=False)+"\n"
        if getattr(args, "out", None):
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(text, encoding="utf-8")
        else:
            print(text, end="")
    except (ValueError, TypeError, OSError, RuntimeError, json.JSONDecodeError) as exc:
        parser.exit(2, f"pysparq: {exc}\n")


if __name__ == "__main__":
    main()
