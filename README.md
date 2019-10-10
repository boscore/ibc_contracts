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
There are currently multiple major versions for all these three software repositories and between major versions maybe incompatible, 
so the three repositories need to use the correct major version number to coordinate their work.

compatible combination one:  

| Repo           |    branch(es) |
|----------------|---------------|
| ibc_contracts  |  master       |
| ibc_plugin_eos |  master(for eosio v1.8.x)/ibc_v2.x.x_branch(for eosio 1.7.x and early version) |
| ibc_plugin_bos |  master 2     |


compatible combination two:  

| Repo           |    branch(es) |
|----------------|---------------|
| ibc_contracts  |  v1.x.x       |
| ibc_plugin_eos |  ibc_v1.x.x_branch |
| ibc_plugin_bos |  ibc_v1.x.x_branch |


### IBC test localhost environment
[ibc_test_env](https://github.com/boscore/ibc_test_env) provides a great localhost IBC test cluster environment, 
you can find all the details related to IBC system deployment, contracts initialization, 
testing inter-blockchain token transfers in the bash scripts.

### Relevant documents
 - [User_Guide](./docs/User_Guide.md) 
   Explains how blockchain users use IBC system for inter-blockchain token transfer by command lines.
 - [Token_Registration_and_Management](./docs/Token_Registration_and_Management.md) 
   Explains how to register a `token` in the IBC contracts to circulate on the two blockchains.
 - [Deployment_and_Test](./docs/Deployment_and_Test.md) Explains how to deploy and test the IBC system.
 - [Troubleshooting](./docs/TROUBLESHOOTING.md) Explains how to troubleshooting when IBC system encounters problems.
 - [EOSIO_IBC_Priciple_and_Design](https://github.com/boscore/Documentation/blob/master/IBC/EOSIO_IBC_Priciple_and_Design.md)
 - [EOSIO_IBC_Priciple_and_Design 中文版](https://github.com/boscore/Documentation/blob/master/IBC/EOSIO_IBC_Priciple_and_Design_zh.md)
 
