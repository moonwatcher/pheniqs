#!/usr/bin/env bash

echo "Building clang 3.9 from ubuntu"

set -x -e

echo <<EOF >>install.sh
wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh
bash Miniconda3-latest-Linux-x86_64.sh -b

apt-get update -y
apt-get install -y gnupg gnupg2 gnupg1 wget
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key |  apt-key add -
apt-add-repository "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-6.0 main"
apt-get install -y clang-3.9 libclang-3.9-dev 

##Begin pheniqs install
export PREFIX=/tmp/pheniqs
export LD_LIBRARY_PATH="${PREFIX}/lib"
make all PREFIX=${PREFIX}
make install PREFIX=${PREFIX}

pheniqs demux --config test/BDGGG/BDGGG_interleave.json --validate --distance
pheniqs demux --config test/BDGGG/BDGGG_interleave.json --compile >> test/BDGGG/BDGGG.log 2>&1
pheniqs demux --config test/BDGGG/BDGGG_interleave.json >> test/BDGGG/BDGGG.log 2>&1
pheniqs demux --config test/BDGGG/BDGGG_annotated.json --validate --distance
pheniqs demux --config test/BDGGG/BDGGG_annotated.json --compile >> test/BDGGG/BDGGG.log 2>&1
pheniqs demux --config test/BDGGG/BDGGG_annotated.json >> test/BDGGG/BDGGG.log 2>&1
EOF

chmod 777 install.sh

docker run \
	-it -v $(pwd):$(pwd) ubuntu \
	$(pwd)/install.sh