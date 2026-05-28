# Building

### Requirements

- CMake `>= 3.16`
- C++20 compiler
- Python `>= 3.6` (resource/packaging scripts)
- Threads package

The CMake project can use bundled/internal or system variants for some libraries (e.g., bzip2, cppdap), and links engine deps like ZMusic/ZVulkan/ZWidget/webp/lzma/abseil.

### Windows (Visual Studio) quick build

```powershell
cmake -S C:\path\to\HCDE -B C:\path\to\HCDE\build -G "Visual Studio 17 2022" -A x64
cmake --build C:\path\to\HCDE\build --config Release
```

Typical outputs:

- `C:\path\to\HCDE\build\Release\hcde.exe`
- `C:\path\to\HCDE\build\Release\hcdeserv.exe`
- `C:\path\to\HCDE\build\Release\hcdemaster.exe`

### Linux quick build

```bash
cmake -S /path/to/HCDE -B /path/to/HCDE/build -DCMAKE_BUILD_TYPE=Release
cmake --build /path/to/HCDE/build -j
```
