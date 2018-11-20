#pragma once

#include <string>
#include <zlib.h>

namespace spectator {

static constexpr int kGzipHeaderSize = 16;  // size of the gzip header

int gzip_compress(char* dest, size_t* destLen, const char* source,
                  size_t sourceLen);
int gzip_uncompress(char* dest, size_t* destLen, const char* source,
                    size_t sourceLen);

}  // namespace spectator
