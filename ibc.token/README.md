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


