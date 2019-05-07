ibc.token
---------

Combine with ibc.chain and ibc_plugin system, this contract allows users to do inter-blockchain assets transfer.


### Contract Design
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
The user calls the action *transfer* of a token contract, 
and provides the information of the receiver account and the perr chain name in memo, then generate a original transaction.

    The original transaction calls the `transfer_notify()` of this contract by notification ( require_recipient(to) ).
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

Actions called by normal users
-------------------------------
  void transfer( name from,name to,asset quantity, string memo );
  void open( name owner, const symbol_code& symcode, name ram_payer );
  void close( name owner, const symbol_code& symcode );
  
Actions called by administrator
-------------------------------
#### setglobal
```
  void setglobal( name       ibc_chain_contract,
                  name       peerchain_name,
                  name       peerchain_ibc_token_contract,
                  uint32_t   max_origtrxs_table_records,
                  uint32_t   cache_cashtrxs_table_records,
                  uint32_t   max_original_trxs_per_block,
                  bool       active );
```
 - **ibc_chain_contract** the ibc.chain contract account
 - **peerchain_name** peer chain name, such as "eos","bos", 
    used to verify the chain name in action `transfer`'s memo string after charactor '@'
 - **peerchain_ibc_token_contract** the peer chain's ibc.token contract name, used to verify original IBC transactions.
 - **max_origtrxs_table_records** maximum `origtrxs` table records, this variable not used currently, set 0 is ok.
 - **cache_cashtrxs_table_records** maximum cashtrxs table records, the recommended value is 1000.
 - **max_original_trxs_per_block** maximum original transactions per block, the recommended value is 5, 
    the recommended range is [1-10]. If set greater then 10, 
    the IBC system may not be able to handle such large throughput of IBC transactions.
 - **active** set the initial global active state (_global_state.active).
    Only when _global_state.active is true can the original IBC transaction be successfully executed.
 - require auth of _self

Examples:  
The following examples assume that IBC systems are deployed between the EOS and BOS mainnet, 
and we name the EOS mainnet in ibc.token contract 'eos', and BOS mainnet 'bos'.
Suppose the ibc.token contracts on both chains are deployed on accounts with same name ibc2token555, then 
```
run on EOS mainnet to set _global_state:
$ cleos push action ibc2token555 setglobal '["ibc2chain555","bos","ibc2token555",0,1000,5,true]' -p ibc2token555
run on BOS mainnet to set _global_state:
$ cleos push action ibc2token555 setglobal '["ibc2chain555","eos","ibc2token555",0,1000,5,true]' -p ibc2token555
```

#### regacpttoken
``` 
  void regacpttoken( name        original_contract,
                     asset       max_accept,
                     name        administrator,
                     asset       min_once_transfer,
                     asset       max_once_transfer,
                     asset       max_daily_transfer,
                     uint32_t    max_tfs_per_minute, // 0 means the default value defined by default_max_trx_per_minute_per_token
                     string      organization,
                     string      website,
                     name        service_fee_mode,
                     asset       service_fee_fixed,
                     double      service_fee_ratio,
                     name        failed_fee_mode,
                     asset       failed_fee_fixed,
                     double      failed_fee_ratio,
                     bool        active,
                     symbol      peerchain_sym );
```
This action is used to register acceptable token.
 - **original_contract** account name of the token contract to be registered.
 - **max_accept** maximum amount of receivable assets.
 - **administrator** this token's administrator, who can set some parameters related to this token.
 - **min_once_transfer** minimum amount of single transfer
 - **max_once_transfer** maximum amount of single transfer
 - **max_daily_transfer**  maximum amount of daily transfer
 - **max_tfs_per_minute** maximum number of transfers per minute, appropriate value range is [10,150];
 - **organization** organization name
 - **website** official website address
 - **service_fee_mode** charging mode, must be "fixed" or "ratio"
 - **service_fee_fixed** if service_fee_mode == fixed, use this value to calculate the successful withdrawal fee.
 - **service_fee_ratio** if service_fee_mode == ratio, use this value to calculate the successful withdrawal fee.
 - **failed_fee_mode** charging mode, must be "fixed" or "ratio"
 - **failed_fee_fixed** if failed_fee_mode == fixed, use this value to calculate fee for the filed original transaction.
 - **failed_fee_ratio** if failed_fee_mode == ratio, use this value to calculate fee for the filed original transaction.
 - **active** set the initial active state of this token, 
    when active is false, IBC transfers are not allowed, but **cash**s trigger by peerchain action **withdraw** can still execute.
 - **peerchain_sym** the peg token symbol on the peer chain's ibc.token contract, can be same with the original token symbol or not.
 - require auth of _self

