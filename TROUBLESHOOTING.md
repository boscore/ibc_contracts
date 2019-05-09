
Troubleshooting
---------------

The IBC system consists of six parts: two relays, two ibc.chain contracts and two ibc.token contracts. 
Each part may fail. Therefore, the possible failures include: 1. the relay node lacks specific data, 2. 
entering the contract logic corner, 3. the system resources of the accounts are insufficient.

Next, these faults are explained in turn, and troubleshooting methods are given.


#### 1. The relay node lacks specific chain data
Fault description:  
Let's take the transfer from Chain A to Chain B as an example. 
Assuming that a cross-chain transfer transaction occurs in the 10000 blocks, 
Chain B's relay (relay B) requests a bunch of headers from Chain A's relay (relay A), 
for example, requests headers 10000-10085, a total of 86 headers.

According to the BP signature verification logic in EOSIO,
relay A needs to get blockroot_merkle of block 10000,
but on ordinary nodes, only blockroot_merkle of blocks in forkdb can be obtained,
so ibc_plugin uses the block's extension to record blockroot_merkle for some blocks.
If in the process of using IBC, if for some reason, 
such as the administrator change a normal fullnode to a relay node at header_num greater then 10000,
relay A may failed to obtain blockroot_merkle of block 10000,
then the subsequent cross-chain behavior can not be carried out.

Restoration:  
 - First: Look for a snapshot whose block num of the head is less than 10000, 
for example 9000, and then start the relay node with this snapshot. 
At this time, the relay node can get the blockroot_merkle of the 10000 blocks.
 - Second: reinitialize the IBC system (see below for details).
 
 
#### 2. Enter the Dead Corner of Contract's Logic
The IBC v1.* system can not use multiple relay pairs(also called relay channel), 
and only one relay node is allowed in each chain.
We assume that we need to validate a transaction in the 10000 blocks (ibctx1), 
so we need to have a section in the ibc.chain contract that contains the 10000th header.
If there are two or more relay channels running simultaneously,
suppose a new section 12000-12085 is created by one channel due to information asynchrony between channels,
at this point, other relay channel can no longer push a section of 10000-10085,
as a result, ibctx1 can no longer be validated, system logic goes into a dead corner.
(There will be no such problem in the IBC v2 contrancts and ibc_plugin)

Restoration:  
 - reinitialize the IBC system (see below for details).
 
#### 3. Accounts resources are insufficient
There are three ibc-related accounts in each chain: relay account, ibc.token and ibc.chain contract accounts,
ibc.token and ibc.chain accounts require sufficient RAM resources,
it's recommended to allocate 5MB or more to ibc.chain and 10MB or more to ibc.token,
if the RAM resource is insufficient during the operation, the cross-chain transaction stops, 
after purchasing enough RAM resources for the account, it can resume operation.

The relay account is responsible for executing all cross-chain transactions and therefore 
requires sufficient CPU and NET resources, if CPU or NET resources are insufficient during the operation, 
cross-chain transactions will stop and the operation will resume after purchasing sufficient resources for the account.
 
 
How to Reinitialize the IBC System
----------------------------------
When you encounter a failure that cannot be repaired directly, you need to reinitialize the IBC system. 
The specific steps are as follows

### Step 1: Stop 2 relays nodes

### Step 2: Lock all tokens
Set ibc.token contract's singleton global's member `active` to false on Chain A and B to lock all token.
run bellow commands on both chain A and B, then check contract status
``` 
$ cleos push action ${ibc_token} lockall '[]' -p ${ibc_token}

# check if global.active is false
$ cleos get table ${ibc_token} ${ibc_token} global 
```

### Step 3: Initialize ibc.chain contract
run bellow commands on both chain A and B, then check contract status
``` 
$ cleos push action ${ibc_chain} forceinit '[]' -p ${ibc_chain}

# check if the three tables is empty
$ cleos get table  ${ibc_chain}  ${ibc_chain} chaindb
$ cleos get table  ${ibc_chain}  ${ibc_chain} prodsches
$ cleos get table  ${ibc_chain}  ${ibc_chain} sections
```

