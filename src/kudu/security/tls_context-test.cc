// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "kudu/security/tls_context.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "kudu/security/cert.h"
#include "kudu/security/crypto.h"
#include "kudu/security/security-test-util.h"
#include "kudu/util/status.h"
#include "kudu/util/test_macros.h"
#include "kudu/util/test_util.h"

using std::string;
using ::testing::HasSubstr;
using ::testing::StartsWith;

namespace kudu {
namespace security {

class TlsContextTest : public KuduTest {
 protected:
  void SetUp() override {
    KuduTest::SetUp();
    ASSERT_OK(generateSelfSignedCaForTests(&caKey_, &caCert_));
    ASSERT_OK(tlsContext_.init());
  }

  static void expectCertDescription(const string& certInfo) {
    EXPECT_THAT(certInfo, StartsWith("CERT subject="));
    EXPECT_THAT(certInfo, HasSubstr(", issuer="));
    EXPECT_THAT(certInfo, HasSubstr(", notAfterDays="));
    EXPECT_THAT(certInfo, HasSubstr(", notAfterSeconds="));
    EXPECT_THAT(certInfo, HasSubstr(", notBeforeDays="));
    EXPECT_THAT(certInfo, HasSubstr(", notBeforeSeconds="));
  }

  PrivateKey caKey_;
  Cert caCert_;
  TlsContext tlsContext_;
};

TEST_F(TlsContextTest, DumpServerCertInfoWithoutCert) {
  ASSERT_OK(
      configureTlsContext(PkiConfig::None, caCert_, caKey_, &tlsContext_));
  string certInfo;
  EXPECT_TRUE(tlsContext_.dumpServerCertInfo(certInfo).IsNotFound());
  EXPECT_TRUE(certInfo.empty());
}

TEST_F(TlsContextTest, DumpServerCertInfoSelfSigned) {
  ASSERT_OK(configureTlsContext(
      PkiConfig::SelfSigned, caCert_, caKey_, &tlsContext_));
  string certInfo;
  ASSERT_OK(tlsContext_.dumpServerCertInfo(certInfo));
  expectCertDescription(certInfo);
}

TEST_F(TlsContextTest, DumpServerCertInfoSigned) {
  ASSERT_OK(
      configureTlsContext(PkiConfig::Signed, caCert_, caKey_, &tlsContext_));
  string certInfo;
  ASSERT_OK(tlsContext_.dumpServerCertInfo(certInfo));
  expectCertDescription(certInfo);
  // adoptSignedCert() replaced the self-signed cert: the issuer is the CA.
  EXPECT_THAT(certInfo, HasSubstr(", issuer=" + caCert_.subjectName() + ","));
}

TEST_F(TlsContextTest, DumpServerCertInfoExternallySigned) {
  ASSERT_OK(configureTlsContext(
      PkiConfig::ExternallySigned, caCert_, caKey_, &tlsContext_));
  string certInfo;
  ASSERT_OK(tlsContext_.dumpServerCertInfo(certInfo));
  expectCertDescription(certInfo);
}

} // namespace security
} // namespace kudu
