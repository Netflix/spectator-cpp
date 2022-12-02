#!/bin/bash

bazel-3.5.0 --output_user_root=$HOME/.cache/bazel --batch build --config asan spectator_test spectator
./bazel-bin/spectator_test

