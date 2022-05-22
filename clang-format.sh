#!/bin/bash

script_path=$(dirname $0)

find $script_path -name "*.cpp" -exec clang-format -style=file -i {} \;
find $script_path -name "*.hpp" -exec clang-format -style=file -i {} \;