Examples:  
Suppose the EOS's peg token's symbol on BOS mainnet's ibc.token contract is EOSPG, 
and the BOS's peg token's symbol on EOS mainnet's ibc.token contract is BOSPG.
``` 
run on EOS mainnet to register token EOS of eosio.token contract:
$ cleos push action ibc2token555 regacpttoken '["eosio.token","5000000.0000 EOS","eostokenadmi","0.1000 EOS","1000.0000 EOS",
        "100000.0000 EOS",100,"block.one","eos.io","fixed","0.0100 EOS",0,"fixed","0.0050 EOS",0,true,"4,EOSPG"]' -p ibc2token555
run on BOS mainnet to register token BOS of eosio.token contract:
$ cleos push action ibc2token555 regacpttoken '["eosio.token","5000000.0000 BOS","bostokenadmi","0.1000 BOS","1000.0000 BOS",
        "100000.0000 BOS",100,"boscore","boscore.io","fixed","0.0100 BOS",0,"fixed","0.0050 BOS",0,true,"4,BOSPG"]' -p ibc2token555
```

#### setacptasset
``` 
void setacptasset( name contract, string which, asset quantity );
```
Modify only one member of type `asset` in currency_accept struct.
 - **contract** the name of one registered acceptable token contract.
 - **which** must be one of "max_accept", "min_once_transfer", "max_once_transfer", "max_daily_transfer".
 - **quantity** the assets' value to be set.
 - require auth of this token's administrator
  
#### setacptstr
``` 
  void setacptstr( name contract, string which, string value );
```
Modify only one member of type `string` in currency_accept struct.
 - **contract** the name of one registered acceptable token contract.
 - **which** must be one of "organization", "website".
 - **value** the value to be set.
 - require auth of this token's administrator
 
#### setacptint
``` 
  void setacptint( name contract, string which, uint64_t value );
```
Modify only one member of type `int` in currency_accept struct.
 - **contract** the name of one registered acceptable token contract.
 - **which** must be "max_tfs_per_minute".
 - **value** the value to be set.
 - require auth of _self
 
#### setacptbool
``` 
  void setacptbool( name contract, string which, bool value );
```
Modify only one member of type `bool` in currency_accept struct.
 - **contract** the name of one registered acceptable token contract.
 - **which** must be "active".
 - **value** the bool value to be set.
 - require auth of this token's administrator
 
#### setacptfee
``` 
  void setacptfee( name   contract,
                   name   kind,
                   name   fee_mode,
                   asset  fee_fixed,
                   double fee_ratio );
```
Modify fee related members in currency_accept struct.
 - **contract** the name of one registered acceptable token contract.
 - **kind** must be "success" or "failed".
 - **fee_mode** must be "fixed" or "ratio".
 - **fee_fixed** fixed fee quota, used when fee_mode == fixed
 - **fee_ratio** charge ratio, used when fee_mode == ratio
 - require auth of _self
 
#### regpegtoken
```
  void regpegtoken( asset       max_supply,
                    asset       min_once_withdraw,
                    asset       max_once_withdraw,
                    asset       max_daily_withdraw,
                    uint32_t    max_wds_per_minute, // 0 means the default value defined by default_max_trx_per_minute_per_token
                    name        administrator,
                    name        peerchain_contract,
                    symbol      peerchain_sym,
                    name        failed_fee_mode,
                    asset       failed_fee_fixed,
                    double      failed_fee_ratio,
                    bool        active ); // when non active, withdraw not allowed, but cash which trigger by peerchain transfer can still execute

```
This action is used to register peg token.
 - **max_supply** maximum supply
 - **min_once_withdraw** minimum amount of single withdraw
 - **max_once_withdraw** maximum amount of single withdraw
 - **max_daily_withdraw** maximum amount of daily withdraw
 - **max_wds_per_minute** maximum number of withdraws per minute, appropriate value range is [10,150];
 - **administrator** this token's administrator, who can set some parameters related to this token.
 - **peerchain_contract** the peg token's original token contract name on it's original chain.
 - **peerchain_sym** the peg token's original token symbol on it's original chain.
 - **failed_fee_mode** charging mode, must be "fixed" or "ratio"
 - **failed_fee_fixed** if failed_fee_mode == fixed, use this value to calculate fee for the filed withdraw transaction.
 - **failed_fee_ratio** if failed_fee_mode == ratio, use this value to calculate fee for the filed withdraw transaction.
 - **active** set the initial active state of this peg token, 
     when active is false, IBC withdraws are not allowed, but **cash**s trigger by peerchain action **transfer** can still execute.
 - require auth of _self

