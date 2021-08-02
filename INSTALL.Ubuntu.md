Ubuntu 16.04 on x86_64, i686
----------------------------

# Build system
Cmake with gcc or clang. Here in this description we build with
gcc >= 4.9 (has C++11 support).

# Prerequisites
Install packages with 'apt-get'/aptitude.

## CMake flags
	-DWITH_LUA=YES
	to enable build with Lua (>= 5.2) language bindings.
	-DWITH_PHP=YES
	to enable build with Php 7 language bindings.
	-DWITH_PYTHON=YES
	to enable build with Python 3 language bindings.

The prerequisites are listen in 5 sections, a common section (first) and for
each of these flags toggled to YES another section.

## Required packages
	boost-all >= 1.57

## Required packages with -DWITH_LUA=YES
	lua5.2

## Required packages with -DWITH_PYTHON=YES
	python3-dev

## Required packages with -DWITH_PHP=YES
	php7.0-dev zlib1g-dev libxml2-dev

# Fetch sources
	git clone https://github.com/patrickfrey/papuga
	cd papuga
	git submodule update --init --recursive
	git submodule foreach --recursive git checkout master
	git submodule foreach --recursive git pull

# Configure with GNU C/C++ with Lua, Php and Python enabled:
	mkdir -p build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release \
		-DWITH_PYTHON=YES \
		-DWITH_PHP=YES \
		-DWITH_LUA=YES ..
	cd ..

# Configure with Clang C/C++ minimal build with Lua only
	mkdir -p build
	cd build
	cmake -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER="clang" -DCMAKE_CXX_COMPILER="clang++" \
		-DWITH_LUA=YES ..
	cd ..

# Build
	cd build
	make

# Run tests
	make test

# Install
	make install

