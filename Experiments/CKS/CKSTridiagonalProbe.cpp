/**
 * @file CKSTridiagonalProbe.cpp
 * @brief CPU probe: CKS (LCU) solver on the Poiseuille tridiagonal case, QRAM vs direct oracle
 * @details Same case as the QDA benchmark: A = tridiag(-1, 2.2666666666564934, -1),
 *          b = ones(n)/sqrt(n). The walk rotation is sign-blind (|v|), so the probe
 *          runs the |A| = D*A*D similarity transform: input D|b> (Hadamard +
 *          TridiagParityPhase), extract m and multiply by (-1)^i classically, which
 *          yields exactly A^{-1}|b>. Two oracle paths are provided and must agree:
 *          --path qram    : the stock QuantumWalkNSteps + QRAM tables (reference)
 *          --path direct  : QuantumWalkNSteps_Tridiagonal (QRAM-free arithmetic oracles)
 */

#include "hamiltonian_simulation.h"
#include "hamiltonian_simulation_tridiagonal.h"
#include "argparse.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdio>

using namespace qram_simulator;
using namespace CKS;

namespace {

	double analytic_kappa(double alpha, double beta, size_t n) {
		double lam_max = 0.0, lam_min = std::numeric_limits<double>::infinity();
		for (size_t k = 1; k <= n; ++k) {
			double lam = std::fabs(alpha + 2.0 * beta * std::cos(M_PI * double(k) / double(n + 1)));
			lam_max = std::max(lam_max, lam);
			lam_min = std::min(lam_min, lam);
		}
		return lam_max / lam_min;
	}

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

	template<typename LCU>
	void run_lcu(LCU& obj, size_t n, double alpha, double beta,
		const std::vector<double>& xref, size_t max_j, const std::string& tag, bool dphase)
	{
		double bnorm_inv = 1.0 / std::sqrt(double(n));

		// input: uniform Hadamard; optionally D|b> (parity phase), then the first walk step
		Hadamard_Int(obj.GetInputVecReg(), obj.quantum_walk_obj.get_init_size())(obj.step_state);
		if (dphase)
			TridiagParityPhase(obj.GetInputVecReg())(obj.step_state);
		ClearZero()(obj.step_state);
		obj.quantum_walk_obj.FirstStep(obj.step_state);
		std::printf("State size = {}\n", obj.step_state.size());

		size_t j = 0;
		size_t input_id = System::get(obj.GetInputVecReg());
		while (obj.Step() && j < max_j) {
			auto&& [state, p_succ] = obj.PartialTrace_Nondestructive();
			(void)state;
			++j;
			if (j % 10 == 0) {
				std::printf("[%s] j=%4zu/%zu p_succ=%.6e max_sys=%zu\n",
					tag.c_str(), j, obj.j0, p_succ, System::max_system_size);
				std::fflush(stdout);
			}
		}

		auto&& [state, p_succ] = obj.PartialTrace_Nondestructive();
		std::vector<complex_t> m(n, complex_t(0.0, 0.0));
		for (System& s : state) {
			size_t idx = s.GetAs(input_id, uint64_t);
			m[idx] = s.amplitude * (((idx & 1) && dphase) ? -1.0 : 1.0); // undo D on the output
		}
		double nrm2 = 0.0;
		for (auto& v : m) nrm2 += std::norm(v);
		double nrm = std::sqrt(nrm2);
		double res_dir = -1, res_opt = -1, fid = 0.0;
		if (nrm > 1e-300) {
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
				complex_t v = m[k] / nrm;
				fr += xref[k] * v.real(); fi += xref[k] * v.imag();
			}
			fid = fr * fr + fi * fi;
		}
		std::printf("[FINAL %s] j=%zu p_succ=%.6e res_dir=%.6e res_opt=%.6e fid=%.8f succP_blk=%.6e\n",
			tag.c_str(), j, p_succ, res_dir, res_opt, fid, nrm2);
	}

} // namespace

