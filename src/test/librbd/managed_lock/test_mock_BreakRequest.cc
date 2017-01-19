// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "test/librbd/test_mock_fixture.h"
#include "test/librbd/test_support.h"
#include "test/librbd/mock/MockImageCtx.h"
#include "test/librados_test_stub/MockTestMemIoCtxImpl.h"
#include "test/librados_test_stub/MockTestMemRadosClient.h"
#include "cls/lock/cls_lock_ops.h"
#include "librbd/ManagedLock.h"
#include "librbd/managed_lock/BreakRequest.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <arpa/inet.h>
#include <list>

namespace librbd {
namespace {

struct MockTestImageCtx : public librbd::MockImageCtx {
  MockTestImageCtx(librbd::ImageCtx &image_ctx)
    : librbd::MockImageCtx(image_ctx) {
  }
};

} // anonymous namespace
} // namespace librbd

// template definitions
#include "librbd/managed_lock/BreakRequest.cc"

namespace librbd {
namespace managed_lock {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::WithArg;

class TestMockManagedLockBreakRequest : public TestMockFixture {
public:
  typedef BreakRequest<MockTestImageCtx> MockBreakRequest;

  void expect_list_watchers(MockTestImageCtx &mock_image_ctx, int r,
                            const std::string &address, uint64_t watch_handle) {
    auto &expect = EXPECT_CALL(get_mock_io_ctx(mock_image_ctx.md_ctx),
                               list_watchers(mock_image_ctx.header_oid, _));
    if (r < 0) {
      expect.WillOnce(Return(r));
    } else {
      obj_watch_t watcher;
      strcpy(watcher.addr, (address + ":0/0").c_str());
      watcher.cookie = watch_handle;

      std::list<obj_watch_t> watchers;
      watchers.push_back(watcher);

      expect.WillOnce(DoAll(SetArgPointee<1>(watchers), Return(0)));
    }
  }

  void expect_blacklist_add(MockTestImageCtx &mock_image_ctx, int r) {
    EXPECT_CALL(get_mock_rados_client(), blacklist_add(_, _))
                  .WillOnce(Return(r));
  }

