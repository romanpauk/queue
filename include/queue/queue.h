//
// This file is part of queue project <https://github.com/romanpauk/queue>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <array>
#include <memory>
#include <atomic>
#include <new>
#include <cstdlib>

namespace queue
{
    enum { CacheLineSize = std::hardware_destructive_interference_size, };

    template < typename T > class cell
    {
    public:
        cell()
            : state(0)
        {}

        T value;
        std::atomic< unsigned > state;
    };

    template < typename T, size_t Size > class static_storage
    {      
        static_assert((Size & Size - 1) == 0);

    public:
        using value_type = cell< T >;

        static_storage(size_t = 0) {}

        constexpr size_t size() const { return Size; }
        constexpr size_t mask() const { return Size - 1; }

        value_type& operator [](size_t i) { return data_[i]; }

    private:
        std::array< value_type, Size > data_;
    };

    template < typename T > class cell2
    {
    public:
        T value;
    };

    template < typename T, size_t Size > class static_storage2
    {
        static_assert((Size& Size - 1) == 0);

    public:
        using value_type = cell2< T >;

        static_storage2(size_t = 0) {}

        constexpr size_t size() const { return Size; }
        constexpr size_t mask() const { return Size - 1; }

        value_type& operator [](size_t i) { return data_[i]; }

    private:
        std::array< value_type, Size > data_;
    };

    template < typename T > class dynamic_storage
    {
    public:
        using value_type = cell< T >;

        dynamic_storage(size_t size)
            : size_(size)
            , mask_(size - 1)
        {
            if((size & size - 1) != 0)
                std::abort();

            data_.reset(new cell< T >[size]);
        }

        size_t size() const { return size_; }
        size_t mask() const { return mask_; }

        value_type& operator [](size_t i) { return data_[i]; }

    private:
        std::unique_ptr< value_type[] > data_;
        size_t size_;
        size_t mask_;
    };

    template < typename T > class dynamic_storage2
    {
    public:
        using value_type = cell2< T >;

        dynamic_storage2(size_t size)
            : size_(size)
            , mask_(size - 1)
        {
            if ((size & size - 1) != 0)
                std::abort();

            data_.reset(new value_type[size]);
        }

        size_t size() const { return size_; }
        size_t mask() const { return mask_; }

        value_type& operator [](size_t i) { return data_[i]; }

    private:
        std::unique_ptr< value_type[] > data_;
        size_t size_;
        size_t mask_;
    };

    template < typename T, typename Storage > class bounded_queue
    {
    public:
        using value_type = T;
        using storage_type = Storage;

        template < typename... Args > bounded_queue(Args&&... args)
            : storage_(std::forward< Args >(args)...)
            , head_(0)
            , tail_(0)
        {}

        template < typename Ty > void push(Ty&& value)
        {
            size_t index = tail_++ & storage_.mask();
            storage_[index].value = std::forward< Ty >(value);
        }

        value_type pop()
        {
            T value = std::move(storage_[head_].value);
            head_ = head_ + 1 & storage_.mask();
            return value;
        }

    private:
        size_t tail_;
        size_t head_;
        storage_type storage_;
    };

    template < typename T, typename Storage > class bounded_queue_mpsc2
    {
    public:
        using value_type = T;
        using storage_type = Storage;

        template< typename... Args > bounded_queue_mpsc2(Args&&... args)
            : storage_(std::forward< Args >(args)...)
            , head_(0)
            , tail_(0)
        {}

        template < typename Ty > void push(Ty&& value)
        {
            size_t index = tail_.fetch_add(1, std::memory_order_relaxed) & storage_.mask();

            while (storage_[index].state.load(std::memory_order_acquire) != 0)
            {
                // wait till consumers consume storage_[index] and it is free to be written
            }

            storage_[index].value = std::forward< Ty >(value);
            storage_[index].state.store(1, std::memory_order_release);
        }

        value_type pop()
        {
            while (storage_[head_].state.load(std::memory_order_acquire) == 0)
            {
                // wait till there is something to consume
            }

            T value = std::move(storage_[head_].value);
            storage_[head_].state.store(0, std::memory_order_release);
            head_ = head_ + 1 & storage_.mask();
            return value;
        }

        template< bool Blocking, size_t N > size_t pop(std::array< T, N >& values)
        {
            while (storage_[head_].state.load(std::memory_order_acquire) == 0)
            {
                if(!Blocking)
                    return 0;
            }

            size_t i = 0;
            while (i < N)
            {
                auto& data = storage_[head_ + i & storage_.mask()];
                if (data.state.load(std::memory_order_relaxed) != 0)
                {
                    values[i] = std::move(data.value);
                    data.state.store(0, std::memory_order_relaxed);
                    i++;
                }
                else
                {
                    break;
                }
            } 

            head_ = head_ + i & storage_.mask();
            std::atomic_thread_fence(std::memory_order_release);
            return i;
        }

        bool empty() const
        {
            return head_ == tail_.load(std::memory_order_relaxed);
        }

        // TODO:
        // void clear()
        // This requires resetting the cells' states

    private:
        alignas(CacheLineSize) std::atomic< size_t > tail_;
        alignas(CacheLineSize) size_t head_;
        alignas(CacheLineSize) storage_type storage_;
    };

