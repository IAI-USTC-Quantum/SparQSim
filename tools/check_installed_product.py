"""Audit the actual installed native wheel and exercise its independent product.

Run after wheel repair in cibuildwheel's isolated test environment. The optional
--notices-only mode is for CI jobs that already run the complete test suite.
"""
from __future__ import annotations

import argparse
import hashlib
from importlib.metadata import distribution
from importlib.resources import files
import json
from pathlib import Path
import subprocess
import sys


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--notices-only", action="store_true")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    core = project / "extern/qram-simulator"
    dist = distribution("pysparq")
    entries = list(dist.files or ())
    notices = [entry for entry in entries if ".dist-info/licenses/" in str(entry)]
    if not notices:
        raise ValueError("installed wheel has no third-party notices")
    hashes = {hashlib.sha256(Path(dist.locate_file(entry)).read_bytes()).hexdigest()
              for entry in notices}
    provenance = json.loads((core / "ThirdParty/notice-provenance.json").read_text())
    for record in provenance["files"]:
        if record["path"].endswith("argparse.h"):
            continue  # Its original attribution is retained in the bundled core notice.
        if record["sha256"] not in hashes:
            raise ValueError("installed wheel omitted/changed notice: " + record["path"])
    for path in (project / "LICENSE", project / "THIRD_PARTY_NOTICES.md",
                 core / "LICENSE", core / "THIRD_PARTY_NOTICES.md",
                 core / "ThirdParty/notice-provenance.json"):
        if hashlib.sha256(path.read_bytes()).hexdigest() not in hashes:
            raise ValueError("installed wheel omitted/changed provenance: " + str(path))
    import pysparq

    installed = Path(pysparq.__file__).resolve()
    if not installed.is_relative_to(Path(dist.locate_file("")).resolve()):
        raise ValueError("import resolved outside installed distribution")
    for name in ("demo.py", "demo_server.py", "demo.html"):
        if files("pysparq").joinpath(name).read_bytes() != (
                project / "PySparQ/pysparq" / name).read_bytes():
            raise ValueError("installed wheel omitted/changed product asset: " + name)
    command = next((entry for entry in dist.entry_points
                    if entry.group == "console_scripts" and entry.name == "pysparq"), None)
    if command is None or command.value != "pysparq.demo:main":
        raise ValueError("installed standalone entry point is missing")
    command.load()
    print(json.dumps({"version": dist.version, "installed_package": str(installed),
                      "notice_files": len(notices),
                      "bundled_openmp": [str(entry) for entry in entries
                                         if "libgomp" in str(entry) and ".so" in str(entry)]}),
          flush=True)
    if not args.notices_only:
        subprocess.run([sys.executable, "-m", "pytest", "-q",
                        str(project / "PySparQ/test/test_demo.py")], check=True)


if __name__ == "__main__":
    main()
