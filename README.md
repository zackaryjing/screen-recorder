# ScreenRecorder

A simple to use, lightweight screenRecorder base on xdg-desktop-portal.

Only compatible with Linux platform.

If you are using clion, set necessary cmake parameter, eg:

```shell
-DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
```

After your build, you could use it like this:

```shell
screenRecorder -r 1920x1080
```

You could find other parameter usage in `parse_cli` function;