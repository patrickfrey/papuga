#!/bin/sh

set -e

OS=$(uname -s)

case $OS in
	Linux)
		sudo apt-get update -qq
		sudo apt-get install -y \
			cmake \
			libboost-all-dev \
			python3-dev
		sudo apt-get install -y language-pack-en-base
		sudo locale-gen en_US.UTF-8
		sudo LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 add-apt-repository -y ppa:ondrej/php
		sudo LC_ALL=en_US.UTF-8 apt-get update
		sudo LC_ALL=en_US.UTF-8 apt-get install -y php7.0 php7.0-dev
		;;

	Darwin)
		brew update
		if test "X$CC" = "Xgcc"; then
			brew install gcc48 --enable-all-languages || true
			brew link --force gcc48 || true
		fi
		brew unlink php56 || true
		brew install cmake gettext boost php7 python3 || true
		# make sure cmake finds the brew version of gettext
		brew link --force gettext || true
		;;

	*)
		echo "ERROR: unknown operating system '$OS'."
		;;
esac

