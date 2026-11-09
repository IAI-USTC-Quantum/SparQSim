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


def test_real_cli_executes_wide_rir_program_natively(tmp_path):
    program = {
        "tag": "Program", "version": "0.3", "entry": "main",
        "modules": [{"tag": "Module", "name": "main", "attributes": [],
                     "registers": [
                         {"tag": "Register", "name": "address",
                          "type": {"tag": "RegType", "kind": "uint", "width": 65}},
                         {"tag": "Register", "name": "data",
                          "type": {"tag": "RegType", "kind": "uint", "width": 129}}],
                     "locals": [],
                     "resources": [{"tag": "Resource", "name": "bank", "type": {
                         "tag": "QRAM", "address_width": 65, "data_width": 129}}],
                     "body": [
                         {"tag": "Primitive", "op": "h", "value": None, "angle": None,
                          "operands": [{"tag": "Ref", "type": {"tag": "RegType", "kind": "uint", "width": 1},
                                        "parts": [{"tag": "Span", "register": "address", "start": 64, "width": 1}]}]},
                         {"tag": "Control",
                          "register": {"tag": "Ref", "type": {"tag": "RegType", "kind": "uint", "width": 1},
                                       "parts": [{"tag": "Span", "register": "address", "start": 64, "width": 1}]},
                          "value": 0,
                          "body": [{"tag": "Load", "resource": "bank",
                                    "address": {"tag": "Ref", "type": {"tag": "RegType", "kind": "uint", "width": 65},
                                                "parts": [{"tag": "Span", "register": "address", "start": 0, "width": 65}]},
                                    "data": {"tag": "Ref", "type": {"tag": "RegType", "kind": "uint", "width": 129},
                                             "parts": [{"tag": "Span", "register": "data", "start": 0, "width": 129}]}}]}]}]}
    memory = {"bank": {0: (1 << 128)+3}}
    program_file = tmp_path/"wide.json"
    program_file.write_text(json.dumps(program))
    memory_file = tmp_path/"memory.json"
    memory_file.write_text(json.dumps(memory))
    output = tmp_path/"result.json"
    child = subprocess.run([sys.executable, "-m", "pysparq.demo", "rir",
                            "--program", str(program_file), "--memory", str(memory_file),
                            "--out", str(output)],
                           capture_output=True, text=True, check=True)
    assert not child.stdout
    report = json.loads(output.read_text())
    assert report["schema"] == "pysparq.rir-run.v1"
    assert len(report["program_sha256"]) == 64
    assert report["registers"] == [{"name": "address", "width": 65}, {"name": "data", "width": 129}]
    assert report["amplitudes"] == [
        {"values": [0, (1 << 128)+3], "real": pytest.approx(2**-0.5),
         "imag": pytest.approx(0), "probability": pytest.approx(0.5)},
        {"values": [1 << 64, 0], "real": pytest.approx(2**-0.5),
         "imag": pytest.approx(0), "probability": pytest.approx(0.5)}]
    assert report["scope"]["quantum_runtime_estimate"] is False


def test_cli_rir_rejects_broken_program_with_clear_error(tmp_path):
    broken = tmp_path/"broken.json"
    broken.write_text('{"tag": "Module"}')
    child = subprocess.run([sys.executable, "-m", "pysparq.demo", "rir", "--program", str(broken)],
                           capture_output=True, text=True)
    assert child.returncode == 2
    assert "Program" in child.stderr


def test_packaged_gui_and_real_subprocess_native_execution():
    with make_server() as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            with urllib.request.urlopen(base) as response:
                html = response.read().decode()
                assert "frame-ancestors 'none'" in response.headers["Content-Security-Policy"]
                assert "Execute RIR natively" in html
            token = re.search(r"const token='([^']+)'", html).group(1)
            data = json.dumps({"case": "stateprep", "qubits": 3}).encode()
            def post(headers, path="/api/run", payload=data):
                return urllib.request.urlopen(urllib.request.Request(base+path, data=payload,
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


def _wide_qram_payload():
    program = {
        "tag": "Program", "version": "0.3", "entry": "main",
        "modules": [{"tag": "Module", "name": "main", "attributes": [],
                     "registers": [
                         {"tag": "Register", "name": "address",
                          "type": {"tag": "RegType", "kind": "uint", "width": 65}},
                         {"tag": "Register", "name": "data",
                          "type": {"tag": "RegType", "kind": "uint", "width": 129}}],
                     "locals": [],
                     "resources": [{"tag": "Resource", "name": "bank", "type": {
                         "tag": "QRAM", "address_width": 65, "data_width": 129}}],
                     "body": [
                         {"tag": "Primitive", "op": "h", "value": None, "angle": None,
                          "operands": [{"tag": "Ref", "type": {"tag": "RegType", "kind": "uint", "width": 1},
                                        "parts": [{"tag": "Span", "register": "address", "start": 64, "width": 1}]}]},
                         {"tag": "Load", "resource": "bank",
                          "address": {"tag": "Ref", "type": {"tag": "RegType", "kind": "uint", "width": 65},
                                      "parts": [{"tag": "Span", "register": "address", "start": 0, "width": 65}]},
                          "data": {"tag": "Ref", "type": {"tag": "RegType", "kind": "uint", "width": 129},
                                   "parts": [{"tag": "Span", "register": "data", "start": 0, "width": 129}]}}]}]}
    memory = {"bank": {0: (1 << 128)+3, 3: 5}}
    return json.dumps({"program": program, "memory": memory}).encode()


def test_gui_executes_wide_rir_and_guards_the_endpoint():
    with make_server() as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            with urllib.request.urlopen(base) as response:
                token = re.search(r"const token='([^']+)'", response.read().decode()).group(1)
            headers = {"Content-Type": "application/json", "X-Session": token}
            with pytest.raises(urllib.error.HTTPError) as missing:
                urllib.request.urlopen(urllib.request.Request(
                    base+"/api/rir", data=_wide_qram_payload(),
                    headers={"Content-Type": "application/json"}))
            assert missing.value.code == 403
            with pytest.raises(urllib.error.HTTPError) as oversized:
                urllib.request.urlopen(urllib.request.Request(
                    base+"/api/rir", data=b'{"program":' + b'{"pad":"' + b"x"*300_000 + b'"}}',
                    headers=headers))
            assert oversized.value.code == 400
            with pytest.raises(urllib.error.HTTPError) as broken:
                urllib.request.urlopen(urllib.request.Request(
                    base+"/api/rir", data=b'{"program": {"tag": "Module"}}', headers=headers))
            assert broken.value.code == 422
            assert "Program" in json.load(broken.value).get("error", "")
            with urllib.request.urlopen(urllib.request.Request(
                    base+"/api/rir", data=_wide_qram_payload(), headers=headers)) as response:
                report = json.load(response)
            assert report["schema"] == "pysparq.rir-run.v1"
            amplitudes = {tuple(item["values"]): item["probability"]
                          for item in report["amplitudes"]}
            # Uncontrolled load queries both superposed branches: address 0 loads
            # the stored word, address 2^64 holds no cell and keeps data at 0.
            assert amplitudes == {(0, (1 << 128)+3): pytest.approx(0.5),
                                  (1 << 64, 0): pytest.approx(0.5)}
            assert report["registers"] == [{"name": "address", "width": 65},
                                           {"name": "data", "width": 129}]
        finally:
            server.shutdown()
            thread.join(timeout=3)
