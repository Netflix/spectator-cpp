#include <fstream>
#include <vector>

// Number of positions of base-2 digits to shift when iterating over the long
// space.
void output_array(std::ostream& os, size_t size, char prefix,
                  const std::string& name) {
  os << "const std::array<std::string, 276>"
     << " " << name << " = {{";
  bool first = true;
  char tag[64];
  for (size_t i = 0; i < size; ++i) {
    if (!first) {
      os << ",\n";
    } else {
      first = false;
    }
    sprintf(tag, "\"%c%04zX\"", prefix, i);
    os << tag;
  }
  os << "}};\n";
}

int main(int argc, char* argv[]) {
  std::ofstream of;
  if (argc > 1) {
    of.open(argv[1]);
  } else {
    of.open("/dev/stdout");
  }

  output_array(of, 276, 'T', "kTimerTags");
  output_array(of, 276, 'D', "kDistTags");
}
