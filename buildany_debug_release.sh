
#!/usr/bin/env bash

set -e

echo "Cross-platform build wrapper"

# Detect OS
unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     PLATFORM=linux;;
    Darwin*)    PLATFORM=mac;;
    CYGWIN*|MINGW*|MSYS*) PLATFORM=windows;;
    *)          PLATFORM="unknown"
esac

echo "Detected platform: $PLATFORM"

# ---- Args ----
QT_PATH="$1"
BUILD_TYPE="${2:-Debug}"   # default Debug

if [ -z "$QT_PATH" ]; then
    echo "Usage: ./build <Qt install path> [Debug|Release]"
    echo "Example: ./build C:/Qt/6.8.3/msvc2022_64 Release"
    exit 1
fi

echo "Qt path: $QT_PATH"
echo "Build type: $BUILD_TYPE"

shift 2 || true

# ---- Linux / macOS ----
if [ "$PLATFORM" = "linux" ] || [ "$PLATFORM" = "mac" ]; then
    ./build.sh "$QT_PATH" "$BUILD_TYPE" "$@"
    exit $?
fi


# ---- Windows ----
if [ "$PLATFORM" = "windows" ]; then
    if command -v cygpath &> /dev/null; then
        QT_PATH_WIN=$(cygpath -w "$QT_PATH")
    else
        QT_PATH_WIN="$QT_PATH"
    fi

    powershell.exe -ExecutionPolicy Bypass -File ./build.ps1 "$QT_PATH_WIN" "$BUILD_TYPE" "$@"
    exit $?
fi

echo "Unsupported platform"
exit 1
