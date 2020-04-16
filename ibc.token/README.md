ibc.token
---------
This ibc.token contract can work in conjunction with multiple ibc.chain contracts, 
enabling users to manage assets on multiple peer chains with one ibc.token contract account.
Therefore, the BOS IBC system can be used to elegantly construct multi sidechains cross-chain network.

Contents
--------
 * [Actions called by normal users](#actions-called-by-normal-users)
 * [Actions called by administrators](#actions-called-by-administrators)
 * [Actions called by ibc_plugin](#actions-called-by-ibc_plugin)
 * [Contract Design](#contract-design)
 
 
Actions called by normal users
------------------------------
#### transfer
``` 
void transfer( name from,name to,asset quantity, string memo );
```
 - **from** from account
 - **to** to account
 - **quantity** asset quantity
 - **memo** memo string
 - Note: ('_self' below is the ibc.token contract account)  
if to != _self, this is a local chain normal transfer.  
if to == _self and memo string start with "local", this is a local chain normal transfer.  
if to == _self and memo string conform to "IBC transfer memo format", this is a IBC transaction.   
if to == _self and memo string does not start with "local", nor does it conform to "IBC transfer memo format", 
this transaction must be fail.


Actions called by administrators
-----------------------------------

#### setglobal
```
  void setglobal( name this_chain, bool active );
```
 - **this_chain** this chan's name, used to verify the chain name specified in the cross-chain transaction memo string
 - **active** set the initial global active state (_global_state.active).
    Only when _global_state.active is true can the original IBC transaction be successfully executed.
 - require auth of _self

#### regpeerchain
``` 
  void regpeerchain( name           peerchain_name,
                     string         peerchain_info,
                     name           peerchain_ibc_token_contract,
                     name           thischain_ibc_chain_contract,
                     name           thischain_free_account,
                     uint32_t       max_original_trxs_per_block,
                     uint32_t       max_origtrxs_table_records,
                     uint32_t       cache_cashtrxs_table_records,
                     bool           active );
```

 - **peerchain_name**， the name of the peer blockchain, 
   used to verify the chain name in action `transfer`'s memo string after character '@'
 - **peerchain_info**， information of the peer chain
 - **peerchain_ibc_token_contract** the peer chain's ibc.token contract name, used to verify original IBC transactions
 - **thischain_ibc_chain_contract** the corresponding ibc.chain contract account of the peer chain
 - **thischain_free_account**, a account name, used by IBC monitor system, 
   transactions which transfer token from or to this account have no charge
 - **max_original_trxs_per_block** maximum original transactions per block, the recommended value is 5, 
    the recommended range is [1-10]. If set greater than 10, 
    the IBC system may not be able to handle such large throughput of IBC transactions.   
 - **max_origtrxs_table_records** maximum `origtrxs` table records, this variable not used currently, set 0 is ok.
 - **cache_cashtrxs_table_records** maximum cashtrxs table records, the recommended value is 1000.
 - **active** set the initial active state (_peerchains.active).
    Only when _peerchains.active is true can the original IBC transaction to this peer chain be successfully executed.
 - require auth of _self

Examples:  
The following examples assume that IBC systems are deployed between the EOS and BOS mainnet, 
and we name the EOS mainnet in ibc.token contract 'eos', and BOS mainnet 'bos'.
Suppose the ibc.token contracts on both chains are deployed on accounts with same name ibc2token555, then 
```
run on EOS mainnet to set _global_state:
$ cleos push action ibc2token555 regpeerchain '["bos","https://boscore.io",ibc2token555","ibc2chain555","eosfreeacnt1",5,1000,1000,true]' -p ibc2token555
run on BOS mainnet to set _global_state:
$ cleos push action ibc2token555 regpeerchain '["eos","https://eos.io",ibc2token555","ibc2chain555","bosfreeacnt1",5,1000,1000,true]' -p ibc2token555
```

#### setchainbool
``` 
void setchainbool( name peerchain_name, string which, bool value );
```
 - **peerchain_name**, peer chain name
 - **which**, must be 'active'
 - **value**, bool value, set the active state (_peerchains.active).
   Only when _peerchains.active is true can the original IBC transaction to this peer chain be successfully executed.
 - require auth of _self
 
#### regacpttoken
``` 
  void regacpttoken( name        original_contract,
                     asset       max_accept,
                     asset       min_once_transfer,
                     asset       max_once_transfer,
                     asset       max_daily_transfer,
                     uint32_t    max_tfs_per_minute, // 0 means the default value defined by default_max_trx_per_minute_per_token
                     string      organization,
                     string      website,
                     name        administrator,
                     name        service_fee_mode,
                     asset       service_fee_fixed,
                     double      service_fee_ratio,
                     asset       failed_fee,
                     bool        active );
```
This action is used to register blockchain native token, which is originally issued on this blockchain.
 - **original_contract** account name of the token contract to be registered.
 - **max_accept** maximum amount of receivable assets.
 - **min_once_transfer** minimum amount of single transfer
 - **max_once_transfer** maximum amount of single transfer
 - **max_daily_transfer**  maximum amount of daily transfer
 - **max_tfs_per_minute** maximum number of transfers per minute, appropriate value range is [10,150];
 - **organization** organization name
 - **website** official website address
 - **administrator** this token's administrator, who can set some parameters related to this token.
 - **service_fee_mode** charging mode, must be "fixed" or "ratio"
 - **service_fee_fixed** if service_fee_mode == fixed, use this value to calculate the successful withdrawal fee.
 - **service_fee_ratio** if service_fee_mode == ratio, use this value to calculate the successful withdrawal fee.
 - **failed_fee** use this value to calculate fee for the filed original transaction.
 - **active** set the initial active state of this token, 
    when active is false, IBC transfers are not allowed, but **cash**s trigger by peerchain action **withdraw** can still execute.
 - require auth of _self

Note: from IBC version 4, the pegged token symbol must same with the original token symbol.

``` 
contract_token=ibc2token555
run on EOS mainnet to register token EOS of eosio.token contract:
$ cleos push action ${contract_token} regacpttoken \
    '["eosio.token","1000000000.0000 EOS","1.0000 EOS","10000.0000 EOS",
    "1000000.0000 EOS",1000,"eos organization","https://eos.io","ibc2token555","fixed","0.1000 EOS",0.01,"0.1000 EOS",true]' -p ${contract_token}
    
run on BOS mainnet to register token BOS of eosio.token contract:
$ cleos push action ${contract_token} regacpttoken \
    '["eosio.token","1000000000.0000 BOS","1.0000 BOS","10000.0000 BOS",
    "1000000.0000 BOS",1000,"bos organization","https://boscore.io","ibc2token555","fixed","0.1000 BOS",0.01,"0.1000 BOS",true]' -p ${contract_token}
```

#### setacptasset
``` 
void setacptasset( symbol_code symcode, string which, asset quantity );
```
Modify only one member of type `asset` in currency_accept struct.
 - **symcode** the token symbol code
 - **which** must be one of "max_accept", "min_once_transfer", "max_once_transfer", "max_daily_transfer".
 - **quantity** the assets' value to be set.
 - require auth of this token's administrator
  
#### setacptstr
``` 
  void setacptstr( symbol_code symcode, string which, string value );
```
Modify only one member of type `string` in currency_accept struct.
 - **symcode** the token symbol code
 - **which** must be one of "organization", "website".
 - **value** the value to be set.
 - require auth of this token's administrator
 
#### setacptint
``` 
  void setacptint( symbol_code symcode, string which, uint64_t value );
```
Modify only one member of type `int` in currency_accept struct.
 - **symcode** the token symbol code
 - **which** must be "max_tfs_per_minute".
 - **value** the value to be set.
 - require auth of _self
 
#### setacptbool
``` 
  void setacptbool( symbol_code symcode, string which, bool value );
```
Modify only one member of type `bool` in currency_accept struct.
 - **symcode** the token symbol code
 - **which** must be "active".
 - **value** the bool value to be set.
 - require auth of this token's administrator
 
#### setacptfee
``` 
  void setacptfee( symbol_code symcode,
                   name   kind,
                   name   fee_mode,
                   asset  fee_fixed,
                   double fee_ratio );
```
Modify fee related members in currency_accept struct.
 - **symcode** the token symbol code
 - **kind** must be "success" or "failed".
 - **fee_mode** must be "fixed" or "ratio".
 - **fee_fixed** fixed fee quota, used when fee_mode == fixed
 - **fee_ratio** charge ratio, used when fee_mode == ratio
 - require auth of _self
 
#### regpegtoken
```
  void regpegtoken( name        peerchain_name,
                    name        peerchain_contract,
                    asset       max_supply,
                    asset       min_once_withdraw,
                    asset       max_once_withdraw,
                    asset       max_daily_withdraw,
                    uint32_t    max_wds_per_minute,
                    name        administrator,
                    asset       failed_fee,
                    bool        active );

```
This action is used to register pegged token, which is originally issued on other blockchains.
 - **peerchain_name** the name of the chain where the original token was located.
 - **peerchain_contract** the pegged token's original token contract name on it's original chain.
 - **max_supply** maximum supply
 - **min_once_withdraw** minimum amount of single withdraw
 - **max_once_withdraw** maximum amount of single withdraw
 - **max_daily_withdraw** maximum amount of daily withdraw
 - **max_wds_per_minute** maximum number of withdraws per minute, appropriate value range is [10,150];
 - **administrator** this token's administrator, who can set some parameters related to this token.
 - **failed_fee** use this value to calculate fee for the filed withdraw transaction.
 - **active** set the initial active state of this pegged token, 
     when active is false, IBC withdraws are not allowed, but **cash**s trigger by peerchain action **transfer** can still execute.
 - require auth of _self

``` 
contract_token=ibc2token555
run on EOS mainnet to register BOS's pegged token, the pegged token symbol is BOS:
$ cleos push action ${contract_token} regpegtoken \
    '["bos","eosio.token","4,BOS","4,BOS","1000000000.0000 BOS","1.0000 BOS","10000.0000 BOS",
    "1000000.0000 BOS",1000,"ibc2token555","0.1000 BOS",true]' -p ${contract_token}

run on BOS mainnet to register EOS's pegged token, the pegged token symbol is EOS:
$ cleos push action ${contract_token} regpegtoken \
    '["eos","eosio.token","4,EOS","4,EOS","1000000000.0000 EOS","1.0000 EOS","10000.0000 EOS",
    "1000000.0000 EOS",1000,"ibc2token555","0.1000 EOS",true]' -p ${contract_token}
```

#### regpegtoken2
```
 void regpegtoken2(     name        peerchain_name,
                        name        peerchain_contract,  // the original token contract on peer chain
                        asset       max_supply,
                        asset       min_once_transfer,
                        asset       max_once_transfer,
                        asset       max_daily_transfer,
                        uint32_t    max_tfs_per_minute,  // 0 means the default value defined by default_max_trxs_per_minute_per_token
                        string      organization,
                        string      website,
                        name        administrator,
                        name        service_fee_mode,
                        asset       service_fee_fixed,
                        double      service_fee_ratio,
                        asset       failed_fee,
                        bool        active );   // when non active, transfer not allowed, but cash which trigger by peerchain transfer can still execute
```
This action is used to register pegged token (which is originally issued on other blockchains) and support hub protocol meanwhile,
this action equals call regpegtoken(...) firstly and then call regacpttoken(...).
 - **peerchain_name** the name of the chain where the original token was located.
 - **peerchain_contract** the pegged token's original token contract name on it's original chain.
 - **max_supply** maximum supply
 - **min_once_transfer** minimum amount of single transfer
 - **max_once_transfer** maximum amount of single transfer
 - **max_daily_transfer** maximum amount of daily transfer
 - **max_tfs_per_minute** maximum number of transfers per minute, appropriate value range is [10,150];
 - **organization** organization name
 - **website** official website address
 - **administrator** this token's administrator, who can set some parameters related to this token.
 - **service_fee_mode** charging mode, must be "fixed" or "ratio"
 - **service_fee_fixed** if service_fee_mode == fixed, use this value to calculate the successful transfer fee.
 - **service_fee_ratio** if service_fee_mode == ratio, use this value to calculate the successful transfer fee.
 - **failed_fee** use this value to calculate fee for the filed transfer transaction.
 - **active** set the initial active state of this pegged token, 
     when active is false, IBC transfers are not allowed, but **cash**s trigger by peerchain action **transfer** can still execute.
 - require auth of _self

``` 
contract_token=ibc2token555
run on BOS mainnet to register EOS's pegged token, the pegged token symbol is EOS:
$ cleos push action ${contract_token} regpegtoken2 \
    '["eos","eosio.token","1000000000.0000 EOS","1.0000 EOS","10000.0000 EOS",
    "1000000.0000 EOS",100,"eosio","https://eos.io","ibc2token555","fixed","0.1000 EOS",0.0,"0.0500 EOS",true]' -p ${contract_token}
```

#### setpegasset
``` 
  void setpegasset( symbol_code symcode, string which, asset quantity )
```
Modify only one member of type `asset` in currency_stats struct.
 - **symcode** the symcode of registered pegged token.
 - **which** must be one of "max_supply", "min_once_withdraw", "max_once_withdraw", "max_daily_withdraw".
 - **quantity** the assets' value to be set.
 - require auth of this token's administrator

#### setpegint
``` 
  void setpegint( symbol_code symcode, string which, uint64_t value )
```
Modify only one member of type `int` in currency_stats struct.
 - **symcode** the symcode of registered pegged token.
 - **which** must be "max_wds_per_minute".
 - **value** the value to be set.
 - require auth of this token's administrator

#### setpegbool
``` 
  void setpegbool( symbol_code symcode, string which, bool value )
```
Modify only one member of type `bool` in currency_stats struct.
 - **symcode** the symcode of registered pegged token.
 - **which** must be "active".
 - **value** the bool value to be set.
 - require auth of this token's administrator

#### setpegtkfee
``` 
  void setpegtkfee( symbol_code symcode, asset fee )
```
Modify fee related members in currency_stats struct.
 - **symcode** the symcode of registered pegged token.
 - **fee** fixed fee quota
 - require auth of _self

#### unregtoken
``` 
   void unregtoken( name table, symbol_code sym_code );
```
 - delete token register record in table accepts or stats.
 - **table** table name, table name must be one of `all` ,`accepts` and `stats`, 
 when it's `all`, all records in table `accepts` or `stats` will be deleted.
 - **sym_code** token symbol code, e.g. `BOS`,`EOS`.
 - require auth of _self

#### fcrollback
``` 
  void fcrollback( name peerchain_name, const std::vector<transaction_id_type> trxs );
```
 - this action may be used when repairing IBC system, 
   used to force rollback (refund) specified original transaction records in table `origtrxs`.
 - **peerchain_name** peer chain name.
 - **trxs** original transactions that need to be rolled back.
 - require auth of _self

#### fcrmorigtrx
``` 
  void fcrmorigtrx( name peerchain_name, const std::vector<transaction_id_type> trxs ); 
```
 - this action may be used when repairing IBC system, 
   used to force remove specified original transaction records in table `origtrxs`.
 - **peerchain_name** peer chain name.
 - **trxs** original transactions that need to be remove.
 - require auth of _self
 
#### lockall
``` 
void lockall();
```
 - set 'active' of global_state false.
 - when locked, ibc-transfer and withdraw will not allowed to execute for all token.
 - require auth of _self
 
#### unlockall
``` 
void unlockall();
```
 - set 'active' of global_state true.
 - when unlocked, the restrictions caused by execute lockall() will be removed.
 - require auth of _self
 
#### forceinit
``` 
  void forceinit( name peerchain_name );
```
 - **peerchain_name** peer chain name.
 - force initialization of this contract.
   this action clears three tables `origtrxs`, `cashtrxs` and `rmdunrbs` and a singleton `globalm`,
   but it will not affect tables `globals`, `accepts` and `stats`.
 - Note that this action deletes up to 200 table records at a time, in order to avoid CPU timeouts, 
   so if the number of records in these three tables is greater than 200, 
   you need to perform this action several times to clear all three tables.
   When the console prints "force initialization complete", it says that all three tables have been cleared.
 - require auth of _self

#### hubinit
```  
    void hubinit( name hub_account );
```
 - **hub_account** the hub account. Just need this hub account to exist, no matter who has the authority, or who owns this account.
 - open the `hub` feature of ibc.token contract.
 - for more information of hub feature, please refer to [ibc_hub.md](../docs/IBC_Hub_Protocol.md).

Actions called by ibc_plugin
----------------------------
#### cash
``` 
  void cash( const uint64_t&                        seq_num,
             const name&                            from_chain,
             const transaction_id_type&             orig_trx_id,          // redundant, facilitate indexing and checking
             const std::vector<char>&               orig_trx_packed_trx_receipt,
             const std::vector<capi_checksum256>&   orig_trx_merkle_path,
             const uint32_t&                        orig_trx_block_num,   // redundant, facilitate indexing and checking
             const std::vector<char>&               orig_trx_block_header,
             const std::vector<capi_checksum256>&   orig_trx_block_id_merkle_path,
             const uint32_t&                        anchor_block_num,
             const name&                            to,                   // redundant, facilitate indexing and checking
             const asset&                           quantity,             // redundant, facilitate indexing and checking
             const string&                          memo );
```
 - **seq_num** The serial number given by the ibc_plugin, incremented one by one from 1.
 - **from_chain** peer chain name.
 - **orig_trx_id**  original transaction id.
 - **orig_trx_packed_trx_receipt** original transaction's packed transaction receipt.
 - **orig_trx_merkle_path** original transaction's merkle path to transaction merkleroot in block header.
 - **orig_trx_block_num** original transaction's block number.
 - **orig_trx_block_header** original transaction's block header.
 - **orig_trx_block_id_merkle_path** original transaction's block id merkle path to one active incremental_merkle node of the anchore block.
 - **anchor_block_num** anchor block in table `chaindb` of ibc.chain contract
 - **to** to account, who receive token transfered from the peer chain.
 - **quantity** quantity of token.
 - **memo** not used.
 - can be called with any account's auth

#### cashconfirm
``` 
  void cashconfirm( const name&                            from_chain,
                    const transaction_id_type&             cash_trx_id,            // redundant, facilitate indexing and checking
                    const std::vector<char>&               cash_trx_packed_trx_receipt,
                    const std::vector<capi_checksum256>&   cash_trx_merkle_path,
                    const uint32_t&                        cash_trx_block_num,     // redundant, facilitate indexing and checking
                    const std::vector<char>&               cash_trx_block_header,
                    const std::vector<capi_checksum256>&   cash_trx_block_id_merkle_path,
                    const uint32_t&                        anchor_block_num,
                    const transaction_id_type&             orig_trx_id );          // redundant, facilitate indexing and checking
```
 - **from_chain** peer chain name.
 - **cash_trx_id** cash transaction id.
 - **cash_trx_packed_trx_receipt** cash transaction packed transaction receipt.
 - **cash_trx_merkle_path** cash transaction's merkle path to transaction merkleroot in block header.
 - **cash_trx_block_num** cash transaction block number.
 - **cash_trx_block_header** cash transaction's block header.
 - **cash_trx_block_id_merkle_path** cash transaction's block id merkle path to one active incremental_merkle node of the anchore block.
 - **anchor_block_num** anchor block in table `chaindb` of ibc.chain contract
 - **to** to account, who receive token transfered from the peer chain.
 - **orig_trx_id** original transaction id of this cash transaction.
 - can be called with any account's auth
 
#### rollback
```
  void rollback( name peerchain_name, const transaction_id_type trx_id ); 
```
 - called by ibc_plugin when there are original transactions need to be rolled back.
 - **peerchain_name** peer chain name.
 - **trx_id**  transaction id, which need to be rollback (refund).
 - can be called with any account's auth
 
#### rmunablerb
```
  void rmunablerb( name peerchain_name, const transaction_id_type trx_id ); 
```
 - called by ibc_plugin when there are unrollbackable original transactions
 - **peerchain_name** peer chain name.
 - **trx_id**  transaction id, which need to be remove.
 - can be called with any account's auth

Contract Design
---------------
Transaction is the core concept of database and ACID is the four basic elements for the correct 
execution of database transactions. Its characteristics are briefly stated as follows:
Transactions are executed as a whole, and the operations on the database included in them 
are either all executed or none executed, and it is impossible to stagnate in the middle.

In addition to single database transactions, there are distributed transactions.
Distributed transactions refer to a transaction that uses multiple independent databases on different hosts.
Distributed transactions still need to satisfy the requirement that their operations on multiple databases 
be either fully executed or not at all.
However, there is a big difference between distributed transactions and single database transactions. 
ACID theory can not be fully applied. In such cases, what we need is final consistency, not strong consistency.

In the design of this contract, A complete cross-chain transaction consists of three separate on-chain transactions. 
One cross-chain transaction is completed only when all the three independent transactions succeed.

The three transactions are, 1. original transaction, 2. cash transaction, 3. cash confirm transaction

 - **1. original transaction**  
    The original transaction refers to the cross-chain transfer transaction triggered by the user.
There are two kind of original IBC transactions.
One is called **forward transfer**, one user calls a normal token contract(not ibc.token)'s action `transfer`, in the format of 
    ``` 
    transfer(from, "ibc.token contract name", quantity, "account_name@peer_chain_name ...")
    ```
    Another is called **withdraw**, one user calls contract ibc.token's action `transfer`, in the same format of 
    ``` 
    transfer(from, "ibc.token contract name", quantity, "account_name@peer_chain_name ...")
    ```
    
    The user calls the action *transfer* of a token contract, include the ibc.token contract itself, 
    and provides the information of the receiver account and the perr chain name in memo, then generate a original transaction.

    In a **forward transfer**, the user calls `transfer`, which will trigger `transfer_notify()` of this contract 
by notification ( require_recipient(to) ), or in a **withdraw** transaction, the user call transfer of 'ibc.token',
Then, this contract checks the original transaction. If the check fails, the original transaction will fail. 
If it passes, the relevant information of the original transaction will be recorded in this contract.
It mainly includes transfer information (which is needed when cross-chain fails to refund) 
and transaction ID and block information (which is used by ibc_plugin to find the transaction in the blockchain data).

 - **2. cash transaction**  
When an original transaction (TX1) occurs (on Chain A, ibc.token contract),
The relay of chain A (relay A) will find the transaction in the blockchain data 
based on the original transaction-related information recorded in the ibc.token contract.
Then the transaction and its related information (merkle path, etc.) are transmitted to the relay of chain B (relay B).
Relay B pushes the original transaction and its related information together to chain B's ibc.token action **cash** (TX2),
if the execution is successful, the user's token is successfully transferred to the account specified on chain B.

    If the cash transaction (TX2) fails to execute, 
there needs to be a way to ensure that the user asset in the original transaction (TX1) on Chain A is refunded.
(It can also be called `rollback a transaction`, but it's not rollback the blockchain, just a refund of assets.)
See text later for details on how to ensure TX1 rollback.

    If the cash transaction TX2 is successfully executed, the user's token has been transferred to this chain successfully,
at this point, a TX2 related information will be recorded in the ibc.token contract on Chain B.

 - **3. cashconfirm transaction**  
When a **cash** transaction (TX2) occurs (on Chain B ibc.token contract),
The relay of chain B (relay B) will transfer TX2 and its related information to the relay of chain A (relay A).
Relay A pushes TX2 and its related information together to chain A's ibc.token action **cashconfirm** (TX3).
In this action, delete TX1's record in chain A's ibc.token contract.
So far a complete inter-blockchain transaction has been completed.


**seq_num in cash action**  
seq_num is the first parameter of action **cash**.
This is a very important sequence number to ensure the integrity of cross-chain transactions, 
which is critical in both actions **case** and **cashconfirm**.

The original transaction is triggered by the user, and the original transaction may ultimately succeed, 
or may ultimately fail (for example, the designated account does not exist in the peer chain), 
so the original transaction should not be given a serial number.

**cash** transaction (TX2) is the most critical transaction of the whole IBC transaction.
And every successful **cash** transaction must be transmitted back to the original chain (chain A) to eliminate the 
recording information of the original transaction (TX1) in the chain A's ibc.token contract.
So it must be done by using a serial number.

This seq_num is given by the ibc_plugin, the plugin monitors the number used by the last successful **cash** action 
recorded in the ibc.token contract(cash_seq_num of _global_mutable) in real time.
When the ibc_plugin needs to perform a **cash** action, it will make the seq_num equal to cash_seq_num + 1 for this transaction.

Similarly, when **cashconfirm** is executed in the chain A's ibc.token contract,
The seq_num will also be checked, and it must be ensured that the number is incremental one by one.
This ensures that every successful cash will be passed on to the peer chain, none of them can be skiped.

**How was the original transaction rolled back after cash failed?**  
There is a line of code in the case function:
``` 
eosio_assert( orig_trx_block_num >= get_cashtrxs_tb_max_orig_trx_block_num(), "orig_trx_block_num error"); 
```
The meaning of this line of code is that the original transactions on one chain can only be passed to the peer chain
in incremental order by it's block number.

*The block number can not be obtained in the contract, only the time slot of the block can be obtained. 
And slot is incremental as block number, except that slot may not increment one by one, 
while block number is incremental one by one. *

*Note that the following examples are just for illustration purposes. Please refer to the contract code for the actual data structure.*
For example, suppose that chain A's ibc.token contract records some cross-chain transactions, as follows.
``` 
block_time_slot 1000 ( has ibctx1 ), block_time_slot 1001 ( has ibctx2, ibctx3 ), block_time_slot 1002 ( has ibctx4 )
```
If you skip ibctrx1 and push ibctx2 **cash** on the peer chain and execute successfully, you can't push ibctx1 any more,
because ibctrx1's time_slot is less then ibctrx2's time_slot, then you can push ibctrx 3, and then ibctx 4.

Push ibctx2, ibctx3 can be done in any order, because they are in the same block.
So when executing **cashconfirm**, the IBC transactions' records in table `origtrxs` are also deleted in the order of increasing time slot.

Let's assume that ibctx1 failed to **cash** on peer chain and ibctx2 excute **cash** successfully on peer chain.
When the **cashconfirm** corresponding to ibctx2 executes, the record of ibctx2 is deleted.
At this point, a value `last_confirmed_orig_trx_block_time_slot of _global_mutable` is recorded in the ibc.token contract.
Based on this value, we can judge that ibctrx1 (Actually, all ibc transactions records whose time slot is less than 
ibctrx2's time slot are must cross-chain failed) must have failed across the chain, so we need to rollback ibctrx1.
