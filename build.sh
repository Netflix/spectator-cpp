#!/bin/bash

bazel --output_user_root=$HOME/.cache/bazel --batch build --config asan spectator_test spectator
GTEST_COLOR=1 ./bazel-bin/spectator_test
