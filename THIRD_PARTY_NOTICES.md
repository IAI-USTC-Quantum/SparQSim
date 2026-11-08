# Third-party notices

SparQSim retains its Apache-2.0 project license. The QRAM-Simulator core is
consumed at the repository gitlink, with its original license and third-party
notices. The source distribution embeds that pinned core; it is not fetched
during a source-distribution build.

Binary wheels include this notice, the core notice and its exact provenance,
{fmt}, Eigen3.4.0, argparse, pybind11 3.0.2 and GCC runtime notices under the
distribution's licenses directory. Original copyright declarations in source
headers remain intact. See `extern/qram-simulator/THIRD_PARTY_NOTICES.md` and
`extern/qram-simulator/ThirdParty/notice-provenance.json` for individual terms,
source commits, URLs and SHA256 values. The Python binding build pins pybind11
to the version named by that provenance. Linux wheel repair may bundle libgomp;
its GPLv3 and GCC Runtime Library Exception texts accompany the wheel. CPU wheels
do not bundle a CUDA toolkit or claim GPU runtime verification.
