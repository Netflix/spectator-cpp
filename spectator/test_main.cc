#include "backward.hpp"
#include <gtest/gtest.h>

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  backward::SignalHandling sh;
  return RUN_ALL_TESTS();
}
