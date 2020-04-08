
ibc.token upgrade from v3 to v4
-------------------------------
All `parallel chain`s and `hub chain` should upgrade their `ibc.token` contract to v4.

Contents
--------
 * [EOSIO mainnet ibc.token upgrade process](#1-eosio-mainnet-ibctoken-upgrade-process)
 * [BOSCORE mainnet ibc.token upgrade process](#2-boscore-mainnet-ibctoken-upgrade-process)
 * [TELOS mainnet ibc.token upgrade process](#3-telos-mainnet-ibctoken-upgrade-process)
 

**Note: In v3 and v4 versions, the definitions of tables `accepts` and `stats` are different, 
so you can't directly use command `cleos set contract ...` to update the ibc.token contract, 
you must first delete these two tables completely, then deploy a new contract(with new table structure definition), 
and then restore the information of the two tables. 
Therefore, the first step is to record the information of the tables, 
the second step is to delete the tables, 
the third step is to deploy a new table ( with new table structure definition), and restore the information,
and because the action used for restore is a special action (not used in normal contract), so the fourth step is required to deploy the final contract.**
 

### 1. EOSIO mainnet ibc.token upgrade process

#### step 1 : get tables info

bash variables
```
cleos_eos='cleos -u http://peer1.eoshuobipool.com:8181'
base_dir=/Users/song/Code/github.com/boscore/bos.contract-prebuild/bosibc
```

get table accepts info
```
$cleos_eos get table bosibc.io bosibc.io accepts

{
  "rows": [{
      "original_contract": "eosio.token",
      "peg_token_symbol": "4,EOS",
      "accept": "6462.3427 EOS",
      "max_accept": "1000000000.0000 EOS",
      "min_once_transfer": "0.2000 EOS",
      "max_once_transfer": "1000000.0000 EOS",
      "max_daily_transfer": "10000000.0000 EOS",
      "max_tfs_per_minute": 100,
      "organization": "block one",
      "website": "https://eos.io",
      "administrator": "eostkadmin33",
      "service_fee_mode": "fixed",
      "service_fee_fixed": "0.0100 EOS",
      "service_fee_ratio": "0.00000000000000000",
      "failed_fee": "0.0010 EOS",
      "total_transfer": "8818.6665 EOS",
      "total_transfer_times": 4339,
      "total_cash": "2356.3238 EOS",
      "total_cash_times": 1531,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1582381949,
        "minute_trxs": 1,
        "daily_tf_start": 1582381949,
        "daily_tf_sum": "1.0000 EOS",
        "daily_wd_start": 0,
        "daily_wd_sum": "0 "
      }
    },{
      "original_contract": "eosiotptoken",
      "peg_token_symbol": "4,TPT",
      "accept": "34196.6781 TPT",
      "max_accept": "5900000000.0000 TPT",
      "min_once_transfer": "100.0000 TPT",
      "max_once_transfer": "10000000.0000 TPT",
      "max_daily_transfer": "100000000.0000 TPT",
      "max_tfs_per_minute": 50,
      "organization": "TokenPocket",
      "website": "https://tokenpocket.pro",
      "administrator": "itokenpocket",
      "service_fee_mode": "fixed",
      "service_fee_fixed": "10.0000 TPT",
      "service_fee_ratio": "0.00000000000000000",
      "failed_fee": "1.0000 TPT",
      "total_transfer": "42918.0000 TPT",
      "total_transfer_times": 39,
      "total_cash": "8721.3219 TPT",
      "total_cash_times": 9,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1585889295,
        "minute_trxs": 1,
        "daily_tf_start": 1585889295,
        "daily_tf_sum": "100.0000 TPT",
        "daily_wd_start": 0,
        "daily_wd_sum": "0 "
      }
    },{
      "original_contract": "tethertether",
      "peg_token_symbol": "4,USDT",
      "accept": "6.0354 USDT",
      "max_accept": "1000000000000.0000 USDT",
      "min_once_transfer": "0.1000 USDT",
      "max_once_transfer": "100000.0000 USDT",
      "max_daily_transfer": "10000000.0000 USDT",
      "max_tfs_per_minute": 200,
      "organization": "bitfinex",
      "website": "https://www.eosfinex.com",
      "administrator": "eostkadmin33",
      "service_fee_mode": "fixed",
      "service_fee_fixed": "0.0100 USDT",
      "service_fee_ratio": "0.00000000000000000",
      "failed_fee": "0.0050 USDT",
      "total_transfer": "9.6633 USDT",
      "total_transfer_times": 9,
      "total_cash": "3.6279 USDT",
      "total_cash_times": 2,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1574636436,
        "minute_trxs": 1,
        "daily_tf_start": 1574636436,
        "daily_tf_sum": "1.0000 USDT",
        "daily_wd_start": 0,
        "daily_wd_sum": "0 "
      }
    }
  ],
  "more": false,
  "next_key": ""
}
```

get table stats info
``` 
$cleos_eos get table bosibc.io bosibc.io stats

{
  "rows": [{
      "supply": "0.0000 UB",
      "max_supply": "10000000000.0000 UB",
      "min_once_withdraw": "1.0000 UB",
      "max_once_withdraw": "50000.0000 UB",
      "max_daily_withdraw": "500000.0000 UB",
      "max_wds_per_minute": 50,
      "administrator": "ubibcmanager",
      "peerchain_name": "bos",
      "peerchain_contract": "unicorntoken",
      "orig_token_symbol": "4,UB",
      "failed_fee": "0.1000 UB",
      "total_issue": "1.0000 UB",
      "total_issue_times": 1,
      "total_withdraw": "1.0000 UB",
      "total_withdraw_times": 1,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1567673870,
        "minute_trxs": 1,
        "daily_isu_start": 0,
        "daily_isu_sum": "0 ",
        "daily_wd_start": 1567673870,
        "daily_wd_sum": "1.0000 UB"
      }
    },{
      "supply": "130817.4718 BOS",
      "max_supply": "1000000000.0000 BOS",
      "min_once_withdraw": "0.2000 BOS",
      "max_once_withdraw": "1000000.0000 BOS",
      "max_daily_withdraw": "10000000.0000 BOS",
      "max_wds_per_minute": 100,
      "administrator": "bostkadmin33",
      "peerchain_name": "bos",
      "peerchain_contract": "eosio.token",
      "orig_token_symbol": "4,BOS",
      "failed_fee": "0.0010 BOS",
      "total_issue": "253534.8747 BOS",
      "total_issue_times": 9355,
      "total_withdraw": "122717.4029 BOS",
      "total_withdraw_times": 4489,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1586242619,
        "minute_trxs": 1,
        "daily_isu_start": 0,
        "daily_isu_sum": "0 ",
        "daily_wd_start": 1586182509,
        "daily_wd_sum": "1.2000 BOS"
      }
    }
  ],
  "more": false,
  "next_key": ""
}
```


#### step 2 : deploy special ibc.token contracts and clear tables
check out branch `upgrade_v3_to_v4` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt` 
or use pre-build contract on [bos.contract-prebuild/bosibc/upgrade_v3_to_v4](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc/upgrade_v3_to_v4/ibc.token).
``` 
ibc_contracts_dir=${base_dir}/upgrade_v3_to_v4
$cleos_eos set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io
$cleos_eos push action bosibc.io deltokentbl '[]' -p bosibc.io
```

check if tables has cleared
``` 
$cleos_eos get table bosibc.io bosibc.io accepts
$cleos_eos get table bosibc.io bosibc.io stats
```

#### step 3 : deploy another special ibc.token contract and add data to tables
check out branch `upgrade_v3_to_v4_step2` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt` 
or use pre-build contract on [bos.contract-prebuild/bosibc/upgrade_v3_to_v4_step2](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc/upgrade_v3_to_v4_step2/ibc.token).
``` 
ibc_contracts_dir=${base_dir}/upgrade_v3_to_v4_step2
$cleos_eos set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io
$cleos_eos push action bosibc.io addacpttoken \
     '["eosio.token","1000000000.0000 EOS","0.2000 EOS","1000000.0000 EOS",
     "10000000.0000 EOS",100,"block one","https://eos.io","eostkadmin33","fixed","0.0100 EOS",0.0,"0.0100 EOS",true,"6462.3427 EOS","8818.6665 EOS","2356.3238 EOS"]' -p bosibc.io

$cleos_eos push action bosibc.io addacpttoken \
     '["eosiotptoken","5900000000.0000 TPT","100.0000 TPT","10000000.0000 TPT",
     "100000000.0000 TPT",50,"TokenPocket","https://tokenpocket.pro","itokenpocket","fixed","10.0000 TPT",0.0,"1.0000 TPT",true,"34196.6781 TPT","42918.0000 TPT","8721.3219 TPT"]' -p bosibc.io

$cleos_eos push action bosibc.io addacpttoken \
     '["tethertether","1000000000000.0000 USDT","0.1000 USDT","100000.0000 USDT",
     "10000000.0000 USDT",200,"bitfinex","https://www.eosfinex.com","eostkadmin33","fixed","0.0100 USDT",0.0,"0.0050 USDT",true,"6.0354 USDT","9.6633 USDT","3.6279 USDT"]' -p bosibc.io


$cleos_eos push action bosibc.io addpegtoken \
        '["bos","eosio.token","1000000000.0000 BOS","0.2000 BOS","1000000.0000 BOS",
        "10000000.0000 BOS",100,"bostkadmin33","0.0010 BOS",true,"130817.4718 BOS","253534.8747 BOS","122717.4029 BOS"]' -p bosibc.io

$cleos_eos push action bosibc.io addpegtoken \
        '["bos","unicorntoken","10000000000.0000 UB","1.0000 UB","50000.0000 UB",
        "500000.0000 UB",50,"ubibcmanager","0.1000 UB",true,"0.0000 UB","1.0000 UB","1.0000 UB"]' -p bosibc.io

```

check if tables had added
``` 
$cleos_eos get table bosibc.io bosibc.io accepts
$cleos_eos get table bosibc.io bosibc.io stats
```

#### step 4 : deploy v4 ibc.token contract
check out branch `master` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt` 
or use pre-build contract on [bos.contract-prebuild/bosibc](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc).

```
ibc_contracts_dir=${base_dir}
$cleos_eos set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io
```

#### step 5 : register pegged tokens (hub)
```
$cleos_eos push action bosibc.io regpegtoken \
     '["bos","bosibc.io","10000000000.0000 TLOS","1.0000 TLOS","1000000.0000 TLOS","10000000.0000 TLOS",50,"bostkadmin33","0.0100 TLOS",true]' -p bosibc.io
```



### 2. BOSCORE mainnet ibc.token upgrade process

#### step 1 : set global and get info

bash variables
```
cleos_bos='cleos -u http://bosapi.tokenpocket.pro'
```

get table accepts info
```
$cleos_bos get table bosibc.io bosibc.io accepts
{
  "rows": [{
      "original_contract": "eosio.token",
      "peg_token_symbol": "4,BOS",
      "accept": "131111.6159 BOS",
      "max_accept": "1000000000.0000 BOS",
      "min_once_transfer": "0.2000 BOS",
      "max_once_transfer": "1000000.0000 BOS",
      "max_daily_transfer": "10000000.0000 BOS",
      "max_tfs_per_minute": 100,
      "organization": "boscore",
      "website": "https://boscore.io",
      "administrator": "bostkadmin33",
      "service_fee_mode": "fixed",
      "service_fee_fixed": "0.0100 BOS",
      "service_fee_ratio": "0.00000000000000000",
      "failed_fee": "0.0010 BOS",
      "total_transfer": "253607.3988 BOS",
      "total_transfer_times": 9368,
      "total_cash": "122495.7829 BOS",
      "total_cash_times": 4483,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1586256230,
        "minute_trxs": 1,
        "daily_tf_start": 1586198735,
        "daily_tf_sum": "2.2000 BOS",
        "daily_wd_start": 0,
        "daily_wd_sum": "0 "
      }
    },{
      "original_contract": "unicorntoken",
      "peg_token_symbol": "4,UB",
      "accept": "0.0100 UB",
      "max_accept": "10000000000.0000 UB",
      "min_once_transfer": "1.0000 UB",
      "max_once_transfer": "50000.0000 UB",
      "max_daily_transfer": "500000.0000 UB",
      "max_tfs_per_minute": 50,
      "organization": "Unico",
      "website": "https://unico.one",
      "administrator": "ubibcmanager",
      "service_fee_mode": "fixed",
      "service_fee_fixed": "0.0100 UB",
      "service_fee_ratio": "0.00000000000000000",
      "failed_fee": "0.0050 UB",
      "total_transfer": "1.0000 UB",
      "total_transfer_times": 1,
      "total_cash": "0.9900 UB",
      "total_cash_times": 1,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1567673490,
        "minute_trxs": 1,
        "daily_tf_start": 1567673490,
        "daily_tf_sum": "1.0000 UB",
        "daily_wd_start": 0,
        "daily_wd_sum": "0 "
      }
    }
  ],
  "more": false
}
```

get table stats info
``` 
$cleos_bos get table bosibc.io bosibc.io stats

{
  "rows": [{
      "supply": "6388.0549 EOS",
      "max_supply": "1000000000.3000 EOS",
      "min_once_withdraw": "0.2000 EOS",
      "max_once_withdraw": "100000.0000 EOS",
      "max_daily_withdraw": "1000000.0000 EOS",
      "max_wds_per_minute": 100,
      "administrator": "eostkadmin33",
      "peerchain_name": "eos",
      "peerchain_contract": "eosio.token",
      "orig_token_symbol": "4,EOS",
      "failed_fee": "0.0010 EOS",
      "total_issue": "8816.0987 EOS",
      "total_issue_times": 4321,
      "total_withdraw": "2428.0438 EOS",
      "total_withdraw_times": 1531,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1576568251,
        "minute_trxs": 1,
        "daily_isu_start": 0,
        "daily_isu_sum": "0 ",
        "daily_wd_start": 1576568251,
        "daily_wd_sum": "0.3562 EOS"
      }
    },{
      "supply": "33856.6781 TPT",
      "max_supply": "5900000000.0000 TPT",
      "min_once_withdraw": "100.0000 TPT",
      "max_once_withdraw": "10000000.0000 TPT",
      "max_daily_withdraw": "100000000.0000 TPT",
      "max_wds_per_minute": 50,
      "administrator": "itokenpocket",
      "peerchain_name": "eos",
      "peerchain_contract": "eosiotptoken",
      "orig_token_symbol": "4,TPT",
      "failed_fee": "1.0000 TPT",
      "total_issue": "42918.0000 TPT",
      "total_issue_times": 39,
      "total_withdraw": "9061.3219 TPT",
      "total_withdraw_times": 9,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1582872776,
        "minute_trxs": 1,
        "daily_isu_start": 0,
        "daily_isu_sum": "0 ",
        "daily_wd_start": 1582872776,
        "daily_wd_sum": "2192.3066 TPT"
      }
    },{
      "supply": "0.0000 TLOS",
      "max_supply": "10000000000.0000 TLOS",
      "min_once_withdraw": "1.0000 TLOS",
      "max_once_withdraw": "1000000.0000 TLOS",
      "max_daily_withdraw": "10000000.0000 TLOS",
      "max_wds_per_minute": 1000,
      "administrator": "bostkadmin33",
      "peerchain_name": "tlos",
      "peerchain_contract": "eosio.token",
      "orig_token_symbol": "4,TLOS",
      "failed_fee": "0.1000 TLOS",
      "total_issue": "2.0000 TLOS",
      "total_issue_times": 2,
      "total_withdraw": "2.0000 TLOS",
      "total_withdraw_times": 2,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1584889259,
        "minute_trxs": 1,
        "daily_isu_start": 0,
        "daily_isu_sum": "0 ",
        "daily_wd_start": 1584889259,
        "daily_wd_sum": "1.0000 TLOS"
      }
    },{
      "supply": "6.0154 USDT",
      "max_supply": "1000000000000.0000 USDT",
      "min_once_withdraw": "0.1000 USDT",
      "max_once_withdraw": "100000.0000 USDT",
      "max_daily_withdraw": "10000000.0000 USDT",
      "max_wds_per_minute": 200,
      "administrator": "eostkadmin33",
      "peerchain_name": "eos",
      "peerchain_contract": "tethertether",
      "orig_token_symbol": "4,USDT",
      "failed_fee": "0.0050 USDT",
      "total_issue": "9.6633 USDT",
      "total_issue_times": 9,
      "total_withdraw": "3.6479 USDT",
      "total_withdraw_times": 2,
      "active": 1,
      "mutables": {
        "minute_trx_start": 1573738457,
        "minute_trxs": 1,
        "daily_isu_start": 0,
        "daily_isu_sum": "0 ",
        "daily_wd_start": 1573738457,
        "daily_wd_sum": "3.4467 USDT"
      }
    }
  ],
  "more": false
}
```

#### step 2 : deploy special ibc.token contracts and clear tables
check out branch `upgrade_v3_to_v4` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt`
or use pre-build contract on [bos.contract-prebuild/bosibc/upgrade_v3_to_v4](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc/upgrade_v3_to_v4/ibc.token).
``` 
ibc_contracts_dir=${base_dir}/upgrade_v3_to_v4
$cleos_bos set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io
$cleos_bos push action bosibc.io deltokentbl '[]' -p bosibc.io
```

