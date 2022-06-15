// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab ft=cpp

/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <system_error>
#include <unistd.h>
#include <sstream>

#include "common/errno.h"
#include "common/environment.h"

#include "rgw_sal.h"
#include "rgw_sal_rados.h"
#include "rgw_d3n_datacache.h"

#include "rgw_sal_simplefile.h"

#ifdef WITH_RADOSGW_DBSTORE
#include "rgw_sal_dbstore.h"
#endif

#ifdef WITH_RADOSGW_MOTR
#include "rgw_sal_motr.h"
#endif


#define dout_subsys ceph_subsys_rgw

extern "C" {
extern rgw::sal::Store* newStore(void);
#ifdef WITH_RADOSGW_SIMPLEFILE
extern rgw::sal::Store* newSimpleFileStore(CephContext *cct);
#endif // WITH_RADOS_SIMPLEFILE
#ifdef WITH_RADOSGW_DBSTORE
extern rgw::sal::Store* newDBStore(CephContext *cct);
#endif
#ifdef WITH_RADOSGW_MOTR
extern rgw::sal::Store* newMotrStore(CephContext *cct);
#endif
}

RGWObjState::RGWObjState() {
}

RGWObjState::~RGWObjState() {
}

RGWObjState::RGWObjState(const RGWObjState& rhs) : obj (rhs.obj) {
  is_atomic = rhs.is_atomic;
  has_attrs = rhs.has_attrs;
  exists = rhs.exists;
  size = rhs.size;
  accounted_size = rhs.accounted_size;
  mtime = rhs.mtime;
  epoch = rhs.epoch;
  if (rhs.obj_tag.length()) {
    obj_tag = rhs.obj_tag;
  }
  if (rhs.tail_tag.length()) {
    tail_tag = rhs.tail_tag;
  }
  write_tag = rhs.write_tag;
  fake_tag = rhs.fake_tag;
  shadow_obj = rhs.shadow_obj;
  has_data = rhs.has_data;
  if (rhs.data.length()) {
    data = rhs.data;
  }
  prefetch_data = rhs.prefetch_data;
  keep_tail = rhs.keep_tail;
  is_olh = rhs.is_olh;
  objv_tracker = rhs.objv_tracker;
  pg_ver = rhs.pg_ver;
  compressed = rhs.compressed;
}

