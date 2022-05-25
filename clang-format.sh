#!/bin/bash

script_path=$(dirname $0)

find $script_path -name "src/*.cpp" -exec clang-format -style=file -i {} \;
find $script_path -name "src/*.hpp" -exec clang-format -style=file -i {} \;
find $script_path -name "include/*.hpp" -exec clang-format -style=file -i {} \;
find $script_path -name "include/*.cpp" -exec clang-format -style=file -i {} \;
find $script_path -name "test/*.cpp" -exec clang-format -style=file -i {} \;
