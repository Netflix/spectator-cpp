FROM ubuntu:22.04

ENV CC=gcc-11
ENV CXX=g++-11
ENV LANG=en_US.UTF-8

RUN apt-get update && \
    apt-get install -y \
    curl \
    gnupg \
    software-properties-common \
    binutils-dev \
    g++-11 \
    git \
    wget \
    && rm -rf /var/lib/apt/lists/*

RUN wget -O /usr/local/bin/bazel https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-$([ $(uname -m) = "aarch64" ] && echo "arm64" || echo "amd64")
RUN chmod +x /usr/local/bin/bazel

WORKDIR /app
COPY . .

# Build the project
ENV BAZEL_OUTPUT_USER_ROOT=/root/.cache/bazel
RUN bazel --batch build --jobs=2 --config asan spectator_test spectator

# Run the tests
CMD ["bash", "-c", "GTEST_COLOR=1 ./bazel-bin/spectator_test"]
