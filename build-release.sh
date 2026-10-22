#!/bin/bash
echo "Building Calibration Tool in release mode"
QT6_PATH="$1"
if [[ -z "$QT6_PATH" ]]; then
    echo "Usage: $0 <Qt install path>"
    echo "Example: $0 /opt/Qt/6.8.3/gcc_64"
    exit 1
fi

echo "Qt path is $QT6_PATH"

shift # remove first argument so $@ still works

function install_conan() {
    # If the sudo command exists then include the config value to use sudo
    include_sudo_config="False"
    if command -v sudo &> /dev/null; then
        include_sudo_config="True"
    fi

    echo "include_sudo_config=$include_sudo_config"
    the_command="conan install . \
        --build=missing \
        -c tools.system.package_manager:sudo=$include_sudo_config \
        -c tools.system.package_manager:mode=install"

    echo $the_command

    $the_command
}

function cmake_build() {
    cmake $@ -S . --preset conan-release -DQt6_DIR="$QT6_PATH/lib/cmake/Qt6" -DCMAKE_BUILD_TYPE=Release
    cmake --build -j --preset conan-release
}

install_conan || { echo "Install failed! "; exit 1; }
cmake_build $@ || { echo "build failed!"; exit 1; }
