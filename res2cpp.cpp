
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstring>
#include <map>

void print_help_message() {
  std::cout <<
    "res2cpp (c) 2024 by Albert Kalchmair\n"
    "\n"
    "Usage: res2cpp [-options]\n"
    "  -c, --config <file>  sets the path of the config file (required).\n"
    "  -s, --source <file>  sets the path of the source file.\n"
    "  -h, --header <file>  sets the path of the header file.\n"
  //"  -e, --embed          use #embed for embedding files.\n"
    "  -d, --data <type>    use type for data (e.g. uint8_t, std::byte, void)\n"
    "  -t, --type <type>    use type for resource (e.g. std::span<const uint8_t>).\n"
    "  -a, --alias <type>   declare an alias for resource type.\n"
    "  -i, --include <file> add #include to generated header.\n"
    "  -n, --native         optimize for native endianness to improve compile-time.\n"
    "\n"
    "All Rights Reserved.\n"
    "This program comes with absolutely no warranty.\n"
    "See the GNU General Public License, version 3 for details.\n"
    "\n";
}

struct Settings {
  std::filesystem::path config_file;
  std::filesystem::path source_file;
  std::filesystem::path header_file;
  std::optional<bool> little_endian;
  std::string data_type{ "unsigned char" };
  std::string resource_type;
  std::string resource_alias;
  std::vector<std::string> includes;
};

struct Definition {
  std::string id;
  std::string path;
  bool is_header;
};

struct Resource {
  std::string id;
  std::filesystem::path path;

  friend bool operator<(const Resource& a, const Resource& b) {
    return std::tie(a.id, a.path) < std::tie(b.id, b.path);
  }
};

struct State {
  std::filesystem::path base_path;
  std::string id_prefix;
  std::string path_prefix;
  std::vector<Resource> resources;
};

bool is_space(char c) {
  return std::isspace(static_cast<unsigned char>(c));
}

bool is_alnum(char c) {
  return (std::isalnum(static_cast<unsigned char>(c)));
}

bool is_digit(char c) {
  return (std::isdigit(static_cast<unsigned char>(c)));
}

std::filesystem::path utf8_to_path(std::string_view utf8_string) {
#if defined(__cpp_char8_t)
  static_assert(sizeof(char) == sizeof(char8_t));
  return std::filesystem::path(
    reinterpret_cast<const char8_t*>(utf8_string.data()),
    reinterpret_cast<const char8_t*>(utf8_string.data() + utf8_string.size()));
#else
  return std::filesystem::u8path(utf8_string);
#endif
}

std::string path_to_utf8(const std::filesystem::path& path) {
#if defined(__cpp_char8_t)
  static_assert(sizeof(char) == sizeof(char8_t));
#endif
  const auto u8string = path.generic_u8string();
  return std::string(
    reinterpret_cast<const char*>(u8string.data()),
    reinterpret_cast<const char*>(u8string.data() + u8string.size()));
}

[[noreturn]] void error(const std::string& message) {
  throw std::runtime_error(message);
}

