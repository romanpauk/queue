//
// This file is part of queue project <https://github.com/romanpauk/queue>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <queue/queue.h>

#include <gtest/gtest.h>

TEST(queue_test, static_basic)
{
    queue::static_storage< int, 1024 > storage;
    queue::bounded_queue_mpsc< int, queue::static_storage< int, 1024 > > q(storage);

    EXPECT_TRUE(q.empty());
    q.push(1);
    EXPECT_FALSE(q.empty());
    auto value = q.pop();
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(q.empty());
}

TEST(queue_test, dynamic_basic)
{
    queue::dynamic_storage< int > storage(1 << 20);
    queue::bounded_queue_mpsc< int, queue::dynamic_storage< int > > q(storage);

    EXPECT_TRUE(q.empty());
    q.push(1);
    EXPECT_FALSE(q.empty());
    auto value = q.pop();
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(q.empty());
}
