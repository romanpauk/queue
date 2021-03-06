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
    queue::bounded_queue_mpsc< int, queue::static_storage< queue::entry< int >, 1024 > > q;

    EXPECT_TRUE(q.empty());
    q.push(1);
    EXPECT_FALSE(q.empty());
    int value;
    EXPECT_TRUE(q.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(q.empty());
}

TEST(queue_test, dynamic_basic)
{    
    queue::bounded_queue_mpsc< int, queue::dynamic_storage< queue::entry< int > > > q(1 << 20);

    EXPECT_TRUE(q.empty());
    q.push(1);
    EXPECT_FALSE(q.empty());
    int value;
    EXPECT_TRUE(q.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(q.empty());
}
