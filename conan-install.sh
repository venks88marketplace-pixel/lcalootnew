#!/bin/bash
conan install . --profile:build=./profile --build=missing  -c tools.system.package_manager:mode=install
