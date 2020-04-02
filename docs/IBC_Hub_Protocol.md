IBC Hub Protocol
----------------

This is a new hub protocol based on the original IBC(inter-blockchain communication) protocol, 
the blockchain deployed with this protocol can be used as the IBC center of multiple blockchains, 
and forming a star structure IBC network, for all tokens of the chains connected with this central chain, 
the user only needs to perform one transfer action to transfer the tokens in one chain to any other chain.

In this paper, we will make a comprehensive description of the hub protocol, in order to make the description clearer, 
we will discuss it based on a hypothetical multi chain environment. Suppose we have four eosio blockchains, 
which are chain_a, chain_b, chain_c and chain_d, among them, chain_a is the hub chain, and other chains are called parallel chain.

Contents
--------
 * [Information about each chain](#information-about-each-chain)
 * [Multi Chain IBC architecture](#multi-chain-ibc-architecture)
 * [Noun definition](#noun-definition)
 * [Shell variables definition](#shell-variables-definition)
 * [Compile/deploy/initialize](#compile--deploy--initialize)
 * [Token Registration](#token-registration)
 * [Only use ibc protocol](#only-use-ibc-protocol)
 * [HUB Protocol - User Protocol](#hub-protocol---user-protocol)
 * [HUB Protocol - Worker Protocol](#hub-protocol---worker-protocol)
 * [Complete examples](#complete-examples)
 * [Safety](#safety)
 * [Internal special protocol](#internal-special-protocol)

  
Information about each chain
------------------


|                    |   chain_a   |   chain_b   |   chain_c   |   chain_d   |               |
|--------------------|-------------|-------------|-------------|-------------|---------------|
|    property        |    value    |    value    |    value    |    value    |     remark    |
|system token symbol |     TOA     |     TOB     |     TOC     |     TOD     |eosio.token    |
|chain short name    |     cha     |     chb     |     chc     |     chd     |used in transfer action's memo string|
|ibc.token account   |    ibc.io   |    ibc.io   |    ibc.io   |    ibc.io   |deploy ibc.token contract|
|hub account         |    hub.io   |      -      |      -      |      -      |only the hub chain need|


Multi Chain IBC architecture
----------------------------

<p align="center">
  <img src="./hub.svg">
</p>


Noun definition
---------------
 - **hub chain**:  
   In multi chain environment, hub chain is the central chain of inter-chain communication, which support the hub protocol.
 - **parallel chain**:  
   In multi chain environment, chains connected to the hub chain are parallel chains.
 - **hub account**:  
   on the hub chain, a hub account is needed to complete the hub protocol, and the hub account can't be the same with ibc.token contract account.  
   In order to query the hub-trxs in blockchain browser conveniently, we need that the hub account must exist. 
   The token transferred through the hub protocol is first transferred to the hub chain, then divided into two situations:
   1) If this token is not the native token of the hub chain, then these tokens will be transferred to the hub account 
   under the ibc.token contract of the hub chain, because this pegged token is kept in the ibc.token contract, 
   so the hub account will follow the ibc.token contract rules, anyone can transfer the token under the hub account, 
   but only to the target account of the target chain specified in the first stage hub-trx, or after the timeout, 
   to the original account of the original chain. So in this case, we only need the hub to be an existing account, 
   and we don't care about its permissions. And you can see the transfer to the hub account in the browser.
   2) If the token is the native token of the hub chain, then these tokens will be transferred to the `ibc.token contract account` 
   under the <native token contract> of the hub chain, rather than transfer to the hub account. 
   However, these tokens are originally under the ibc.token account, therefore, there is no need for real transfer operation, 
   so in this case, we only need the hub account to be an existing account without caring about its authorities. 
   In this case, because there is no real transfer action, the transfer to the hub account cannot be found in the browser.  
   **To sum up**, we only need the existence of the hub account and don't care who its authority belongs to. 
   However, in order to avoid causing unnecessary worry to users, we suggest that you resign the authority of the hub account to `eosio`.
 - **hub-trx/hub-protocol**:  
   trx is the short form of transaction. When an ibc-trx use the hub feature, it becomes a hub-trx.
 - **hub-trx-first-phase/user-protocol**:  
   A hub-trx needs to go through two phases, which are two protocols, we call the first phase as `user-protocol`, 
   because it's triggered by the user.
 - **hub-trx-seconed-phase/worker-protocol**: 
   as above, we call the second phase as `worker-protocol`, because it's triggered automatically by the worker.
 - **chain native token**:  
   Any token that is originally issued on this chain, not only contains the chain system token.
 - **chain_x/chain_y**:  
   represent two parallel chains.
 - **TOX/TOY**:  
   native token symbols of chain_x and chain_y.
   
Shell variables definition
--------------------------
``` shell
cleos_a='cleos -u <end point of chain_a>'
cleos_b='cleos -u <end point of chain_b>'
cleos_c='cleos -u <end point of chain_c>'
cleos_d='cleos -u <end point of chain_d>'
contract_token=ibc.io  # the account deployed ibc.token contract
hub_account=hub.io
```

Compile / deploy / initialize
-----------------------------
The ibc.token contract supports hub protocol from v4, so both hub chain and all parallel chains need to use the v4+ ibc.token contract. 
*Note that because v3 and v4 use different contract table structure definitions, 
they cannot be upgraded directly by deploying the v4 ibc.token contract,
please refer to [Upgrade_v3_to_v4.md](./Upgrade_v3_to_v4.md) for detailed upgrade process.

In addtion, we upgraded [ibc_plugin_bos](https://github.com/boscore/ibc_plugin_bos) to v4 to provide funcitons to complete worker-protocol automatically.

#### Build
The contract(ibc.token) compilation command for hub chain is:
```shell
$ ./build.sh bos.cdt HUB_PROTOCOL=ON    # or compile with eosio.cdt
$ ./build.sh eosio.cdt HUB_PROTOCOL=ON
```

The contract(ibc.token) compilation command for parallel chains is:
```shell
$ ./build.sh bos.cdt  # or compile with eosio.cdt
$ ./build.sh eosio.cdt
```
In fact, when compiling the ibc.token contract for parallel chains, you can also take the option HUB_PROTOCOL=ON, 
but this will add a lot of logic related to the hub operation, which is not needed on the parallel chains, so this option is not recommended.

#### Deployment
You should refer to [Deployment_and_Test.md](./Deployment_and_Test.md) for the whole deployment process.

#### Hub Init
You need to initialize the related parameters of the ibc.token contract on the hub chain to enable the hub function,
the command is as follows:
```shell
$cleos_a push action ${contract_token} hubinit "[$hub_account]" -p ${contract_token}
```

Token Registration
------------------
If a token wants to use the hub protocol, it needs to be registered in table `accepts` of the ibc.token contract on the hub chain. 
That is to say, for a native token of the parallel chains, it needs to be registered twice in the ibc.token contract on the hub chain,
once in the table `stats`, once in the table `accetps`, 
**To make the operation easier, we provide a convenient action `regpegtoken2`, which can register pegged token in two tables in one time.**
Any other parallel chain that wants to accept this token also needs to register this token. 
Let's take chain_b's system token as an example and register it to the hub chain and all other parallel chains.

assume that all token's administrator account is ibc2token555

#### 1) register TOB to table `accepts` of chain_b
```shell
$cleos_b push action ${contract_token} regacpttoken \
    '["eosio.token","1000000000.0000 TOB","1.0000 TOB","100000.0000 TOB",
    "1000000.0000 TOB",1000,"chain_b organization","htttp://www.chain_b.io","ibc2token555",
    "fixed","0.1000 TOB",0.01,"0.1000 TOB",true]' -p ${contract_token}
```

#### 2) register TOB to table `stats` and `accepts` of chain_a
 1) by one action `regpegtoken2`
    ```shell
    $cleos_a push action ${contract_token} regpegtoken2 \
        '["chb","eosio.token",,"1000000000.0000 TOB","1.0000 TOB","10000.0000 TOB",
        "1000000.0000 TOB",1000,"chain_b organization","https://www.chain_b.io","ibc2token555",
        "fixed","0.1000 TOB",0.01,"0.1000 TOB",true]' -p ${contract_token}
    ```
 2) or by actions `regpegtoken` and `regacpttoken`
    ```shell
    # --- register TOB to table stats of chain_a --- 
    $cleos_a push action ${contract_token} regpegtoken \
        '["chb","eosio.token","1000000000.0000 TOB","1.0000 TOB","100000.0000 TOB",
        "1000000.0000 TOB",1000,"ibc2token555","0.1000 TOB",true]' -p ${contract_token}
    
    # --- register TOB to table accepts of chain_a --- 
    $cleos_a push action ${contract_token} regacpttoken \
        '['${contract_token}',"1000000000.0000 TOB","1.0000 TOB","10000.0000 TOB",
        "1000000.0000 TOB",1000,"chain_b organization","https://www.chain_b.io","ibc2token555",
        "fixed","0.1000 TOB",0.01,"0.1000 TOB",true]' -p ${contract_token}
    ```

#### 3) register TOB to table `stats` of chain_c and chain_d
```
$cleos_c push action ${contract_token} regpegtoken \
    '["cha",'${contract_token}',"1000000000.0000 TOB","1.0000 TOB","100000.0000 TOB",
    "1000000.0000 TOB",1000,"ibc2token555","0.1000 TOB",true]' -p ${contract_token}

$cleos_d push action ${contract_token} regpegtoken \
    '["cha",'${contract_token}',"1000000000.0000 TOB","1.0000 TOB","100000.0000 TOB",
    "1000000.0000 TOB",1000,"ibc2token555","0.1000 TOB",true]' -p ${contract_token}
```
token TOB can use the hub protocol only if it is registered in the table 'accept' of the hub chain, 
and can only transfer TOB successfully to the parallel chains that have registered TOB as pegtoken.

Only use ibc protocol
---------------------
Let's say that you have a token TOX on parallel chain B (*this token can be any native token of parallel chains or the hub chain*), 
and you want to transfer it to parallel chain C, you have two options 1) not use the hub protocol, 2) use the hub protocol.
if you don't use the hub protocol,this requires that you have an account of your own on the hub chain (for example, chaina2acnt1), 
you need to transfer the token to chaina2acnt1 on chain_a through the IBC protocol first, 
and then transfer the token on chaina2acnt1 of chain_a to destination account on chain_c through the IBC protocol. 
In the whole process, users need to perform two operations, one on chain_b and one on chain_a.

Suppose you want transfer 100.0000 TOB from chain_b's account chainb2acnt1 to chain_c's account chainc2acnt1.
```shell
# --- transfer TOB from chain_b to chain_a ---
$cleos_b push action -f eosio.token  transfer '["chainb2acnt1","ibc2token555","100.0000 TOB" "chaina2acnt1@cha additional string"]' -p chainb2acnt1

# --- transfer TOB from chain_a to chain_c ---
$cleos_a push action -f ${contract_token} transfer '["chaina2acnt1","ibc2token555","100.0000 TOB" "chainc2acnt1@chc additional string"]' -p chaina2acnt1
```

HUB Protocol - User Protocol
----------------------------
Only token transfers between parallel chains need the hub protocol, the advantages of using hub protocol are 
1) users can transfer token from one parallel chain to another parallel chain without having an account on the hub chain, 
2）users only need to operate on the initial parallel chain once, and the subsequent operation is completed by the trustless worker program automatically.

protocol definition:
```shell
cleos_s='cleos -u <end point of one parallel chain>'
$cleos_s push action ${contract} transfer [from ${ibc_token_contract} quantity 
         ”${hub_account}@c{hub_chain_short_name} >> ${dest_account}@${dest_chain_short_name} <additional string> ] -p from
```
 - **${contract}**  
Among them, ${contract} is the originally issued contract of the token to be transferred, 
and ${contract} can be ${ibc_token_contract} itself if the token is a pegtoken, 
or other token contracts if the token is originally issued on this parallel chain.

 - **${ibc_token_contract}**  
the account that deployed ibc.token contract.

 - **${hub_account}**  
the hub account on hub chain.

 - **${hub_chain_short_name}**  
hub chain's short name, such as "cha".

 - **${dest_account}**  
destination account.

 - **${dest_chain_short_name}**  
destination chain short name, such as "chc".

Example:
Suppose you want to transfer 100.0000 TOB on chain_b account chainb2acnt1 to chain_c account chainc2acnt1.
```shell
$cleos_b push action eosio.token transfer "[chainb2acnt1, ibc.io, '100.0000 TOB' 'hub.io@cha >> chainc2acnt1@chc additional string']" -p chainb2acnt1
```

#### Special symbol description
The `@` `>>` and `=` symbols can have spaces at both sides, but they are not required, so the following two memo strings are equivalent
```shell
"hub.io @ cha >> chainc2acnt1@  chc key1=value1 key2 = value2 additional string"
"hub.io@cha>>chainc2acnt1@chc key1=value1 key2=value2 additional string"
```

HUB Protocol - Worker Protocol
-----------------------------
The whole hub protocol is divided into two phases. The first phase is the user protocol, which is triggered by the user, 
the second phase is the worker protocol, which can be triggered by anyone with any account's authoriy. 
(we added related logic in [ibc_plugin_bos](https://github.com/boscore/ibc_plugin_bos),
 which can automatically complete the second phase of hub protocol).

When users transfer tokens from one parallel chain to another through the hub protocol, 
these tokens will first be transferred to the hub account of the hub chain 
(if the original chain of the token is hub chain, it will be transferred to the IBC. Token contract account of the hub chain), 
and then a worker is needed to transfer these tokens to the target parallelchain. 
When a hub TRX completes the first stage, within two minutes, 
worker can only transfer the token that stays on the hub account to the destination account of the destination chain. 
After more than two minutes, worker can transfer the token that stays on the hub account to the original account of the original chain.

protocol definition:
```shell
cleos_h='cleos -u <end point of hub chain>'
$cleos_h push action ${ibc_token_contract} transfer "[${hub_account}, ${ibc_token_contract},${quantity},
    "${dest_account}@${dest_chain_short_name} orig_trx_id=${orig_trx_id} worker=${worker} addtional string"]" -p ${any authority}
```

 - **${ibc_token_contract}**  
   the account that deployed ibc.token contract.
   
 - **${hub_account}**  
   the hub account
   
 - **${quantity}**  
   In hub chain's ibc.token contract‘s table `hubtrx`, the maximum and minimum values of quantity are recorded, 
   so the quantity value here must be within this range, and transaction fee equal to maximum minus quantity, 
   and will transfer to the ${worker} if it's appointed in the memo string by worker=${worker}.
   
 - **${dest_account}**  
   destination account.
   
 - **${dest_chain_short_name}**  
   destination chain short name, such as "chc".
   
 - **${orig_trx_id}**  
   In hub chain's ibc.token contract‘s table `hubtrx`, the original transaction id is recored, the value muse equal
   to the record.
   
 - **${worker}**  [optional]  
   Any existing account can be specified. When the hub-trx is completed, the transaction fee will be transferred to the account.
   
 - **${any authority}**  
   you can appoint it to any account of which you have its authority

We still use the scenario described in the previous section, so worker completes the operation as follows:

```shell
$cleos_a push action ibc2token555  transfer '["hub.io","ibc2token555","99.9000 TOB"
"chainc2acnt1@chc orig_trx_id=151b40701f48f4d0df0a924de8ab046340f8b6f8f68d5f7edeed04835bd5aae3 worker=chaina2acnt2  notes infomation"]' -p myaccount
```

Complete examples
-----------------

please refer to [ibc_test_env/task_ibc_test_v4.sh](https://github.com/boscore/ibc_test_env/blob/master/task_ibc_test_v4.sh) 
for a complete test case.


Safety
------
Security is the premise. The hub protocol is based on the IBC protocol,
 and the hub protocol can prevent the double-spend attack, and the holder of the hub account can't do evil.

Internal special protocol
-------------------------
In order to allow the native token of hub chain (take TOA as an example) to use the hub protocol, 
that is, you can directly transfer TOA from one parallel chain to another parallel chain through the hub protocol, 
which requires parallel chains to accept that the following two actions on the hub chain are equivalent,
only in this way can anyone be allowed to execute the second phase of the hub protocol

```shell
$cleos_a push action eosio.token transfer [ hub.ibc <to> '1.0000 TOA' <memo>] -p bosibc.io
$cleos_a push action bosibc.io   transfer [ hub.ibc <to> '1.0000 TOA' <memo>] -p bosibc.io
```
