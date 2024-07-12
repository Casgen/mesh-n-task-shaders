#!/bin/bash

dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
echo "Clearing all the Makefiles, intermediate files and binary files... ($dir)"
cd $dir

rm -r obj
rm -r bin

rm Makefile

cd VulkanCore

rm Makefile

cd Vendor

rm GLM.make
rm GLFW.make

cd ../../MeshletCulling

rm Makefile


