// generate the atlas valid charsets

#include <array>
#include <fstream>

void dump_array(std::ostream& os, const std::string& name, const std::array<bool, 256>& chars) {
  os << "static constexpr std::array<bool, 256> " << name << " = {{";

  os << chars[0];
  for (auto i = 1u; i < chars.size(); ++i) {
    os << ", " << chars[i];
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

  // default false
  std::array<bool, 256> charsAllowed{};
  for (int i = 0; i < 256; ++i) {
    charsAllowed[i] = false;
  }

  // configure allowed characters
  charsAllowed['.'] = true;
  charsAllowed['-'] = true;

  for (auto ch = '0'; ch <= '9'; ++ch) {
    charsAllowed[ch] = true;
  }
  for (auto ch = 'a'; ch <= 'z'; ++ch) {
    charsAllowed[ch] = true;
  }
  for (auto ch = 'A'; ch <= 'Z'; ++ch) {
    charsAllowed[ch] = true;
  }
  charsAllowed['~'] = true;
  charsAllowed['^'] = true;

  dump_array(of, "kAtlasChars", charsAllowed);
}
