

### How to Make IBC system Compatible with EOSIO 1.8 
The upgrade process is very simple:
1. build [ibc_plugin_eos](https://github.com/boscore/ibc_plugin_eos).
2. use this nodeos to run relay nodes of current EOS mainnet, 
   and use the same configuration file as relay nodes before, except for modifying the corresponding IP and port.
3. stop relay nodes of previous version when EOS mainnet upgraded to v1.8.x.

Note:  
eosio v1.8 did not modify any IBC-related data structure, 
so this upgrade only needs to upgrade the ibc_plugin_eos node, the rest of the IBC system does not need to change.