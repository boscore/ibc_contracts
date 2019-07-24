ibc_contracts
-------------

### Build
The two contracts are developed entirely on eosio.cdt, so you can compile them with eosio.cdt. 
Also, you can compile them with bos.cdt, for bos.cdt only adds some contract interfaces, 
the existing interfaces of eosio.cdt have not been changed. 
However, eosio.cdt and bos.cdt use different version number, so you should use following commands to compile:  

if your host is installed eosio.cdt, compile with the following command  
```
$ ./build.sh eosio.cdt
```

if your host is installed bos.cdt, compile with the following command  
```
$ ./build.sh bos.cdt
```


### IBC related softwares' version description

There are three IBC related softwares, [ibc_contracts](https://github.com/boscore/ibc_contracts),
[ibc_plugin_eos](https://github.com/boscore/ibc_plugin_eos) 
and [ibc_plugin_bos](https://github.com/boscore/ibc_plugin_bos), 
There are currently two major versions for all these three software repositories and between major versions are incompatible, 
so the three repositories need to use the same major version number to coordinate their work.

Each head of the current master branch of the three repositories is belongs the major version 2. 
If you need the old major version 1 of these repositories, 
please checkout the corresponding branch where the version 1 is located. As shown in the table below.

| Repo           | master's head | version 1's branch |
|----------------|---------------|--------------------|
| ibc_contracts  |  version 2    | v1.x.x             |
| ibc_plugin_eos |  version 2    | ibc_v1.x.x_branch  |
| ibc_plugin_bos |  version 2    | ibc_v1.x.x_branch  |


### IBC test localhost environment
[ibc_test_env](https://github.com/boscore/ibc_test_env) provides a great localhost IBC test cluster environment.
