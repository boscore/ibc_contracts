BOSIBC Cmdline Demo
--------------------------------
## Cmdlines to register a parallel chain and register the system TOKEN in IBC ecosystem
## 完整注册一个平行链以及注册该链系统币到IBC生态

```
# take YAS as the example
p_chain="yas"
origin_contract_act="eosio.token"
organization="enumivo.org"
website="https://enumivo.org"
max_accept="100000000.0000 YAS"
min_once_transfer="1.0000 YAS"
max_once_transfer="1000000.0000 YAS"
max_daily_transfer="10000000.0000 YAS"
max_tfs_per_minute=600
service_fee_fixed="0.1000 YAS"
failed_fee="0.0100 YAS"

ibc_free_act="ibcfreeacunt"

bos_admin_act="bosibc4tkadm"
wax_admin_act="bosibc4tkadm"
yas_admin_act="bosibc4tkadm"
tlos_admin_act="bosibc4tkadm"
eos_admin_act="bosibc4tkadm"

service_fee_ratio=0.01
service_fee_mode="fixed"
contract_token="bosibc.io"

# infomation for BOS token
bos_organization="boscore.io"
bos_website="https://boscore.io"
bos_max_accept="1000000000.0000 BOS"
bos_min_once_transfer="0.2000 BOS"
bos_max_once_transfer="1000000.0000 BOS"
bos_max_daily_transfer="10000000.0000 BOS"
bos_max_tfs_per_minute=600
bos_service_fee_fixed="0.1000 BOS"
bos_failed_fee="0.0100 BOS"

# relay and chain info
relay_act="bosibc5relay"
contract_chain="ibc5chainyas"
contract_token_pubkey="EOS5wvjaxA86Xk9w8ZHkoSjBqHLPUSWbXAkGMnnyx29U7ssN9KK2Z"
bos_cid="d5a3d18fbb3c084e3b1f3fa98c21014b5f3db536cc15d08f9f6479517c6a3d86"
yas_cid="ed8636abfe625d99fc9a759d49a016fd8dcae9193676a020aae2540c9fffe32f"
```

## These commands are called in parallel chain
## 在平行链上面执行

### Note:  

