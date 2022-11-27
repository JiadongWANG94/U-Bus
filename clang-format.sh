#!/bin/bash

script_path=$(dirname $0)

skip_folder=("./nimportequoi" "./3rdparty")

declare skip_string
for folder in ${skip_folder[@]}
do
  skip_string="${skip_string} -path ${folder} -a -prune -type f -o " 
done


#find ./ -path  ./nimportequoi -a -prune -type f -o -path ./3rdparty -a -prune -type f -o -name *.hpp

find $script_path ${skip_string} -name "*.hpp" -exec clang-format --verbose -style=file -i {} \;
find $script_path ${skip_string} -name "*.cpp" -exec clang-format --verbose -style=file -i {} \;

