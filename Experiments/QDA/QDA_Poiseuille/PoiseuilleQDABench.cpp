/**
 * @file PoiseuilleQDABench.cpp
 * @brief Production benchmark driver for the Poiseuille (tridiagonal) QDA linear solver
 * @details Mirrors bench/linear_solver_bench.py (05-测试报告) so that artifacts are
 *          schema-identical, but runs the SparQSim C++ core instead of the pysparq
 *          binding: at n = 2^20 the per-amplitude Python overhead of the binding is
 *          prohibitive (~tens of minutes per walk step, measured 2026-10-20), while
 *          the C++ sparse-state core with OpenMP performs the same walk in
 *          milliseconds-to-seconds per step.
 *
 *          Conventions are aligned with the validated pysparq run
 *          (Experiments cross-checked at dim 8/256, fid = 1.0 / 0.9999996):
 *            - A = tridiag(beta, alpha, beta), NOT Frobenius-normalized
 *              (res_dir is scale-invariant), b = ones(n)/sqrt(n) (hadamard_b)
 *            - enc_b = Hadamard_Int_Full (uniform superposition), enc_A =
 *              Block_Encoding_Tridiagonal via Walk_s_Tridiagonal
 *            - registers: main(nq), anc_UA(4 bit), anc_1..anc_4(1 bit)
 *            - steps = ceil(step_rate * 2305 * kappa), forced even
 *            - solution extraction follows the C++ GetOutput convention:
 *              post-select anc_UA = anc_2 = anc_3 = 0,
 *              layout idx = main + anc_1*n + anc_4*2n, solution block out[n:2n)
 *            - res_dir = || A xh/||A xh|| - b ||_2, res_opt = sqrt(||Axh||^2 - |<b,Axh>|^2),
 *              fid = |<xref, xh>|^2, succP = ||solution block||^2
 *
 *          Outputs (in --outdir): residual_curve.csv (incremental, flushed per log
 *          point) and qda_summary_cpp.json; a collector script converts them to
 *          residual_curve.npz / qda_summary.json / run_summary.json.
 */

#include "global_macros.h"
#include "block_encoding.h"
#include "matrix.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <cstdio>
#include <algorithm>
#include "DiscreteAdiabatic/qda_tridiagonal.h"
#include "argparse.h"

using namespace qram_simulator;
using namespace QDA;
using namespace QDA::QDA_tridiagonal;
using namespace block_encoding;

