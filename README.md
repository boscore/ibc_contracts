ibc_contracts
------

### build
These two contracts are developed entirely on eosio.cdt, so you can compile them using eosio.cdt. 
Also, you can compile them with bos.cdt, for bos.cdt only adds some contract interfaces, 
the existing interface of eosio.cdt has not been changed. However, eosio.cdt and bos.cdt use different versions, 
so you should use following command to compile:  

if your host is installed eosio.cdt, compile with the following command  
```bash
$ build.sh eosio.cdt
```

if your host is installed bos.cdt, compile with the following command  
```bash
$ build.sh bos.cdt
```






