#include <benchmark/benchmark.h>
#include "../objc/runtime.h"
#include <assert.h>

static id testClass;
static SEL answerSel;

static void objc_msgSend(benchmark::State& state) {
  id testClass = objc_getClass("TestClass");
  SEL answerSel = sel_registerName("answer");
    for (auto _ : state)
	  {
      id a = objc_msgSend(testClass, answerSel);
      assert((uintptr_t)a == 42);
    }
  }
  // Register the function as a benchmark
  BENCHMARK(objc_msgSend)->Repetitions(25);
  
  BENCHMARK_MAIN();
