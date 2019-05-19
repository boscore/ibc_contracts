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