Examples:  
Suppose the EOS's peg token's symbol on BOS mainnet's ibc.token contract is EOSPG, 
and the BOS's peg token's symbol on EOS mainnet's ibc.token contract is BOSPG.
``` 
run on EOS mainnet to register BOS's peg token, the peg token symbol is BOSPG:
$ cleos push action ibc2token555 regpegtoken '["5000000.0000 BOSPG","0.1000 BOSPG","1000.0000 BOSPG",
        "100000.0000 BOSPG",100,"bostokenadmi","eosio.token","4,BOS","fixed","0.0050 BOSPG",0,true]' -p ibc2token555
run on BOS mainnet to register EOS's peg token, the peg token symbol is EOSPG:
$ cleos push action ibc2token555 regpegtoken '["5000000.0000 EOSPG","0.1000 EOSPG","1000.0000 EOSPG",
        "100000.0000 EOSPG",100,"eostokenadmi","eosio.token","4,EOS","fixed","0.0050 EOSPG",0,true]' -p ibc2token555
```

#### setpegasset
``` 
  void setpegasset( symbol_code symcode, string which, asset quantity )
```
Modify only one member of type `asset` in currency_stats struct.
 - **symcode** the symcode of registered peg token.
 - **which** must be one of "max_supply", "min_once_withdraw", "max_once_withdraw", "max_daily_withdraw".
 - **quantity** the assets' value to be set.
 - require auth of this token's administrator

#### setpegint
``` 
  void setpegint( symbol_code symcode, string which, uint64_t value )
```
Modify only one member of type `int` in currency_stats struct.
 - **symcode** the symcode of registered peg token.
 - **which** must be "max_wds_per_minute".
 - **value** the value to be set.
 - require auth of this token's administrator

#### setpegbool
``` 
  void setpegbool( symbol_code symcode, string which, bool value )
```
Modify only one member of type `bool` in currency_stats struct.
 - **symcode** the symcode of registered peg token.
 - **which** must be "active".
 - **value** the bool value to be set.
 - require auth of this token's administrator

#### setpegtkfee
``` 
  void setpegtkfee( symbol_code symcode,
                    name        fee_mode,
                    asset       fee_fixed,
                    double      fee_ratio )
```
Modify fee related members in currency_stats struct.
 - **symcode** the symcode of registered peg token.
 - **fee_mode** must be "fixed" or "ratio".
 - **fee_fixed** fixed fee quota, used when fee_mode == fixed
 - **fee_ratio** charge ratio, used when fee_mode == ratio
 - require auth of _self

#### fcrollback
``` 
  void fcrollback( const std::vector<transaction_id_type> trxs, string memo );
```



  void fcrmorigtrx( const std::vector<transaction_id_type> trxs, string memo ); 
  void fcinit();



Actions called by ibc_plugin
-------------------------------

  void cash( uint64_t                               seq_num,
             const uint32_t                         orig_trx_block_num,
             const std::vector<char>&               orig_trx_packed_trx_receipt,
             const std::vector<capi_checksum256>&   orig_trx_merkle_path,
             transaction_id_type                    orig_trx_id,    
             name                                   to,             
             asset                                  quantity,       
             string                                 memo,
             name                                   relay );


  void cashconfirm( const uint32_t                         cash_trx_block_num,
                    const std::vector<char>&               cash_trx_packed_trx_receipt,
                    const std::vector<capi_checksum256>&   cash_trx_merkle_path,
                    transaction_id_type                    cash_trx_id,   
                    transaction_id_type                    orig_trx_id ); 


  void rollback( const transaction_id_type trx_id, name relay );

  void rmunablerb( const transaction_id_type trx_id, name relay );



Troubleshooting
---------------









