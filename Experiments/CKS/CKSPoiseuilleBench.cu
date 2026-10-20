/**
 * @file CKSPoiseuilleBench.cu
 * @brief GPU (CUDA) production benchmark for the Poiseuille tridiagonal CKS solver
 * @details QRAM-free CKS (Chebyshev quantum-walk LCU) linear-system solver on the
 *          same case as the QDA benchmark: A = tridiag(beta, alpha, beta) with
 *          alpha = 2.2666666666564934, beta = -1, b = ones(n)/sqrt(n) (1D Poisson
 *          / Poiseuille flow). Benchmark contract mirrors PoiseuilleQDABench.cu
 *          (identical artifact schema: residual_curve.csv + qda_summary_cpp.json).
 *
 *          Physics / conventions (all verified on CPU 2026-10-20):
 *          - The walk rotation is sign-blind (|v| encoding), i.e. the walk
 *            implements |A|; for the all-negative off-diagonal case
 *            |A| = D*A*D with D = diag((-1)^i), so the solver runs on the |A|
 *            system with input D|b> (Hadamard + TridiagParityPhase) and the
 *            extracted amplitudes are multiplied by (-1)^i classically.
 *          - The walk oracles are direct arithmetic mirrors of the QRAM circuit
 *            (hamiltonian_simulation_tridiagonal.h): direct path == QRAM path
 *            bit-exactly at dim16 (stock coefficients, all metrics identical).
 *          - LCU coefficients: exact 1/x collocation least squares on the
 *            quantized walk spectrum (ChebyshevInverseCoefficients, J terms),
 *            or the stock binomial-tail family (--coef stock).
 *          CPU reference results (Experiment_CKS_TridiagonalProbe):
 *            dim16 --coef true --J 640:  res_dir = 2.1e-8, fid = 1.0
 *            dim64 --coef true --J 1024: res_dir <= 1e-7, fid = 1.0
 */

#include "hamiltonian_simulation.h"
#include "hamiltonian_simulation_tridiagonal.h"
#include "cuda/basic_components.cuh"
#include "cuda/sparse_state_simulator.cuh"
#include "argparse.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <cstdio>
#include <algorithm>

using namespace qram_simulator;
using namespace CKS;

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
		for (size_t ii = n - 1; ii-- > 0;)
			x[ii] = dp[ii] - cp[ii] * x[ii + 1];
		double nrm = 0.0;
		for (double v : x) nrm += v * v;
		nrm = std::sqrt(nrm);
		for (double& v : x) v /= nrm;
		return x;
	}

	struct BenchResult {
		size_t term;
		double res_dir;
		double res_opt;
		double succ_prob;
		double fidelity;
	};

	// Residual block metrics from the extracted amplitude vector m (same formulas as
	// the CPU probe and the QDA bench: y = A x for normalized x = m/|m| against |b>)
	void residual_metrics(const std::vector<complex_t>& m, size_t n,
		double alpha, double beta, double bnorm_inv, const std::vector<double>& xref,
		double& res_dir, double& res_opt, double& fid, double& blk_norm2)
	{
		double nrm2 = 0.0;
		for (auto& v : m) nrm2 += std::norm(v);
		double nrm = std::sqrt(nrm2);
		blk_norm2 = nrm2;
		res_dir = res_opt = fid = std::nan("");
		if (nrm <= 1e-300) return;
		std::vector<complex_t> y(n, complex_t(0.0, 0.0));
		for (size_t k = 0; k < n; ++k) {
			complex_t xk = m[k] / nrm;
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
		res_dir = std::sqrt(std::max(2.0 - 2.0 * proj_re / y_nrm, 0.0));
		res_opt = std::sqrt(std::max(y_nrm2 - proj2, 0.0));
		double fr = 0.0, fi = 0.0;
		for (size_t k = 0; k < n; ++k) {
			complex_t xk = m[k] / nrm;
			fr += xref[k] * xk.real(); fi += xref[k] * xk.imag();
		}
		fid = fr * fr + fi * fi;
	}

} // namespace