  void expect_break_lock(MockTestImageCtx &mock_image_ctx, int r) {
    EXPECT_CALL(get_mock_io_ctx(mock_image_ctx.md_ctx),
                exec(mock_image_ctx.header_oid, _, StrEq("lock"), StrEq("break_lock"), _, _, _))
                  .WillOnce(Return(r));
  }
};

TEST_F(TestMockManagedLockBreakRequest, DeadLockOwner) {
  REQUIRE_FEATURE(RBD_FEATURE_EXCLUSIVE_LOCK);

  librbd::ImageCtx *ictx;
  ASSERT_EQ(0, open_image(m_image_name, &ictx));

  MockTestImageCtx mock_image_ctx(*ictx);
  expect_op_work_queue(mock_image_ctx);

  InSequence seq;
  expect_list_watchers(mock_image_ctx, 0, "dead client", 123);
  expect_blacklist_add(mock_image_ctx, 0);
  expect_break_lock(mock_image_ctx, 0);

  C_SaferCond ctx;
  Locker locker{entity_name_t::CLIENT(1), "auto 123", "1.2.3.4:0/0", 123};
  MockBreakRequest *req = MockBreakRequest::create(
      mock_image_ctx.md_ctx, ictx->op_work_queue, mock_image_ctx.header_oid,
      locker, true, 0, false, &ctx);
  req->send();
  ASSERT_EQ(0, ctx.wait());
}

TEST_F(TestMockManagedLockBreakRequest, ForceBreak) {
  REQUIRE_FEATURE(RBD_FEATURE_EXCLUSIVE_LOCK);

  librbd::ImageCtx *ictx;
  ASSERT_EQ(0, open_image(m_image_name, &ictx));

  MockTestImageCtx mock_image_ctx(*ictx);
  expect_op_work_queue(mock_image_ctx);

  InSequence seq;
  expect_list_watchers(mock_image_ctx, 0, "1.2.3.4", 123);
  expect_blacklist_add(mock_image_ctx, 0);
  expect_break_lock(mock_image_ctx, 0);

  C_SaferCond ctx;
  Locker locker{entity_name_t::CLIENT(1), "auto 123", "1.2.3.4:0/0", 123};
  MockBreakRequest *req = MockBreakRequest::create(
      mock_image_ctx.md_ctx, ictx->op_work_queue, mock_image_ctx.header_oid,
      locker, true, 0, true, &ctx);
  req->send();
  ASSERT_EQ(0, ctx.wait());
}

TEST_F(TestMockManagedLockBreakRequest, GetWatchersError) {
  REQUIRE_FEATURE(RBD_FEATURE_EXCLUSIVE_LOCK);

  librbd::ImageCtx *ictx;
  ASSERT_EQ(0, open_image(m_image_name, &ictx));

  MockTestImageCtx mock_image_ctx(*ictx);
  expect_op_work_queue(mock_image_ctx);

  InSequence seq;
  expect_list_watchers(mock_image_ctx, -EINVAL, "dead client", 123);

  C_SaferCond ctx;
  Locker locker{entity_name_t::CLIENT(1), "auto 123", "1.2.3.4:0/0", 123};
  MockBreakRequest *req = MockBreakRequest::create(
      mock_image_ctx.md_ctx, ictx->op_work_queue, mock_image_ctx.header_oid,
      locker, true, 0, false, &ctx);
  req->send();
  ASSERT_EQ(-EINVAL, ctx.wait());
}

TEST_F(TestMockManagedLockBreakRequest, GetWatchersAlive) {
  REQUIRE_FEATURE(RBD_FEATURE_EXCLUSIVE_LOCK);

  librbd::ImageCtx *ictx;
  ASSERT_EQ(0, open_image(m_image_name, &ictx));

  MockTestImageCtx mock_image_ctx(*ictx);
  expect_op_work_queue(mock_image_ctx);

  InSequence seq;
  expect_list_watchers(mock_image_ctx, 0, "1.2.3.4", 123);

  C_SaferCond ctx;
  Locker locker{entity_name_t::CLIENT(1), "auto 123", "1.2.3.4:0/0", 123};
  MockBreakRequest *req = MockBreakRequest::create(
      mock_image_ctx.md_ctx, ictx->op_work_queue, mock_image_ctx.header_oid,
      locker, true, 0, false, &ctx);
  req->send();
  ASSERT_EQ(-EAGAIN, ctx.wait());
}

TEST_F(TestMockManagedLockBreakRequest, BlacklistDisabled) {
  REQUIRE_FEATURE(RBD_FEATURE_EXCLUSIVE_LOCK);

  librbd::ImageCtx *ictx;
  ASSERT_EQ(0, open_image(m_image_name, &ictx));

  MockTestImageCtx mock_image_ctx(*ictx);
  expect_op_work_queue(mock_image_ctx);

  InSequence seq;
  expect_list_watchers(mock_image_ctx, 0, "dead client", 123);
  expect_break_lock(mock_image_ctx, 0);

  C_SaferCond ctx;
  Locker locker{entity_name_t::CLIENT(1), "auto 123", "1.2.3.4:0/0", 123};
  MockBreakRequest *req = MockBreakRequest::create(
      mock_image_ctx.md_ctx, ictx->op_work_queue, mock_image_ctx.header_oid,
      locker, false, 0, false, &ctx);
  req->send();
  ASSERT_EQ(0, ctx.wait());
}

TEST_F(TestMockManagedLockBreakRequest, BlacklistError) {
  REQUIRE_FEATURE(RBD_FEATURE_EXCLUSIVE_LOCK);

  librbd::ImageCtx *ictx;
  ASSERT_EQ(0, open_image(m_image_name, &ictx));

  MockTestImageCtx mock_image_ctx(*ictx);
  expect_op_work_queue(mock_image_ctx);

  InSequence seq;
  expect_list_watchers(mock_image_ctx, 0, "dead client", 123);
  expect_blacklist_add(mock_image_ctx, -EINVAL);

  C_SaferCond ctx;
  Locker locker{entity_name_t::CLIENT(1), "auto 123", "1.2.3.4:0/0", 123};
  MockBreakRequest *req = MockBreakRequest::create(
      mock_image_ctx.md_ctx, ictx->op_work_queue, mock_image_ctx.header_oid,
      locker, true, 0, false, &ctx);
  req->send();
  ASSERT_EQ(-EINVAL, ctx.wait());
}

TEST_F(TestMockManagedLockBreakRequest, BreakLockMissing) {
  REQUIRE_FEATURE(RBD_FEATURE_EXCLUSIVE_LOCK);

  librbd::ImageCtx *ictx;
  ASSERT_EQ(0, open_image(m_image_name, &ictx));

  MockTestImageCtx mock_image_ctx(*ictx);
  expect_op_work_queue(mock_image_ctx);

  InSequence seq;
  expect_list_watchers(mock_image_ctx, 0, "dead client", 123);
  expect_blacklist_add(mock_image_ctx, 0);
  expect_break_lock(mock_image_ctx, -ENOENT);

  C_SaferCond ctx;
  Locker locker{entity_name_t::CLIENT(1), "auto 123", "1.2.3.4:0/0", 123};
  MockBreakRequest *req = MockBreakRequest::create(
      mock_image_ctx.md_ctx, ictx->op_work_queue, mock_image_ctx.header_oid,
      locker, true, 0, false, &ctx);
  req->send();
  ASSERT_EQ(0, ctx.wait());
}

TEST_F(TestMockManagedLockBreakRequest, BreakLockError) {
  REQUIRE_FEATURE(RBD_FEATURE_EXCLUSIVE_LOCK);

  librbd::ImageCtx *ictx;
  ASSERT_EQ(0, open_image(m_image_name, &ictx));

  MockTestImageCtx mock_image_ctx(*ictx);
  expect_op_work_queue(mock_image_ctx);

  InSequence seq;
  expect_list_watchers(mock_image_ctx, 0, "dead client", 123);
  expect_blacklist_add(mock_image_ctx, 0);
  expect_break_lock(mock_image_ctx, -EINVAL);

  C_SaferCond ctx;
  Locker locker{entity_name_t::CLIENT(1), "auto 123", "1.2.3.4:0/0", 123};
  MockBreakRequest *req = MockBreakRequest::create(
      mock_image_ctx.md_ctx, ictx->op_work_queue, mock_image_ctx.header_oid,
      locker, true, 0, false, &ctx);
  req->send();
  ASSERT_EQ(-EINVAL, ctx.wait());
}

} // namespace managed_lock
} // namespace librbd

