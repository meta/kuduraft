// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

#include <fb303/ServiceData.h>
#include <fb303/ThreadCachedServiceData.h>

namespace kudu::consensus {

// Includes increments still buffered in thread-local stats
inline int64_t raftDirectDecompressionCount() {
  facebook::fb303::ThreadCachedServiceData::get()->publishStats();
  return facebook::fb303::ServiceData::get()
      ->getCounterIfExists("raft_direct_decompressions.sum")
      .value_or(0);
}

} // namespace kudu::consensus
