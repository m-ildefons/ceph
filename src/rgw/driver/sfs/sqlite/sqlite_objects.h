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
#pragma once

#include "dbconn.h"

namespace rgw::sal::sfs::sqlite {

class SQLiteObjects {
  DBConnRef conn;

 public:
  explicit SQLiteObjects(DBConnRef _conn);
  virtual ~SQLiteObjects() = default;

  SQLiteObjects(const SQLiteObjects&) = delete;
  SQLiteObjects& operator=(const SQLiteObjects&) = delete;

  std::vector<DBObject> get_objects(const std::string& bucket_id) const;

  std::optional<DBObject> get_object(const uuid_d& uuid) const;

  std::optional<DBObject> get_object(
      const std::string& bucket_id, const std::string& object_name
  ) const;

  void store_object(const DBObject& object) const;
  void remove_object(const uuid_d& uuid) const;
};

}  // namespace rgw::sal::sfs::sqlite
