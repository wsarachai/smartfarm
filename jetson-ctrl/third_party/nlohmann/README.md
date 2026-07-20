# Vendored dependency slot: nlohmann/json

Drop the single-header `json.hpp` here:

```
curl -L -o json.hpp \
  https://github.com/nlohmann/json/releases/latest/download/json.hpp
```

so the include resolves as `#include <nlohmann/json.hpp>` (this directory is on
the include path as `third_party/`).

`CMakeLists.txt` prefers this vendored header; if it's absent it falls back to a
system `find_package(nlohmann_json)` (`apt install nlohmann-json3-dev`). Vendoring
keeps the build self-contained on an offline Jetson.
