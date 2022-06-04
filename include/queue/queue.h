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

    template < typename T, size_t Alignment > class cell
    {
    public:
        alignas(Alignment) T value;
    private:
        char padding[Alignment - sizeof(value)];
    };

    template < typename T, size_t Size > class static_storage
    {      
        static_assert((Size & Size - 1) == 0);

    public:
        using value_type = cell< T, CacheLineSize >;

        static_storage(size_t = 0) {}

        constexpr size_t size() const { return Size; }
        value_type& operator [](size_t i) { return data_[i]; }

    private:
        std::array< value_type, Size > data_;
    };

    template < typename T > class dynamic_storage
    {
    public:
        using value_type = cell< T, CacheLineSize >;

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

    template < typename T, typename Storage > class bounded_queue_mpsc
    {
    public:
        using value_type = T;
        using storage_type = Storage;

        bounded_queue_mpsc(storage_type& storage)
            : storage_(storage)
            , consumer_begin_(0)
            , consumer_end_(0)
            , producer_end_(0)
        {}

        template < typename Ty > void push(Ty&& value)
        {
            size_t index = producer_end_.fetch_add(1, std::memory_order_relaxed) & (storage_.size() - 1);

            while (consumer_begin_ <= index && index < consumer_end_)
            {
                // wait till consumers consume storage_[index] and it is free to be written
            }

            storage_[index].value = std::forward< Ty >(value);

            while (consumer_end_ != index)
            {
                // wait till other producers advertise their pushes before advertising this one
            }

            consumer_end_ = (index + 1) & (storage_.size() - 1);

            std::atomic_thread_fence(std::memory_order_release);
        }

        value_type pop()
        {
            while (consumer_begin_ == consumer_end_)
            {
                // wait till there is something to consume
            }

            std::atomic_thread_fence(std::memory_order_acquire);

            T value = std::move(storage_[consumer_begin_].value);
            consumer_begin_ = (consumer_begin_ + 1) & (storage_.size() - 1);
            return value;
        }

        bool empty() const
        {
            return consumer_begin_ == consumer_end_;
        }

        void clear()
        {
            consumer_begin_ = consumer_end_ = producer_end_ = 0;
        }

    private:
        alignas(CacheLineSize) storage_type& storage_;

        // written by producer
        alignas(CacheLineSize) size_t consumer_end_;
        std::atomic< size_t > producer_end_;

        // written by consumer
        alignas(CacheLineSize) size_t consumer_begin_;
    };

    template < typename T, typename Storage > class bounded_queue_spsc
    {
        enum { CacheLineSize = std::hardware_destructive_interference_size, };

    public:
        using value_type = T;
        using storage_type = Storage;

        bounded_queue_spsc(storage_type& storage)
            : storage_(storage)
            , consumer_begin_(0)
            , consumer_end_(0)
            , producer_end_(0)
        {}

        template < typename Ty > void push(Ty&& value)
        {
            size_t index = producer_end_++ & (storage_.size() - 1);

            while (consumer_begin_ <= index && index < consumer_end_)
            { 
                // wait till consumers consume storage_[index] and it is free to be written
            }

            storage_[index].value = std::forward< Ty >(value);

            while (consumer_end_ != index)
            {
                // wait till other producers advertise their pushes before advertising this one
            }

            consumer_end_ = index + 1 & (storage_.size() - 1);
            std::atomic_thread_fence(std::memory_order_release);
        }

        value_type pop()
        {
            while (consumer_begin_ == consumer_end_)
            {
                // wait till there is something to consume
            }

            std::atomic_thread_fence(std::memory_order_acquire);

            T value = std::move(storage_[consumer_begin_].value);
            consumer_begin_ = (consumer_begin_ + 1) & (storage_.size() - 1);            
            return value;
        }

        bool empty() const { return consumer_begin_ == consumer_end_; }

    private:
        alignas(CacheLineSize) storage_type& storage_;
        
        // written by producer
        alignas(CacheLineSize) size_t consumer_end_;
        size_t producer_end_;

        // written by consumer
        alignas(CacheLineSize) size_t consumer_begin_;
    };
}
