#include "../spectator/gzip.h"
#include <gtest/gtest.h>

namespace {
TEST(Gzip, SanityCheck) {
  using spectator::gzip_compress;
  using spectator::gzip_uncompress;

  std::string str_to_compress;
  for (auto i = 0; i < 1024; ++i) {
    str_to_compress += "abcdefABCDEF";
  }

  char buf[64 * 1024];
  auto compressed_size = sizeof buf;
  ASSERT_EQ(gzip_compress(buf, &compressed_size, str_to_compress.c_str(),
                          str_to_compress.length()),
            Z_OK);

  char uncompressed[64 * 1024];
  auto uncompressed_size = sizeof uncompressed;
  ASSERT_EQ(
      gzip_uncompress(uncompressed, &uncompressed_size, buf, compressed_size),
      Z_OK);
  EXPECT_TRUE(memcmp(str_to_compress.c_str(), uncompressed,
                     str_to_compress.length()) == 0);
}

}  // namespace
