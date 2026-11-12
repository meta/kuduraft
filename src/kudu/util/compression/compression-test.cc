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

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "kudu/util/compression/compression.pb.h"
#include "kudu/util/compression/compression_codec.h"
#include "kudu/util/slice.h"
#include "kudu/util/test_macros.h"
#include "kudu/util/test_util.h"

namespace kudu {

using std::vector;

class TestCompression : public KuduTest {};

static void testCompressionCodec(CompressionType compression) {
  const int kInputSize = 64;

  std::shared_ptr<CompressionCodec> codec;
  uint8_t iBuffer[kInputSize];
  uint8_t uBuffer[kInputSize];
  size_t compressed;

  // Fill the test input buffer
  memset(iBuffer, 'Z', kInputSize);

  // Get the specified compression codec
  ASSERT_OK(CompressionCodecManager::getCodec(compression, &codec));

  // Allocate the compression buffer
  size_t maxCompressed = codec->maxCompressedLength(kInputSize);
  ASSERT_LT(maxCompressed, (kInputSize * 2));
  std::unique_ptr<uint8_t[]> cBuffer(new uint8_t[maxCompressed]);

  // Compress and uncompress
  ASSERT_OK(
      codec->compress(Slice(iBuffer, kInputSize), cBuffer.get(), &compressed));
  ASSERT_OK(
      codec->uncompress(Slice(cBuffer.get(), compressed), uBuffer, kInputSize));
  ASSERT_EQ(0, memcmp(iBuffer, uBuffer, kInputSize));

  // Compress slices and uncompress
  vector<Slice> v;
  v.emplace_back(iBuffer, 1);
  for (int i = 1; i <= kInputSize; i += 7)
    v.emplace_back(iBuffer + i, 7);
  ASSERT_OK(
      codec->compress(Slice(iBuffer, kInputSize), cBuffer.get(), &compressed));
  ASSERT_OK(
      codec->uncompress(Slice(cBuffer.get(), compressed), uBuffer, kInputSize));
  ASSERT_EQ(0, memcmp(iBuffer, uBuffer, kInputSize));
}

TEST_F(TestCompression, TestNoCompressionCodec) {
  std::shared_ptr<CompressionCodec> codec;
  ASSERT_OK(CompressionCodecManager::getCodec(NO_COMPRESSION, &codec));
  ASSERT_EQ(nullptr, codec);
}

TEST_F(TestCompression, TestSnappyCompressionCodec) {
  testCompressionCodec(SNAPPY);
}

TEST_F(TestCompression, TestLz4CompressionCodec) {
  testCompressionCodec(LZ4);
}

TEST_F(TestCompression, TestZlibCompressionCodec) {
  testCompressionCodec(ZLIB);
}

TEST_F(TestCompression, TestUncompressRejectsDeclaredSizeMismatch) {
  const std::string input(64, 'Z');
  for (const CompressionType type :
       {SNAPPY, LZ4, LZ4_DICT, ZLIB, ZSTD, ZSTD_DICT}) {
    SCOPED_TRACE(CompressionType_Name(type));
    std::shared_ptr<CompressionCodec> codec;
    ASSERT_OK(CompressionCodecManager::getCodec(type, &codec));
    if (type == LZ4_DICT || type == ZSTD_DICT) {
      ASSERT_OK(codec->setDictionary(std::string(4096, 'd')));
    }
    std::vector<uint8_t> compressed(codec->maxCompressedLength(input.size()));
    size_t compressedLen = 0;
    ASSERT_OK(codec->compress(Slice(input), compressed.data(), &compressedLen));
    const Slice frame(compressed.data(), compressedLen);

    // One spare byte keeps a codec that ignores the declared size in bounds
    std::vector<uint8_t> output(input.size() + 1);
    ASSERT_OK(codec->uncompress(frame, output.data(), input.size()));
    EXPECT_TRUE(codec->uncompress(frame, output.data(), input.size() + 1)
                    .IsCorruption());
    EXPECT_TRUE(codec->uncompress(frame, output.data(), input.size() - 1)
                    .IsCorruption());
    // A rejected call must not break the next one
    ASSERT_OK(codec->uncompress(frame, output.data(), input.size()));
  }
}

} // namespace kudu
