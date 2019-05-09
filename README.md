ibc_contracts
------

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


### Troubleshooting

The IBC system consists of six parts: two relays, two ibc.chain contracts and two ibc.token contracts. 
Each part may fail. Therefore, the possible failures include: 1. the relay node lacks specific data, 2. 
entering the contract logic corner, 3. the system resources of the accounts are insufficient.

Next, these faults are explained in turn, and troubleshooting methods are given.


#### 1. the relay node lacks specific data
Fault description:  
Let's take the transfer from Chain A to Chain B as an example. 
Assuming that a cross-chain transfer transaction occurs in the 10000 blocks, 
Chain B's relay (relay B) requests a bunch of headers from Chain A's relay (relay A), 
for example, requests headers 10000-10085, a total of 86 headers.

According to the BP signature verification logic in EOSIO,
relay A needs to get blockroot_merkle of block 10000,
but on ordinary nodes, only blockroot_merkle of blocks in forkdb can be obtained,
so ibc_plugin uses the block's extension to record blockroot_merkle for some blocks.
If in the process of using IBC, for some reason, relay A failed to obtain blockroot_merkle of block 10000,
then the subsequent cross-chain behavior can not be carried out.

Restoration:  
 - First: Look for a snapshot whose block num of the head is less than 10000, 
for example 9000, and then start the relay node with this snapshot. 
At this time, the relay node can get the blockroot_merkle of the 10000 blocks.
 - Second: reinitialize the IBC system (see below for details).
 
 
#### 2. Enter the Dead Corner of Contract Logic
The v1 version of IBC system can not use multiple relay pairs, or called relay channels, 
and only one relay node is allowed in each chain.
We assume that we need to validate a transaction in the 10000 blocks (ibctx1), 
so we need to have a section in the ibc.chain contract that contains the 10000th header.