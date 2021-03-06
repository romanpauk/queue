
// This file is part of queue project <https://github.com/romanpauk/queue>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <queue/queue.h>

#include <benchmark/benchmark.h>

#include <thread>

const size_t QueuePushIterations = 1 << 20;
template < typename T > static void queue_push(benchmark::State& state)
{    
    T queue(QueuePushIterations << 1);

    for (auto _ : state)
    {        
        while(!queue.push(1));
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

template < typename T > static void queue_pop(benchmark::State& state)
{
    typename T::storage_type storage(QueuePushIterations << 1);
    T queue(storage);
    for (size_t i = 0; i < state.iterations(); ++i)
    {
        while(!queue.push(1));
    }

    typename T::value_type value;
    for (auto _ : state)
    {
        while(!queue.pop(value));
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

template < typename T > static void queue_push_pop_parallel(benchmark::State& state)
{    
    static T queue(1024);
    static auto thread = std::thread([&]
    {
        typename T::value_type value;
        while (true)
        {         
            while(!queue.pop(value));
        }            
    });

    for (auto _ : state)
    {
        for (auto i = state.range(0); i--;)
        {
            while(!queue.push(1));
        }
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

template < typename T > static void queue_push_pop_parallel_batch(benchmark::State& state)
{    
    static T queue(1024);
    static auto thread = std::thread([&]
    {
        std::array< typename T::value_type, 1024 > values;
        while (true)
        {            
            queue.pop(values);
        }            
    });

    for (auto _ : state)
    {
        for (auto i = state.range(0); i--;)
        {
            while(!queue.push(1));
        }
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

template < typename T > static void queue_push_pop_sequential(benchmark::State& state)
{    
    T queue(1024);
    typename T::value_type value;

    for (auto _ : state)
    {
        for (auto i = state.range(0); i--;)
        {
            queue.push(1);
            while(!queue.pop(value));
        }
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

static void faa_shared(benchmark::State& state)
{
    static std::atomic< size_t > value;

    for (auto _ : state)
    {
        for (auto i = state.range(0); i--;)
        {
            value.fetch_add(1);
        }
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

static void faa_local(benchmark::State& state)
{
    static thread_local std::atomic< size_t > value;

    for (auto _ : state)
    {
        for (auto i = state.range(0); i--;)
        {
            value.fetch_add(1);
        }
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * state.range(0));
}

auto threads_max = std::thread::hardware_concurrency();
auto range_min = 1 << 20;
auto range_max = 1 << 20;

//BENCHMARK(faa_shared)->UseRealTime()->ThreadRange(1, threads_max)->Range(range_min, range_max);
//BENCHMARK(faa_local)->UseRealTime()->ThreadRange(1, threads_max)->Range(range_min, range_max);

BENCHMARK_TEMPLATE(queue_push, queue::bounded_queue< int, queue::dynamic_storage< int > >)->UseRealTime()->Threads(1)->Iterations(QueuePushIterations);
BENCHMARK_TEMPLATE(queue_push, queue::bounded_queue_spsc1< int, queue::dynamic_storage< queue::entry< int > > >)->UseRealTime()->Threads(1)->Iterations(QueuePushIterations);
//BENCHMARK_TEMPLATE(queue_pop, queue::bounded_queue_spsc2< int, queue::dynamic_storage< int > >)->UseRealTime()->Threads(1)->Iterations(QueuePushIterations);
BENCHMARK_TEMPLATE(queue_push, queue::bounded_queue_spsc2< int, queue::dynamic_storage< int > >)->UseRealTime()->Threads(1)->Iterations(QueuePushIterations);
//BENCHMARK_TEMPLATE(queue_push, queue::bounded_queue_mpsc< int, queue::dynamic_storage< queue::entry< int > > >)->UseRealTime()->Threads(1)->Iterations(QueuePushIterations);
//BENCHMARK_TEMPLATE(queue_pop, queue::bounded_queue_mpsc2< int, queue::dynamic_storage< int > >)->UseRealTime()->Threads(1)->Iterations(QueuePushIterations);

BENCHMARK_TEMPLATE(queue_push_pop_parallel, queue::bounded_queue_spsc1< int, queue::static_storage< queue::entry< int >, 1024 > >)->UseRealTime()->Threads(1)->Range(range_min, range_max);
BENCHMARK_TEMPLATE(queue_push_pop_parallel_batch, queue::bounded_queue_spsc1< int, queue::static_storage< queue::entry< int >, 1024 > >)->UseRealTime()->Threads(1)->Range(range_min, range_max);
BENCHMARK_TEMPLATE(queue_push_pop_parallel, queue::bounded_queue_spsc2< int, queue::static_storage< int, 1024 > >)->UseRealTime()->Threads(1)->Range(range_min, range_max);
BENCHMARK_TEMPLATE(queue_push_pop_parallel_batch, queue::bounded_queue_spsc2< int, queue::static_storage< int, 1024 > >)->UseRealTime()->Threads(1)->Range(range_min, range_max);

//BENCHMARK_TEMPLATE(queue_push_pop_parallel, queue::bounded_queue_mpsc< int, queue::static_storage< queue::entry< int >, 1024 > >)->UseRealTime()->ThreadRange(1, threads_max)->Range(range_min, range_max);
//BENCHMARK_TEMPLATE(queue_push_pop_parallel_batch, queue::bounded_queue_mpsc2< int, queue::static_storage< int, 1024 > >)->UseRealTime()->ThreadRange(1, threads_max)->Range(range_min, range_max);

BENCHMARK_TEMPLATE(queue_push_pop_sequential, queue::bounded_queue_spsc1< int, queue::static_storage< queue::entry< int >, 1024 > >)->UseRealTime()->Threads(1)->Range(range_min, range_max);
BENCHMARK_TEMPLATE(queue_push_pop_sequential, queue::bounded_queue_spsc2< int, queue::static_storage< int, 1024 > >)->UseRealTime()->Threads(1)->Range(range_min, range_max);
//BENCHMARK_TEMPLATE(queue_push_pop_sequential, queue::bounded_queue_mpsc< int, queue::static_storage< queue::entry< int >, 1024 > >)->UseRealTime()->Threads(1)->Range(range_min, range_max);

BENCHMARK_MAIN();
