res2cpp
=======

<a href="#introduction">Introduction</a> |
<a href="#configuration">Configuration</a> |
<a href="#command-line-arguments">Command line arguments</a> |
<a href="#building">Building</a> |
<a href="https://github.com/houmain/res2cpp/releases">Changelog</a>
</p>

`res2cpp` allows to embed the data of resource files in C++ source code. The generation of a header file containing a declaration for each embedded resource has the following advantages over [other](https://github.com/graphitemaster/incbin/) [popular](https://github.com/vector-of-bool/cmrc) [solutions](https://github.com/cyrilcode/embed-resource):

- there are no filenames in binary.
- there are no string lookups at runtime.
- the compiler checks, that all required files are embedded.
- the IDE can provide autocompletion and find all references of a resource.

Building `res2cpp` and integrating it in any build chain should be easy, since it is distributed as a single .cpp file without dependencies. Care has been taken, that the generated header and source files are only updated when the inputs change.

## Introduction

The resources to embed are defined in a [configuration](#configuration) file. In its most simple form, it just contains a list of paths to the files. 

```ini
# comments start with a hash
resources/resource_0.ext
resources/resource_1.ext
```

When running the tool with `res2cpp -c resources.conf`, a header and a source file are generated, with a constant `std::pair` declaration/definition per resource, each containing the file's _data_ and _size_. The generated source can be customized by the [command line arguments](#command-line-arguments).

The generated header file `resources.h` looks like this:

```c++
#pragma once

#include <cstddef>
#include <utility>

namespace resources {
  extern const std::pair<const unsigned char*, size_t> resource_0;
  extern const std::pair<const unsigned char*, size_t> resource_1;
} // namespace resources
```

and the source file `resources.cpp` like this:

```c++
#include "resources.h"
#include <cstdint>

namespace resources {
  const uint8_t ressource_0_data_[] { $DATA };
  const std::pair<const unsigned char*, size_t> resource_0{
    reinterpret_cast<const unsigned char*>(resource_0_data_), $SIZE };

  const uint8_t ressource_1_data_[] { $DATA };
  const std::pair<const unsigned char*, size_t> resource_1{
    reinterpret_cast<const unsigned char*>(resource_1_data_), $SIZE };
} // namespace resources
```

## Configuration

The configuration file specifies which files to embed and by which _id_ they should be accessible. 

There are a few ways to specify _id to filename_ mappings. In the rest of the section the _`comments`_ visualize the resulting mappings.

When only a resource _filename_ is provided, the _id_ is automatically deduced from it, by:
  - removing the extension.
  - replacing not allowed characters with an underscore.
  - collapsing multiple consecutive underscores.
  - adding a namespace per folder.

```ini
# resources::filename = resources/filename.ext
resources/filename.ext
```

The _id_ can also be explicitly set:

```ini
# resources::id = resources/filename.ext
resources::id = resources/filename.ext
```

Group headers, which set a common prefix for the following mappings, can be defined:

```ini
# resources::type::id = resources/type/filename.ext
[resources/type]
id = filename.ext
```

Separate prefixes for _id_ and _filename_ can be provided:
```ini
# resources::type::id = resources/type/filename.ext
[resources::type = resources/type]
id = filename.ext
```

Both prefixes in a header can be left empty:

```ini
# id = resources/type/filename.ext
[ = resources/type]
id = filename.ext

# resources::type::id = filename.ext
[resources::type = ]
id = filename.ext
```

Paths need to be enclosed in quotes, when they contain special characters:

```bash
id = "filename containing #.ext"
```

## Command line arguments

```
Usage: res2cpp [-options]
  -c, --config <file>  sets the path of the config file (required).
  -s, --source <file>  sets the path of the source file.
  -h, --header <file>  sets the path of the header file.
  -d, --data <type>    use type for data (e.g. uint8_t, std::byte, void)
  -t, --type <type>    use type for resource (e.g. std::span<const uint8_t>).
  -a, --alias <type>   declare an alias for resource type.
  -i, --include <file> add #include to generated header.
  -n, --native         optimize for native endianness to improve compile-time.  
```

### --config

The path to the [configuration](#configuration) file is the only required parameter.

### --source and --header

The source and header filenames are deduced from configuration filename unless specified explicitly.

### --data

Replaces the default data type `unsigned char` with another one. With the arguments `res2cpp -c resources.conf --data std::byte` the generated header looks like:

```c++
#pragma once

#include <cstddef>
#include <utility>

namespace resources {
  extern const std::pair<const std::byte*, size_t> resource_0;
  extern const std::pair<const std::byte*, size_t> resource_1;
} // namespace resources
```

### --type and --include

`--type` replaces the default resource type `std::pair` with another one. It can be either a compatible standard type like `std::span` or `std::string_view` or a custom type.

`--include` allows to add an `#include` directive in the generated header, so the type declaration is found.

Running the tool with `res2cpp -c resources.conf --data std::byte --type std::span<const std::byte> --include "<span>"` results in the following header:

```c++
#pragma once

#include <cstddef>
#include <utility>
#include <span>

namespace resources {
  extern const std::span<std::byte> resource_0;
  extern const std::span<std::byte> resource_1;
} // namespace resources
```

A custom type `resources::Resource`, defined in a custom header file `Resource.h`, can be used with `res2cpp -c resources.conf --include Resource.h --type resources::Resource`. The generated header looks like:

```c++
#pragma once

#include "Resource.h"

namespace resources {
  extern const Resource resource_0;
  extern const Resource resource_1;
} // namespace resources
```

The custom `Resource.h` could contain a type definition like:

```c++
#pragma once

#include <cstddef>

namespace resources {
  struct Resource {
    const unsigned char* data;
    size_t size;
  };
} // namespace
```

### --alias

An alternative for defining a custom data type, is defining only a type alias. `res2cpp -c resources.conf --alias resources::Resource` results in the following header:

```c++
#pragma once

#include <cstddef>
#include <utility>

namespace resources {
  using Resource = std::pair<const unsigned char*, size_t>;

  extern const Resource resource_0;
  extern const Resource resource_1;
} // namespace resources
```

### --native 

By default each byte of a resource file is encoded separately in hex notation, e.g. `0xFF,`. So each byte of a file results in 5 bytes in the generated source.
In order to improve the compile time, this option allows to encode 8 bytes per hex number, which in total is much shorter. Since this makes the generated source dependent on the current processor's byte order, it is not enabled by default.

## Building

A C++17 conforming compiler is required. A script for the
[CMake](https://cmake.org) build system is provided.

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