### Step 4: Initialize ibc.token contract
Initialization of ibc.token contracts is somewhat complicated and requires great care because it involves user assets.
More importantly, there may be IBC transactions in the intermediate state.

#### What is a intermediate state IBC transaction?
For detailed principles, please refer to [ibc.token design document](abc).
When a IBC transaction only takes the first step or the first two steps, 
we say that the IBC transaction is in the intermediate state.

Only the first step of the IBC transaction has been executed, we say it is in `stage 1`.
Only the first two steps of IBC transaction have been executed, we say it is in `stage 2`.

**For transactions in stage 1, we need to rollback them first, then delete their records in table `origtrxs`.
For transactions in stage 2, their token has been successfully transferred to the peer chain, 
so we only need to delete their records in table `origtrxs`, but do not rollback them, if rollback, it's double spend.**

#### 4.1 Find out IBC transactions that start from chain A and rollback transactions that need to be rolled back.

##### 4.1.1 Dump all info in table `origtrxs` in Chain A's ibc.token contract
All transactions dumped are in intermediate state.
``` 
# run below command on chain A
$ cleos get table ${ibc_token} ${ibc_token} origtrxs 
```
Note that if `more` is true in the output, you need to use the interval option to get all transactions in this table
until `more` is false.

For example, we can get the following table (Note: The following values are not actual values, just examples):
``` 
time_slot   trx_id  
---------------------
10000       ibctrx100   
10001       ibctrx101       
10002       ibctrx102       
10003       ibctrx103       
10003       ibctrx104   // in the same block with ibctrx103
10004       ibctrx105       
10005       ibctrx106   
```

##### 4.1.2 Get the last 20 records of `cashtrxs` of Chain B's ibc.tokoen contract.
``` 
time_slot   trx_id  orig_trx_id
---         ---     ibctrx96
---         ---     ibctrx97
---         ---     ibctrx98
---         ---     ibctrx99
---         ---     ibctrx101
---         ---     ibctrx102
---         ---     ibctrx103
```

Then we can determine the stages of these transactions in table `origtrxs` of chain A's ibc.token contract:
```
time_slot   trx_id  
10000       ibctrx100   // in stage 1, rollback is required
10001       ibctrx101   // in stage 2, delete directly
10002       ibctrx102   // in stage 2, delete directly
10003       ibctrx103   // in stage 2, delete directly
10003       ibctrx104   // in stage 1, rollback is required
10004       ibctrx105   // in stage 1, rollback is required
10005       ibctrx106   // in stage 1, rollback is required
```

##### 4.1.3 Rollback transactions that need to be rolled back
```
$ cleos push action ${ibc_token} fcrollback "[[ ibctrx100,ibctrx104,ibctrx105,ibctrx106]]" -p ${ibc_token}
```
After executing this command, check the `origtrxs` table again to ensure that all transactions that need to be rolled back are rolled back.

#### 4.2 Find out IBC transactions that start from chain B and rollback transactions that need to be rolled back.
The principle is exactly the same as 4.1, except that two chains exchange names.

#### 4.3 Force initialize the two ibc.token contracts on Chain A and B
Warning: Please ensure that 4.1 adn 4.2 is executed correctly before this step can be executed.

This command may need to be executed multiple times on each chain to thoroughly circumvent three tables, 
because at most 200 records are deleted at a time.
``` 
# run on both chain
$ cleos push action ${ibc_token} forceinit '[]' -p ${ibc_token}

# check if the three tables is empty
$ cleos get table ${ibc_token} ${ibc_token} origtrxs 
$ cleos get table ${ibc_token} ${ibc_token} cashtrxs 
$ cleos get table ${ibc_token} ${ibc_token} rmdunrbs 
```

### Step 5: Restart two relay nodes

### Step 6: Unlock all tokens
Set ibc.token contract's singleton global's member `active` to true on Chain A and B to unlock all token.
run bellow commands on both chain A and B, then check contract status
``` 
$ cleos push action ${ibc_token} unlockall '[]' -p ${ibc_token}

# check if global.active is true
$ cleos get table ${ibc_token} ${ibc_token} global 
```

So far, the reinitialization of the IBC system has been completed.
