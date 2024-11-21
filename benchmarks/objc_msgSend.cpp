#include <benchmark/benchmark.h>
#include "../objc/runtime.h"
#include <assert.h>

static id testClass;
static SEL answerSel;

static void objc_msgSendSetup(const benchmark::State& state) {
  testClass = objc_getClass("TestClass");
  answerSel = sel_registerName("answer");
}

static void objc_msgSend(benchmark::State& state) {
    for (auto _ : state)
	  {
      id a = objc_msgSend(testClass, answerSel);
      assert((uintptr_t)a == 42);
    }
  }
  // Register the function as a benchmark
  BENCHMARK(objc_msgSend)->Setup(objc_msgSendSetup)->Repetitions(25);
  
  BENCHMARK_MAIN();