The API reference can be found at:
- [ibc.token](https://github.com/boscore/ibc_contracts/tree/master/ibc.token)
- [ibc.chain](https://github.com/boscore/ibc_contracts/tree/master/ibc.chain)

```
yascleos set contract bosibc.io ibc.token
yascleos set contract ${contract_chain} ibc.chain
yascleos push action ${contract_chain} relay  '["add",'${relay_act}']' -p ${contract_chain}

yascleos push action ${contract_chain} setglobal '["bos",'${bos_cid}',"batch"]' -p ${contract_chain}
yascleos set account permission ${contract_token} active '{"threshold": 1, "keys":[{"key":"'${contract_token_pubkey}'", "weight":1}], "accounts":[ {"permission":{"actor":"'${contract_token}'","permission":"eosio.code"},"weight":1}], "waits":[] }' owner -p ${contract_token}

yascleos push action ${contract_token} setglobal '['${p_chain}',true]' -p ${contract_token}
params='["bos","'${bos_organization}'","bosibc.io","'${contract_chain}'","'${ibc_free_act}'",5,1000,1000,true]'
yascleos push action ${contract_token} regpeerchain "${params}" -p ${contract_token}

# set administrator before the permission resign
yascleos push action ${contract_token} setadmin '['${yas_admin_act}']' -p ${contract_token}
yascleos push action ${contract_chain} setadmin '['${yas_admin_act}']' -p ${contract_chain}
```

## These commands are called in hub chain, BOSCore 
## 在HUB链BOSCore上面执行
```
boscleos set contract ${contract_chain} ibc.chain
boscleos push action ${contract_chain} relay  '["add",'${relay_act}']' -p ${bos_admin_act}

boscleos push action ${contract_chain} setglobal '['${p_chain}','${yas_cid}',"pipeline",true,1]' -p ${bos_admin_act} 
params='["'${p_chain}'","'${website}'","bosibc.io","'${contract_chain}'","'${ibc_free_act}'",5,1000,1000,true]'
boscleos push action ${contract_token} regpeerchain "${params}" -p ${bos_admin_act} 

# set free fee account for hub contract
boscleos push action ${contract_token} setfreeacnt '['${p_chain}','${ibc_free_act}']' -p ${bos_admin_act} 
```

## Register the parallel chain Token 
## 注册平行链系统代币

```
# call regacpttoken() in parallel chain
params='["'${origin_contract_act}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'", "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${organization}'","'${website}'","'${yas_admin_act}'","'${service_fee_mode}'","'${service_fee_fixed}'",'${service_fee_ratio}',"'${failed_fee}'",true]'
yascleos push action ${contract_token} regacpttoken "${params}" -p ${contract_token}


# in BOS，call regpegtoken() & regacpttoken()
params='["yas","'${origin_contract_act}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${bos_admin_act}'","'${failed_fee}'",true]'
boscleos push action ${contract_token} regpegtoken "${params}" -p ${bos_admin_act} 

params='["'${contract_token}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${organization}'","'${website}'","'${bos_admin_act}'","'${service_fee_mode}'","'${service_fee_fixed}'",'${service_fee_ratio}',"'${failed_fee}'",true]'
boscleos push action ${contract_token} regacpttoken "${params}" -p ${bos_admin_act}

# YAS
params='["bos","eosio.token","'${bos_max_accept}'","'${bos_min_once_transfer}'","'${bos_max_once_transfer}'",
    "'${bos_max_daily_transfer}'",'${bos_max_tfs_per_minute}',"'${bos_admin_act}'","'${bos_failed_fee}'",true]'
yascleos push action ${contract_token} regpegtoken "${params}" -p ${contract_token} 

# if something wrong, we can call unregtoken() to clean and retry again
#yascleos push action bosibc.io unregtoken '["all","BOS"]' -p bosibc.io

# EOS
params='["bos","bosibc.io","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${eos_admin_act}'","'${failed_fee}'",true]'
cleos push action ${contract_token} regpegtoken "${params}" -p ${eos_admin_act} 

# TLOS
params='["bos","bosibc.io","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${tlos_admin_act}'","'${failed_fee}'",true]'
teloscleos push action ${contract_token} regpegtoken "${params}" -p ${tlos_admin_act} 

# WAX
params='["bos","bosibc.io","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${wax_admin_act}'","'${failed_fee}'",true]'
waxcleos push action ${contract_token} regpegtoken "${params}" -p ${wax_admin_act} 
```

Above steps finish the registration of parallel chain into EOSIO IBC ecosystem.  
上面的步骤就完成了注册一条新的平行链到 EOSIO IBC 生态。 

## Register token in EOSIO IBC ecosystem 
## 在 EOSIO IBC 生态中注册一个代币

Next, it will give command how to register a Token in parallel chain into EOSIO IBC ecosystem.  
接下来会给出例子，怎样注册一个平行链的代币到 EOSIO IBC 生态。

We will take of registion of `USDT` in other parallel chains as the example. 
`USDT` exist in EOS and the contract name is: `tethertether`

接下来会将 `USDT` 注册到其他平行链来作为例子，`USDT` 存在于 EOS 上面，合约地址为 `tethertether`.

```
# constant variables
ibc_contract="bosibc.io"
ibc_register="bosibc4tkadm"
default_admin="bosibc4tkadm"
hub_chain="bos"

# variable assignment need to change for different token
peerchain_name="eos"
peerchain_contract="tethertether"
organization="bitfinex"
website="https://www.eosfinex.com"
max_accept="3321000000.0000 USDT"
min_once_transfer="1.0000 USDT"
max_once_transfer="100000000.0000 USDT"
max_daily_transfer="1000000000.0000 USDT"
max_tfs_per_minute=100
service_fee_fixed="0.1000 USDT"
failed_fee="0.0100 USDT"
service_fee_mode="fixed"
admin="${default_admin}"       # need to create this account in other parallel chains, otherwise you can use `default_admin`
service_fee_ratio=0.01


# Called in EOS
params='["'${peerchain_contract}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'", "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${organization}'","'${website}'","'${admin}'","'${service_fee_mode}'","'${service_fee_fixed}'",'${service_fee_ratio}',"'${failed_fee}'",true]'
cleos push action ${ibc_contract} regacpttoken "${params}" -p ${ibc_register}

# Called in BOS
params='["${peerchain_name}","'${peerchain_contract}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${admin}'","'${failed_fee}'",true]'
boscleos push action ${ibc_contract} regpegtoken "${params}" -p ${ibc_register} 

params='["'${ibc_contract}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${organization}'","'${website}'","'${admin}'","'${service_fee_mode}'","'${service_fee_fixed}'",'${service_fee_ratio}',"'${failed_fee}'",true]'
boscleos push action ${ibc_contract} regacpttoken "${params}" -p ${ibc_register}

# Called in WAX
params='["'${hub_chain}'","'${ibc_contract}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${admin}'","'${failed_fee}'",true]'
waxcleos push action ${ibc_contract} regpegtoken "${params}" -p ${admin}

# Called in TELOS
params='["'${hub_chain}'","'${ibc_contract}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${admin}'","'${failed_fee}'",true]'
teloscleos push action ${ibc_contract} regpegtoken "${params}" -p ${ibc_register} 

# Called in YAS
params='["'${hub_chain}'","'${ibc_contract}'","'${max_accept}'","'${min_once_transfer}'","'${max_once_transfer}'",
    "'${max_daily_transfer}'",'${max_tfs_per_minute}',"'${admin}'","'${failed_fee}'",true]'
yascleos push action ${ibc_contract} regpegtoken "${params}" -p ${ibc_register} 

```

Now you can start a testcase:  
下面就可以开启一个测试:

```
# transfer TPT from EOS to BOS
cleos push action ${peerchain_contract} transfer '[ "YOU_EOS_ACT", "bosibc.io","10.0000 TPT", "YOUR_ACT_BOS@bos Hello BOS" ]' -p YOU_EOS_ACT@active

# transfer TPT from EOS to TELOS
cleos push action ${peerchain_contract} transfer '[ "YOU_EOS_ACT", "bosibc.io","10.0000 TPT", "hub.io@bos >> YOUR_ACT_BOS@bos Hello TELOS" ]' -p YOU_EOS_ACT@active
```

## Resign the IBC account permission into eosio.prods
## 将 IBC 账户的权限指定给 eosio.prods
```
# for contract_token
yascleos set account permission ${contract_token} active '{"threshold": 1, "keys":[], "accounts":[ {"permission":{"actor":"'${contract_token}'","permission":"eosio.code"},"weight":1},{"permission":{"actor":"eosio.prods","permission":"active"},"weight":1}], "waits":[] }' owner -p ${contract_token}

yascleos set account permission ${contract_token} owner '{"threshold": 1, "keys":[], "accounts":[{"permission":{"actor":"eosio.prods","permission":"active"},"weight":1}], "waits":[] }' -p ${contract_token}@owner

# for contract_chain
yascleos set account permission ${contract_chain} active '{"threshold": 1, "keys":[], "accounts":[{"permission":{"actor":"eosio.prods","permission":"active"},"weight":1}], "waits":[] }' owner -p ${contract_chain}

yascleos set account permission ${contract_chain} owner '{"threshold": 1, "keys":[], "accounts":[{"permission":{"actor":"eosio.prods","permission":"active"},"weight":1}], "waits":[] }' -p ${contract_chain}@owner
```