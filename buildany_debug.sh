
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

# Ensure at least one argument (Qt path)
if [ -z "$1" ]; then
    echo "Usage: ./build <Qt install path>"
    echo "Linux example: ./build /opt/Qt/6.8.3/gcc_64"
    echo "Windows example: ./build C:/Qt/6.8.3/msvc2022_64"
    exit 1
fi

QT_PATH="$1"
shift

# ---- Linux / macOS ----
if [ "$PLATFORM" = "linux" ] || [ "$PLATFORM" = "mac" ]; then
    echo "Running Linux/macOS build script..."
    #./build_debug.sh "$QT_PATH" "$@"
    exit $?
fi


# ---- Windows ----
if [ "$PLATFORM" = "windows" ]; then
    echo "Running Windows PowerShell build script..."

    # Convert path if needed (Git Bash → Windows)
    if command -v cygpath &> /dev/null; then
        QT_PATH_WIN=$(cygpath -w "$QT_PATH")
    else
        QT_PATH_WIN="$QT_PATH"
    fi

    # Call PowerShell
    #powershell.exe -ExecutionPolicy Bypass -File ./build_debug.ps1 "$QT_PATH_WIN" "$@"
    exit $?
fi

# ---- Unknown platform ----
echo "Unsupported platform: $PLATFORM"
exit 1
