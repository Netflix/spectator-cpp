#include <fstream>
#include <limits>
#include <vector>

// Number of positions of base-2 digits to shift when iterating over the long
// space.
constexpr int kDigits = 2;

static std::vector<int64_t> bucketValues;
static std::vector<size_t> powerOf4Index;

static void init() {
  powerOf4Index.push_back(0);
  bucketValues.push_back(1);
  bucketValues.push_back(2);
  bucketValues.push_back(3);

  auto exp = kDigits;
  while (exp < 64) {
    auto current = int64_t{1} << exp;
    auto delta = current / 3;
    auto next = (current << kDigits) - delta;

    powerOf4Index.push_back(bucketValues.size());
    while (current < next) {
      bucketValues.push_back(current);
      current += delta;
    }
    exp += kDigits;
  }
  bucketValues.push_back(std::numeric_limits<int64_t>::max());
}

int main(int argc, char* argv[]) {
  init();
  std::ofstream of;
  if (argc > 1) {
    of.open(argv[1]);
  } else {
    of.open("/dev/stdout");
  }
  of << "// Do not modify - auto-generated\n//\n"
     << "const std::array<int64_t, " << bucketValues.size()
     << "> kBucketValues = {{";
  bool first = true;
  for (auto v : bucketValues) {
    if (!first) {
      of << ",\n";
    } else {
      first = false;
    }
    of << "  " << v << "LL";
  }
  of << "}};\n";

  of << "const std::array<size_t, " << powerOf4Index.size()
     << "> kPowerOf4Index = {{\n";
  first = true;
  for (auto v : powerOf4Index) {
    if (!first) {
      of << ",\n";
    } else {
      first = false;
    }
    of << "  " << v << "u";
  }
  of << "\n}};\n";
}
