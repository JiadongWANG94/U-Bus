#!/bin/bash

script_path=$(dirname $0)

find $script_path/src -name "*.cpp" -exec clang-format -style=file -i {} \;
find $script_path/src -name "*.hpp" -exec clang-format -style=file -i {} \;
find $script_path/include -name "*.hpp" -exec clang-format -style=file -i {} \;
find $script_path/include -name "*.cpp" -exec clang-format -style=file -i {} \;
find $script_path/test -name "*.cpp" -exec clang-format -style=file -i {} \;
