#!/bin/bash

set -e

# https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html
# Release: Your typical release build with no debugging information and full optimization.
# MinSizeRel: A special Release build optimized for size rather than speed.
# RelWithDebInfo: Same as Release, but with debugging information.
# Debug: Usually a classic debug build including debugging information, no optimization etc.
CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-"Debug"}

CEPH_DIR="$(realpath "${CEPH_DIR:-"/srv/ceph"}")"
SFS_CCACHE_DIR="$(realpath "${SFS_CCACHE_DIR:-"${CEPH_DIR}/build.ccache"}")"
SFS_BUILD_DIR="$(realpath "${SFS_BUILD_DIR:-"${CEPH_DIR}/build"}")"

WITH_TESTS=${WITH_TESTS:-"OFF"}
WITH_CCACHE=${WITH_CCACHE:-"ON"}
WITH_RADOSGW_DBSTORE=${WITH_RADOSGW_DBSTORE:-"OFF"}
WITH_SYSTEM_BOOST=${WITH_SYSTEM_BOOST:-"ON"}
WITH_JAEGER=${WITH_JAEGER:-"OFF"}

RUN_TESTS=${RUN_TESTS:-"OFF"}
UNIT_TESTS=()
NPROC=${NPROC:-$(nproc --ignore=2)}

CEPH_CMAKE_ARGS=(
  "-GNinja"
  "-DBOOST_J=${NPROC}"
  "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  "-DCMAKE_CXX_COMPILER=g++-11"
  "-DCMAKE_C_COMPILER=gcc-11"
  "-DENABLE_GIT_VERSION=ON"
  "-DWITH_CCACHE=${WITH_CCACHE}"
  "-DWITH_JAEGER=${WITH_JAEGER}"
  "-DWITH_LTTNG=OFF"
  "-DWITH_MANPAGE=OFF"
  "-DWITH_OPENLDAP=OFF"
  "-DWITH_PYTHON3=3"
  "-DWITH_RADOSGW_AMQP_ENDPOINT=OFF"
  "-DWITH_RADOSGW_DBSTORE=${WITH_RADOSGW_DBSTORE}"
  "-DWITH_RADOSGW_KAFKA_ENDPOINT=OFF"
  "-DWITH_RADOSGW_LUA_PACKAGES=OFF"
  "-DWITH_RADOSGW_MOTR=OFF"
  "-DWITH_RADOSGW_SELECT_PARQUET=OFF"
  "-DWITH_RDMA=OFF"
  "-DWITH_SYSTEM_BOOST=${WITH_SYSTEM_BOOST}"
  "-DWITH_TESTS=${WITH_TESTS}"
  "${CEPH_CMAKE_ARGS[@]}"
)


_configure() {
  cd "${CEPH_DIR}"
  echo "Building radosgw ..."
  echo "CEPH_DIR=${CEPH_DIR}"
  echo "NPROC=${NPROC}"
  echo "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  echo "CCACHE_DIR=${SFS_CCACHE_DIR}"
  for arg in "${CEPH_CMAKE_ARGS[@]}" ; do
    echo "${arg}"
  done

  export CCACHE_DIR="${SFS_CCACHE_DIR}"
  if [ ! -d "${CCACHE_DIR}" ]; then
    echo "ccache dir not found, create."
    mkdir "${CCACHE_DIR}"
    echo "Created by build-radosgw container" > \
      "${CCACHE_DIR}/README"
  fi

  # This is necessary since git v2.35.2 because of CVE-2022-24765
  # but we have to continue in case CEPH_DIR is not a git repo
  # Since git 2.36 the the wildcard '*' is also accepted
  if ! git config --global safe.directory > /dev/null ; then
    git config --global --add safe.directory "*" || true
  fi

  if [ ! -d "${SFS_BUILD_DIR}" ] ; then
    echo "build dir not found, create."
    mkdir "${SFS_BUILD_DIR}"
    echo "Created by build-radosgw container" > \
      "${SFS_BUILD_DIR}/README"
  fi

  cd "${SFS_BUILD_DIR}"

  cmake "${CEPH_CMAKE_ARGS[@]}" ..
}


_build() {
  cd "${SFS_BUILD_DIR}"

  ninja -j "${NPROC}" bin/radosgw

  if [ "${WITH_TESTS}" == "ON" ] ; then
    # discover tests from build.ninja so we don't need to update this after
    # adding a new unit test
    # SFS unittests should be named unittest_rgw_sfs*
    IFS=" " read -r -a \
      UNIT_TESTS <<< "$(grep "build unittest_rgw_sfs" build.ninja \
                          | awk '{print $4}')"
    ninja -j "${NPROC}" "${UNIT_TESTS[@]}"
  fi
}


_strip() {
  echo "Stripping files ..."
  strip --strip-debug --strip-unneeded \
    --remove-section=.comment --remove-section=.note.* \
    "${SFS_BUILD_DIR}"/bin/radosgw \
    "${SFS_BUILD_DIR}"/lib/*.so
}


_run_tests() {
  echo "Running tests..."
  for unit_test in "${UNIT_TESTS[@]}"
  do
    echo "running...${SFS_BUILD_DIR}/bin/${unit_test}"
    "${SFS_BUILD_DIR}/${unit_test}"
  done
}


pushd .

_configure
_build

# Don't strip symbols off of debug builds
[ "${CMAKE_BUILD_TYPE}" == "Debug" ] \
  || [ "${CMAKE_BUILD_TYPE}" == "RelWithDebInfo" ] \
  || _strip

[ "${RUN_TESTS}" == "ON" ] && _run_tests

popd
