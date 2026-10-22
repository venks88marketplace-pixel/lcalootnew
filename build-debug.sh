#!/bin/bash
echo "Building Calibration Tool in debug mode"
QT6_PATH="$1"
BUILD_TYPE="${2:-Debug}"
BUILD_TYPE_LOWER=$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')

if [[ -z "$QT6_PATH" ]]; then
    echo "Usage: $0 <Qt install path> [Debug|Release"
    echo "Example: $0 /opt/Qt/6.8.3/gcc_64"
    exit 1
fi

echo "Qt path is $QT6_PATH"
echo "Build type is $BUILD_TYPE"
echo "Build type lower is $BUILD_TYPE_LOWER"

#shift # remove first argument so $@ still works
shift 2 || true

function install_conan() {
    # If the sudo command exists then include the config value to use sudo
    include_sudo_config="False"
    if command -v sudo &> /dev/null; then
        include_sudo_config="True"
    fi

    echo "include_sudo_config=$include_sudo_config"
    the_command="conan install . \
          -pr:h=$BUILD_TYPE_LOWER \
        --build=missing \
        -c tools.system.package_manager:sudo=$include_sudo_config \
        -c tools.system.package_manager:mode=install"

    echo $the_command

    $the_command
}

PRESET="conan-$BUILD_TYPE_LOWER"
echo "Using CMake preset: $PRESET"

function cmake_build()
{
    local build_dir="build/$BUILD_TYPE"
    cmake -S . \
        -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$PWD/build/generators/conan_toolchain.cmake" \
        -DQt6_DIR="$QT6_PATH/lib/cmake/Qt6" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        "$@" || return 1
    cmake --build "$build_dir" --parallel || return 1
}
install_conan || { echo "Install failed! "; exit 1; }
cmake_build $@ || { echo "build failed!"; exit 1; }
