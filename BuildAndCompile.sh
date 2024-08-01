#!/bin/bash

dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
echo "Starting build and compilation at directory $dir"
cd $dir

./Vendor/premake5  --with-vulkan  gmake2 && bear -- make



