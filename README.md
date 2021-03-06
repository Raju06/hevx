HEVx [![Build Status](https://travis-ci.org/usnistgov/hevx.svg?branch=master)](https://travis-ci.org/usnistgov/hevx)
====

High End Visualization (HEV) is a software environment for developing
visualization applications in both desktop and immersive environments.

This development environment is mainly designed for supporting the NIST CAVE
and other immersive environments at NIST for visualization purposes. The code
is being made publically available for the benefit of the visualization
community.

Currently this code is under active development and in a pre-release state.

## Building

### Requirements
- CMake 3.11
- Python 3.6
  - Wheezy Template: `pip install --user wheezy.template`
- GCC >= 7
- X11 XCB development libraries

#### Installing requirements

##### CentOS 7
~~~
yum install -y centos-release-scl
yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
yum install -y devtoolset-7\* rh-python36 git cmake3 xcb-util-wm-devel libxcb-devel libX11-devel
scl enable rh-python36 -- pip3 install --user wheezy.template
~~~

##### Fedora 28 / 29
~~~
dnf install -y git cmake gcc-c++ make xcb-util-wm-devel libxcb-devel libX11-devel
pip3 install --user wheezy.template
~~~

##### Ubuntu 18.10
~~~
apt install -y curl python3-pip cmake git pkg-config libx11-dev libx11-xcb-dev libxcb1-dev libxcb-icccm4-dev
pip3 install --user wheezy.template
~~~

### Dependencies
The following packages are fetched and managed with CMake:
- [01org/tbb](https://github.com/01org/tbb)
- [abseil/apseil-cpp](https://github.com/abseil/abseil-cpp)
- [g-truc/glm](https://github.com/g-truc/glm)
- [gabime/spdlog](https://github.com/gabime/spdlog)
- [google/googletest](https://github.com/google/googletest)
- [google/shaderc](https://github.com/google/shaderc)
- [GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [Microsoft/GSL](https://github.com/Microsoft/GSL)
- [mosra/flextgl](https://github.com/mosra/flextgl)
- [nanomsg/nng](https://github.com/nanomsg/nng)
- [ocornut/imgui](https://github.com/ocornut/imgui)
- [protocolbuffers/protobuf](https://github.com/protocolbuffers/protobuf)
- [nlohmann/json](https://github.com/nlohmann/json)
- [TartanLlama/expected](https://github.com/TartanLlama/expected)
- [sailormoon/flags](https://github.com/sailormoon/flags)

### Instructions

##### CentOS 7
~~~
$ mkdir build && cd build
$ scl enable devtoolset-7 rh-python36 -- cmake3 -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
$ cmake --build .
~~~

##### Fedora 28 / 29 and Ubuntu 18.10
~~~
$ mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
$ cmake --build .
~~~

## Other

### Developers
- Wesley Griffin wesley.griffin@nist.gov

### License
This software was developed by employees of the National Institute of
Standards and Technology (NIST), an agency of the Federal Government and is
being made available as a public service. Pursuant to title 17 United States
Code Section 105, works of NIST employees are not subject to copyright
protection in the United States.  This software may be subject to foreign
copyright.  Permission in the United States and in foreign countries, to the
extent that NIST may hold copyright, to use, copy, modify, create derivative
works, and distribute this software and its documentation without fee is
hereby granted on a non-exclusive basis, provided that this notice and
disclaimer of warranty appears in all copies. 

THE SOFTWARE IS PROVIDED 'AS IS' WITHOUT ANY WARRANTY OF ANY KIND, EITHER
EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, ANY WARRANTY
THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS, ANY IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND FREEDOM FROM
INFRINGEMENT, AND ANY WARRANTY THAT THE DOCUMENTATION WILL CONFORM TO THE
SOFTWARE, OR ANY WARRANTY THAT THE SOFTWARE WILL BE ERROR FREE.  IN NO EVENT
SHALL NIST BE LIABLE FOR ANY DAMAGES, INCLUDING, BUT NOT LIMITED TO, DIRECT,
INDIRECT, SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR
IN ANY WAY CONNECTED WITH THIS SOFTWARE, WHETHER OR NOT BASED UPON WARRANTY,
CONTRACT, TORT, OR OTHERWISE, WHETHER OR NOT INJURY WAS SUSTAINED BY PERSONS
OR PROPERTY OR OTHERWISE, AND WHETHER OR NOT LOSS WAS SUSTAINED FROM, OR AROSE
OUT OF THE RESULTS OF, OR USE OF, THE SOFTWARE OR SERVICES PROVIDED HEREUNDER.