check if tables has cleared
``` 
$cleos_bos get table bosibc.io bosibc.io accepts
$cleos_bos get table bosibc.io bosibc.io stats
```

#### step 3 : deploy another special ibc.token contract and set tables
check out branch `upgrade_v3_to_v4_step2` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt`
or use pre-build contract on [bos.contract-prebuild/bosibc/upgrade_v3_to_v4_step2](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc/upgrade_v3_to_v4_step2/ibc.token).
``` 
ibc_contracts_dir=${base_dir}/upgrade_v3_to_v4_step2
$cleos_bos set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io
$cleos_bos push action bosibc.io addacpttoken \
     '["eosio.token","1000000000.0000 BOS","0.2000 BOS","1000000.0000 BOS",
     "10000000.0000 BOS",100,"boscore","https://boscore.io","bostkadmin33","fixed","0.0100 BOS",0.0,"0.0010 BOS",true,"131111.6159 BOS","253607.3988 BOS","122495.7829 BOS"]' -p bosibc.io

$cleos_bos push action bosibc.io addacpttoken \
     '["unicorntoken","10000000000.0000 UB","1.0000 UB","50000.0000 UB",
     "500000.0000 UB",50,"Unico","https://unico.one","ubibcmanager","fixed","1.0000 UB",0.0,"0.0050 UB",true,"0.0100 UB","1.0000 UB","0.9900 UB"]' -p bosibc.io


