# Phase-exact native state preparation

The former canonical-basis Gram-Schmidt completion became rank deficient when the target first amplitude was zero. Its complex projection used the conjugate in the wrong order. The old native build fails11 of15 new full-matrix/invalid-input cases, including NaNs and nonunitary complex gates.

The public stateprep_unitary_build_schmidt function and both Rot_GeneralStatePrep constructors are retained. They now use a deterministic complex Householder completion; columns other than the prepared first column change deliberately. Finite nonzero inputs are normalized with scaling, preserving tiny/large amplitudes; invalid dimensions, zero and nonfinite vectors are rejected.

For normalized psi, choose phase=psi[0]/abs(psi[0]) (or1 at zero), v=psi+phase*e0, H=I-2vv*/(v*v), and D=diag(-phase,1,...). Since v*v>=2, this avoids cancellation. H is unitary, H e0=-conj(phase)*psi, and U=HD satisfies Ue0=psi, including its global complex phase. Construction is O(N²), replacing O(N³) completion.

## Local regression

The installed final-source CPU wheel passes507 tests; two pre-existing CKS replacement-API skips remain. The zero-diagonal tridiagonal case first XPASSed with its original normalization assertion, then its xfail marker was removed.20 new cases reconstruct the complete native gate/dagger matrices, compare target phases, test complex random inputs and numerical extremes, and retain the historical public builder. Native CUDA compilation is tracked separately; no GPU runtime result is asserted.

The same native extension additionally passes the five fixed-OracQ15a82e pure-function real-backend tests (131.39s), including checkpoint/uncompute execution, and the real PySparQ/OriginIR algorithm-contract comparison (2.34s). UniQC is fixed to a8a936; these are local simulator runs, not hardware submissions. Extension SHA256 was checked again after installing compatibility dependencies.

## Constructor timing

Intel Xeon Gold5118,2sockets×12physical cores,48SMT threads. This benchmark uses OMP_NUM_THREADS=1 and OPENBLAS_NUM_THREADS=1. Identical positive-real normalized targets are constructed three times at each size; medians below include the complete native gate constructor. Old C++ source is the unchanged SparQSim4b9a129 core with the integrated QRAM8016d201 dependency; new source is the Householder candidate. This measures classical simulator setup, not quantum algorithm execution, logical gate counts or end-to-end application performance.

| Dimension | Old median / s | New median / s | Ratio |
|---|---:|---:|---:|
|64|0.0020458729|5.4016942e-05|37.87×|
|128|0.010585354|0.00017691404|59.83×|
|256|0.13998787|0.0007613441|183.87×|
|512|2.0460575|0.0042421969|482.31×|
|1024|19.01027|0.04699949|404.48×|

Old extension SHA256: `beeab9d8854dc8efa07a3d4a46762fdca237e3cceba269b2ddea9b569e8f1129`. New extension SHA256: `cdc4b44201577ce756c1f0e0c1f944fdecd42a32a4debf681170ce733cec9797`. Raw three-sample data follows.

```json
{
  "old": {
    "64": {
      "dimension": 64,
      "samples_s": [
        0.0032558219972997904,
        0.0020458728540688753,
        0.001415211008861661
      ],
      "median_s": 0.0020458728540688753
    },
    "128": {
      "dimension": 128,
      "samples_s": [
        0.012273977044969797,
        0.010585353942587972,
        0.00904040620662272
      ],
      "median_s": 0.010585353942587972
    },
    "256": {
      "dimension": 256,
      "samples_s": [
        0.16289289016276598,
        0.13998786895535886,
        0.13815295905806124
      ],
      "median_s": 0.13998786895535886
    },
    "512": {
      "dimension": 512,
      "samples_s": [
        2.159201023867354,
        1.9324694981332868,
        2.0460574829485267
      ],
      "median_s": 2.0460574829485267
    },
    "1024": {
      "dimension": 1024,
      "samples_s": [
        23.621543391840532,
        19.010270470986143,
        18.85394919803366
      ],
      "median_s": 19.010270470986143
    }
  },
  "new": {
    "64": {
      "dimension": 64,
      "samples_s": [
        0.00024201208725571632,
        4.919711500406265e-05,
        5.401694215834141e-05
      ],
      "median_s": 5.401694215834141e-05
    },
    "128": {
      "dimension": 128,
      "samples_s": [
        0.0005452320910990238,
        0.0001767938956618309,
        0.0001769140362739563
      ],
      "median_s": 0.0001769140362739563
    },
    "256": {
      "dimension": 256,
      "samples_s": [
        0.0022882060147821903,
        0.0007613440975546837,
        0.0007213340140879154
      ],
      "median_s": 0.0007613440975546837
    },
    "512": {
      "dimension": 512,
      "samples_s": [
        0.009450689889490604,
        0.004242196911945939,
        0.003656506072729826
      ],
      "median_s": 0.004242196911945939
    },
    "1024": {
      "dimension": 1024,
      "samples_s": [
        0.06955881114117801,
        0.03800852014683187,
        0.046999490121379495
      ],
      "median_s": 0.046999490121379495
    }
  }
}
```
