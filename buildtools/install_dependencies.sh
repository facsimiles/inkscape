#!/bin/bash
set -ex
. /etc/os-release || { echo "Error: Cannot determine which distribution you are running."; exit 1; }

######################################
# Debian, Ubuntu
# and derived distributions
######################################

if [[ "$ID" == "debian" || "$ID" == "ubuntu" ]]; then
    sudo apt-get update -yqq
    # Note: the following may fail if source repositories are disabled. It is optional.
    sudo apt-get build-dep inkscape || true
    sudo apt-get install -y -qq \
        cmake \
        intltool \
        pkg-config \
        python-dev \
        libtool \
        ccache \
        doxygen \
        git \
        clang \
        clang-format \
        clang-tidy \
        python-yaml
    # For Debian stretch, the package "clang-tools" is called "clang-tools-4.0".
    sudo apt-get install -y -qq clang-tools || apt-get -y -qq install clang-tools-4.0
    sudo apt-get install -y -qq \
        wget \
        software-properties-common \
        libart-2.0-dev \
        libaspell-dev \
        libblas3 \
        liblapack3 \
        libboost-dev \
        libboost-python-dev \
        libcdr-dev \
        libdouble-conversion-dev \
        libgc-dev \
        libgdl-3-dev \
        libglib2.0-dev \
        libgsl-dev \
        libgtk-3-dev \
        libgtkmm-3.0-dev \
        libgtkspell3-3-dev \
        libhunspell-dev \
        libjemalloc-dev \
        liblcms2-dev \
        libmagick++-dev \
        libpango1.0-dev \
        libpng-dev \
        libpoppler-glib-dev \
        libpoppler-private-dev \
        libpotrace-dev \
        librevenge-dev \
        libsigc++-2.0-dev \
        libsoup2.4-dev \
        libvisio-dev \
        libwpg-dev \
        libxml-parser-perl \
        libxml2-dev \
        libxslt1-dev \
        libyaml-dev \
        python-lxml \
        zlib1g-dev
    # Test tools, optional
    sudo apt-get install -y -qq \
        google-mock \
        imagemagick \
        libgtest-dev \
        fonts-dejavu || echo "Installation of optional test tools failed. Building should still work."

######################################
# Fedora
######################################
elif [[ "$ID" == "fedora" ]]; then
    sudo dnf builddep inkscape
    sudo dnf -y install \
        ccache \
        make \
        libsoup-devel \
        double-conversion-devel \
        gtk3-devel \
        gtkmm30-devel \
        gdlmm-devel \
        gtkspell3-devel \
        gmock \
        gmock-devel \
        gtest-devel \
        ImageMagick \
        libcdr-devel \
        libvisio-devel \
        libyaml-devel \
        jemalloc-devel 
    
######################################
# example for adding a new distribution
######################################
elif [[ "$ID" == "my favourite linux distribution" ]]; then
    # add your commands here
    echo "do something"
    
######################################
# Error handling
######################################
else
    echo "Error: Sorry, we don't have instructions for your distribution yet. Please contribute on https://inkscape.org/contribute/report-bugs/ ."
    exit 1
fi
echo "Done."