$cleos_bos push action bosibc.io addpegtoken \
        '["eos","eosio.token","1000000000.0000 EOS","0.2000 EOS","100000.0000 EOS",
        "1000000.0000 EOS",100,"eostkadmin33","0.0010 EOS",true,"6388.0549 EOS","8816.0987 EOS","2428.0438 EOS"]' -p bosibc.io

$cleos_bos push action bosibc.io addpegtoken \
        '["eos","eosiotptoken","5900000000.0000 TPT","100.0000 TPT","10000000.0000 TPT",
        "100000000.0000 TPT",50,"itokenpocket","1.0000 TPT",true,"33856.6781 TPT","42918.0000 TPT","9061.3219 TPT"]' -p bosibc.io

$cleos_bos push action bosibc.io addpegtoken \
        '["eos","tethertether","1000000000000.0000 USDT","0.1000 USDT","100000.0000 USDT",
        "10000000.0000 USDT",50,"eostkadmin33","0.0050 USDT",true,"6.0154 USDT","9.6633 USDT","3.6479 USDT"]' -p bosibc.io

$cleos_bos push action bosibc.io addpegtoken \
        '["tlos","eosio.token","10000000000.0000 TLOS","1.0000 TLOS","1000000.0000 TLOS",
        "10000000.0000 TLOS",50,"bostkadmin33","0.1000 TLOS",true,"0.0000 TLOS","2.0000 TLOS","2.0000 TLOS"]' -p bosibc.io

