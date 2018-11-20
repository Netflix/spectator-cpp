#include "gzip.h"

#include <string>

#ifndef z_const
#define z_const
#endif

namespace spectator {

static constexpr int kWindowBits =
    15 + kGzipHeaderSize;            // 32k window (max size)
static constexpr int kMemLevel = 9;  // max memory for optimal speed

int gzip_compress(char* dest, size_t* destLen, const char* source,
                  size_t sourceLen) {
  // no initialization due to gcc 4.8 bug
  z_stream stream;

  stream.next_in = reinterpret_cast<z_const Bytef*>(const_cast<char*>(source));
  stream.avail_in = static_cast<uInt>(sourceLen);
  stream.next_out = reinterpret_cast<Bytef*>(dest);
  stream.avail_out = static_cast<uInt>(*destLen);
  stream.zalloc = static_cast<alloc_func>(nullptr);
  stream.zfree = static_cast<free_func>(nullptr);
  stream.opaque = static_cast<voidpf>(nullptr);

  auto err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                          kWindowBits, kMemLevel, Z_DEFAULT_STRATEGY);
  if (err != Z_OK) {
    return err;
  }

  err = deflate(&stream, Z_FINISH);
  if (err != Z_STREAM_END) {
    deflateEnd(&stream);
    return err == Z_OK ? Z_BUF_ERROR : err;
  }
  *destLen = stream.total_out;

  return deflateEnd(&stream);
}

class inflate_guard {
  z_stream* stream_;
  int init;

 public:
  explicit inflate_guard(z_stream* stream) : stream_(stream) {
    init = inflateInit2(stream_, kWindowBits);
  }
  ~inflate_guard() {
    if (init == Z_OK) {
      inflateEnd(stream_);
    }
  }
  int init_result() const { return init; }
};

int gzip_uncompress(char* dest, size_t* destLen, const char* source,
                    size_t sourceLen) {
  // no initialization due to gcc 4.8 bug
  z_stream stream;
  stream.next_in = reinterpret_cast<z_const Bytef*>(const_cast<char*>(source));
  stream.avail_in = static_cast<uInt>(sourceLen);
  stream.next_out = reinterpret_cast<Bytef*>(dest);
  stream.avail_out = static_cast<uInt>(*destLen);
  stream.zalloc = static_cast<alloc_func>(nullptr);
  stream.zfree = static_cast<free_func>(nullptr);

  inflate_guard guard(&stream);
  auto err = guard.init_result();
  if (err != Z_OK) {
    return err;
  }

  err = inflate(&stream, Z_FINISH);
  if (err != Z_STREAM_END) {
    if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0)) {
      return Z_DATA_ERROR;
    }
    return err;
  }
  *destLen = stream.total_out;

  return Z_OK;
}

}  // namespace spectator
