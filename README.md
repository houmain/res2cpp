# res2cpp

## Command line arguments

```
Usage: res2cpp [-options]
  -c, --config <file>  sets the path of the config file (required).
  -s, --source <file>  sets the path of the source file.
  -h, --header <file>  sets the path of the header file.
  -n, --native         reduce overhead by optimizing for native endianness.
  -d, --data <type>    use type for data (e.g. uint8_t, std::byte, void)
  -t, --type <type>    use type for resource (e.g. std::span<const uint8_t>).
  -a, --alias <type>   declare an alias for resource type.
  -i, --include <file> add #include to generated header.
```

## Building

A C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.

**Installing dependencies on Debian Linux and derivatives:**

```
sudo apt install build-essential git cmake
```

**Checking out the source:**

```
git clone https://github.com/houmain/res2cpp
```

**Building:**

```
cd res2cpp
cmake -B build
cmake --build build
```

## License

**res2cpp** is released under the GNU GPLv3. It comes with absolutely no warranty. Please see `LICENSE` for license details.
