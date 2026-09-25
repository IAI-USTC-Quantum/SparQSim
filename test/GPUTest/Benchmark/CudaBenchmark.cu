// CUDA-vs-CPU pipeline benchmark (correctness first, timing second).
//
// 同一流水线分别在 CPU 路径（std::vector<System>）与 GPU 路径（CuSparseState）
// 上执行，逐基态对比最终振幅（键完全一致 + 复振幅容差比较），并对小规模
// 场景做解析值抽查。计时为预热后的单次运行（GPU 每算子后同步）。
//
// Pipeline: Hadamard_Int(addr,n) → Add_UInt_UInt(addr,addr,sum) →
//           Sqrt_UInt(addr,sq) → X_Bool(b) → Rot_Bool(b, R(π/3)) →
//           GlobalPhase(e^{iπ/5}) → Normalize
//
// 解析期望：Hadamard 后每基态振幅 1/√2^n；X→b=1；R(π/3) 把 |1⟩ 分支振幅
// 变为 c=cos(π/3)=1/2，并分裂出 b=0 分支振幅 -s=-√3/2；GlobalPhase 乘
// e^{iπ/5}；Normalize 数值上不变。即：
//   amp(addr=k, sum=2k, sq=isqrt(k), b=1) = (1/2)·e^{iπ/5}/√(2^n)
//   amp(addr=k, sum=2k, sq=isqrt(k), b=0) = -(√3/2)·e^{iπ/5}/√(2^n)
#include <algorithm>
#include <chrono>
#include <complex>
#include <cstdio>
#include <vector>
#include "cuda/sparse_state_simulator.cuh"

using namespace qram_simulator;

namespace
{
	using clock_t_ = std::chrono::steady_clock;

	double elapsed_ms(clock_t_::time_point t0, clock_t_::time_point t1)
	{
		return std::chrono::duration<double, std::milli>(t1 - t0).count();
	}

	// —— 状态比较：按寄存器键排序后逐基态比较复振幅 ——
	// 返回 0 键个数不一致；否则返回最大振幅偏差。
	double compare_states(std::vector<System>& cpu, std::vector<System>& gpu,
		size_t nregs, std::string& detail)
	{
		auto key_less = [nregs](const System& a, const System& b) {
			for (size_t i = 0; i < nregs; ++i)
			{
				uint64_t va = a.get(i).value, vb = b.get(i).value;
				if (va != vb) return va < vb;
			}
			return false;
		};
		std::sort(cpu.begin(), cpu.end(), key_less);
		std::sort(gpu.begin(), gpu.end(), key_less);

		if (cpu.size() != gpu.size())
		{
			detail = fmt::format("state count mismatch: cpu={} gpu={}", cpu.size(), gpu.size());
			return -1.0;
		}
		double max_diff = 0.0;
		for (size_t i = 0; i < cpu.size(); ++i)
		{
			for (size_t r = 0; r < nregs; ++r)
			{
				if (cpu[i].get(r).value != gpu[i].get(r).value)
				{
					detail = fmt::format("key mismatch at #{} reg{}", i, r);
					return -2.0;
				}
			}
			max_diff = std::max(max_diff,
				std::abs(cpu[i].amplitude - gpu[i].amplitude));
		}
		return max_diff;
	}

	// 由 (addr, b) 查最终振幅（用于解析抽查）
	complex_t find_amp(const std::vector<System>& s, size_t addr_id,
		size_t b_id, uint64_t addr, uint64_t b)
	{
		for (const auto& st : s)
			if (st.get(addr_id).value == addr && st.get(b_id).value == b)
				return st.amplitude;
		return complex_t(NAN, NAN);
	}
}

