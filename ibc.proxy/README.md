ibc.proxy
---------
为了支持通过inline action或者defered的方式发送跨链交易，需要本合约作为中间代理，因为ibc.token合约无法直接支持inline action或defered的跨链交易。


需要通过inline 或者defered action 给ibc.token合约账户的交易（即transfer交易），需要将其token转发到本合约账户，而不是ibc.token合约账户，

ibc.proxy账户权限需要和ibc.token一样，resign给多个权威账户，可以是活跃的21个BP账户。


格式
----

假设部署ibc.token合约的账户是bosibc.io，部署ibc.proxy合约的账户是proxy2bosibc
使用inline action或者defered action 发送跨链交易的示例

``` 

```
transfer 某个`token`合约（假设为xxx.token合约，可以使bosibc.io）内发行的token
如果不适用ibc.proxy账户，your inline action or defered action should do like bellow 

`inline action`
``` 
call contract `xxx.token`'s action `transfer` with parameters `<from>  bosibc.io <quantity> <account@chain notes string>`
```


but this will fail directly for `ibc.token` contract cannot accept inline ibc actions/transactions and you should 
not send defered action/transactions in this way, for there is no related data in `block.log` so the ibc system will ignore it and your token will rollback to you finally.

so instead your inline action should do like bellow


`inline action`
``` 
call contract `xxx.token`'s action `transfer` with parameters `<from>  proxy2bosibc <quantity> <account@chain notes-string>`, 
```
then anyone or the relay accounts can send another transaction which contail only one action like bellow.

`Transaction`
```
call contract `proxy2bosibc`'s action `transfer` with parameters `proxy2bosibc bosibc.io <quantity> <account@chain notes-string  p_orig_trx_id=<trx id>  p_orig_from=<original from>  >`
```
p_orig_account 用于告诉ibc.token合约，原始账户是谁，以便ibc.token退款的时候直接退给该账户，而不需再经过proxy合约处理。

如果是转回


`Transaction`
```
call contract `proxy2bosibc`'s action `transfer` with parameters `proxy2bosibc <orig_from> <quantity> < p_orig_trx_id=<trx id> >`
```


ibc.token看到是来自proxy账户的转账，提取orig from，在orig_trx表中进行替换，需要在transfer_notify和withdraw两个表中进行修改。


when sended, remove record.

 p_orig_from=<original from> 这两个是给 ibc.token合约用的

### actions

transfer_notify(from some token's contract action transfer)

tansfer

rollback