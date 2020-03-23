ibc.chain
---------

This contract implements eosio blockchain lightweight client, it is also called SPV client in [Bitcoin](https://bitcoin.org/bitcoin.pdf)
combine with merkle path validation, this lightweight client can be used to verify transaction existence.

Actions called by the administrator
-------------------------------
#### setglobal( chain_name, chain_id, consensus_algo )
 - **chain_name**, The name of the original blockchain of this light client, such as `eos`, `bos`.
 - **chain_id**, chain id of the original blockchain of this light client.
 - **consensus_algo**, consensus algorithm of the original blockchain of this light client, 
   must be one of `pipeline` (represent DPOS pipeline bft consensus, such as the current EOSIO mainnet consensus) 
   and `batch` (represent DPOS batch pbft consensus, such as the new BOS 3 seconds LIB consensus).
 - require auth of _self
 
#### forceinit( )
 - three table ( _chaindb, _prodsches, _sections ) will be clear.
 - this action is needed when repairing the ibc system manually, 
   please refer to [TROUBLESHOOTING](../docs/Troubles_Shooting.md) for detailed IBC system recovery process.
 - require auth of _self

Actions called by ibc_plugin
----------------------------
#### chaininit( header, active_schedule, blockroot_merkle )
 - **header**, packed block header data.
 - **active_schedule**, the active_schedule of this block
 - **blockroot_merkle**, the blockroot_merkle of this block
 - set the first block header of the light client, the `genesis header` of the light client, 
   and all subsequent headers' validation is based on this header
 - this action is called by ibc_plugin once automatically
 - require auth of _self, or if light client not initialized, can be called with any account's auth

#### pushsection( headers, blockroot_merkle )
 - **headers**, packed a bunch of headers' data.
 - **blockroot_merkle**, the blockroot_merkle of the first block of `headers`
 - create a new section or add a bunch of continuous headers to an existing section
 - this action is called by ibc_plugin repeatedly as needed
 - can be called with any account's auth

#### rmfirstsctn( )
 - delete the old section and chaindb data in order to save contract memory consumption.
 - this function is called by ibc_plugin repeatedly as needed
 - can be called with any account's auth

Relay management
----------------
#### check_relay_auth
This parameter is hard coded. both ibc.chain and ibc.token contracts use this parameter to control whether to check the operation permission of a specific action. If it is set to `true`, function `require_relay_auth( name ibc_contract_account, name relay )`
will check the corresponding relay permission, if set to false, it will not be checked.  

Currently, the actions to check the relay permission include  
ibc.chain : `chaininit`,`pushsection`,`rmfirstsctn`,`pushblkcmits`  
ibc.token : `cash`,`rollback`,`rmunablerb`  

#### void relay( string action, name relay )
 - **action**, the string value must be **`add`** to add a relay to the relay set or **`remove`** to remove a relay, you can add multiple relays.
 - **relay**, the relay account.

#### void reqrelayauth( )
This action is used to facilitate the administrator to check the value of `check_relay_auth`, because this parameter is hard coded in the code and cannot be viewed through the contract table.

Resource requirement
--------------------
It's better to have not less than 5MB RAM for a light client of `pipeline` consensus blockchains,
and it's better to have not less than 2MB RAM for a light client of `batch` consensus blockchains.
Consumption of CPU and NET is very small.

Contract Design
---------------
### Core Concept
In a blockchain with POW consensus, such as BTC and ETH, in order to get a trusted light client, 
starting from a block number, all block headers thereafter must be passed to the light client. 

However, eosio uses DPOS consensus mechanism, so we don't need to pass all headers continually to get trusted
headers, as long as we pass a bunch of continuous headers, then we collect enough BPs validation, 
and then we can get trusted points.

More efficiently, in a blockchain of `batch` consensus (such as BOS), 
we only need one block header and corresponding BPs signatures, then we can confirm that this header is in LIB.

In order to maintain the trusted evidence chain of the light client, 
the most critical thing is to maintain the replacement of the BPs schedule.

The two tables `chaindb` and `sections` are combined to form the core data structures of this contract, and `section` is
the core concept. A section consists of a bunch of continuous block headers (such as block header from 10000 to 10300,
then this section contains 301 block headers), the section record in table `sections` do not store block headers data 
directly, just store information about these continuous headers, specific headers' data are stored in the `chaindb` table.

section_type's definition as below:
``` 
struct [[eosio::table("sections"), eosio::contract("ibc.chain")]] section_type {
  uint64_t                first;
  uint64_t                last;
  uint64_t                newprod_block_num = 0;  // the block's block_header contains new_producers, 0 means in this section all block_header have no new_producers
  bool                    valid = false;
  std::vector<name>       producers;
  std::vector<uint32_t>   block_nums;

  uint64_t primary_key()const { return first; }
  void add( name producer, uint32_t num, uint32_t tslot = 0, const producer_schedule& sch = producer_schedule() ); // important function, used to prevent attack
  void clear_from( uint32_t num );

  EOSLIB_SERIALIZE( section_type, (first)(last)(newprod_block_num)(valid)(producers)(block_nums) )
};
typedef eosio::multi_index< "sections"_n, section_type >  sections;
```
Members description:
 -  `first`  
    the first header's block number.
 -  `last`  
    the last header's block number.
 -  `newprod_block_num`   
    the header's block number whose member new_producers is not empty, 
    if all headers' new_producers in this section is empty, newprod_block_num will always be zero.  
 -  `valid`   
    this is a very important member, only when this value is true, then irreversible headers in this section can be 
    used to verify cross-chain transactions. Under what circumstances will valid become true? there are two cases:  
    - There is no BPs schedule change in this section  
    Then, When the section's length (length = last - first + 1) greater than lib_depth, the value becomes true.
    - There is BPs schedule change in this section  
    No mater the current valid's value is ture or not, whenever a header contains new_producers be added to this section, 
    the valid value of this section becomes false, the next process is a BPs schedule replacement process, 
    and only after this process is over, then valid can be set to true.
 -  `producers` and `block_nums`   
    these two data are used to record BP changes in the block generation process. 
    using these two data, it ensures that each BP can produce up to 12 blocks continuity, 
    and ensure that the last 15 BPs cannot be repeated, this will prevent BPs from doing evil.
 -  Note: `newprod_block_num`, `valid`, `producers` and `block_nums` are only used in a EOSIO blockchains with 
    Pipeline BFT consensus (such as EOS mainnet), in a EOSIO blockchain with PBFT 
    consensus (such as BOSCore with LIB boost), they are always the default values.

**between sections**  
There is no need for continuity between two sections.
Any number of headers can be spaced, but these neglected headers cannot contain new_producers, 
that is to say, the process of BPs schedule replacement cannot be ignored, 
and the BPs replacement process must be complete within a section, only in this way can the light-client work properly.

Functions
---------
 - `assert_anchor_block_and_transaction_mroot(...)`  
    This static function is called directly by the ibc.token contract to validate cross-chain transactions.

Attack Dimensions and Security Scheme
-------------------------------------
Hackers may want to attack this contract in a variety of ways,
below are some possible ways of attacking they want to try and describe how this contract can defend against these attacks.

 -  One BP collude with the relay to do evil
    Attack mode: One BP and the relay collude to do evil and push continuously headers generated by this BP.
    Defense: Each BP can only produce up to 12 blocks continuity, more than 12 headers will assert failed.
    
 -  less than 15 continuous BPs on current active schedule collude with a relay to do evil
    The lib_depth is a const number 325, this can defend against such attack.
 
 -  relay do evil
    As long as the initial block header of the ibc.chain contract is correct, 
    a relay cannot push headers that are not correctly signed by BPs, 
    so the relay either chooses to push headers signed by BPs or strikes.

