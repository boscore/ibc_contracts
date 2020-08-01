ibc.proxy
---------

In order to support sending inter-blockchain transactions through EOSIO `inline action` or `deferred transaction`, 
this proxy contract is needed as an intermediary agent, 
because the `ibc.token` contract cannot directly support `inline or deferred` inter-blockchain transactions.

If you need to send inter-blockchain transactions through `inline or deferred` action, 
you need to transfer your token to the proxy account instead of directly to the `ibc.token contract` account(**bosibc.io**).

When the token is transferred to the proxy account through `inline or deferred` transaction, 
anyone can use any account's authority to transfer the token to `ibc.token` contract(the ibc plugin will do this automatically with relay authority) , 
or transfer back to the original account after 2 minutes of the original transaction,

Same as the `ibc.token` contract, the `ibc.proxy` contract account's authority should also resign to multiple authoritative accounts, 
which can be the 21 active BP accounts.

Protocal Format
---------------

Assume:
`bosibc.io` is the account which depolied `ibc.token` contract.
`ibcproxy.io` is the account which depolied `ibc.proxy` contract.

assume you want to transfer token "10.0000 BOS" from account `bossender` of BOS mainnet to account `eosreceiver` of EOS mainnet through `inline or deferred` transaction,
you should send the inter-blockchain action as bellow:
``` 
    name to = "ibcproxy.io"_n;  // Note: Don't transfer to bosibc.io
    transfer_action_type action_data{ from, to, quantity, memo };
    action( permission_level{ _self, "active"_n }, token_contract, "transfer"_n, action_data ).send();
```
The memo string format is **{account_name}@{chain_name} {user-defined string}**. for more information refers to [User_Guide.md](../docs/User_Guide.md#2-transfer-action)
And the {user-defined string} length should not more then 64;

Then anyone or the relay accounts can send another transaction which contail only one action as bellow:
```
$cleos push action <ibc.proxy> transfer "[<ibc.proxy> <ibc.token> <quantity> <account@chain user-defined-notes-string orig_trxid=<trx id>  orig_from=<original from>>]"
That is:
$cleos push action ibcproxy.io transfer "[ibcproxy.io bosibc.io "10.0000 BOS" <eosreceiver@eos user-defined-notes-string orig_trxid=<trx id>  orig_from=bossender>]"
```

**<quantity>** must equal to the original quantity user tansfers to ibcproxy.io;  
**orig_trxid** must be the user triggerd original transaction's id;   
**orig_from** must be the original transfer action's `from` account, 
this parameter is used to tell `ibc.token` the original from account, 
inorder to convenient returning the token to the original account instead of the proxy account when rollback ibc transactions;   

If the above transaction fails for some reason, the token needs to be transferred back to the original account:
```
$cleos push action <ibc.proxy> transfer '[<ibc.proxy> <from> <quantity> "orig_trxid=<trx id>"]'
That is:
$cleos push action ibcproxy.io transfer '[ibcproxy.io bossender "10.0000 BOS" "orig_trxid=<trx id>"]'
```
