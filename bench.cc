#include <benchmark/benchmark.h>

#define LIBSIG_MAIN
#include <sig.hh>

#define B(name, block) \
	static void BM_##name(benchmark::State &state) block \
	BENCHMARK(BM_##name)

using namespace libsig;

B(sig_write, {
	sig<int> i;
	for (auto _ : state) {
		benchmark::DoNotOptimize(i.operator=(10));
	}
});

B(value_write, {
	val<int> i;
	for (auto _ : state) {
		benchmark::DoNotOptimize(i.operator=(10));
	}
});

B(sig_sample, {
	sig<int> i(42);
	for (auto _ : state) {
		benchmark::DoNotOptimize(i.sample());
	}
});

B(value_sample, {
	val<int> i(42);
	for (auto _ : state) {
		benchmark::DoNotOptimize(i.sample());
	}
});

B(sig_retrieve, {
	sig<int> i(42);
	for (auto _ : state) {
		benchmark::DoNotOptimize((int)i);
	}
});

B(value_retrieve, {
	val<int> i(42);
	for (auto _ : state) {
		benchmark::DoNotOptimize((int)i);
	}
});

B(sig_eq_true, {
	sig<int> l(10);
	sig<int> r(10);

	for (auto _ : state) {
		benchmark::DoNotOptimize(l == r);
	}
});

B(value_eq_true, {
	val<int> l(10);
	val<int> r(10);

	for (auto _ : state) {
		benchmark::DoNotOptimize(l == r);
	}
});

B(sig_eq_false, {
	sig<int> l(10);
	sig<int> r(9999);

	for (auto _ : state) {
		benchmark::DoNotOptimize(l == r);
	}
});

B(value_eq_false, {
	val<int> l(10);
	val<int> r(9999);

	for (auto _ : state) {
		benchmark::DoNotOptimize(l == r);
	}
});

B(basic_computation, {
	val<int> i(10);
	val<int> j;

	sig_root root([=]() mutable {
		S([=]() mutable {
			j = i * 10;
		});
	});

	for (auto _ : state) {
		benchmark::DoNotOptimize(i = 20);
	}
});

BENCHMARK_MAIN();
