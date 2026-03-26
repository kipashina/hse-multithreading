#include "apply_function.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <numeric>
#include <vector>

namespace {

void add_one(int& value) {
  ++value;
}

std::uint64_t medium_calc(std::uint64_t value) {
  for (int i = 0; i < 50; ++i) {
    value ^= (value << 13);
    value ^= (value >> 7);
    value ^= (value << 17);
    value += 0x9e3779b97f4a7c15ULL;
  }
  return value;
}

void medium_work(std::uint64_t& value) {
  value = medium_calc(value);
}

std::uint64_t heavy_calc(std::uint64_t value) {
  for (int i = 0; i < 2000; ++i) {
    value ^= (value << 13);
    value ^= (value >> 7);
    value ^= (value << 17);
    value += 0x9e3779b97f4a7c15ULL;
  }
  return value;
}

void heavy_work(std::uint64_t& value) {
  value = heavy_calc(value);
}

} 

//Меняется только размер массива.

static void bm_by_size(benchmark::State& state) {
  const int data_size = static_cast<int>(state.range(0));
  const int thread_count = static_cast<int>(state.range(1));

  std::vector<std::uint64_t> data(data_size);

  for (auto _ : state) {
    state.PauseTiming();
    std::iota(data.begin(), data.end(), 1ULL);
    state.ResumeTiming();

    ApplyFunction<std::uint64_t>(data, medium_work, thread_count);

    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * data_size);
}

BENCHMARK(bm_by_size)
    ->Args({256, 1})
    ->Args({256, 4})
    ->UseRealTime();

BENCHMARK(bm_by_size)
    ->Args({1 << 20, 1})
    ->Args({1 << 20, 4})
    ->UseRealTime();

//Меняется только функция

static void bm_light_function(benchmark::State& state) {
  const int data_size = static_cast<int>(state.range(0));
  const int thread_count = static_cast<int>(state.range(1));

  std::vector<int> data(data_size);

  for (auto _ : state) {
    state.PauseTiming();
    std::iota(data.begin(), data.end(), 0);
    state.ResumeTiming();

    ApplyFunction<int>(data, add_one, thread_count);

    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * data_size);
}

static void bm_heavy_function(benchmark::State& state) {
  const int data_size = static_cast<int>(state.range(0));
  const int thread_count = static_cast<int>(state.range(1));

  std::vector<std::uint64_t> data(data_size);

  for (auto _ : state) {
    state.PauseTiming();
    std::iota(data.begin(), data.end(), 1ULL);
    state.ResumeTiming();

    ApplyFunction<std::uint64_t>(data, heavy_work, thread_count);

    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
  }

  state.SetItemsProcessed(state.iterations() * data_size);
}

BENCHMARK(bm_light_function)
    ->Args({1024, 1})
    ->Args({1024, 4})
    ->UseRealTime();

BENCHMARK(bm_heavy_function)
    ->Args({1024, 1})
    ->Args({1024, 4})
    ->UseRealTime();

BENCHMARK_MAIN();