// 单个场景：寄存器宽 n 比特。返回是否通过。
bool run_scenario(const char* name, size_t n, bool analytic_check)
{
	System::clear();
	auto addr = System::add_register("addr", UnsignedInteger, n);
	auto sum = System::add_register("sum", UnsignedInteger, n + 1);
	auto sq = System::add_register("sq", UnsignedInteger, n / 2 + 1);
	auto b = System::add_register("b", UnsignedInteger, 1);
	const size_t nregs = 4;

	const double theta = pi / 3.0;
	u22_t rot(
		complex_t(std::cos(theta), 0), complex_t(-std::sin(theta), 0),
		complex_t(std::sin(theta), 0), complex_t(std::cos(theta), 0));
	const complex_t phase(std::cos(pi / 5.0), std::sin(pi / 5.0));

	// —— 构造流水线（两侧共享同一组算子对象与参数）——
	auto build_init_state = [&](auto& state) {
		Init_Unsafe(addr, 0)(state);
		Init_Unsafe(sum, 0)(state);
		Init_Unsafe(sq, 0)(state);
		Init_Unsafe(b, 0)(state);
		Hadamard_Int(addr, n)(state);
	};
	auto run_pipeline = [&](auto& state) {
		Add_UInt_UInt(addr, addr, sum)(state);
		Sqrt_UInt(addr, sq)(state);
		X_Bool{b}(state);            // 单实参临时对象 + 调用须用花括号，避免 most-vexing-parse
		Rot_Bool{b, rot}(state);
		GlobalPhase{phase}(state);
		Normalize()(state);
	};

	fmt::print("\n===== scenario {} (addr {} bits, {} basis states) =====\n",
		name, n, pow2(n));

	// —— CPU 路径：warmup + 计时 ——
	// 注意：CPU 路径的算子（如 Hadamard_Int）假定状态非空——
	// 须先播种一个基态（GPU 路径的 Init_Unsafe 会自行创建，行为不对称）。
	std::vector<System> cpu_state;
	{
		cpu_state.emplace_back();
		build_init_state(cpu_state);
		run_pipeline(cpu_state);                      // warmup
		cpu_state.clear();
		cpu_state.emplace_back();
		build_init_state(cpu_state);                  // 重建初始态
		auto t0 = clock_t_::now();
		run_pipeline(cpu_state);
		auto t1 = clock_t_::now();
		fmt::print("CPU  pipeline: {:>10.3f} ms  ({} states)\n",
			elapsed_ms(t0, t1), cpu_state.size());
	}

	// —— GPU 路径：warmup + 计时（含 H2D/D2H）——
	std::vector<System> gpu_state;
	{
		CuSparseState warm;
		build_init_state(warm);
		run_pipeline(warm);                           // warmup（含上下文初始化）
	}
	double gpu_total_ms = 0.0, gpu_ops_ms = 0.0;
	{
		CuSparseState s;
		auto t0 = clock_t_::now();
		build_init_state(s);                          // CPU 驻留初始化，流水线自行迁移上 GPU
		auto t_ops0 = clock_t_::now();
		run_pipeline(s);
		CUDA_CHECK(cudaDeviceSynchronize());
		auto t_ops1 = clock_t_::now();
		gpu_state = s.get_cpu_copy();
		auto t1 = clock_t_::now();
		CUDA_CHECK(cudaGetLastError());
		gpu_total_ms = elapsed_ms(t0, t1);
		gpu_ops_ms = elapsed_ms(t_ops0, t_ops1);
		fmt::print("GPU  pipeline: {:>10.3f} ms  (ops {:>8.3f} ms + transfers/init, {} states)\n",
			gpu_total_ms, gpu_ops_ms, gpu_state.size());
	}

	// —— 结果对比 ——
	std::string detail;
	double max_diff = compare_states(cpu_state, gpu_state, nregs, detail);
	bool ok = (max_diff >= 0.0) && (max_diff < 1e-9);
	fmt::print("CPU-vs-GPU: {}  max|dAmp| = {:.3e}{}\n",
		ok ? "PASS" : "FAIL", max_diff < 0 ? -1 : max_diff,
		detail.empty() ? "" : "  [" + detail + "]");

	// —— 概率归一性（两侧）——
	auto prob_sum = [](const std::vector<System>& s) {
		double p = 0;
		for (const auto& st : s) p += std::norm(st.amplitude);
		return p;
	};
	double p_cpu = prob_sum(cpu_state), p_gpu = prob_sum(gpu_state);
	bool norm_ok = std::abs(p_cpu - 1.0) < 1e-9 && std::abs(p_gpu - 1.0) < 1e-9;
	fmt::print("normalization: {}  (P_cpu = {:.12f}, P_gpu = {:.12f})\n",
		norm_ok ? "PASS" : "FAIL", p_cpu, p_gpu);
	ok = ok && norm_ok;

	// —— 解析值抽查（小规模）——
	if (analytic_check)
	{
		const double inv_sqrt = 1.0 / std::sqrt((double)pow2(n));
		const complex_t expect_b1 = 0.5 * phase * inv_sqrt;
		const complex_t expect_b0 = -std::sqrt(3.0) / 2 * phase * inv_sqrt;
		bool ana_ok = true;
		for (uint64_t k : { (uint64_t)0, (uint64_t)3, pow2(n) - 1 })
		{
			complex_t a1 = find_amp(cpu_state, addr, b, k, 1);
			complex_t a0 = find_amp(cpu_state, addr, b, k, 0);
			if (std::abs(a1 - expect_b1) > 1e-12 || std::abs(a0 - expect_b0) > 1e-12)
			{
				ana_ok = false;
				fmt::print("analytic mismatch at addr={}: b1=({}, {}) expect=({}, {})\n",
					k, a1.real(), a1.imag(), expect_b1.real(), expect_b1.imag());
			}
			// sum = 2·addr, sq = isqrt(addr) 键校验
			for (const auto& st : cpu_state)
			{
				if (st.get(addr).value == k)
				{
					if (st.get(sum).value != 2 * k) { ana_ok = false; fmt::print("sum wrong at k={}\n", k); }
					uint64_t r = 0; while ((r + 1) * (r + 1) <= k) ++r;
					if (st.get(sq).value != r) { ana_ok = false; fmt::print("sqrt wrong at k={}\n", k); }
					break;
				}
			}
		}
		fmt::print("analytic spot-check: {}\n", ana_ok ? "PASS" : "FAIL");
		ok = ok && ana_ok;
	}

	return ok;
}

int main()
{
	try
	{
		fmt::print("== SparQSim CUDA-vs-CPU benchmark ==\n");
		std::fflush(stdout);
		bool ok = true;
		ok = run_scenario("small", 8, /*analytic_check=*/true) && ok;
		std::fflush(stdout);
		ok = run_scenario("large", 14, /*analytic_check=*/false) && ok;
		fmt::print("\n== {}: {} ==\n", ok ? "ALL PASS" : "FAILURES DETECTED", ok ? "CPU 与 GPU 计算结果一致" : "结果不一致，请检查");
		return ok ? 0 : 1;
	}
	catch (const std::exception& e)
	{
		fmt::print("Error: {}\n", e.what());
		return 2;
	}
}
