
#if defined(NDEBUG)
# error "NDEBUG must not be defined"
#endif
#include <cassert>

#define main main_res2cpp
#include "res2cpp.cpp"
#undef main

void test_parse_definition() {
  const auto check_null = [](const char* definition) {
    return !parse_definition(definition).has_value();
  };
  const auto check = [](const char* definition, const char* id,
      const char* path, bool is_header) {
    const auto a = parse_definition(definition);
    if (!a)
      return false;
    const auto b = Definition{ id, path, is_header };
    return (std::tie(a->id, a->path, a->is_header) ==
            std::tie(b.id, b.path, b.is_header));
  };
  const auto check_throws = [](const char* definition) {
    try {
      parse_definition(definition);
      return false;
    }
    catch (...) {
      return true;
    }
  };

  assert(check_null(""));
  assert(check_null(" "));
  assert(check_null(" #"));
  assert(check_null(" # x"));

  assert(check("[]", "", "", true));
  assert(check("[]#", "", "", true));
  assert(check("[] # x", "", "", true));
  assert(check("a", "a", "a", false));
  assert(check(" a ", "a", "a", false));
  assert(check("[a] # x", "a", "a", true));
  assert(check("[ a ] # x", "a", "a", true));
  assert(check(" 'a' ", "a", "a", false));
  assert(check("[ ' a ' ]", "a", " a ", true));
  assert(check("a=# x", "a", "", false));
  assert(check(" a= ", "a", "", false));
  assert(check("[ = 'b#']", "", "b#", true));
  assert(check("[ = ' b]']", "", " b]", true));
  assert(check("a=b", "a", "b", false));
  assert(check(" a = b c # x", "a", "b c", false));
  assert(check(" a = ' b c [' # x", "a", " b c [", false));
  assert(check(" _1 = b", "_1", "b", false));

  assert(check_throws("["));
  assert(check_throws("]"));
  assert(check_throws("[] a"));
  assert(check_throws("[a] a"));
  assert(check_throws("[] a#"));
  assert(check_throws("[a] a#"));
  // only the path can be a string
  assert(check_throws("[ 'a'= ]"));
  assert(check_throws("[ ' a'= ]"));
  assert(check_throws("'a b' = 'c d'"));
  assert(check_throws(" ' a '=  b "));
  assert(check_throws(" 'a' a =  b "));
  assert(check_throws(" a =  ' b ' b "));
  // invalid identifier
  assert(check_throws(" = b "));
  assert(check_throws(" a a = b "));
  assert(check_throws(" a/ = b "));
  assert(check_throws(" /a = b "));
  assert(check_throws(" a$ = b "));
  assert(check_throws(" :a = b "));
  assert(check_throws(" a: = b "));
  assert(check_throws(" a:b = b "));
  assert(check_throws(" a:: = b "));
  assert(check_throws(" ? = b "));
  assert(check_throws(" 1 = b "));

  // normalize path / deducing id from path
  assert(check("a/b.txt", "a/b", "a/b.txt", false));
  assert(check(" a\\b.txt", "a/b", "a/b.txt", false));
  assert(check("a\\b.txt ", "a/b", "a/b.txt", false));
  assert(check("a::b", "a_b", "a::b", false));
  assert(check("::.txt", "_", "::.txt", false));
  assert(check("a::b = c/d", "a/b", "c/d", false));
  assert(check("C:\\a.txt", "C_/a", "C:/a.txt", false));
  assert(check("../a.txt", "_/a", "../a.txt", false));
  assert(check("[a/b.txt]", "a/b_txt", "a/b.txt", true));
  assert(check("[ a\\b.txt]", "a/b_txt", "a/b.txt", true));
  assert(check("[a\\b.txt ]", "a/b_txt", "a/b.txt", true));
  assert(check("1/2.txt", "_1/_2", "1/2.txt", false));
  assert(check("[1/2]", "_1/_2", "1/2", true));
  assert(check("a $b.ext", "a_b", "a $b.ext", false));

  // normalize id
  assert(check("a::b = c", "a/b", "c", false));
}

