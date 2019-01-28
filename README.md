ibc_contracts
-----

The IBC project consists of two contracts 
[ibc.chain](https://github.com/boscore/bosibc.contracts/tree/master/ibc.chain),
[ibc.token](https://github.com/boscore/bosibc.contracts/tree/master/ibc.token) and one plugin ibc_plugin.

 
### ibc_contracts
These two contracts are developed entirely on eosio.cdt, so you can compile them using eosio.cdt or bos.cdt,
for bos.cdt only adds some contract interfaces, the existing interface of eosio.cdt has not been changed.
eosio.cdt and bos.cdt use different versions, so you should use following command to compile:  
if your host is installed eosio.cdt, compile with the following command  
`$ build.sh eosio.cdt`  
if your host is installed bos.cdt, compile with the following command  
`$ build.sh bos.cdt`  

After compiling with eosio.cdt or bos.cdt, you can deploy them on two chains.

### ibc_plugin
Because ibc_plugin is required for each chain and run as a relay node, and because the underlying source code of BOS 
and EOS is slightly different, a separate plugin repository needs to be maintained for each chain, the plugin 
repository for eosio is [ibc_plugin_eos](https://github.com/boscore/ibc_plugin_eos), 
for bos is [ibc_plugin_bos](https://github.com/boscore/ibc_plugin_bos).
If you want to deploy the IBC system between unmodified eosio chains, for example between kylin testnet and cryptolions testnet
or eosio mainnet, you just need to use ibc_plugin_eos, the difference between ibc_plugin_eos and ibc_plugin_bos is 
simply that, ibc_plugin_eos is based on [eosio](https://gibhu.com/EOSIO/eos), 
ibc_plugin_bos is based on [bos](https://gibhu.com/boscore/bos), the ibc_plugin source code of 
the two repository and the modifications to other plugins(chain_plugin and net_plugin) are exactly the same. 
Doing so makes it easier to maintain the source code.

- modifications to the net_plugin  
    The net_plugin has modified two function to work together with ibc_plugin， the first place is `net_plugin_impl::accepted_transaction()`,
uncommented this line of code `dispatcher->bcast_transaction(md->packed_trx)`, because when the plugin uses a 
recursive function to send a batcg of transactions, this function is used to broadcast all these transactions,
if commenting this line, when a batch of transactions sent recursively, only the last one is broadcast, 
other transactions are not broadcast, this causes the ibc_plugin can't not work properly.
    
    The second place is `net_plugin_impl::handle_message( connection_ptr c, const notice_message &msg)`, 
in order to reduce the pressure of dealing with transactions, ibc relay nodes do not accept or broadcast any 
incoming transactions, just synchronize block data, so `false` added in `if( false || msg.known_trx.pending > 0)`.

- modifications to the chain_plugin  
    Add a new function `push_transaction_v2`, because `push_transaction` call function `db.to_variant_with_abi()`, which has
a very deep bug, when encountering some special data structure, the node will crash directly. so a new funciton is added
witch dose not call `db.to_variant_with_abi()`.

- special read mode  
:warning:**The nodeos witch run with ibc_plugin enabled, can neither run as a block producer node nor as a api node**,
for this not will not accept incomming transactions and the ibc_plugin customized read mode. 
we add `chain_plug->chain().abort_block()` and `chain_plug->chain().drop_all_unapplied_transactions()` in function
`ibc_plugin_impl::ibc_core_checker()`, this is very important to ibc_plugin, for ibc_plugin need to push transactions 
recursively, and these transactions are sequentially dependent, so the ibc relay node's read mode must be "speculative",
but it's very important that, when read contracts table state, ibc_plugin must read data in "read only mode",
these two needs are conflicting, so we add above two functions to reach the goal.








