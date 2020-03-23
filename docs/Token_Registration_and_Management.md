 Token Registration and Management
---------

The two blockchains connected by IBC channel are peer-to-peer chains, and IBC supports the transfer of assets 
from each chain to the peer chain. Any token must first be registered in a cross-chains contract (ibc.token) 
on both chains to use the IBC channel. Currently, the cross-chain contract (ibc.token) accounts provided by 
the BOSCORE team are **bosibc.io** on both the EOS and BOS mainnet. IBC system provides administrator function 
for each token to facilitate project owner management token, so project owners need to register administrator 
accounts on EOS and BOS mainnet respectively.

For the convenience of description, the following is illustrated by registering EOS mainnet token to enable 
it to circulate in BOS mainnet.

### 1. Requirements for token contracts
1. The definition of transfer action interface must be exactly the same as that of `transfer` interface of `eosio.token` contract;
2. The transfer function must contain the statement `require_recipient (to)`;

### 2. Terminological description
- Forward transfer
Token of EOS mainnet transfers to BOS mainnet through IBC channel.
- Reverse withdrawal
withdrawn back the peg token issued on the BOS mainnet to the EOS mainnet.

### 3. Projector owner need to provide the following information

- project name  
- project official website  
- token contract account on EOS mainnet  
- token's symbol  
  including symbol and precision  
- peg token symbol to be used in BOS mainnet  
  including symbols and precision, (on BOS, the peg token can use the same or different symbol with the original token)  
- administrator account in EOS mainnet  
- administrator account in BOS mainnet  
- maximum acceptance  
- minimum once forward transfer  
- maximum once forward transfer  
- maximum daily forward transfers  
- minimum once reverse withdrawal  
- maximum once reverse withdrawal  
- maximum daily reverse withdrawal  
- initial active state  

After the project owner provides the above information, the BOSCORE team will review the application, 
after it has been approved, BOCCORE team will register the token information on two chains.
After registration, this token can start using the IBC channel.

### 3. Administrator privileges

Suppose that a registered token contract account is "eosgoldtoken" and the token symbol is "4,GOLD".

*1. In the EOS mainnet `bosibc.io` contract (table:`accepts`)*
Token-related parameters that administrators can modify include:

- maximum acceptance  
  This amount shall be equal to or less than `max_supply` of the original token contract.
  The purpose of limiting the maximum acceptance quantity is to reduce the security risk.
```bash
$ cleos push action bosibc.io setacptasset '["GOLD","max_accept","1000000000.0000 GOLD"]' -p ${eos_admin}
```
- Minimum amount of once forward transfer  
  Note: This quota must be larger than the transaction fee set by the IBC project side.
```bash
$ cleos push action bosibc.io setacptasset '["GOLD","min_once_transfer","100.0000 GOLD"]' -p ${eos_admin}
```
- maximum once forward transfer  
```bash
$ cleos push action bosibc.io setacptasset '["GOLD","max_once_transfer","1000000.0000 GOLD"]' -p ${eos_admin}
```
- maximum daily forward transfers  
```bash
$ cleos push action bosibc.io setacptasset '["GOLD","max_daily_transfer","10000000.0000 GOLD"]' -p ${eos_admin}
```
- project name  
```bash
$ cleos push action bosibc.io setacptstr '["GOLD","organization","organization name"]' -p ${eos_admin}
```
- project official website  
```bash
$ cleos push action bosibc.io setacptstr '["GOLD","website","https://www.website.com"]' -p ${eos_admin}
```
- initial active state  
  when acitve is false, the contract will not accept this token's forward transfer, 
  but will not affect the reverse withdrawal.
```bash
$ cleos push action bosibc.io setacptstr '["GOLD","active",true]' -p ${eos_admin}
```

*2. In the BOS mainnet `bosibc.io` contract (table:`stats`)*
Token-related parameters that administrators can modify include:

- maximum supply  
  This value should be the same as the maximum acceptance value in the EOS mainnet `bosibc.io` contract.
```bash
$ cleos push action bosibc.io setpegasset '["GOLD","max_supply","1000000000.0000 GOLD"]' -p ${bos_admin}
```
- minimum once reverse withdrawal  
```bash
$ cleos push action bosibc.io setpegasset '["GOLD","min_once_withdraw","100.0000 GOLD"]' -p ${bos_admin}
```
- maximum once reverse withdrawal  
```bash
$ cleos push action bosibc.io setpegasset '["GOLD","max_once_withdraw","1000000.0000 GOLD"]' -p ${bos_admin}
```
- maximum daily reverse withdrawal  
```bash
$ cleos push action bosibc.io setpegasset '["GOLD","max_daily_withdraw","10000000.0000 GOLD"]' -p ${bos_admin}
```
- initial active state  
  when acitve is false, the contract will not accept this token's reverse withdrawal, 
  but will not affect the forward transfer.
```bash
$ cleos push action bosibc.io setpegbool '["GOLD","active",true]' -p ${bos_admin}
```