const auto res1 = std::string("0123456789");
const auto res2 = std::string("abcdefghijklmnopqrstuvwxyz");

#if defined(TEST_GENERATE)

void res2cpp(const std::string& arguments) {
  const auto result = std::system((
#if !defined(_WIN32)
    "./"
#endif
    "res2cpp " + arguments).c_str());
  assert(result == 0);
}

int main() {
  test_parse_definition();

  // config1
  write_textfile("config1/res1.txt", res1);
  write_textfile("config1/res2.txt", res2);
  const auto config1 = R"(
    [config1]
    res1.txt
    res2.txt
  )";
  write_textfile("config1.conf", config1);
  res2cpp("-d char -c config1.conf");

  // config2
  write_textfile("config2/sub/res1.txt", res1);
  write_textfile("config2/res2.txt", res2);
  const auto config2 = R"(
    [config2]
    a::id = sub/res1.txt
    b::id = res2.txt
  )";
  write_textfile("config2.conf", config2);
  res2cpp("-n -d char -t std::string_view "
    "-i \"<string>\" -a config2::Asset -c config2.conf "
    "-s config2/source/file.cpp -h config2/header/file.h");

  // config3
  write_textfile("config3/res1.txt", res1);
  write_textfile("config3/sub/res2.txt", res2);
  const auto config3 = R"(
    [config3::a = config3]
    id1 = res1.txt

    [ = config3/sub]
    config3::a::id2 = res2.txt
    config3::b::id2 = res2.txt # deduplicated
  )";
  const auto asset_h = R"(
    #pragma once
    #include <utility>
    #include <string>

    struct Asset {
      const std::byte* data;
      size_t size;

      std::string to_string() const {
        return std::string(reinterpret_cast<const char*>(data), size);
      }
    };
  )";
  write_textfile("config3.conf", config3);
  write_textfile("Asset.h", asset_h);
  res2cpp("-n -d std::byte -t config3::Asset -i Asset.h -c config3.conf");

  // config4
  write_textfile("config4/res1.txt", res1);
  write_textfile("config4/res2.txt", res2);
  const auto config4 = R"(
    [config4]
    res1.txt
    res2.txt
  )";
  write_textfile("config4.conf", config4);
  res2cpp("-x sEcurepa55 -c config4.conf");
}

#else // !TEST_GENERATE

#include "config1.cpp"
#include "config2/source/file.cpp"
#include "config3.cpp"
#include "config4.cpp"

template<typename T>
std::vector<T> xor_cipher(const T* data, size_t size, std::string_view key) {
  std::vector<T> decoded(size, T{ });
  if (!key.empty())
    for (size_t i = 0; i < size; ++i)
      decoded[i] = data[i] ^ static_cast<T>(key[i % key.size()]);
  return decoded;
}

int main() {
  // config1
  assert(res1 == std::string_view(config1::res1.first, config1::res1.second));
  assert(res2 == std::string_view(config1::res2.first, config1::res2.second));

  // config2
  assert(res1 == config2::a::id);
  assert(res2 == config2::b::id);

  // config3
  assert(res1 == config3::a::id1.to_string());
  assert(res2 == config3::a::id2.to_string());
  assert(config3::a::id2.data == config3::b::id2.data &&
         config3::a::id2.size == config3::b::id2.size);

  // config4
  const auto key = std::string("sEcurepa55");
  const auto res4_1 = xor_cipher(config4::res1.first, config4::res1.second, key);
  const auto res4_2 = xor_cipher(config4::res2.first, config4::res2.second, key);
  assert(res1 == std::string_view(
    reinterpret_cast<const char*>(res4_1.data()), res4_1.size()));
  assert(res2 == std::string_view(
    reinterpret_cast<const char*>(res4_2.data()), res4_2.size()));

  std::cout << "All tests succeeded!" << std::endl;
}

#endif // !TEST_GENERATE