int main(int argc, const char** argv) {
	argparse::ArgumentParser parser("CKS_PoiseuilleBench_cuda",
		"CKS (Chebyshev LCU quantum walk) linear-solver production benchmark "
		"(tridiagonal Poiseuille case, QRAM-free direct oracles, CUDA backend)");
	parser.add_argument().name("--nqubit").description("main register qubit count (dimension = 2^nq)").required(false);
	parser.add_argument().name("--alpha").description("tridiagonal diagonal coefficient").required(false);
	parser.add_argument().name("--beta").description("tridiagonal sub-diagonal coefficient").required(false);
	parser.add_argument().name("--kappa").description("override condition number (default 16)").required(false);
	parser.add_argument().name("--eps").description("CKS target precision").required(false);
	parser.add_argument().name("--ds").description("matrix element quantization bits").required(false);
	parser.add_argument().name("--coef").description("true | stock (default true)").required(false);
	parser.add_argument().name("--J").description("truncation for true coefficients (default 640)").required(false);
	parser.add_argument().name("--dphase").description("apply the D-transform sign handling (default 1)").required(false);
	parser.add_argument().name("--logEvery").description("LCU terms between log points").required(false);
	parser.add_argument().name("--tol").description("res_dir convergence threshold").required(false);
	parser.add_argument().name("--outdir").description("output directory").required(false);
	parser.add_argument().name("--debugStages").description("per-op norm/NaN instrumentation through input prep + FirstStep").required(false);
	parser.enable_help();
	auto err = parser.parse(argc, argv);
	if (err) { std::cout << err << std::endl; std::exit(2); }
	if (parser.exists("help")) { parser.print_help(); return 0; }

	size_t nqubit = parser.exists("nqubit") ? parser.get<size_t>("nqubit") : 20;
	double alpha = parser.exists("alpha") ? parser.get<double>("alpha") : 2.2666666666564934;
	double beta = parser.exists("beta") ? parser.get<double>("beta") : -1.0;
	double kappa_arg = parser.exists("kappa") ? parser.get<double>("kappa") : 0.0;
	double eps = parser.exists("eps") ? parser.get<double>("eps") : 0.01;
	size_t ds = parser.exists("ds") ? parser.get<size_t>("ds") : 32;
	std::string coef_mode = parser.exists("coef") ? parser.get<std::string>("coef") : "true";
	size_t J_true = parser.exists("J") ? parser.get<size_t>("J") : 640;
	bool dphase = parser.exists("dphase") ? parser.get<int>("dphase") != 0 : true;
	size_t log_every = parser.exists("logEvery") ? parser.get<size_t>("logEvery") : 64;
	double tol = parser.exists("tol") ? parser.get<double>("tol") : 1e-6;
	std::string outdir_s = parser.exists("outdir") ? parser.get<std::string>("outdir") : ".";

	size_t n = size_t(1) << nqubit;
	double kappa_analytic = analytic_kappa(alpha, beta, n);
	double kappa = kappa_arg > 0 ? kappa_arg : 16.0;

	std::filesystem::path outdir(outdir_s);
	std::filesystem::create_directories(outdir);

	auto [mat, spec] = make_tridiagonal_matrix_spec(alpha, beta, n, ds);
	std::vector<double> xref = thomas_reference(alpha, beta, n);
	double bnorm_inv = 1.0 / std::sqrt(double(n));

	// exact walk-matrix spectral interval from the quantized entries (same as the CPU probe)
	double ccos = std::cos(M_PI / double(n + 1));
	double denom = double(pow2(ds)) * 4.0;
	double lam1 = (double(spec.alpha_enc) + 2.0 * double(spec.beta_enc) * ccos) / denom;
	double lamn = (double(spec.alpha_enc) - 2.0 * double(spec.beta_enc) * ccos) / denom;
	double m_spec = std::min(lam1, lamn) * (1.0 - 1e-12);
	double M_spec = std::max(lam1, lamn) * (1.0 + 1e-12);

	static ChebyshevInverseCoefficients inv(m_spec, M_spec, J_true);

	std::printf("[INFO] n=%zu nqubit=%zu kappa=%.17g (analytic=%.17g) eps=%g ds=%zu nnz_col=%zu\n",
		n, nqubit, kappa, kappa_analytic, eps, ds, mat.nnz_col);
	std::printf("[INFO] alpha_enc=%llu beta_enc=%llu\n",
		(unsigned long long)spec.alpha_enc, (unsigned long long)spec.beta_enc);
	std::printf("[INFO] Ahat spectrum [%.9f, %.9f] (kappa_interval=%.6f)\n",
		m_spec, M_spec, M_spec / m_spec);
	std::printf("[INFO] coef=%s J=%zu dphase=%d log_every=%zu tol=%.17g outdir=%s\n",
		coef_mode.c_str(), coef_mode == "true" ? J_true : size_t(0), (int)dphase, log_every, tol,
		outdir.string().c_str());
	std::fflush(stdout);

	LCU_Container_NoiseFree_Tridiagonal<CuSparseState> obj(mat, spec, kappa, eps);
	if (coef_mode == "true") {
		std::printf("[INFO] true coefficients: J=%zu sum|d|=%.6e\n", inv.J, inv.a);
		obj.set_inverse_coefficients(&inv);
	}
	std::fflush(stdout);

	// input preparation: uniform Hadamard, optional D-transform phase, then the first
	// walk step (mirrors the CPU probe run_lcu)
	bool debug_stages = parser.exists("debugStages");
	bool debug_top = debug_stages;
	auto stage_check = [&](const char* tag) {
		obj.step_state.move_to_gpu();
		auto& v = obj.step_state.sparse_state_gpu;
		double nrm2 = thrust::transform_reduce(thrust::device,
			v.begin(), v.end(), AbsSqrFunctor{}, 0.0, thrust::plus<double>());
		int nans = (int)thrust::count_if(thrust::device, v.begin(), v.end(),
			[] __host__ __device__(const System & s) {
				const double* a = CuSystemAmplitude(s);
				return std::isnan(a[0]) || std::isnan(a[1]);
			});
		cudaDeviceSynchronize();
		std::printf("[stage] %-22s |psi|2=%.12f nan=%d systems=%zu\n", tag, nrm2, nans, v.size());
		if (debug_top) {
			auto cpu = obj.step_state.get_cpu_copy();
			std::partial_sort(cpu.begin(), cpu.begin() + std::min<size_t>(3, cpu.size()), cpu.end(),
				[](const System& a, const System& b) { return std::norm(a.amplitude) > std::norm(b.amplitude); });
			size_t j_id = System::get(obj.GetInputVecReg()), k_id = System::get(obj.quantum_walk_obj.k),
				b2_id = System::get(obj.quantum_walk_obj.b2), kc_id = System::get(obj.quantum_walk_obj.k_comp);
			for (size_t t = 0; t < std::min<size_t>(3, cpu.size()); ++t) {
				auto& s = cpu[t];
				std::printf("    top%zu: j=%2zu k=%2zu b2=%zu kcomp=%2zu amp=(%.4f,%+.4f)\n", t + 1,
					(size_t)s.GetAs(j_id, uint64_t), (size_t)s.GetAs(k_id, uint64_t),
					(size_t)s.GetAs(b2_id, uint64_t), (size_t)s.GetAs(kc_id, uint64_t),
					s.amplitude.real(), s.amplitude.imag());
			}
		}
		std::fflush(stdout);
	};
	if (debug_stages) stage_check("init");
	if (debug_stages) {
		// unit test: CondRot scatter+merge on a 4-system known state
		auto& ss = obj.step_state;
		auto& W0 = obj.quantum_walk_obj;
		ss.move_to_gpu();
		AddRegister("rotation", Rational, 32)(ss);
		// two keys (j=5, j=9), each with b2=0/1 pair; rotation = pi/4
		uint64_t rq = get_rational(0.125, 32); // theta/(2pi) = 1/8 -> theta = pi/4
		auto mk = [&](size_t jv, size_t b2v, double re, double im) {
			System s{};
			s.get(System::get(W0.j)).value = jv;
			s.get(System::get(W0.b2)).value = b2v;
			s.get(System::get("rotation")).value = rq;
			s.amplitude = complex_t(re, im);
			return s;
		};
		std::vector<System> host4{ mk(5, 0, 0.6, 0.0), mk(5, 1, 0.0, 0.8), mk(9, 0, 0.1, 0.0), mk(9, 1, 0.0, 0.99) };
		ss.sparse_state_gpu.resize(4);
		thrust::copy(host4.begin(), host4.end(), ss.sparse_state_gpu.begin());
		{
			auto pre = ss.get_cpu_copy();
			std::fprintf(stderr, "[ut] pre-condrot: %zu systems, sizeof(System)=%zu\n", pre.size(), sizeof(System));
			for (auto& s : pre)
				std::fprintf(stderr, "[ut-pre] j=%zu b2=%zu rot=%llu amp=(%.6f,%+.6f)\n",
					(size_t)s.GetAs(System::get(W0.j), uint64_t), (size_t)s.GetAs(System::get(W0.b2), uint64_t),
					(unsigned long long)s.GetAs(System::get("rotation"), uint64_t),
					s.amplitude.real(), s.amplitude.imag());
		}
		CondRot_Fixed_Bool("rotation", W0.b2)(ss);
		auto cpu = ss.get_cpu_copy();
		for (auto& s : cpu)
			std::printf("[condrot-unit] j=%zu b2=%zu rot=%llu amp=(%.6f,%+.6f) expected j5: (0.4243,-0.5657)/(0.4243,0.5657)\n",
				(size_t)s.GetAs(System::get(W0.j), uint64_t), (size_t)s.GetAs(System::get(W0.b2), uint64_t),
				(unsigned long long)s.GetAs(System::get("rotation"), uint64_t),
				s.amplitude.real(), s.amplitude.imag());
		// restore a clean single-system start
		ss.sparse_state_gpu.resize(1);
		thrust::fill(thrust::device, ss.sparse_state_gpu.begin(), ss.sparse_state_gpu.end(), System{});
		thrust::for_each(thrust::device, ss.sparse_state_gpu.begin(), ss.sparse_state_gpu.end(),
			[] __host__ __device__(System & s) { CuSystemAmplitude(s)[0] = 1.0; });
		RemoveRegister("rotation")(ss);
	}
	Hadamard_Int(obj.GetInputVecReg(), obj.quantum_walk_obj.get_init_size())(obj.step_state);
	if (debug_stages) stage_check("input H");
	if (dphase) {
		TridiagParityPhase(obj.GetInputVecReg())(obj.step_state);
		if (debug_stages) stage_check("parity");
	}
	ClearZero()(obj.step_state);
	if (debug_stages) stage_check("ClearZero");
	if (debug_stages) {
		auto& W = obj.quantum_walk_obj;
		AddRegister("data", UnsignedInteger, W.default_register_size)(obj.step_state); stage_check("T.AddRegister data");
		Hadamard_Int(W.k, log2(TridiagonalSpec::slot_count))(obj.step_state); stage_check("T.H k2");
		TridiagOracle1(W.data_offset, W.j, W.k, "data", spec)(obj.step_state); stage_check("T.O1 load");
		TridiagOracle2(W.sparse_offset, W.j, W.k, W.k_comp, spec).dag(obj.step_state); stage_check("T.O2d slot2col");
		AddRegister("rotation", Rational, 32)(obj.step_state); stage_check("T.AddReg rot");
		GetQWRotateAngle_Int_Int_Int("data", W.j, W.k, "rotation", &mat)(obj.step_state); stage_check("T.G1");
		CondRot_Fixed_Bool("rotation", W.b2)(obj.step_state); stage_check("T.C");
		GetQWRotateAngle_Int_Int_Int("data", W.j, W.k, "rotation", &mat)(obj.step_state); stage_check("T.G2");
		RemoveRegister("rotation")(obj.step_state); stage_check("T.rm rot");
		TridiagOracle2(W.sparse_offset, W.j, W.k, W.k_comp, spec)(obj.step_state); stage_check("T.O2 col2slot");
		TridiagOracle1(W.data_offset, W.j, W.k, "data", spec)(obj.step_state); stage_check("T.O1 unload");
		RemoveRegister("data")(obj.step_state); stage_check("T.rm data");
		TridiagOracle2(W.sparse_offset, W.j, W.k, W.k_comp, spec).dag(obj.step_state); stage_check("T.O2d final");
		Swap_General_General(W.j, W.k)(obj.step_state); stage_check("Swap jk");
		Swap_General_General(W.b1, W.b2)(obj.step_state); stage_check("Swap b1b2");
		Swap_General_General(W.j_comp, W.k_comp)(obj.step_state); stage_check("Swap jckc");
		T_Tridiagonal(W.data_offset, W.sparse_offset, W.j, W.b1,
			W.k, W.b2, W.k_comp, W.default_register_size, &mat, spec).dag(obj.step_state);
		stage_check("FirstStep.Tdag");
	}
	else {
		obj.quantum_walk_obj.FirstStep(obj.step_state);
	}
	std::printf("[INFO] state size after FirstStep = %zu\n", obj.step_state.size());
	std::fflush(stdout);

	FILE* curve_file = std::fopen((outdir / "residual_curve.csv").string().c_str(), "w");
	if (!curve_file) { std::fprintf(stderr, "cannot open residual_curve.csv\n"); return 3; }
	std::fprintf(curve_file, "step,res_dir,res_opt,success_prob,fidelity\n");

	std::vector<BenchResult> curve;
	std::string termination = "full_lcu_expansion";
	size_t input_id = System::get(obj.GetInputVecReg());

	double t0 = now_seconds();
	double t_extract_total = 0.0;
	size_t j = 0;
	while (obj.Step()) {
		++j;
		if (j % log_every == 0 || j == obj.j0 + 1) {
			double t_ex0 = now_seconds();
			auto&& [state, p_succ] = obj.PartialTrace_Nondestructive();
			state.move_to_cpu();

			std::vector<complex_t> m(n, complex_t(0.0, 0.0));
			for (System& s : state.sparse_state_cpu) {
				size_t idx = s.GetAs(input_id, uint64_t);
				m[idx] = s.amplitude * (((idx & 1) && dphase) ? -1.0 : 1.0); // undo D on the output
			}
			double rd, ro, fid, blk2;
			residual_metrics(m, n, alpha, beta, bnorm_inv, xref, rd, ro, fid, blk2);
			t_extract_total += now_seconds() - t_ex0;

			curve.push_back({ j, rd, ro, p_succ, fid });
			std::fprintf(curve_file, "%zu,%.17g,%.17g,%.17g,%.17g\n", 2 * j + 1, rd, ro, p_succ, fid);
			std::fflush(curve_file);
			double wall = now_seconds() - t0;
			std::printf("%s | term %5zu/%zu (walk %5zu) res_dir=%.6e res_opt=%.6e succP=%.6e fid=%.8f wall=%.1fs max_sys=%zu\n",
				now_datetime().c_str(), j, obj.j0, 2 * j + 1, rd, ro, p_succ, fid, wall,
				System::max_system_size);
			std::fflush(stdout);
			if (!std::isnan(rd) && rd < tol) {
				termination = fmt::format("residual_below_tol({:g})", tol);
				break;
			}
		}
	}
	double wall = now_seconds() - t0;
	std::fclose(curve_file);

	BenchResult last{ 0, std::nan(""), std::nan(""), std::nan(""), std::nan("") };
	if (!curve.empty()) last = curve.back();
	std::string note = "CUDA backend run; QRAM-free direct oracles bit-exact vs the QRAM path "
		"(dim16 stock-coefficient CPU cross-check 2026-10-20); true 1/x collocation "
		"coefficients on the quantized walk spectrum; D = diag((-1)^i) sign transform "
		"for the |A| walk";
	std::string summary = fmt::format(R"json({{
  "engine": "cks (SparQSim CUDA QuantumWalkNSteps_Tridiagonal QRAM-free direct oracle + Chebyshev LCU)",
  "backend": "CUDA (CuSparseState)",
  "dimension": {},
  "nqubit": {},
  "kappa": {:.17g},
  "kappa_analytic": {:.17g},
  "eps": {:.17g},
  "alpha": {:.17g},
  "beta": {:.17g},
  "ds": {},
  "coef_mode": "{}",
  "J": {},
  "dphase": {},
  "problem": "1D Poisson tridiagonal, b=ones/sqrt(n)",
  "lcu_terms": {},
  "walk_steps": {},
  "termination": "{}",
  "tol": {:.17g},
  "final_res_dir": {},
  "final_res_opt": {},
  "final_success_prob": {},
  "final_fidelity_vs_classical": {},
  "wall_time_s": {:.17g},
  "time_per_term_s": {:.17g},
  "extract_time_s": {:.17g},
  "max_system_size": {},
  "note": "{}"
}})json",
		n, nqubit, kappa, kappa_analytic, eps, alpha, beta, ds, coef_mode,
		coef_mode == "true" ? J_true : size_t(0), (int)dphase,
		obj.j0 + 1, 2 * (obj.j0 + 1), termination, tol,
		std::isnan(last.res_dir) ? std::string("null") : fmt::format("{:.17g}", last.res_dir),
		std::isnan(last.res_opt) ? std::string("null") : fmt::format("{:.17g}", last.res_opt),
		std::isnan(last.succ_prob) ? std::string("null") : fmt::format("{:.17g}", last.succ_prob),
		std::isnan(last.fidelity) ? std::string("null") : fmt::format("{:.17g}", last.fidelity),
		wall, wall / std::max<size_t>(1, j), t_extract_total,
		System::max_system_size, note);

	std::ofstream f((outdir / "qda_summary_cpp.json"));
	f << summary << std::endl;
	f.close();

	std::printf("[OK] terms=%zu walk_steps=%zu final_res_dir=%.6e fid=%.8f wall=%.1fs -> %s\n",
		j, 2 * j + 1, last.res_dir, last.fidelity, wall, outdir.string().c_str());
	std::printf("%s\n", profiler::get_all_profiles_v2().c_str());
	return 0;
}
