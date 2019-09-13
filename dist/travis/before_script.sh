#!/bin/sh

set -e

OS=$(uname -s)

case $OS in
	Linux)
		sudo apt-get update -qq
		sudo apt-get install -y \
			cmake \
			libboost-all-dev
		if test "x$PAPUGA_WITH_PYTHON" = "xYES"; then
			sudo apt-get install -y python3-dev
		fi
		if test "x$PAPUGA_WITH_PHP" = "xYES"; then
			sudo apt-get install -y libssl-dev
			sudo apt-get install -y language-pack-en-base
			sudo apt-get install -y php7.0 php7.0-dev
		fi
		;;

	Darwin)
		brew update
		brew upgrade cmake
		brew install boost@1.70
		if test "x$PAPUGA_WITH_PHP" = "xYES"; then
			brew install openssl php71 || true
		fi
		# make sure cmake finds the brew version of gettext
		brew install gettext || true
		brew link --force gettext || true
		;;

	*)
		echo "ERROR: unknown operating system '$OS'."
		;;
esac

