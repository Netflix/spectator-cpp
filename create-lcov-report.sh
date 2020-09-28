# Creating report
set -ex
if [ x$TRAVIS_BUILD_DIR != x ]; then
  cd $TRAVIS_BUILD_DIR
fi
echo $PWD
# capture coverage info
lcov --directory . --capture --output-file coverage.info
# filter out system, cmake-build, tests and 3rd-party code from our test coverage
lcov --remove coverage.info '/usr/*' '*/usr/*' '*_test*' --output-file coverage.info
# print report to stdout for debugging
lcov --list coverage.info