```

check if tables had added
``` 
$cleos_bos get table bosibc.io bosibc.io accepts
$cleos_bos get table bosibc.io bosibc.io stats
```

#### step 4 : deploy v4 hub ibc.token contract
check out branch `master` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt HUB_PROTOCOL=ON`
or use pre-build contract on [bos.contract-prebuild/bosibc/v4.0.0_hub_ibc_token](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc/v4.0.0_hub_ibc_token).

```
ibc_contracts_dir=${base_dir}/v4.0.0_hub_ibc_token
$cleos_bos set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io
```

set hub globals
``` 
$cleos_bos push action bosibc.io hubinit '["hub.io"]' -p bosibc.io
```

register pegged tokens to accetps table to support hub transaction. 
``` 

$cleos_bos push action bosibc.io regacpttoken \
    '["bosibc.io","1000000000.0000 EOS","0.2000 EOS","100000.0000 EOS",
    "1000000.0000 EOS",100,"block one","https://eos.io","bostkadmin33","fixed","0.0100 EOS",0.0,"0.0100 EOS",true]' -p bosibc.io

$cleos_bos push action bosibc.io regacpttoken \
    '["bosibc.io","1000000000000.0000 USDT","0.1000 USDT","100000.0000 USDT",
    "10000000.0000 USDT",200,"bitfinex","https://www.eosfinex.com","bostkadmin33","fixed","0.0100 USDT",0.0,"0.0050 USDT",true]' -p bosibc.io

$cleos_bos push action bosibc.io regacpttoken \
    '["bosibc.io","10000000000.0000 TLOS","1.0000 TLOS","1000000.0000 TLOS",
    "10000000.0000 TLOS",50,"telos","https://www.telosfoundation.io","bostkadmin33","fixed","0.1000 TLOS",0.0,"0.0100 TLOS",true]' -p bosibc.io

$cleos_bos push action bosibc.io regacpttoken \
    '["bosibc.io","5900000000.0000 TPT","100.0000 TPT","10000000.0000 TPT",
    "100000000.0000 TPT",50,"TokenPocket","https://tokenpocket.pro","itokenpocket","fixed","10.0000 TPT",0.0,"1.0000 TPT",true]' -p bosibc.io

```

