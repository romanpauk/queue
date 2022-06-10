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
    queue::bounded_queue_mpsc2< int, queue::static_storage< int, 1024 > > q;

    EXPECT_TRUE(q.empty());
    q.push(1);
    EXPECT_FALSE(q.empty());
    auto value = q.pop();
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(q.empty());
}

TEST(queue_test, dynamic_basic)
{    
    queue::bounded_queue_mpsc2< int, queue::dynamic_storage< int > > q(1 << 20);

    EXPECT_TRUE(q.empty());
    q.push(1);
    EXPECT_FALSE(q.empty());
    auto value = q.pop();
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(q.empty());
}
