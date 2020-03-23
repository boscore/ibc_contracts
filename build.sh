#! /bin/bash

print_usage(){
    echo "usage:"
	echo "if your host is installed eosio.cdt, compile with the following command"
	echo "$ build.sh eosio.cdt"
	echo "if your host is installed bos.cdt, compile with the following command"
	echo "$ build.sh bos.cdt"
}

eosio_cdt_version=1.5
bos_cdt_version=3.0.1

if [ $# -gt 2 ];then
    echo "too much arguments" && exit 0
fi

if [ "$1" == "bos.cdt" ];then
    cdt_version=${bos_cdt_version}
elif [ "$1" == "eosio.cdt" ];then
    cdt_version=${eosio_cdt_version}
else
    print_usage && exit 0
fi

ARCH=$( uname )
unset set_tag

replace_in_file(){
    if [ "$ARCH" == "Darwin" ]; then
        sed -i '' "s/$1/$2/g"  ./CMakeLists.txt
    else
        sed -i  "s/$1/$2/g"  ./CMakeLists.txt
    fi
}

sed 's/set(EOSIO_CDT_VERSION_MIN.*/set(EOSIO_CDT_VERSION_MIN '${cdt_version}')/g' ./CMakeLists_gen.txt > CMakeLists.txt
replace_in_file "set(EOSIO_CDT_VERSION_SOFT_MAX.*" 'set(EOSIO_CDT_VERSION_SOFT_MAX '${cdt_version}')'

if [ $# -eq 2 ];then
    if [ "$2" == "HUB_PROTOCOL=ON" ];then
        replace_in_file "HUB_PROTOCOL_SWITCH" "add_definitions(-DHUB)"
    else
        echo "unknown parameter: " $2 && exit 0
    fi
else
    replace_in_file "HUB_PROTOCOL_SWITCH" ""
fi

printf "\t=========== building ibc_contracts ===========\n\n"

RED='\033[0;31m'
NC='\033[0m'

CORES=`getconf _NPROCESSORS_ONLN`
mkdir -p build
pushd build &> /dev/null
cmake ../
make -j${CORES}
popd &> /dev/null
