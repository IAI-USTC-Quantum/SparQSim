"""Run the installed native simulator product, including its real GUI worker."""
import cmath
import json
import math
import re
import subprocess
import sys
import threading
import urllib.error
import urllib.request

import pytest
from pysparq.demo import Experiment, run
from pysparq.demo_server import make_server


@pytest.mark.parametrize("case", ["ghz", "qft", "stateprep"])
@pytest.mark.parametrize("qubits", [2, 4])
def test_actual_native_state_and_phase_sensitive_inverse(case, qubits):
    result = run(Experiment(case=case, qubits=qubits))
    assert result["resources"]["logical_qubits"] == qubits
    for key in ("trace_error", "phase_sensitive_l2_error", "inverse_l2_error"):
        assert result["validation"][key] < 2e-11
    assert result["validation"]["reference_fidelity"] == pytest.approx(1, abs=2e-11)
    assert result["software"]["gpu_execution"] is False
    assert result["scope"]["quantum_runtime_estimate"] is False
    if case == "qft":
        amplitudes = {item["basis"]: complex(item["real"], item["imag"])
                      for item in result["amplitudes"]}
        for index in range(2**qubits):
            assert amplitudes[index] == pytest.approx(
                cmath.exp(2j*math.pi*3*index/2**qubits)/math.sqrt(2**qubits), abs=2e-11)
    if case == "stateprep":
        assert result["input"]["stateprep_first_amplitude"] == [0, 0]
        assert len(result["input"]["target_sha256"]) == 64


def test_32_qubit_ghz_stays_sparse_and_preserves_exact_support():
    result = run(Experiment(qubits=32))
    assert result["resources"]["hilbert_dimension"] == 2**32
    assert result["resources"]["nonzero_amplitudes"] == 2
    assert {item["basis"] for item in result["amplitudes"]} == {0, 2**32-1}
    assert [item["probability"] for item in result["amplitudes"]] == pytest.approx([0.5, 0.5])


def test_state_preparation_is_reproducible_and_phase_sensitive():
    first = run(Experiment(case="stateprep", seed=9))
    second = run(Experiment(case="stateprep", seed=9))
    other = run(Experiment(case="stateprep", seed=10))
    assert first["amplitudes"] == second["amplitudes"]
    assert first["input"] == second["input"]
    assert first["input"] != other["input"]
    assert any(abs(item["imag"]) > 0.1 for item in first["amplitudes"])


@pytest.mark.parametrize("kwargs", [{"qubits": 1}, {"qubits": 33}, {"qubits": True},
    {"case": "qft", "qubits": 11}, {"case": "stateprep", "qubits": 11}, {"seed": -1},
    {"seed": True}, {"seed": 2**32}, {"basis": -1}, {"basis": 16}, {"case": "unknown"}])
def test_invalid_inputs_fail_before_native_simulation(kwargs):
    with pytest.raises(ValueError):
        run(Experiment(**kwargs))


def test_real_cli_writes_complete_native_report(tmp_path):
    output = tmp_path/"state.json"
    child = subprocess.run([sys.executable, "-m", "pysparq.demo", "run", "--case", "ghz",
                            "--qubits", "4", "--out", str(output)],
                           capture_output=True, text=True, check=True)
    assert not child.stdout
    report = json.loads(output.read_text())
    assert report["schema"] == "pysparq.demo.v1"
    assert report["resources"]["nonzero_amplitudes"] == 2
    assert len(report["software"]["native_sha256"]) == 64


def test_packaged_gui_and_real_subprocess_native_execution():
    with make_server() as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            with urllib.request.urlopen(base) as response:
                html = response.read().decode()
                assert "frame-ancestors 'none'" in response.headers["Content-Security-Policy"]
            token = re.search(r"const token='([^']+)'", html).group(1)
            data = json.dumps({"case": "stateprep", "qubits": 3}).encode()
            def post(headers):
                return urllib.request.urlopen(urllib.request.Request(base+"/api/run", data=data,
                    headers={"Content-Type": "application/json", **headers}))
            with pytest.raises(urllib.error.HTTPError) as missing:
                post({})
            assert missing.value.code == 403
            with pytest.raises(urllib.error.HTTPError) as foreign:
                post({"X-Session": token, "Origin": "https://example.invalid"})
            assert foreign.value.code == 403
            with post({"X-Session": token}) as response:
                report = json.load(response)
            assert report["validation"]["phase_sensitive_l2_error"] < 1e-11
            assert report["scope"]["application_speedup_certificate"] is False
            with urllib.request.urlopen(base+"/api/info") as response:
                assert json.load(response)["hardware_submission"] is False
        finally:
            server.shutdown()
            thread.join(timeout=3)