int main(int argc, const char** argv) {
	argparse::ArgumentParser parser("CKSTridiagonalProbe",
		"CKS LCU solver probe on the Poiseuille tridiagonal case (CPU)");
	parser.add_argument().name("--nqubit").description("main register qubit count (n = 2^q)").required(false);
	parser.add_argument().name("--kappa").description("override condition number").required(false);
	parser.add_argument().name("--eps").description("CKS target precision").required(false);
	parser.add_argument().name("--ds").description("matrix element quantization bits").required(false);
	parser.add_argument().name("--maxJ").description("cap on LCU terms j").required(false);
	parser.add_argument().name("--path").description("qram | direct | both").required(false);
	parser.add_argument().name("--dphase").description("apply D-transform (default 1)").required(false);
	parser.add_argument().name("--j0").description("override LCU truncation point j0").required(false);
	parser.add_argument().name("--coef").description("stock | true (default true: exact 1/x collocation coefficients)").required(false);
	parser.add_argument().name("--J").description("truncation for true coefficients (default 640)").required(false);
	parser.enable_help();
	auto err = parser.parse(argc, argv);
	if (err) { std::cout << err << std::endl; std::exit(2); }
	if (parser.exists("help")) { parser.print_help(); return 0; }

	size_t nqubit = parser.exists("nqubit") ? parser.get<size_t>("nqubit") : 4;
	double alpha = 2.2666666666564934, beta = -1.0;
	size_t n = size_t(1) << nqubit;
	double kappa_arg = parser.exists("kappa") ? parser.get<double>("kappa") : 0.0;
	double eps = parser.exists("eps") ? parser.get<double>("eps") : 0.01;
	size_t ds = parser.exists("ds") ? parser.get<size_t>("ds") : 32;
	size_t max_j = parser.exists("maxJ") ? parser.get<size_t>("maxJ") : size_t(1) << 30;
	std::string path = parser.exists("path") ? parser.get<std::string>("path") : "both";
	bool dphase = parser.exists("dphase") ? parser.get<int>("dphase") != 0 : true;
	size_t j0_override = parser.exists("j0") ? parser.get<size_t>("j0") : 0;
	std::string coef_mode = parser.exists("coef") ? parser.get<std::string>("coef") : "true";
	size_t J_true = parser.exists("J") ? parser.get<size_t>("J") : 640;

	double kappa_analytic = analytic_kappa(alpha, beta, n);
	double kappa = kappa_arg > 0 ? kappa_arg : kappa_analytic;

	auto [mat, spec] = make_tridiagonal_matrix_spec(alpha, beta, n, ds);
	std::printf("[INFO] n=%zu kappa=%.6f (analytic=%.6f) eps=%g ds=%zu nnz_col=%zu alpha_enc=%llu beta_enc=%llu\n",
		n, kappa, kappa_analytic, eps, ds, mat.nnz_col,
		(unsigned long long)spec.alpha_enc, (unsigned long long)spec.beta_enc);

	std::vector<double> xref = thomas_reference(alpha, beta, n);

	// exact walk-matrix spectral interval from the quantized entries:
	// lambda_k(Ahat) = (alpha_q + 2 beta_q cos(k pi/(n+1))) / (2^ds * 4), extremes at k = 1, n
	double ccos = std::cos(M_PI / double(n + 1));
	double denom = double(pow2(ds)) * 4.0;
	double lam1 = (double(spec.alpha_enc) + 2.0 * double(spec.beta_enc) * ccos) / denom;
	double lamn = (double(spec.alpha_enc) - 2.0 * double(spec.beta_enc) * ccos) / denom;
	double m_spec = std::min(lam1, lamn) * (1.0 - 1e-12);
	double M_spec = std::max(lam1, lamn) * (1.0 + 1e-12);
	std::printf("[INFO] Ahat spectrum [%.9f, %.9f] (kappa_interval=%.6f)\n",
		m_spec, M_spec, M_spec / m_spec);

	if (path == "ttest") {
		System::clear();
		auto [mat2, spec2] = make_tridiagonal_matrix_spec(alpha, beta, n, ds);
		QuantumWalkNSteps_Tridiagonal<SparseState> walk2(mat2, spec2);
		walk2.InitEnvironment();
		SparseState st = walk2.CreateSys();
		Hadamard_Int(walk2.j, walk2.get_init_size())(st);
		ClearZero()(st);
		double n0 = 0; for (auto& s : st) n0 += std::norm(s.amplitude);
		std::printf("[ttest] initial |psi|=%f systems=%zu\n", std::sqrt(n0), st.size());
		T_Tridiagonal(walk2.data_offset, walk2.sparse_offset, walk2.j, walk2.b1,
			walk2.k, walk2.b2, walk2.k_comp, 32, &mat2, spec2)(st);
		double n1 = 0; for (auto& s : st) n1 += std::norm(s.amplitude);
		std::printf("[ttest] after T |psi|=%f systems=%zu\n", std::sqrt(n1), st.size());
		T_Tridiagonal(walk2.data_offset, walk2.sparse_offset, walk2.j, walk2.b1,
			walk2.k, walk2.b2, walk2.k_comp, 32, &mat2, spec2).dag(st);
		double n2 = 0; for (auto& s : st) n2 += std::norm(s.amplitude);
		std::printf("[ttest] after T.dag |psi|=%f systems=%zu\n", std::sqrt(n2), st.size());
		return 0;
	}
	if (path == "walkdebug") {
		System::clear();
		auto [mat2, spec2] = make_tridiagonal_matrix_spec(alpha, beta, n, ds);
		QuantumWalkNSteps_Tridiagonal<SparseState> walk2(mat2, spec2);
		walk2.InitEnvironment();
		SparseState st = walk2.CreateSys();

		auto chk = [&](const char* tag) {
			double nn = 0; for (auto& s : st) nn += std::norm(s.amplitude);
			std::printf("    [walkdebug] %-24s |psi|=%.12f systems=%zu\n",
				tag, std::sqrt(nn), st.size());
			std::fflush(stdout);
		};

		Hadamard_Int(walk2.j, walk2.get_init_size())(st); chk("input H");
		ClearZero()(st); chk("ClearZero");

		for (int step = 0; step < 4; ++step) {
			std::printf("[walkdebug] === step %d ===\n", step);
			std::fflush(stdout);
			if (step == 0) {
				T_Tridiagonal(walk2.data_offset, walk2.sparse_offset, walk2.j, walk2.b1,
					walk2.k, walk2.b2, walk2.k_comp, walk2.default_register_size, &mat2, spec2)(st);
				chk("FirstStep.T");
			}
			else {
				ZeroConditionalPhaseFlip({ walk2.j_comp, walk2.k_comp, walk2.b1, walk2.k, walk2.b2 })(st);
				chk("P0");
				T_Tridiagonal(walk2.data_offset, walk2.sparse_offset, walk2.j, walk2.b1,
					walk2.k, walk2.b2, walk2.k_comp, walk2.default_register_size, &mat2, spec2)(st);
				chk("Step.T");
			}
			Swap_General_General(walk2.j, walk2.k)(st); chk("Swap jk");
			Swap_General_General(walk2.b1, walk2.b2)(st); chk("Swap b1b2");
			Swap_General_General(walk2.j_comp, walk2.k_comp)(st); chk("Swap jckc");
			// T.dag fully inlined, op by op (exact mirror sequence)
			{
				const auto& W = walk2;
				AddRegister("data", UnsignedInteger, W.default_register_size)(st); chk("  Td.AddRegister data");
				TridiagOracle2(W.sparse_offset, W.j, W.k, W.k_comp, spec2)(st); chk("  Td.O2f #1");
				TridiagOracle1(W.data_offset, W.j, W.k, "data", spec2)(st); chk("  Td.O1 #1");
				TridiagOracle2(W.sparse_offset, W.j, W.k, W.k_comp, spec2)(st); chk("  Td.O2f #2");
				AddRegister("rotation", Rational, 32)(st); chk("  Td.AddReg rot");
				GetQWRotateAngle_Int_Int_Int("data", W.j, W.k, "rotation", &mat2)(st); chk("  Td.G #1");
				CondRot_Fixed_Bool("rotation", W.b2).dag(st); chk("  Td.Cdag");
				GetQWRotateAngle_Int_Int_Int("data", W.j, W.k, "rotation", &mat2)(st); chk("  Td.G #2");
				RemoveRegister("rotation")(st); chk("  Td.rm rot");
				ClearZero()(st); chk("  Td.ClearZero a");
				TridiagOracle2(W.sparse_offset, W.j, W.k, W.k_comp, spec2).dag(st); chk("  Td.O2d #3");
				TridiagOracle1(W.data_offset, W.j, W.k, "data", spec2)(st); chk("  Td.O1 #2");
				Hadamard_Int(W.k, log2(TridiagonalSpec::slot_count))(st); chk("  Td.H");
				ClearZero()(st); chk("  Td.ClearZero b");
				RemoveRegister("data")(st); chk("  Td.rm data");
			}
		}
		return 0;
	}
	if (path == "qwalkdebug") {
		System::clear();
		auto [mat2, spec2] = make_tridiagonal_matrix_spec(alpha, beta, n, ds);
		QuantumWalkNSteps<SparseState> walk2(mat2);
		walk2.InitEnvironment();
		SparseState st = walk2.CreateSys();

		auto chk = [&](const char* tag) {
			double nn = 0; for (auto& s : st) nn += std::norm(s.amplitude);
			std::printf("    [qwdb] %-24s |psi|=%.12f systems=%zu\n",
				tag, std::sqrt(nn), st.size());
			std::fflush(stdout);
		};
		auto dump = [&](const char* tag) {
			size_t j_id = System::get(walk2.j), k_id = System::get(walk2.k),
				b1_id = System::get(walk2.b1), b2_id = System::get(walk2.b2),
				kc_id = System::get(walk2.k_comp);
			std::printf("    [qwdb] --- %s (first 24) ---\n", tag);
			size_t cnt = 0;
			for (auto& s : st) {
				if (cnt++ >= 24) { std::printf("    ...\n"); break; }
				std::printf("      j=%2zu k=%2zu b1=%zu b2=%zu kcomp=%2zu amp=(%.6f,%+.6f)\n",
					(size_t)s.GetAs(j_id, uint64_t), (size_t)s.GetAs(k_id, uint64_t),
					(size_t)s.GetAs(b1_id, uint64_t), (size_t)s.GetAs(b2_id, uint64_t),
					(size_t)s.GetAs(kc_id, uint64_t),
					s.amplitude.real(), s.amplitude.imag());
			}
			std::fflush(stdout);
		};

		Hadamard_Int(walk2.j, walk2.get_init_size())(st); chk("input H");
		ClearZero()(st); chk("ClearZero");

		for (int step = 0; step < 2; ++step) {
			std::printf("[qwdb] === step %d ===\n", step);
			if (step == 0) {
				T(walk2.qram, walk2.data_offset, walk2.sparse_offset, walk2.j, walk2.b1,
					walk2.k, walk2.b2, walk2.k_comp, walk2.nnz_col, walk2.default_register_size, &mat2)(st);
				chk("FirstStep.T");
			}
			else {
				ZeroConditionalPhaseFlip({ walk2.j_comp, walk2.k_comp, walk2.b1, walk2.k, walk2.b2 })(st);
				chk("P0");
				T(walk2.qram, walk2.data_offset, walk2.sparse_offset, walk2.j, walk2.b1,
					walk2.k, walk2.b2, walk2.k_comp, walk2.nnz_col, walk2.default_register_size, &mat2)(st);
				chk("Step.T");
			}
			Swap_General_General(walk2.j, walk2.k)(st); chk("Swap jk");
			Swap_General_General(walk2.b1, walk2.b2)(st); chk("Swap b1b2");
			Swap_General_General(walk2.j_comp, walk2.k_comp)(st); chk("Swap jckc");
			// QRAM T.dag inlined
			{
				const auto& W = walk2;
				AddRegister("data", UnsignedInteger, W.default_register_size)(st); chk("  Td.AddRegister data");
				SparseMatrixOracle2(W.qram, W.sparse_offset, W.j, W.k, W.k_comp, W.nnz_col)(st); chk("  Td.O2f #1");
				SparseMatrixOracle1(W.qram, W.data_offset, W.j, W.k, "data", W.nnz_col)(st); chk("  Td.O1 #1");
				SparseMatrixOracle2(W.qram, W.sparse_offset, W.j, W.k, W.k_comp, W.nnz_col)(st); chk("  Td.O2f #2");
				AddRegister("rotation", Rational, 32)(st); chk("  Td.AddReg rot");
				GetQWRotateAngle_Int_Int_Int("data", W.j, W.k, "rotation", &mat2)(st); chk("  Td.G #1");
				CondRot_Fixed_Bool("rotation", W.b2).dag(st); chk("  Td.Cdag");
				GetQWRotateAngle_Int_Int_Int("data", W.j, W.k, "rotation", &mat2)(st); chk("  Td.G #2");
				RemoveRegister("rotation")(st); chk("  Td.rm rot");
				ClearZero()(st); chk("  Td.ClearZero a");
				dump("before O2dag #3");
				SparseMatrixOracle2(W.qram, W.sparse_offset, W.j, W.k, W.k_comp, W.nnz_col).dag(st); chk("  Td.O2d #3");
				SparseMatrixOracle1(W.qram, W.data_offset, W.j, W.k, "data", W.nnz_col)(st); chk("  Td.O1 #2");
				dump("before H");
				Hadamard_Int(W.k, log2(W.nnz_col))(st); chk("  Td.H");
				ClearZero()(st); chk("  Td.ClearZero b");
				RemoveRegister("data")(st); chk("  Td.rm data");
			}
		}
		return 0;
	}
	if (path == "qram" || path == "both") {
		System::clear();
		LCU_Container_NoiseFree<SparseState> obj(mat, kappa, eps);
		if (j0_override) obj.j0 = j0_override;
		std::printf("[INFO qram] b=%zu j0=%zu dphase=%d coef=stock\n", obj.b, obj.j0, (int)dphase);
		run_lcu(obj, n, alpha, beta, xref, max_j, "qram", dphase);
	}
	if (path == "direct" || path == "both") {
		System::clear();
		LCU_Container_NoiseFree_Tridiagonal<SparseState> obj(mat, spec, kappa, eps);
		if (j0_override) obj.j0 = j0_override;
		if (coef_mode == "true") {
			static ChebyshevInverseCoefficients inv(m_spec, M_spec, J_true);
			std::printf("[INFO] true coefficients: J=%zu sum|d|=%.6e\n", inv.J, inv.a);
			obj.set_inverse_coefficients(&inv);
		}
		std::printf("[INFO direct] b=%zu j0=%zu dphase=%d coef=%s\n",
			obj.b, obj.j0, (int)dphase, coef_mode.c_str());
		run_lcu(obj, n, alpha, beta, xref, max_j, "direct", dphase);
	}
	return 0;
}