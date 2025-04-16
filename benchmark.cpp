#include <benchmark/benchmark.h>

static void BM_Example(benchmark::State& state) {
    for (auto _ : state) {
        int x = 123 * 123;
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_Example);
BENCHMARK_MAIN();
