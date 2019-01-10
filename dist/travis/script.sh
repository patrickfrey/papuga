#!/bin/sh

set -e

OS=$(uname -s)

PROJECT=papuga

build_project() {
	prj_cmakeflags=$1
	case $OS in
		Linux)
			mkdir build
			cd build
			cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release \
				-DLIB_INSTALL_DIR=lib -DCMAKE_CXX_FLAGS=-g $prj_cmakeflags \
				..
			make VERBOSE=1
			make VERBOSE=1 CTEST_OUTPUT_ON_FAILURE=1 test
			sudo make VERBOSE=1 install
			cd ..
			;;
	
		Darwin)
			mkdir build
			cd build
			cmake \
				-DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release \
				-DCMAKE_CXX_FLAGS=-g -G Xcode $prj_cmakeflags \
				..
			xcodebuild -configuration Release -target ALL_BUILD
			xcodebuild -configuration Release -target RUN_TESTS
			sudo xcodebuild -configuration Release -target install
			cd ..
			;;
			
		*)
			echo "ERROR: unknown operating system '$OS'."
			;;
	esac
}

setup_env() {
	case $OS in
		Linux)
			;;
		
		Darwin)
			# forcing brew versions (of gettext) over Mac versions
			export CFLAGS="-I/usr/local"
			export CXXFLAGS="-I/usr/local"
			export LDFLAGS="-L/usr/local/lib"
			;;
	
		*)
			echo "ERROR: unknown operating system '$OS'."
			;;
	esac
}

# set up environment
setup_env

# build the package itself
echo "BUILD papuga WITH -DWITH_PHP=${PAPUGA_WITH_PHP} -DWITH_PYTHON=${PAPUGA_WITH_PYTHON} -DWITH_LUA=${PAPUGA_WITH_LUA}"
build_project "-DWITH_PYTHON=${PAPUGA_WITH_PYTHON} -DWITH_PHP=${PAPUGA_WITH_PHP} -DWITH_LUA=${PAPUGA_WITH_LUA}"

