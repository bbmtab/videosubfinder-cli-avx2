//////////////////////////////////////////////////////////////////////////////////
// VSFProfiler.h - STEP 2 instrumentation (build with -DVSF_PROFILE)
//
// Reports per-function wall time and call counts for the hot comparison /
// intersection functions. Design constraints:
//   * must not change program logic or output (only clock reads and counters),
//   * must be safe under concurrency::parallel_invoke and BS::thread_pool,
//   * zero cost when VSF_PROFILE is not defined,
//   * one table at process exit, no per-call I/O.
//
// Thread model: each thread claims a slot on first use and increments its own
// slot with plain adds (no atomics in the hot path). Slots live in a global
// array that is never destroyed, so a worker pool thread exiting before the
// atexit flush cannot invalidate anything.
//////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef VSF_PROFILE

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace vsfprof
{

enum FnId : int
{
	P_CMP_TWO_SUBS_OPTIMAL = 0,
	P_ANALYSE_IMAGE,
	P_INTERSECT_Y_2,
	P_INTERSECT_Y_N,
	P_INTERSECT_TWO_IMAGES,
	P_INTERSECT_IMAGES,
	P_COUNT
};

inline const char* const g_names[P_COUNT] = {
	"CompareTwoSubsOptimal",
	"AnalyseImage",
	"IntersectYImages(2)",
	"IntersectYImages(N)",
	"IntersectTwoImages",
	"IntersectImages"
};

struct PerThread
{
	unsigned long long calls[P_COUNT];
	unsigned long long ns[P_COUNT];
};

constexpr int kMaxThreads = 1024;

inline PerThread g_tls[kMaxThreads] = {};
inline std::atomic<int> g_threads_used(0);
inline std::chrono::steady_clock::time_point g_first_call;

inline int thread_index()
{
	// Function-local thread_local in an inline function: one instance per thread
	// for the whole program, across every translation unit.
	static thread_local int idx = -1;
	if (idx < 0)
	{
		idx = g_threads_used.fetch_add(1, std::memory_order_relaxed);
		if (idx >= kMaxThreads)
		{
			idx = kMaxThreads - 1;
		}
	}
	return idx;
}

inline void report()
{
	const auto elapsed_ns = (unsigned long long)std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now() - g_first_call).count();

	const int used = g_threads_used.load(std::memory_order_relaxed) < kMaxThreads
		? g_threads_used.load(std::memory_order_relaxed)
		: kMaxThreads;

	unsigned long long calls[P_COUNT] = {};
	unsigned long long ns[P_COUNT] = {};

	for (int t = 0; t < used; t++)
	{
		for (int f = 0; f < P_COUNT; f++)
		{
			calls[f] += g_tls[t].calls[f];
			ns[f] += g_tls[t].ns[f];
		}
	}

	unsigned long long sum_ns = 0;
	for (int f = 0; f < P_COUNT; f++)
	{
		sum_ns += ns[f];
	}

	// Order by total time, descending (fixed 6 entries -> insertion sort).
	int order[P_COUNT];
	for (int f = 0; f < P_COUNT; f++)
	{
		order[f] = f;
	}
	for (int a = 1; a < P_COUNT; a++)
	{
		int key = order[a];
		int b = a - 1;
		while (b >= 0 && ns[order[b]] < ns[key])
		{
			order[b + 1] = order[b];
			b--;
		}
		order[b + 1] = key;
	}

	std::printf("\n=== VSF PROFILE (instrumented build, times in ms) ===\n");
	std::printf("elapsed since first call : %.3f s\n", (double)elapsed_ns / 1e9);
	std::printf("instrumented total       : %.3f s (inclusive, all threads)\n", (double)sum_ns / 1e9);
	std::printf("thread slots used        : %d\n", used);
	std::printf("%-26s %12s %14s %9s %9s %12s\n",
		"function", "calls", "total_ms", "%busy", "%wall", "ns/call");
	for (int i = 0; i < P_COUNT; i++)
	{
		const int f = order[i];
		const double total_ms = (double)ns[f] / 1e6;
		const double busy_pct = sum_ns > 0 ? (double)ns[f] * 100.0 / (double)sum_ns : 0.0;
		const double wall_pct = elapsed_ns > 0 ? (double)ns[f] * 100.0 / (double)elapsed_ns : 0.0;
		const double ns_per_call = calls[f] > 0 ? (double)ns[f] / (double)calls[f] : 0.0;
		std::printf("%-26s %12llu %14.3f %9.2f %9.2f %12.1f\n",
			g_names[f], calls[f], total_ms, busy_pct, wall_pct, ns_per_call);
	}
	std::printf("=== END VSF PROFILE ===\n");
	std::fflush(stdout);
}

inline void ensure_report_registered()
{
	static const bool registered = [] {
		g_first_call = std::chrono::steady_clock::now();
		std::atexit(report);
		return true;
	}();
	(void)registered;
}

// RAII wall-clock scope: one clock read on entry, one on exit, two plain adds.
class Scope
{
public:
	explicit Scope(FnId fn) : m_fn(fn)
	{
		ensure_report_registered();
		m_t0 = std::chrono::steady_clock::now();
	}

	~Scope()
	{
		const unsigned long long dt = (unsigned long long)std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now() - m_t0).count();
		PerThread& p = g_tls[thread_index()];
		p.calls[m_fn]++;
		p.ns[m_fn] += dt;
	}

	Scope(const Scope&) = delete;
	Scope& operator=(const Scope&) = delete;

private:
	FnId m_fn;
	std::chrono::steady_clock::time_point m_t0;
};

} // namespace vsfprof

#define VSF_PROFILE_SCOPE(id) vsfprof::Scope _vsf_prof_scope_(vsfprof::id)

#else

#define VSF_PROFILE_SCOPE(id) ((void)0)

#endif // VSF_PROFILE