rgw::sal::Store* StoreManager::init_storage_provider(const DoutPrefixProvider* dpp, CephContext* cct, const std::string svc, bool use_gc_thread, bool use_lc_thread, bool quota_threads, bool run_sync_thread, bool run_reshard_thread, bool use_cache, bool use_gc)
{
  if (svc.compare("rados") == 0) {
    rgw::sal::Store* store = newStore();
    RGWRados* rados = static_cast<rgw::sal::RadosStore* >(store)->getRados();

    if ((*rados).set_use_cache(use_cache)
                .set_use_datacache(false)
                .set_use_gc(use_gc)
                .set_run_gc_thread(use_gc_thread)
                .set_run_lc_thread(use_lc_thread)
                .set_run_quota_threads(quota_threads)
                .set_run_sync_thread(run_sync_thread)
                .set_run_reshard_thread(run_reshard_thread)
                .init_begin(cct, dpp) < 0) {
      delete store;
      return nullptr;
    }
    if (store->initialize(cct, dpp) < 0) {
      delete store;
      return nullptr;
    }
    if (rados->init_complete(dpp) < 0) {
      delete store;
      return nullptr;
    }
    return store;
  }
  else if (svc.compare("d3n") == 0) {
    rgw::sal::RadosStore *store = new rgw::sal::RadosStore();
    RGWRados* rados = new D3nRGWDataCache<RGWRados>;
    store->setRados(rados);
    rados->set_store(static_cast<rgw::sal::RadosStore* >(store));

    if ((*rados).set_use_cache(use_cache)
                .set_use_datacache(true)
                .set_run_gc_thread(use_gc_thread)
                .set_run_lc_thread(use_lc_thread)
                .set_run_quota_threads(quota_threads)
                .set_run_sync_thread(run_sync_thread)
                .set_run_reshard_thread(run_reshard_thread)
                .init_begin(cct, dpp) < 0) {
      delete store;
      return nullptr;
    }
    if (store->initialize(cct, dpp) < 0) {
      delete store;
      return nullptr;
    }
    if (rados->init_complete(dpp) < 0) {
      delete store;
      return nullptr;
    }
    return store;
  }

#ifdef WITH_RADOSGW_SIMPLEFILE
  if (svc.compare("simplefile") == 0) {
    const auto& data_path =
      g_conf().get_val<std::string>("rgw_simplefile_data_path");
    ldpp_dout(dpp, 0) << "simplefile store init!" << dendl;
    rgw::sal::SimpleFileStore *store =
      new rgw::sal::SimpleFileStore(cct, data_path);
    const char *id = get_env_char(
      "RGW_DEFAULT_USER_ID",
      "testid");
    const char *display_name = get_env_char(
      "RGW_DEFAULT_USER_DISPLAY_NAME",
      "M. Tester");
    const char *email = get_env_char(
      "RGW_DEFAULT_USER_EMAIL",
      "tester@ceph.com");
    const char *access_key = get_env_char(
      "RGW_DEFAULT_USER_ACCESS_KEY",
      "test");
    const char *secret_key = get_env_char(
      "RGW_DEFAULT_USER_SECRET_KEY",
      "test");
    const char *caps = get_env_char(
      "RGW_DEFAULT_USER_CAPS");
    const int system = get_env_int(
      "RGW_DEFAULT_USER_SYSTEM"); // Defaults to 0.
    const char *assumed_role_arn = get_env_char(
      "RGW_DEFAULT_USER_ASSUMED_ROLE_ARN");

    /* XXX: temporary - create testid user */
    rgw_user testid_user("", id, "");
    std::unique_ptr<rgw::sal::User> user = store->get_user(testid_user);
    user->get_info().display_name = display_name;
    user->get_info().user_email = email;
    RGWAccessKey k1(access_key, secret_key);
    user->get_info().access_keys[access_key] = k1;
    user->get_info().max_buckets = RGW_DEFAULT_MAX_BUCKETS;
    user->get_info().system = system;
    user->get_info().admin = 1;   // TODO remove when ACL is implemented
    if (assumed_role_arn != nullptr) {
        user->get_info().assumed_role_arn = assumed_role_arn;
    }
    if (caps != nullptr) {
        RGWUserCaps rgw_caps;
        rgw_caps.add_from_string(caps);
        user->get_info().caps = rgw_caps;
    }

    int r = user->store_user(dpp, null_yield, true);
    if (r < 0) {
      ldpp_dout(dpp, 0) << "ERROR: failed inserting " << id << " user in dbstore error r=" << r << dendl;
    }
    return store;
  }
#endif // WITH_RADOSGW_SIMPLEFILE

#ifdef WITH_RADOSGW_DBSTORE
  if (svc.compare("dbstore") == 0) {
    rgw::sal::Store* store = newDBStore(cct);

    if ((*(rgw::sal::DBStore*)store).set_run_lc_thread(use_lc_thread)
                                    .initialize(cct, dpp) < 0) {
      delete store;
      return nullptr;
    }

    return store;
  }
#endif

#ifdef WITH_RADOSGW_MOTR
  if (svc.compare("motr") == 0) {
    rgw::sal::Store* store = newMotrStore(cct);
    if (store == nullptr) {
      ldpp_dout(dpp, 0) << "newMotrStore() failed!" << dendl;
      return store;
    }
    ((rgw::sal::MotrStore *)store)->init_metadata_cache(dpp, cct);

    /* XXX: temporary - create testid user */
    rgw_user testid_user("tenant", "tester", "ns");
    std::unique_ptr<rgw::sal::User> user = store->get_user(testid_user);
    user->get_info().user_id = testid_user;
    user->get_info().display_name = "Motr Explorer";
    user->get_info().user_email = "tester@seagate.com";
    RGWAccessKey k1("0555b35654ad1656d804", "h7GhxuBLTrlhVUyxSPUKUV8r/2EI4ngqJxD7iBdBYLhwluN30JaT3Q==");
    user->get_info().access_keys["0555b35654ad1656d804"] = k1;

    ldpp_dout(dpp, 20) << "Store testid and user for Motr. User = " << user->get_info().user_id.id << dendl;
    int rc = user->store_user(dpp, null_yield, true);
    if (rc < 0) {
      ldpp_dout(dpp, 0) << "ERROR: failed to store testid user ar Motr: rc=" << rc << dendl;
    }

    // Read user info and compare.
    rgw_user ruser("", "tester", "");
    std::unique_ptr<rgw::sal::User> suser = store->get_user(ruser);
    suser->get_info().user_id = ruser;
    rc = suser->load_user(dpp, null_yield);
    if (rc != 0) {
      ldpp_dout(dpp, 0) << "ERROR: failed to load testid user from Motr: rc=" << rc << dendl;
    } else {
      ldpp_dout(dpp, 20) << "Read and compare user info: " << dendl;
      ldpp_dout(dpp, 20) << "User id = " << suser->get_info().user_id.id << dendl;
      ldpp_dout(dpp, 20) << "User display name = " << suser->get_info().display_name << dendl;
      ldpp_dout(dpp, 20) << "User email = " << suser->get_info().user_email << dendl;
    }

    return store;
  }
#endif

  return nullptr;
}