std::string replace_all(std::string str,
    std::string_view search, std::string_view replace) {
  auto pos = size_t{ };
  while ((pos = str.find(search, pos)) != std::string::npos) {
    str.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return str;
}

std::string trim(std::string str) {
  while (!str.empty() && is_space(str.front()))
    str.erase(str.begin());
  while (!str.empty() && is_space(str.back()))
    str.pop_back();
  return str;
}

std::string remove_extension(std::string filename) {
  const auto dot = filename.rfind('.');
  if (dot == 0 ||
      dot == std::string_view::npos)
    return filename;
  return filename.substr(0, dot);
}

void add_resource(State& state, std::string&& id, std::string&& path) {
  if (!state.id_prefix.empty())
    id = state.id_prefix + id;

  if (!state.path_prefix.empty())
    path = state.path_prefix + path;

  state.resources.push_back({ std::move(id), std::move(path) });
}

bool interpret_commandline(Settings& settings, int argc, const char* argv[]) {
  for (auto i = 1; i < argc; i++) {
    const auto argument = std::string_view(argv[i]);

    if (argument == "-c" || argument == "--config") {
      if (++i >= argc)
        return false;
      settings.config_file = utf8_to_path(argv[i]);
    }
    else if (argument == "-s" || argument == "--source") {
      if (++i >= argc)
        return false;
      settings.source_file = utf8_to_path(argv[i]);
    }
    else if (argument == "-h" || argument == "--header") {
      if (++i >= argc)
        return false;
      settings.header_file = utf8_to_path(argv[i]);
    }
    else if (argument == "-n" || argument == "--native") {

      static const union {
        int dummy;
        char little;
      } native_endianness = { 1 };

      settings.little_endian.emplace(native_endianness.little == 1);
    }
    else if (argument == "-d" || argument == "--data") {
      if (++i >= argc)
        return false;
      settings.data_type = argv[i];
    }
    else if (argument == "-t" || argument == "--type") {
      if (++i >= argc)
        return false;
      settings.resource_type = argv[i];
    }
    else if (argument == "-a" || argument == "--alias") {
      if (++i >= argc)
        return false;
      settings.resource_alias = argv[i];
    }
    else if (argument == "-i" || argument == "--include") {
      if (++i >= argc)
        return false;
      settings.includes.push_back(argv[i]);
    }
    else {
      return false;
    }
  }

  // config file path is required
  if (settings.config_file.empty())
    return false;

  // other paths can be deduced
  if (settings.source_file.empty()) {
    settings.source_file = settings.config_file;
    settings.source_file.replace_extension("cpp");
  }
  if (settings.header_file.empty()) {
    settings.header_file = settings.source_file;
    settings.header_file.replace_extension("h");
  }
  return true;
}

std::string normalize_path(std::string&& path) {
  return replace_all(std::move(path), "\\", "/");
}

std::string normalize_id(std::string&& id) {
  return replace_all(
    replace_all(std::move(id), "/", "$"), "::", "/");
}

bool is_valid_identifier(std::string_view id) {
  if (id.empty())
    return false;

  auto after_slash = true;
  for (const auto& c : id) {
    if (!is_alnum(c) && c != '_' && c != '/')
      return false;
    if (after_slash && is_digit(c))
      return false;
    after_slash = (c == '/');
  }
  if (id.back() == '/')
    return false;
  return true;
}

std::string deduce_id_from_path(bool is_header, const std::string& path) {
  auto id = trim(is_header ? path : remove_extension(path));

  // replace not allowed characters
  for (auto& c : id)
    if (!is_alnum(c) && c != '/')
      c = '_';

  // insert _ before initial digits
  auto after_slash = true;
  for (auto it = id.begin(); it != id.end(); ++it) {
    if (after_slash && is_digit(*it))
      it = id.insert(it, '_');
    after_slash = (*it == '/');
  }
  return id;
}

std::optional<Definition> parse_definition(const std::string& line) {
  auto it = line.begin();
  auto end = line.end();

  const auto skip_space = [&]() {
    while (it != end && is_space(*it))
      ++it;
  };
  const auto skip = [&](auto c) {
    if (it != end && *it == c) {
      ++it;
      return true;
    }
    return false;
  };
  const auto skip_until = [&](auto... c) {
    auto begin = it;
    for (; it != end; ++it)
      if (((*it == c) || ...))
        return true;
    it = begin;
    return false;
  };
  const auto skip_string = [&]() {
    if (it == end || !(*it == '"' || *it == '\''))
      return false;
    const auto c = *it;
    ++it;
    if (!skip_until(c))
      error("unterminated string");
    ++it;
    return true;
  };
  const auto skip_until_not_in_string = [&](auto... c) {
    auto begin = it;
    for (;; ++it) {
      skip_string();
      if (it == end)
        break;
      if (((*it == c) || ...))
        return true;
    }
    it = begin;
    return false;
  };

  auto definition = Definition{ };

  // check if it is a header and remove comment
  skip_space();
  auto begin = it;
  if (skip('[')) {
    skip_space();
    begin = it;
    if (!skip_until_not_in_string(']', '#') ||
        *it != ']')
      error("missing ']'");
    definition.is_header = true;
    end = it;
  }
  else {
    if (skip_until_not_in_string(']', '#')) {
      if (*it == ']')
        error("invalid definition");
      end = it;
    }
  }
  it = begin;
  if (it == end && !definition.is_header)
    return std::nullopt;

  // content can be a single sequence or two separated by '='
  // the single or the second sequence can be enclosed in quotes
  if (skip_string()) {
    // single string
    definition.path = normalize_path({ begin + 1, it - 1 });
    definition.id = deduce_id_from_path(
      definition.is_header, definition.path);
  }
  else if (skip_until('=')) {
    // first is no string
    definition.id = normalize_id(trim({ begin, it }));
    if (!definition.id.empty() &&
        !is_valid_identifier(definition.id))
      error("invalid identifier");
    ++it;
    skip_space();
    begin = it;
    if (skip_string()) {
      // second is a string
      definition.path = normalize_path({ begin + 1, it - 1 });
    }
    else {
      // second is no string
      definition.path =
        normalize_path(trim({ begin, end }));
      it = end;
    }
  }
  else {
    // single non string
    definition.path = normalize_path(trim({ begin, end }));
    definition.id = deduce_id_from_path(
      definition.is_header, definition.path);
    it = end;
  }

  // check that there is nothing following
  skip_space();
  if (it != end)
    error("invalid definition");

  if (definition.is_header) {
    it = end + 1;
    end = line.end();
    skip_space();
    if (it != end && *it != '#')
      error("invalid definition");
  }
  else if (definition.id.empty()) {
    error("missing id");
  }
  return definition;
}

void apply_definition(State& state, const Definition& definition) {
  if (definition.is_header) {
    state.id_prefix = definition.id;
    state.path_prefix = definition.path;
  }
  else {
    auto id = definition.id;
    if (!state.id_prefix.empty())
      id = state.id_prefix + "/" + id;

    auto path = state.base_path;
    if (!state.path_prefix.empty())
      path /= state.path_prefix;
    path /= definition.path;

    state.resources.push_back({ id, path });
  }
}

std::vector<Resource> read_config(const std::filesystem::path& config_file) {
  auto is = std::ifstream(config_file);
  if (!is.good())
    error("opening configuration '" + path_to_utf8(config_file) + "' failed");

  auto line_no = 0;
  try {
    auto state = State();
    state.base_path = config_file.parent_path();

    auto line = std::string{ };
    while (is.good()) {
      std::getline(is, line);
      ++line_no;
      if (auto definition = parse_definition(line))
        apply_definition(state, *definition);
    }
    return std::move(state.resources);
  }
  catch (const std::exception& ex) {
    throw std::runtime_error(ex.what() +
      (" in line " + std::to_string(line_no)));
  }
}

std::string read_textfile(const std::filesystem::path& filename) {
  auto file = std::ifstream(filename, std::ios::in | std::ios::binary);
  if (!file.good())
    error("reading file '" + path_to_utf8(filename) + "' failed");
  return std::string(std::istreambuf_iterator<char>{ file }, { });
}

std::ofstream open_file_for_writing(const std::filesystem::path& filename) {
  auto error_code = std::error_code{ };
  std::filesystem::create_directories(filename.parent_path(), error_code);
  auto file = std::ofstream(filename, std::ios::out | std::ios::binary);
  if (!file.good())
    error("writing file '" + path_to_utf8(filename) + "' failed");
  return file;
}

void write_textfile(const std::filesystem::path& filename,
    std::string_view text) {
  auto file = open_file_for_writing(filename);
  file.write(text.data(), static_cast<std::streamsize>(text.size()));
}

bool update_textfile(const std::filesystem::path& filename,
    std::string_view text) {
  auto error_code = std::error_code{ };
  if (std::filesystem::exists(filename, error_code))
    if (const auto current = read_textfile(filename); current == text)
      return false;
  write_textfile(filename, text);
  return true;
}

size_t hexdump_file(std::ostream& os,
    const std::filesystem::path& filename,
    int word_size, bool little_endian) {
  const auto max_per_line = 100 / (2 * word_size + 3);
  auto file = std::ifstream(filename, std::ios::in | std::ios::binary);
  if (!file.good())
    error("reading file '" + path_to_utf8(filename) + "' failed");
  auto total_size = size_t{ };
  char input[8];
  for (auto i = 0; ; ++i) {
    file.read(input, word_size);
    const auto read = file.gcount();
    if (read != word_size)
      std::memset(input + read, 0x00, word_size - read);
    if (read) {
      if (i > 0) {
        os.put(',');
        if (i % max_per_line == 0)
          os.put('\n');
      }
      os.write("0x", 2);
      for (auto j = 0; j < word_size; ++j) {
        const auto hex = "0123456789ABCDEF";
        const auto b = static_cast<unsigned char>(
          input[little_endian ? word_size - j - 1 : j]);
        os.put(hex[b / 16]);
        os.put(hex[b % 16]);
      }
      total_size += read;
    }
    if (read != word_size)
      break;
  }
  return total_size;
}

template<typename F>
void for_each_identifier(const std::string& string, F&& function) {
  const auto end = string.data() + string.size();
  for (auto it = string.data(), begin = it; ; ++it)
    if (it == end || *it == '/') {
      const auto ident = std::string_view(begin, std::distance(begin, it));
      const auto last = (it == end);
      function(ident, last);
      if (last)
        break;
      begin = it + 1;
    }
}

void generate_output(std::ostream& os, const Settings& settings,
    const std::vector<Resource>& resources, bool is_header) {

  auto qualified_resource_type =
    (!settings.resource_type.empty() ? settings.resource_type :
    "std::pair<const " + settings.data_type + "*, size_t>");
  auto current_namespace = std::vector<std::string_view>();
  auto resource_type_parts = std::vector<std::string_view>();
  auto resource_type = std::string_view();
  auto resource_by_path = std::map<std::filesystem::path, std::string_view>();

  // depending on current namespace make resource_type point
  // to fully qualified type name or last part only
  const auto qualify_resource_type = [&]() {
    resource_type = qualified_resource_type;
    if (resource_type_parts.size() > 1 &&
        current_namespace.size() >= resource_type_parts.size() - 1 &&
        std::equal(resource_type_parts.begin(),
                   std::prev(resource_type_parts.end()),
                   current_namespace.begin()))
      resource_type = resource_type_parts.back();
  };
  const auto write_indent = [&]() {
    for (auto i = size_t{ }; i < 2 * current_namespace.size(); ++i)
      os.put(' ');
  };
  const auto open_namespace = [&](std::string_view name) {
    write_indent();
    os << "namespace " << name << " {\n";
    current_namespace.push_back(name);
    qualify_resource_type();
  };
  const auto close_namespace = [&](std::string_view name) {
    current_namespace.pop_back();
    write_indent();
    os << "} // namespace " << name << "\n";
  };
  const auto close_namespaces = [&](size_t level) {
    if (current_namespace.size() <= level)
      return false;
    while (current_namespace.size() > level)
      close_namespace(current_namespace.back());
    return true;
  };
  const auto write_header = [&](std::string_view name) {
    write_indent();
    os << "extern const " << resource_type << " " << name << ";\n";
  };
  const auto write_output = [&](std::string_view name,
      const std::filesystem::path& path) {
    write_indent();
    os << "const "
      << (settings.little_endian.has_value() ? "uint64_t " : "uint8_t ")
      << name << "_data_[] {\n";
    const auto data_size = hexdump_file(os, path,
      (settings.little_endian.has_value() ? 8 : 1),
      settings.little_endian.value_or(true));
    os << "\n";
    write_indent();
    os << "};\n";
    write_indent();
    os << "const " << resource_type << " " << name
      << "{ reinterpret_cast<const " + settings.data_type + "*>("
      << name << "_data_), " << data_size << " };\n";
  };
  const auto write_duplicate = [&](std::string_view name, std::string_view first) {
    write_indent();
    os << "const " << resource_type << " " << name << " = " << first << ";\n";
  };

  if (is_header)
    os << "#pragma once\n";
  os << "\n";
  os << "// automatically generated by res2cpp";
  // ensure that all settings affect header, which invalidates output
  if (settings.little_endian.has_value())
    os << (settings.little_endian.value() ? " [LE]" : " [BE]");
  os << "\n";
  os << "// https://github.com/houmain/res2cpp\n\n";

  if (!is_header) {
    os << "#include \"" << path_to_utf8(settings.header_file) << "\"\n";
    os << "#include <cstdint>\n";
  }
  else if (settings.includes.size() == 1 &&
           settings.includes.front().front() != '<') {
    // a single local include
    os << "#include \"" << settings.includes.front() << "\"\n";
  }
  else {
    os << "#include <cstddef>\n";
    os << "#include <utility>\n";
    for (const auto& include : settings.includes)
      os << "#include " << include << "\n";
  }
  os << "\n";

  // declare type alias
  const auto resource_alias_id =
    replace_all(settings.resource_alias, "::", "/");
  if (!settings.resource_alias.empty()) {
    if (is_header)
      for_each_identifier(resource_alias_id,
        [&](std::string_view ident, bool last) {
          if (last) {
            write_indent();
            os << "using " << ident << " = " << resource_type << ";\n\n";
          }
          else {
            open_namespace(ident);
          }
        });

    qualified_resource_type = settings.resource_alias;
    resource_type = qualified_resource_type;
  }

  const auto resource_type_ident =
    replace_all(qualified_resource_type, "::", "/");
  for_each_identifier(resource_type_ident,
    [&](std::string_view ident, bool last) {
      resource_type_parts.push_back(ident);
    });

  for (const auto& [id, path] : resources) {
    auto level = size_t{ };
    for_each_identifier(id, [&](std::string_view ident, bool last) {
      if (last) {
        close_namespaces(level);
        if (is_header) {
          write_header(ident);
        }
        else if (const auto it = resource_by_path.find(path); 
                 it != resource_by_path.end()) {
          write_duplicate(ident, replace_all(std::string(it->second), "/", "::"));
        }
        else {
          write_output(ident, path);
          resource_by_path[path] = id;
        }
      }
      else if (level >= current_namespace.size() ||
               current_namespace[level] != ident) {
        if (close_namespaces(level))
          os << "\n";
        open_namespace(ident);
      }
      ++level;
    });
  }
  close_namespaces(0);
}

std::optional<std::filesystem::file_time_type> get_last_write_time(
    const std::filesystem::path& filename) {
  auto error_code = std::error_code{ };
  const auto time = std::filesystem::last_write_time(filename, error_code);
  return (error_code ? std::nullopt : std::make_optional(time));
}

bool input_files_modified(const Settings& settings,
    const std::vector<Resource>& resources) {
  const auto config_time = get_last_write_time(settings.config_file);
  const auto header_time = get_last_write_time(settings.header_file);
  const auto source_time = get_last_write_time(settings.source_file);
  if (!header_time || config_time > header_time ||
      !source_time || config_time > source_time ||
      header_time > source_time)
    return true;

  for (const auto& [id, path] : resources) {
    const auto resource_time = get_last_write_time(path);
    if (!resource_time || resource_time > source_time)
      return true;
  }
  return false;
}

int main(int argc, const char* argv[]) try {
  auto settings = Settings();
  if (!interpret_commandline(settings, argc, argv)) {
    print_help_message();
    return 1;
  }

  auto resources = read_config(settings.config_file);
  std::sort(begin(resources), end(resources));
  auto it = std::adjacent_find(begin(resources), end(resources),
    [](const Resource& a, const Resource& b) { return a.id == b.id; });
  if (it != end(resources))
    error("duplicate id '" + it->id + "'");

  // update header
  auto ss = std::ostringstream();
  generate_output(ss, settings, resources, true);
  update_textfile(settings.header_file, ss.str());

  // write source
  if (input_files_modified(settings, resources)) {
    auto os = open_file_for_writing(settings.source_file);
    generate_output(os, settings, resources, false);
  }
  return EXIT_SUCCESS;
}
catch (const std::exception& ex) {
  std::cerr << "ERROR: " << ex.what() << std::endl;
  return EXIT_FAILURE;
}