    template < typename T, typename Storage > class bounded_queue_spsc2
    {
    public:
        using value_type = T;
        using storage_type = Storage;

        template< typename... Args > bounded_queue_spsc2(Args&&... args)
            : storage_(std::forward< Args >(args)...)
            , head_(0)
            , tail_(0)
        {}

        template < typename Ty > void push(Ty&& value)
        {
            size_t index = tail_++ & storage_.mask();

            while (storage_[index].state.load(std::memory_order_acquire) != 0)
            {
                // wait till consumers consume storage_[index] and it is free to be written
            }

            storage_[index].value = std::forward< Ty >(value);
            storage_[index].state.store(1, std::memory_order_release);
        }

        value_type pop()
        {
            while (storage_[head_].state.load(std::memory_order_acquire) == 0)
            {
                // wait till there is something to consume
            }

            T value = std::move(storage_[head_].value);
            storage_[head_].state.store(0, std::memory_order_release);
            head_ = head_ + 1 & storage_.mask();
            return value;
        }

        template < bool Blocking, size_t N > size_t pop(std::array< T, N >& values)
        {
            while (storage_[head_].state.load(std::memory_order_acquire) == 0)
            {
                if(!Blocking)
                    return 0;
            }

            size_t i = 0;
            while (i < N)
            {
                auto& data = storage_[head_ + i & storage_.mask()];
                if (data.state.load(std::memory_order_relaxed) != 0)
                {
                    values[i] = std::move(data.value);
                    data.state.store(0, std::memory_order_relaxed);
                    i++;
                }
                else
                {
                    break;
                }
            }

            head_ = head_ + i & storage_.mask();
            return i;
        }

        bool empty() const
        {
            return head_ == tail_;
        }

        void clear()
        {
            head_ = tail_ = 0;
        }

    private:
        alignas(CacheLineSize) size_t tail_;
        alignas(CacheLineSize) size_t head_;        
        alignas(CacheLineSize) storage_type storage_;
    };

    template < typename T, typename Storage > class bounded_queue_spsc3
    {
        // Correct and Efficient Bounded FIFO Queues
        // https://www.irif.fr/~guatto/papers/sbac13.pdf

    public:
        using value_type = T;
        using storage_type = Storage;

        template< typename... Args > bounded_queue_spsc3(Args&&... args)
            : storage_(std::forward< Args >(args)...)
            , head_(0)
            , head_local_(0)
            , tail_(0)
            , tail_local_(0)
        {}

        template < typename Ty > void push(Ty&& value)
        {
            intptr_t tail = tail_.load(std::memory_order_relaxed);
            if (head_local_ + storage_.size() - tail < 1)
            {
                while (true)
                {
                    head_local_ = head_.load(std::memory_order_acquire);
                    if (head_local_ + storage_.size() - tail >= 1)
                    {
                        break;
                    }
                }
            }

            storage_[tail & storage_.mask()].value = std::forward< Ty >(value);
            tail_.store(tail + 1, std::memory_order_release);
        }

        T pop()
        {
            intptr_t head = head_.load(std::memory_order_relaxed);
            if (tail_local_ - head < 1)
            {
                while (true)
                {
                    tail_local_ = tail_.load(std::memory_order_acquire);
                    if (tail_local_ - head >= 1)
                    {
                        break;
                    }
                }
            }

            T value = std::move(storage_[head & storage_.mask()].value);
            head_.store(head + 1, std::memory_order_release);
            return value;
        }

        template < bool Blocking, size_t N > size_t pop(std::array< T, N >& values)
        {
            intptr_t head = head_.load(std::memory_order_relaxed);
            if (tail_local_ - head < 1)
            {
                while (true)
                {
                    tail_local_ = tail_.load(std::memory_order_acquire);
                    if (tail_local_ - head >= 1)
                    {
                        break;
                    }

                    if(!Blocking)
                        return 0;
                }
            }

            size_t n = std::min(N, size_t(tail_local_ - head));
            for (size_t i = 0; i < n; ++i)
            {
                values[i] = std::move(storage_[head + i & storage_.mask()].value);
            }

            head_.store(head + n, std::memory_order_release);
            return n;
        }

        bool empty() const
        {
            // TODO: could this use producer/consumer local variables?
            return head_local_ == tail_local_;
        }

        void clear()
        {
            head_ = tail_ = 0;
            head_local_ = tail_local_ = 0;
        }

    private:
        alignas(CacheLineSize) std::atomic< intptr_t > tail_;
        intptr_t tail_local_;
        alignas(CacheLineSize) std::atomic< intptr_t > head_;
        intptr_t head_local_;
        alignas(CacheLineSize) storage_type storage_;
    };
}
