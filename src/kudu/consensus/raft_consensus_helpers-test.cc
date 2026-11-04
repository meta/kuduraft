// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "kudu/consensus/raft_consensus.h"

#include <gtest/gtest.h>

#include "kudu/consensus/consensus.pb.h"
#include "kudu/consensus/metadata.pb.h"
#include "kudu/util/status.h"

namespace kudu::consensus {
namespace {

TEST(RaftConsensusHelpersTest, InvalidTermDoesNotCacheThrottleState) {
  ConsensusRequestPB request;
  request.mutable_throttle_state()->set_lag_milliseconds(5000);

  ConsensusResponsePB response;
  response.mutable_status()->mutable_error()->set_code(
      ConsensusErrorPB::INVALID_TERM);
  EXPECT_FALSE(shouldCacheThrottleState(request, response, Status::OK()));

  response.mutable_status()->clear_error();
  EXPECT_TRUE(shouldCacheThrottleState(request, response, Status::OK()));
  EXPECT_FALSE(shouldCacheThrottleState(
      request, response, Status::IllegalState("append failed")));
}

TEST(RaftConsensusHelpersTest, CopiesThrottleStateToProxyRequest) {
  ConsensusRequestPB source;
  source.set_dest_uuid("destination");
  source.set_tablet_id("tablet");
  source.set_caller_uuid("leader");
  source.set_caller_term(7);
  source.set_proxy_hops_remaining(2);
  auto& state = *source.mutable_throttle_state();
  state.set_lag_milliseconds(5000);
  auto& entity = *state.add_entities();
  entity.set_dimension(1);
  entity.set_value("heavy-writer");
  entity.set_throttle_rate(75);

  ConsensusRequestPB destination;
  copyProxyRequestFields(source, destination);

  ASSERT_TRUE(destination.has_throttle_state());
  EXPECT_EQ(
      source.throttle_state().SerializeAsString(),
      destination.throttle_state().SerializeAsString());
  EXPECT_EQ(1, destination.proxy_hops_remaining());
}

} // namespace
} // namespace kudu::consensus