rgw::sal::Store* StoreManager::init_raw_storage_provider(const DoutPrefixProvider* dpp, CephContext* cct, const std::string svc)
{
  rgw::sal::Store* store = nullptr;
  if (svc.compare("rados") == 0) {
    store = newStore();
    RGWRados* rados = static_cast<rgw::sal::RadosStore* >(store)->getRados();

    rados->set_context(cct);

    int ret = rados->init_svc(true, dpp);
    if (ret < 0) {
      ldout(cct, 0) << "ERROR: failed to init services (ret=" << cpp_strerror(-ret) << ")" << dendl;
      delete store;
      return nullptr;
    }

    if (rados->init_rados() < 0) {
      delete store;
      return nullptr;
    }
    if (store->initialize(cct, dpp) < 0) {
      delete store;
      return nullptr;
    }
  }

  if (svc.compare("simplefile") == 0) {
    store = newSimpleFileStore(cct);
  }

  if (svc.compare("dbstore") == 0) {
#ifdef WITH_RADOSGW_DBSTORE
    store = newDBStore(cct);

    if ((*(rgw::sal::DBStore*)store).initialize(cct, dpp) < 0) {
      delete store;
      return nullptr;
    }

#else
    store = nullptr;
#endif
  }

  if (svc.compare("motr") == 0) {
#ifdef WITH_RADOSGW_MOTR
    store = newMotrStore(cct);
#else
    store = nullptr;
#endif
  }
  return store;
}

void StoreManager::close_storage(rgw::sal::Store* store)
{
  if (!store)
    return;

  store->finalize();

  delete store;
}

namespace rgw::sal {
int Object::range_to_ofs(uint64_t obj_size, int64_t &ofs, int64_t &end)
{
  if (ofs < 0) {
    ofs += obj_size;
    if (ofs < 0)
      ofs = 0;
    end = obj_size - 1;
  } else if (end < 0) {
    end = obj_size - 1;
  }

  if (obj_size > 0) {
    if (ofs >= (off_t)obj_size) {
      return -ERANGE;
    }
    if (end >= (off_t)obj_size) {
      end = obj_size - 1;
    }
  }
  return 0;
}
}
