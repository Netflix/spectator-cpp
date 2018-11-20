#!/bin/sh

set -ex

curl https://curl.haxx.se/download/curl-7.53.1.tar.gz | tar zx
cd curl-7.53.1
./configure  --enable-optimize --disable-dict --disable-gopher --disable-ftp --disable-imap --disable-ldap --disable-ldaps --disable-pop3 --disable-proxy --disable-rtsp --disable-smtp --disable-telnet --disable-tftp --disable-zlib --without-ca-bundle --without-gnutls --without-libidn --without-librtmp --without-libssh2 --without-nss --without-ssl --without-zlib --disable-shared  --enable-threaded-resolver
make -j4