check table accepts
``` 
$cleos_bos get table bosibc.io bosibc.io accepts
```


### 3. TELOS mainnet ibc.token upgrade process

#### step 1 : set global and get info

bash variables
```
cleos_tls='cleos -u '
```

get table accepts info
```
$cleos_tls get table bosibc.io bosibc.io accepts



```

get table stats info
``` 
$cleos_tls get table bosibc.io bosibc.io stats


```

#### step 2 : deploy special ibc.token contracts and clear tables
check out branch `upgrade_v3_to_v4` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt`
or use pre-build contract on [bos.contract-prebuild/bosibc/upgrade_v3_to_v4](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc/upgrade_v3_to_v4/ibc.token).
``` 
ibc_contracts_dir=${base_dir}/upgrade_v3_to_v4
$cleos_tls set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io
$cleos_tls push action bosibc.io deltokentbl '[]' -p bosibc.io
```

check if tables has cleared
``` 
$cleos_tls get table bosibc.io bosibc.io accepts
$cleos_tls get table bosibc.io bosibc.io stats
```

#### step 3 : deploy another special ibc.token contract and set tables
check out branch `upgrade_v3_to_v4_step2` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt`
or use pre-build contract on [bos.contract-prebuild/bosibc/upgrade_v3_to_v4_step2](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc/upgrade_v3_to_v4_step2/ibc.token).
``` 
ibc_contracts_dir=${base_dir}/upgrade_v3_to_v4_step2
$cleos_tls set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io






```

check if tables had added
``` 
$cleos_tls get table bosibc.io bosibc.io accepts
$cleos_tls get table bosibc.io bosibc.io stats
```

#### step 4 : deploy v4 ibc.token contract
check out branch `master` of [ibc_contracts](https://github.com/boscore/ibc_contracts) project and build with command `./build.sh bos.cdt` 
or use pre-build contract on [bos.contract-prebuild/bosibc](https://github.com/boscore/bos.contract-prebuild/tree/master/bosibc).

```
ibc_contracts_dir=${base_dir}
$cleos_eos set contract bosibc.io ${ibc_contracts_dir}/ibc.token -x 1000 -p bosibc.io
```

#### step 5 : register pegged tokens (hub)
```

```
