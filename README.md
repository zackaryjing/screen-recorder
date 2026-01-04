# ScreenRecorder

A simple and lightweight screen recorder based on `xdg-desktop-portal`.  
Designed for Linux platform only.

## Features

- Record your screen in windowed mode.
- Supports fixed input rate recording.
- Easy to use and minimal dependencies.

## Requirements

- Linux system
- CMake >= 3.15
- A C++17 compatible compiler
- `xdg-desktop-portal` installed
- Optional: [vcpkg](https://github.com/microsoft/vcpkg) for dependency management

## Build

This is a minimal CMake project. You can build it with the following steps:

```bash
git clone https://github.com/zackaryjing/screen-recorder 
cd screen-recorder
mkdir build && cd build
cmake ..
cmake --build .
````

If you are using **CLion** and manage dependencies with vcpkg, make sure to set the CMake toolchain file:

```bash
-DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## Usage

After building, you can run the recorder:

```bash
./screenRecorder -r 1920x1080
```

### CLI Options

| Option         | Short | Argument            | Description                                   |
|----------------|-------|---------------------|-----------------------------------------------|
| `--input-fps`  | -i    | Default 1           | Set the input frame rate                      |
| `--output-fps` | -o    | Default 30          | Set the output frame rate                     |
| `--resolution` | -r    | Default screen size | Set the recording resolution (e.g. 1920x1080) |
| `--output`     | -f    | Default             | Set the output file path                      |
| `--help`       | -h    | None                | Show this help message                        |

## License

This project is based on [OBS Studio](https://github.com/obsproject/obs-studio), licensed under GPL-2.0.
This modified version is also under [GPL-2.0](COPYING.txt) License.