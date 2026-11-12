// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "kudu/consensus/consensus.pb.h"
#include "kudu/consensus/ref_counted_replicate.h"
#include "kudu/consensus/replicate_msg_wrapper-test-util.h"
#include "kudu/consensus/replicate_msg_wrapper.h"
#include "kudu/util/compression/compression.pb.h"
#include "kudu/util/compression/compression_codec.h"
#include "kudu/util/faststring.h"
#include "kudu/util/status.h"
#include "kudu/util/test_macros.h"
#include "kudu/util/test_util.h"

namespace kudu::consensus {
namespace {

class ReplicateMsgWrapperTest : public KuduTest {
 protected:
  struct Decoded {
    Status status;
    ReplicateRefPtr msg;
    int64_t directCount;
  };

  void SetUp() override {
    KuduTest::SetUp();
    ASSERT_OK(CompressionCodecManager::setCurrentCodec(LZ4));

    // Every byte value, so NUL truncation or misplaced output changes bytes
    std::string payload;
    for (int i = 0; i < 1024; ++i) {
      payload.push_back(static_cast<char>(i));
    }
    std::unique_ptr<ReplicateMsg> msg(new ReplicateMsg);
    msg->mutable_id()->set_term(1);
    msg->mutable_id()->set_index(2);
    msg->set_timestamp(12345);
    msg->set_op_type(WRITE_OP_EXT);
    msg->mutable_write_payload()->set_payload(std::move(payload));
    original_ = makeScopedRefptrReplicate(std::move(msg), Source::Memory);

    faststring buffer;
    ReplicateMsgWrapper compressor(original_);
    ASSERT_OK(compressor.init(&buffer));
    compressed_ = compressor.getCompressedMsg();
    ASSERT_NE(nullptr, compressed_);
  }

  void TearDown() override {
    CHECK_OK(CompressionCodecManager::setCurrentCodec(NO_COMPRESSION));
    KuduTest::TearDown();
  }

  Decoded decompress(faststring* buffer, bool direct) {
    ReplicateMsgWrapper wrapper(compressed_);
    const int64_t before = raftDirectDecompressionCount();
    Status status = wrapper.init(buffer, direct);
    return {
        std::move(status),
        wrapper.getUncompressedMsg(),
        raftDirectDecompressionCount() - before};
  }

  void expectOriginal(const ReplicateRefPtr& msg) {
    ASSERT_NE(nullptr, msg);
    EXPECT_EQ(
        original_->get()->ShortDebugString(), msg->get()->ShortDebugString());
  }

  ReplicateRefPtr original_;
  ReplicateRefPtr compressed_;
};

TEST_F(ReplicateMsgWrapperTest, BufferedDecompression) {
  faststring buffer;
  const Decoded decoded = decompress(&buffer, /*direct=*/false);
  ASSERT_OK(decoded.status);
  EXPECT_EQ(0, decoded.directCount);
  expectOriginal(decoded.msg);
}

TEST_F(ReplicateMsgWrapperTest, DirectDecompressionSkipsCallerBuffer) {
  faststring buffer;
  buffer.append("sentinel");
  const Decoded decoded = decompress(&buffer, /*direct=*/true);
  ASSERT_OK(decoded.status);
  EXPECT_EQ(1, decoded.directCount);
  EXPECT_EQ("sentinel", buffer.toString());
  expectOriginal(decoded.msg);
}

// A missing caller buffer must not fall back to buffered decompression
TEST_F(ReplicateMsgWrapperTest, DirectDecompressionWithoutBuffer) {
  const Decoded decoded = decompress(nullptr, /*direct=*/true);
  ASSERT_OK(decoded.status);
  EXPECT_EQ(1, decoded.directCount);
  expectOriginal(decoded.msg);
}

TEST_F(ReplicateMsgWrapperTest, FailedDirectDecompressionPublishesNothing) {
  compressed_->get()->mutable_write_payload()->set_payload("not lz4");
  const Decoded decoded = decompress(nullptr, /*direct=*/true);
  EXPECT_FALSE(decoded.status.ok());
  EXPECT_EQ(0, decoded.directCount);
  EXPECT_EQ(nullptr, decoded.msg);
}

TEST_F(ReplicateMsgWrapperTest, DirectDecompressionFlagLeavesCompressionAlone) {
  faststring buffer;
  ReplicateMsgWrapper compressor(original_);
  const int64_t before = raftDirectDecompressionCount();
  ASSERT_OK(compressor.init(&buffer, /*directDecompression=*/true));
  EXPECT_EQ(before, raftDirectDecompressionCount());
  ASSERT_NE(nullptr, compressor.getCompressedMsg());
  EXPECT_EQ(
      buffer.toString(),
      compressor.getCompressedMsg()->get()->write_payload().payload());
}

} // namespace
} // namespace kudu::consensus
