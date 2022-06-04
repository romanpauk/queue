//
// This file is part of queue project <https://github.com/romanpauk/queue>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <queue/queue.h>

#include <benchmark/benchmark.h>

#include <thread>

template < typename T > static void queue_push(benchmark::State& state)
{
    static typename T::storage_type storage(1 << 20);
    static T queue(storage);
    
    for (auto _ : state)
    {        
        for (auto i = state.range(0); i--;)
        {
            queue.push(1);
        }
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

template < typename T > static void queue_push_pop(benchmark::State& state)
{
    static typename T::storage_type storage(1024);
    static T queue(storage);

    for (auto _ : state)
    {
        for (auto i = state.range(0); i--;)
        {
            if (state.thread_index == 0)
            {
                if (!queue.empty())
                    queue.pop();
            }
            else
            {
                queue.push(1);
            }
        }
    }

    if(state.thread_index != 0)
        state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

static void faa(benchmark::State& state)
{
    static std::atomic< size_t > value;

    for (auto _ : state)
    {
        for (auto i = state.range(0); i--;)
        {
            value.fetch_add(1);
        }
    }

    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) * state.range(0));
}

auto threads_max = std::thread::hardware_concurrency();
auto range_min = 1 << 16;
auto range_max = 1 << 16;

//BENCHMARK(faa)->UseRealTime()->Threads(2)->Range(range_min, range_max)->ThreadRange(2, threads_max);

BENCHMARK_TEMPLATE(queue_push, queue::bounded_queue_spsc< int, queue::dynamic_storage< int > >)->UseRealTime()->Threads(1)->Range(range_min, range_max);
BENCHMARK_TEMPLATE(queue_push, queue::bounded_queue_mpsc< int, queue::dynamic_storage< int > >)->UseRealTime()->ThreadRange(1, threads_max)->Range(range_min, range_max);

BENCHMARK_TEMPLATE(queue_push_pop, queue::bounded_queue_spsc< int, queue::static_storage< int, 1024 > >)->UseRealTime()->Threads(2)->Range(range_min, range_max);
BENCHMARK_TEMPLATE(queue_push_pop, queue::bounded_queue_mpsc< int, queue::static_storage< int, 1024 > >)->UseRealTime()->ThreadRange(2, threads_max)->Range(range_min, range_max);

BENCHMARK_MAIN();
