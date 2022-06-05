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
        {
            if((size & size - 1) != 0)
                std::abort();

            data_.reset(new cell< T, CacheLineSize >[size]);
        }

        size_t size() const { return size_; }
        value_type& operator [](size_t i) { return data_[i]; }

    private:
        std::unique_ptr< value_type[] > data_;
        size_t size_;
    };

    template < typename T, typename Storage > class bounded_queue_mpsc2
    {
    public:
        using value_type = T;
        using storage_type = Storage;

        bounded_queue_mpsc2(storage_type& storage)
            : storage_(storage)
            , storage_mask_(storage.size() - 1)
            , head_(0)
            , tail_(0)
        {}

        template < typename Ty > void push(Ty&& value)
        {
            size_t index = tail_.fetch_add(1, std::memory_order_relaxed) & storage_mask_;

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
            head_ = head_ + 1 & storage_mask_;
            return value;
        }

        template< size_t N > size_t pop(std::array< T, N >& values)
        {
            while (storage_[head_].state.load(std::memory_order_acquire) == 0)
            {
                // wait till there is something to consume
            }

            size_t i = 0;
            while (i < N)
            {
                auto& data = storage_[head_ + i & storage_mask_];
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

            head_ = head_ + i & storage_mask_;
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
        alignas(CacheLineSize) storage_type& storage_;
        const size_t storage_mask_;

        // written by producer
        alignas(CacheLineSize) std::atomic< size_t > tail_;
        
        // written by consumer
        alignas(CacheLineSize) size_t head_;
    };

    template < typename T, typename Storage > class bounded_queue_spsc2
    {
    public:
        using value_type = T;
        using storage_type = Storage;

        bounded_queue_spsc2(storage_type& storage)
            : storage_(storage)
            , storage_mask_(storage.size() - 1)
            , head_(0)
            , tail_(0)
        {}

        template < typename Ty > void push(Ty&& value)
        {
            size_t index = tail_++ & storage_mask_;

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
            head_ = head_ + 1 & storage_mask_;
            return value;
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
        alignas(CacheLineSize) storage_type& storage_;
        const size_t storage_mask_;

        // written by producer
        alignas(CacheLineSize) size_t head_;

        // written by consumer
        alignas(CacheLineSize) size_t tail_;
    };

    template < typename T, typename Storage > class bounded_queue_spsc3
    {
        // Correct and Efficient Bounded FIFO Queues
        // https://www.irif.fr/~guatto/papers/sbac13.pdf

    public:
        using value_type = T;
        using storage_type = Storage;

        bounded_queue_spsc3(storage_type& storage)
            : storage_(storage)
            , storage_mask_(storage.size() - 1)
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

            storage_[tail & storage_mask_].value = std::forward< Ty >(value);
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

            T value = std::move(storage_[head & storage_mask_].value);
            head_.store(head + 1, std::memory_order_release);
            return value;
        }

        template < size_t N > size_t pop(std::array< T, N >& values)
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

            size_t n = std::min(N, size_t(tail_local_ - head));
            for (size_t i = 0; i < n; ++i)
            {
                values[i] = std::move(storage_[head + i & storage_mask_].value);
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
        alignas(CacheLineSize) storage_type& storage_;
        const size_t storage_mask_;

        // written by producer
        alignas(CacheLineSize) std::atomic< intptr_t > tail_;
        intptr_t tail_local_;

        // written by consumer
        alignas(CacheLineSize) std::atomic< intptr_t > head_;
        intptr_t head_local_;
    };
}
