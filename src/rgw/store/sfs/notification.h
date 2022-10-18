// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t
// vim: ts=8 sw=2 smarttab ft=cpp
/*
 * Ceph - scalable distributed file system
 * SFS SAL implementation
 *
 * Copyright (C) 2022 SUSE LLC
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 */
#ifndef RGW_STORE_SFS_NOTIFICATION_H
#define RGW_STORE_SFS_NOTIFICATION_H

#include "rgw_sal.h"

namespace rgw::sal {

class SFSNotification : public Notification {

public:

  SFSNotification(
    Object *obj,
    Object *src_obj,
    rgw::notify::EventType type
  ) : Notification(obj, src_obj, type) { }

  ~SFSNotification() = default;

  virtual int publish_reserve(
    const DoutPrefixProvider *dpp,
    RGWObjTags *obj_tags = nullptr
  ) override {
    return 0;
  }

  virtual int publish_commit(
    const DoutPrefixProvider *dpp,
    uint64_t size,
    const ceph::real_time &mtime,
    const std::string &etag,
    const std::string &version
  ) override {
    return 0;
  }

};

} // ns rgw::sal

#endif // RGW_STORE_SFS_NOTIFICATION_H