namespace {

double now_seconds() {
    return std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

std::string now_datetime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

double analytic_kappa(double alpha, double beta, size_t n) {
    double lam_max = 0.0, lam_min = std::numeric_limits<double>::infinity();
    for (size_t k = 1; k <= n; ++k) {
        double lam = std::fabs(alpha + 2.0 * beta * std::cos(M_PI * double(k) / double(n + 1)));
        lam_max = std::max(lam_max, lam);
        lam_min = std::min(lam_min, lam);
    }
    return lam_max / lam_min;
}

// Thomas solve of tridiag(beta, alpha, beta) x = b; returns unit-norm solution.
// b_i = 1/sqrt(n) (uniform unit-norm right-hand side).
std::vector<double> thomas_reference(double alpha, double beta, size_t n) {
    std::vector<double> cp(n - 1), dp(n);
    double bi = 1.0 / std::sqrt(double(n));
    cp[0] = beta / alpha;
    dp[0] = bi / alpha;
    for (size_t i = 1; i < n; ++i) {
        double m = alpha - beta * cp[i - 1];
        if (i < n - 1) cp[i] = beta / m;
        dp[i] = (bi - beta * dp[i - 1]) / m;
    }
    std::vector<double> x(n);
    x[n - 1] = dp[n - 1];
    for (size_t ii = n - 1; ii-- > 0;) {
        x[ii] = dp[ii] - cp[ii] * x[ii + 1];
    }
    double nrm = 0.0;
    for (double v : x) nrm += v * v;
    nrm = std::sqrt(nrm);
    for (double& v : x) v /= nrm;
    return x;
}

struct BenchResult {
    size_t step;
    double res_dir;
    double res_opt;
    double succ_prob;
    double fidelity;
};

} // namespace

int main(int argc, const char** argv) {
    argparse::ArgumentParser parser("PoiseuilleQDABench",
        "QDA linear-solver production benchmark (tridiagonal Poiseuille case)");
    parser.add_argument().names({ "-s", "--steprate" }).description("step budget rate: steps = ceil(rate*2305*kappa)").required(false);
    parser.add_argument().name("--p").description("walk parameter p").required(false);
    parser.add_argument().name("--nqubit").description("main register qubit count (dimension = 2^nq)").required(false);
    parser.add_argument().name("--alpha").description("tridiagonal diagonal coefficient").required(false);
    parser.add_argument().name("--beta").description("tridiagonal sub-diagonal coefficient").required(false);
    parser.add_argument().name("--kappa").description("override analytic condition number").required(false);
    parser.add_argument().name("--logEvery").description("steps between residual log points (even)").required(false);
    parser.add_argument().name("--tol").description("res_dir convergence threshold").required(false);
    parser.add_argument().name("--maxSteps").description("hard cap on executed steps").required(false);
    parser.add_argument().name("--outdir").description("output directory").required(false);
    parser.add_argument().name("--czEps").description("ClearZero pruning threshold (default 1e-14)").required(false);
    parser.enable_help();
    auto err = parser.parse(argc, argv);
    if (err) { std::cout << err << std::endl; std::exit(2); }
    if (parser.exists("help")) { parser.print_help(); return 0; }

    double step_rate = parser.exists("steprate") ? parser.get<double>("steprate") : 1.1;
    double p = parser.exists("p") ? parser.get<double>("p") : 1.3;
    size_t nqubit = parser.exists("nqubit") ? parser.get<size_t>("nqubit") : 20;
    double alpha = parser.exists("alpha") ? parser.get<double>("alpha") : 2.2666666666564934;
    double beta = parser.exists("beta") ? parser.get<double>("beta") : -1.0;
    double kappa_arg = parser.exists("kappa") ? parser.get<double>("kappa") : 0.0;
    size_t log_every = parser.exists("logEvery") ? parser.get<size_t>("logEvery") : 250;
    double tol = parser.exists("tol") ? parser.get<double>("tol") : 1e-6;
    size_t max_steps = parser.exists("maxSteps") ? parser.get<size_t>("maxSteps") : size_t(1e9);
    std::string outdir_s = parser.exists("outdir") ? parser.get<std::string>("outdir") : ".";
    double cz_eps = parser.exists("czEps") ? parser.get<double>("czEps") : 1e-14;
    qram_simulator::clearzero_default_eps = cz_eps; // walk-internal ClearZero() calls follow the same threshold
    if (log_every % 2 == 1) log_every += 1; // 2-step periodicity: keep log points on valid solution blocks

    size_t n = size_t(1) << nqubit;
    double kappa_analytic = analytic_kappa(alpha, beta, n);
    double kappa = kappa_arg > 0 ? kappa_arg : kappa_analytic;

    std::filesystem::path outdir(outdir_s);
    std::filesystem::create_directories(outdir);

    std::vector<double> xref = thomas_reference(alpha, beta, n);
    double bnorm_inv = 1.0 / std::sqrt(double(n)); // unit b_i

    std::printf("[INFO] n=%zu nqubit=%zu kappa=%.17g (analytic=%.17g) rate=%.17g alpha=%.17g beta=%.17g p=%.17g\n",
        n, nqubit, kappa, kappa_analytic, step_rate, alpha, beta, p);
    std::printf("[INFO] outdir=%s log_every=%zu tol=%.17g\n", outdir.string().c_str(), log_every, tol);
    std::fflush(stdout);

    std::vector<System> state;
    state.emplace_back();
    auto main_reg = AddRegister("main", UnsignedInteger, nqubit)(state);
    AddRegister("anc_UA", UnsignedInteger, 4)(state);
    AddRegister("anc_4", Boolean, 1)(state);
    AddRegister("anc_3", Boolean, 1)(state);
    AddRegister("anc_2", Boolean, 1)(state);
    AddRegister("anc_1", Boolean, 1)(state);

    // initial state |b>: uniform superposition over main register (enc_b = Hadamard_Int_Full)
    (Hadamard_Int_Full(main_reg))(state);

    int pos_main = System::get("main");
    int pos_UA = System::get("anc_UA");
    int pos_a1 = System::get("anc_1");
    int pos_a2 = System::get("anc_2");
    int pos_a3 = System::get("anc_3");
    int pos_a4 = System::get("anc_4");

    constexpr int StepConstant = 2305;
    size_t budget = (size_t)std::ceil(step_rate * StepConstant * kappa);
    size_t steps = std::min(budget, max_steps);
    if (steps % 2 == 1) steps -= 1;
    steps = std::max(steps, size_t(2));

    FILE* curve_file = std::fopen((outdir / "residual_curve.csv").string().c_str(), "w");
    if (!curve_file) { std::fprintf(stderr, "cannot open residual_curve.csv\n"); return 3; }
    std::fprintf(curve_file, "step,res_dir,res_opt,success_prob,fidelity\n");

    std::vector<complex_t> out(4 * n);
    std::vector<BenchResult> curve;
    std::string termination = "fixed_step_budget";

    double t0 = now_seconds();
    double t_extract_total = 0.0;
    std::printf("[INFO] steps budget=%zu (StepConstant=%d)\n", steps, StepConstant);
    std::fflush(stdout);

    for (size_t i = 0; i < steps; ++i) {
        double s = double(i) / double(steps);
        Walk_s_Tridiagonal walk("main", "anc_UA", "anc_1", "anc_2", "anc_3", "anc_4",
                                s, kappa, p, alpha, beta);
        walk(state);
        ClearZero clear_op{cz_eps};
        clear_op(state);

        if ((i + 1) % log_every == 0 || i == steps - 1) {
            double t_ex0 = now_seconds();
            std::fill(out.begin(), out.end(), complex_t(0.0, 0.0));
            for (auto& st : state) {
                if (st.get(pos_UA).value != 0 || st.get(pos_a2).value != 0 || st.get(pos_a3).value != 0)
                    continue;
                size_t idx = st.get(pos_main).value
                           + st.get(pos_a1).value * n
                           + st.get(pos_a4).value * 2 * n;
                out[idx] += st.amplitude;
            }
            // solution block out[n:2n), normalize
            double blk_nrm2 = 0.0;
            for (size_t k = n; k < 2 * n; ++k) blk_nrm2 += std::norm(out[k]);
            double blk_nrm = std::sqrt(blk_nrm2);
            double rd = std::nan(""), ro = std::nan(""), fid = std::nan(""), succP = blk_nrm2;
            if (blk_nrm > 1e-300) {
                // xh = block / ||block||
                // y = A xh ; accumulate ||y||^2, <b,y>, and ||y/nrm - b||^2 in one pass
                // b is real & uniform (unit norm); A real symmetric tridiag
                std::vector<complex_t> y(n, complex_t(0.0, 0.0));
                for (size_t k = 0; k < n; ++k) {
                    complex_t xk = out[n + k] / blk_nrm;
                    y[k] += alpha * xk;
                    if (k > 0) y[k - 1] += beta * xk;
                    if (k + 1 < n) y[k + 1] += beta * xk;
                }
                double y_nrm2 = 0.0, proj_re = 0.0, proj_im = 0.0;
                for (size_t k = 0; k < n; ++k) y_nrm2 += std::norm(y[k]);
                double y_nrm = std::sqrt(y_nrm2);
                for (size_t k = 0; k < n; ++k) {
                    proj_re += bnorm_inv * y[k].real();
                    proj_im += bnorm_inv * y[k].imag();
                }
                double proj2 = proj_re * proj_re + proj_im * proj_im;
                // res_dir^2 = ||y/y_nrm - b||^2 = 1 - 2<y/y_nrm, b> + 1 (b unit, y/y_nrm unit)
                rd = std::sqrt(std::max(2.0 - 2.0 * proj_re / y_nrm, 0.0));
                ro = std::sqrt(std::max(y_nrm2 - proj2, 0.0));
                // fid = |<xref, xh>|^2, xref real unit
                double fr = 0.0, fi = 0.0;
                for (size_t k = 0; k < n; ++k) {
                    complex_t xk = out[n + k] / blk_nrm;
                    fr += xref[k] * xk.real();
                    fi += xref[k] * xk.imag();
                }
                fid = fr * fr + fi * fi;
            }
            t_extract_total += now_seconds() - t_ex0;

            curve.push_back({ i + 1, rd, ro, succP, fid });
            std::fprintf(curve_file, "%zu,%.17g,%.17g,%.17g,%.17g\n", i + 1, rd, ro, succP, fid);
            std::fflush(curve_file);
            double wall = now_seconds() - t0;
            std::printf("%s | step %7zu/%zu res_dir=%.6e res_opt=%.6e succP=%.4f fid=%.6f wall=%.1fs max_sys=%zu\n",
                now_datetime().c_str(), i + 1, steps, rd, ro, succP, fid, wall, System::max_system_size);
            std::fflush(stdout);
            if (!std::isnan(rd) && rd < tol) {
                termination = fmt::format("residual_below_tol({:g})", tol);
                break;
            }
        }
    }
    double wall = now_seconds() - t0;
    std::fclose(curve_file);

    // gather machine-readable summary (schema mirrors linear_solver_bench.py)
    BenchResult last{ 0, std::nan(""), std::nan(""), std::nan(""), std::nan("") };
    if (!curve.empty()) last = curve.back();
    std::string note = "C++ core run; conventions identical to pysparq bench "
                       "(dim8/256 cross-validated)";
    std::string summary = fmt::format(R"json({{
  "engine": "qda (SparQSim C++ Walk_s_Tridiagonal + GetOutput 口径后选提取)",
  "dimension": {},
  "nqubit": {},
  "kappa": {:.17g},
  "kappa_analytic": {:.17g},
  "eps": 0.01,
  "alpha": {:.17g},
  "beta": {:.17g},
  "p": {:.17g},
  "clearzero_eps": {:.17g},
  "problem": "1D Poisson tridiagonal, b=ones/sqrt(n)",
  "step_budget": {},
  "steps_executed": {},
  "step_rate": {:.17g},
  "step_constant": {},
  "termination": "{}",
  "tol": {:.17g},
  "final_res_dir": {},
  "final_res_opt": {},
  "final_success_prob": {},
  "final_fidelity_vs_classical": {},
  "wall_time_s": {:.17g},
  "time_per_step_s": {:.17g},
  "extract_time_s": {:.17g},
  "max_system_size": {},
  "converged_note": "步数预算远低于 kappa*log(kappa/eps) 时残差不会收敛，属预期的绝热条件不足，非模拟器错误",
  "note": "{}"
}})json",
        n, nqubit, kappa, kappa_analytic, alpha, beta, p, cz_eps,
        budget, last.step, step_rate, StepConstant, termination, tol,
        std::isnan(last.res_dir) ? std::string("null") : fmt::format("{:.17g}", last.res_dir),
        std::isnan(last.res_opt) ? std::string("null") : fmt::format("{:.17g}", last.res_opt),
        std::isnan(last.succ_prob) ? std::string("null") : fmt::format("{:.17g}", last.succ_prob),
        std::isnan(last.fidelity) ? std::string("null") : fmt::format("{:.17g}", last.fidelity),
        wall, wall / std::max<size_t>(1, last.step), t_extract_total,
        System::max_system_size, note);

    std::ofstream f((outdir / "qda_summary_cpp.json"));
    f << summary << std::endl;
    f.close();

    std::printf("[OK] steps=%zu final_res_dir=%.6e fid=%.6f wall=%.1fs -> %s\n",
        last.step, last.res_dir, last.fidelity, wall, outdir.string().c_str());
    std::printf("%s\n", profiler::get_all_profiles_v2().c_str());
    return 0;
}
