"""Check that the source archive builds independently of the developer git tree."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile


def check(archive: Path) -> None:
    with tarfile.open(archive) as package:
        members = package.getmembers()
        roots = {PurePosixPath(member.name).parts[0] for member in members}
        if len(roots) != 1:
            raise ValueError("source archive must have one package root")
        root = next(iter(roots))
        names = {member.name.removeprefix(root + "/") for member in members}
        required = {
            "pyproject.toml", "CMakeLists.txt", "LICENSE", "THIRD_PARTY_NOTICES.md",
            "SparQ/include/sparse_state_simulator.h",
            "SparQ/include/quantum_arithmetic.h",
            "SparQ_Algorithm/include/state_preparation.h",
            "extern/qram-simulator/CMakeLists.txt",
            "extern/qram-simulator/QRAM/include/qram_circuit_qubit.h",
            "extern/qram-simulator/ThirdParty/fmt/src/format.cc",
            "extern/qram-simulator/ThirdParty/eigen-3.4.0/COPYING.MPL2",
            "extern/qram-simulator/ThirdParty/notice-provenance.json",
            "extern/qram-simulator/ThirdParty/pybind11/LICENSE",
            "extern/qram-simulator/ThirdParty/gcc-runtime/COPYING3",
            "PySparQ/pysparq/demo.html", "PySparQ/test/test_demo.py",
            "tools/check_installed_product.py", "tools/check_sdist_contents.py",
        }
        if required - names:
            raise ValueError("source archive missing: " + str(sorted(required - names)))
        for member in members:
            path = PurePosixPath(member.name)
            if path.is_absolute() or ".." in path.parts or ".git" in path.parts:
                raise ValueError("source archive contains a git pointer or unsafe path")
            if member.issym() or member.islnk():
                raise ValueError("source archive must carry regular source files")
            if member.name.startswith(root + "/extern/qram-simulator/.github/"):
                raise ValueError("source archive unexpectedly contains core CI")
    print(json.dumps({"source_archive": str(archive), "files": len(members),
                      "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    check(parser.parse_args().archive)
