Upgrade_v3_to_v4
----------------


In v4, the definitions of tables `accept` and `stats` have been modified, 
so we need to delete these two tables first, then deploy ibc.token v4 contract and set the corresponding values of the tables.

#### Step 1: get the two tables' contents and record them down
``` 
$cleos get table <ibc_token_contract> <ibc_token_contract> accepts
$cleos get table <ibc_token_contract> <ibc_token_contract> stats
```

#### Step 2: set special ibc.token contract
check out branch `upgrade_v3_to_v4`, compile and set code to ibc.token contract.
this branch add a new action:
```code
   void token::deltokentbl(  ){
      while ( _accepts.begin()  != _accepts.end()   ){ _accepts.erase(_accepts.begin());   }
      while ( _stats.begin()    != _stats.end()     ){ _stats.erase(_stats.begin());       }
   }
```

#### Step 3: delete the two tables
``` 
cleos push action <ibc_token_contract> deltokentbl '[]' -p <some_account>
```


#### Step4: set v4 ibc.token contract


#### Step5: register token
register accepts and pegtokens again use the data obtained previously
``` 
cleos push action <ibc_token_contract> regacpttoken '[....]' ...
cleos push action <ibc_token_contract> regpegtoken '[....]' ...
```

