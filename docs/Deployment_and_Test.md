
IBC System Deployment and Test
------------------------------

This article will detail how to build, deploy and use the IBC system, and take the deployment between `BOS miannet`
and `EOS mainnet` as an example. If you want to deploy IBC system between EOSIO blockchains, 
such as between `EOS mainnet` and `Kylin or Jungle testnet` or between other side/sister chains 
whose consensus algorithem didn't modified, you need not `ibc_plugin_bos`, 
only `ibc_plugin_eos` is needed; if you want to deploy between EOSIO chains 
whose underlying source code especially the consensus algorithem has been modified,
it's need to determine whether those chains can use the `ibc_contracts` and `ibc_plugin_eos` according to the actual situation.

Notes: 
- The current version of the Kylin test network is eosio-v1.8, at present, 
BOS IBC does not support eosio-v1.8, eosio-v1.7 and previous versions are supported.
- Current BOS mainnt's consensus algorithem is batch-pbft, it's diffrrent with EOS mainnet's pipeline-pbft.


### Build
- Build contracts  
  Please refer to [ibc_contracts](https://github.com/boscore/ibc_contracts).
  
- Build ibc_plugin_eos and ibc_plugin_bos  
  Please refer to [ibc_plugin_eos](https://github.com/boscore/ibc_plugin_eos) and [ibc_plugin_bos](https://github.com/boscore/ibc_plugin_bos).
  
### Create Accounts
four accounts need to be registered on each blockchain， one relay account for ibc_plugin, and two contract accounts 
for deploying IBC contracts, let's assume that the three accounts on both chains are:   
**ibc3relay333**: used by ibc_plugin, and need a certain amount of cpu and net.   
**ibc3chain333**: used to deploy ibc.chain contract, and need a certain amount of ram, such as 5Mb.  
**ibc3token333**: used to deploy ibc.token contract, and need a certain amount of ram, such as 10Mb.  
**freeaccount1**: inter-blockchain transfer with this account is free of charge and is used for health monitoring of the IBC system in production environment.
  
### Configure and Start Relay Nodes
Suppose that the IP address and port of the relay node of Kylin testnet are 192.168.1.101:5678, node of BOS testnet are
192.168.1.102:5678

Some key configurations and ibc-related configurations are listed below. 

- configure relay node of Kylin testnet  
``` 
max-transaction-time = 1000
contracts-console = true

# ------------ ibc related configures ------------ #
plugin = eosio::ibc::ibc_plugin

ibc-listen-endpoint = 0.0.0.0:5678
#ibc-peer-address = 192.168.1.102:5678 

ibc-token-contract = ibc3token333
ibc-chain-contract = ibc3chain333
ibc-relay-name = ibc3relay333
ibc-relay-private-key = <the pulic key of ibc3relay333>=KEY:<the private key of ibc3relay333>

ibc-peer-private-key = <ibc peer public key>=KEY:<ibc peer private key>

# ------------------- p2p peers -------------------#
p2p-peer-address = <Kylin testnet p2p endpoint>
```

- configure relay node of BOS testnet  
``` 
max-transaction-time = 1000
contracts-console = true

# ------------ ibc related configures ------------ #
plugin = eosio::ibc::ibc_plugin

ibc-listen-endpoint = 0.0.0.0:5678
ibc-peer-address = 192.168.1.101:5678 

ibc-token-contract = ibc3token333
ibc-chain-contract = ibc3chain333
ibc-relay-name = ibc3relay333
ibc-relay-private-key = <the pulic key of ibc3relay333>=KEY:<the private key of ibc3relay333>

ibc-peer-private-key = <ibc peer public key>=KEY:<ibc peer private key>

# ------------------- p2p peers -------------------#
p2p-peer-address = <BOS testnet p2p endpoint>
```

- relay nodes's log  
When the relay nodes of two chains started, they try to connect with each other, 
and check the ibc contracts on their respective chains, and print the logs according to the IBC contracts status.


### Deploy and Init IBC Contracts
- ibc.chain and ibc.token contracts on EOS mainnet  
```bash
cleos1='cleos -u <EOS mainnet http endpoint>'
contract_chain=ibc3token333
contract_token=ibc3chain333
contract_token_pubkey='<public key of contract_token>'

$cleos1 set contract ${contract_chain} <contract_chain_folder> -x 1000 -p ${contract_chain}
$cleos1 set contract ${contract_token} <contract_token_folder> -x 1000 -p ${contract_token}

$cleos1 push action ${contract_chain} setglobal '["bos","<bos mainnet chain_id>","batch"]' -p ${contract_chain}

$cleos1 set account permission ${contract_token} active '{"threshold": 1, "keys":[{"key":"'${contract_token_pubkey}'", "weight":1}], "accounts":[ {"permission":{"actor":"'${contract_token}'","permission":"eosio.code"},"weight":1}], "waits":[] }' owner -p $ {contract_token}
$cleos1 push action ${contract_token} setglobal '["eos",true]' -p ${contract_token}
$cleos1 push action ${contract_token} regpeerchain '["bos","https://boscore.io","ibc3token333","ibc3chain333","freeaccount1",5,1000,1000,true]' -p ${contract_token}
```

- ibc.chain and ibc.token contracts on BOS testnet  
```bash
cleos2='cleos -u <BOS mainnet http endpoint>'
contract_chain=ibc3token333
contract_token=ibc3chain333
contract_token_pubkey='<public key of contract_token>'

$cleos2 set contract ${contract_chain} <contract_chain_folder> -x 1000 -p ${contract_chain}
$cleos2 set contract ${contract_token} <contract_token_folder> -x 1000 -p ${contract_token}

$cleos1 push action ${contract_chain} setglobal '["eos","<eos mainnet chain_id>","pipeline"]' -p ${contract_chain}

$cleos2 set account permission ${contract_token} active '{"threshold": 1, "keys":[{"key":"'${contract_token_pubkey}'", "weight":1}], "accounts":[ {"permission":{"actor":"'${contract_token}'","permission":"eosio.code"},"weight":1}], "waits":[] }' owner -p $ {contract_token}
$cleos1 push action ${contract_token} setglobal '["bos",true]' -p ${contract_token}
$cleos1 push action ${contract_token} regpeerchain '["eos","https://eos.io","ibc3token333","ibc3chain333","freeaccount1",5,1000,1000,true]' -p ${contract_token}
```

After initializing the two contracts, the relay nodes of two chains will start synchronizing the first block header of 
peer chain as the genesis block header of ibc.chain contract, then synchronize a batch of block headers.


### Register Token
Please refer to detailed content [Token_Registration_and_Management](./Token_Registration_and_Management.md)

- register `EOS Token` of EOS mainnet in `ibc.token contract on both EOS and BOS mainnet`   
```bash
eos_admin=ibc3admin333

$cleos1 push action ${contract_token} regacpttoken \
    '["eosio.token","4,EOS","4,EOSPG","1000000000.0000 EOS","10.0000 EOS","5000.0000 EOS",
    "100000.0000 EOS",1000,"eos organization","https://eos.io","ibc3admin333","fixed","0.1000 EOS",0.01,"0.1000 EOS",true]' -p ${contract_token}
        
$cleos2 push action ${contract_token} regpegtoken \
    '["eos","eosio.token","4,EOS","4,EOSPG","1000000000.0000 EOSPG","10.0000 EOSPG","5000.0000 EOSPG",
    "100000.0000 EOSPG",1000,"ibc3admin333","0.1000 EOSPG",true]' -p ${contract_token} 
```

- register `BOS Token` of BOS mainnet in `ibc.token contract on both EOS and BOS mainnet`   
```bash
bos_admin=ibc3admin333

$cleos2 push action ${contract_token} regacpttoken \
    '["eosio.token","4,BOS","4,BOSPG","1000000000.0000 BOS","10.0000 BOS","5000.0000 BOS",
    "100000.0000 BOS",1000,"bos organization","https://boscore.io","ibc3admin333","fixed","0.1000 BOS",0.01,"0.1000 BOS",true]' -p ${contract_token}
    
$cleos1 push action ${contract_token} regpegtoken \
    '["bos","eosio.token","4,BOS","4,BOSPG","1000000000.0000 BOSPG","10.0000 BOSPG","5000.0000 BOSPG",
    "100000.0000 BOSPG",1000,"ibc3admin333","0.1000 BOSPG",true]' -p ${contract_token}
```

After performing the above steps, you need to wait for a period of time, preferably more than 6 minutes, and watch the logs of the two relay nodes.
You can see a log like the one below. It is better to wait for the last part of the following log to change from false to true for the first time before sending inber-blockchain transactions.

``` 
ibc_plugin.cpp:3168           start_ibc_heartbeat_ ] origtrxs_table_id_range [0,0] cashtrxs_table_seq_num_range [0,0] new_producers_block_num 0, lwcls_range [75468428,75469424,true]

```

### Send IBC Transactions

- create some accounts on both chains  
```bash
eos_acnt1=eosaccount11  # account on Kylin testnet
eos_acnt2=eosaccount12  # account on Kylin testnet
bos_acnt1=bosaccount11  # account on BOS testnet
bos_acnt2=bosaccount12  # account on BOS testnet
```

- send EOS form Kylin testnet ${eos_acnt1} to BOS testnet ${bos_acnt1}  
```bash
$cleos1 transfer ${eos_acnt1} ${contract_token} "1.0000 EOS" ${bos_acnt1}"@bos this is a ibc transaction" -p ${eos_acnt1}
```

- withdraw EOS(peg token) form BOS testnet ${bos_acnt1} to Kylin testnet ${eos_acnt2}  
```bash
$cleos2 push action ${contract_token} transfer '["'${bos_acnt1}'","'${contract_token}'","1.0000 EOS" "'${eos_acnt2}'@eos"]' -p ${bos_acnt1}
```

- send BOS form BOS testnet ${bos_acnt1} to Kylin testnet ${eos_acnt1}  
```bash
$cleos2 transfer ${bos_acnt1} ${contract_token} "1.0000 BOS" ${eos_acnt1}"@eos this is a ibc transaction" -p ${bos_acnt1}
```

- withdraw BOS(peg token) form Kylin testnet ${eos_acnt1} to BOS testnet ${bos_acnt2}  
```bash
$cleos1 push action ${contract_token} transfer '["'${eos_acnt1}'","'${contract_token}'","1.0000 BOS" "'${bos_acnt2}'@bos"]' -p ${eos_acnt1}
```
After executing the commands, you need to wait for a period of time to receive your token on the peer chain.
It takes about six minutes to transfer from EOS to BOS, and transfer from BOS to EOS takes about 10 seconds.

