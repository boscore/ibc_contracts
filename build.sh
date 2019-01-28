#! /bin/bash

print_usage(){
    echo "usage:"
	echo "if your host is installed eosio.cdt, compile with the following command"
	echo "$ build.sh eosio.cdt"
	echo "if your host is installed bos.cdt, compile with the following command"
	echo "$ build.sh bos.cdt"
}

eosio_cdt_version=1.4
bos_cdt_version=2.0.1

if [ "$1" == "bos.cdt" ];then
    sed -i '' 's/set(EOSIO_CDT_VERSION_MIN.*/set(EOSIO_CDT_VERSION_MIN "'${bos_cdt_version}'")/g'           ./CMakeLists.txt
    sed -i '' 's/set(EOSIO_CDT_VERSION_SOFT_MAX.*/set(EOSIO_CDT_VERSION_SOFT_MAX "'${bos_cdt_version}'")/g' ./CMakeLists.txt
elif [ "$1" == "eosio.cdt" ];then
    sed -i '' 's/set(EOSIO_CDT_VERSION_MIN.*/set(EOSIO_CDT_VERSION_MIN "'${eosio_cdt_version}'")/g'           ./CMakeLists.txt
    sed -i '' 's/set(EOSIO_CDT_VERSION_SOFT_MAX.*/set(EOSIO_CDT_VERSION_SOFT_MAX "'${eosio_cdt_version}'")/g' ./CMakeLists.txt
else
    print_usage && exit 0
fi


printf "\t=========== Building eosio.contracts ===========\n\n"

RED='\033[0;31m'
NC='\033[0m'

CORES=`getconf _NPROCESSORS_ONLN`
mkdir -p build
pushd build &> /dev/null
cmake ../
make -j${CORES}
popd &> /dev/null
