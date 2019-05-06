ibc.chain
-----------

This contract implements eosio blockchain lightweight client, it is also called SPV client in [Bitcoin](https://bitcoin.org/bitcoin.pdf)
combine with merkle path validation, this lightweight client can be used to verify transaction existence.

### Core Concept
In a blockchain with POW consensus, such as BTC and ETH, starting from a block number, 
all block headers thereafter must be passed to the lightwight-client, then we can get a trusted light client. 
However, eosio uses DPOS consensus mechanism, so we don't need to pass all headers continually to get trusted
headers, as long as we pass a bunch of continuous headers, then we can get trusted points.
In order to maintain the trusted evidence chain of the light client, 
the most critical thing is to maintain the replacement of the BPs schedule.

The two tables `chaindb` and `sections` are combined to form the core data structures of the contract, and `section` is
the core concept. A section consists of a bunch of continuous block headers (such as block header from 10000 to 10300,
then this section contains 301 block headers), the section record in table `sections` do not store block headers data 
directly, just store information about these continuous headers, specific headers' data are stored in the `chaindb` table.

section_type's definition as bellow: 
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
    if all headers's new_producers in this section is empty, newprod_block_num will always be zero.  
    Note: This member is only used in a EOSIO blcokchains with Pipeline BFT consensus (such as EOS mainnet), 
    it's not used in a EOSIO blockchain with PBFT consensus (such as BOSCore with LIB boost).
 -  `valid`  
    this is a very importand member, only when this value is true, then irreversible headers in this section can be 
    used to verify cross-chain transactions. Under what circumstances will valid become true? there are two cases:  
    - There is no BPs schedule change in this section  
    Then, When the section's length (length = last - first + 1) greater then lib_depth, the value becomes true.
    - There is BPs schedule change in this section  
    No mater the current valid's value is ture or not, whenever a header contains new_producers be added to this section, 
    the valid value of this section becomes false, the next process is a BPs schedule replacement process, 
    and only after this process is over can valid be set to true.
 -  `producers` and `block_nums`  
    these two data are used to record BP changes in the block generation process. 
    using these two data, it ensures that each BP can produce up to 12 blocks continuity, 
    and ensure that the last 15 BPs can not be repeated, this will prevent BPs from doing evil.

**between sections**  
There is no need for continuity between two sections.
Any number of headers can be spaced, but these neglected headers can not contain new_producers, 
that is to say, the process of BPs schedule replacement can not be ignored, 
and the BPs replacement process must be complete within a section, only in this way can the light-client work properly.

**lib_depth**  
In a completely de-centralized environment, this value should be 325. 
In the ibc version 1.* contract, this value is set to be less than 325 due to the use of relay account privileges 
and in order to reduce CPU and NET consumption, but in ibc version 2.*, it will be set to a constant of 325.

### Actions and Functions

 - `chaininit(...)`  
    Set the first block header of the light client, and all subsequent headers' validation is based on this header.
 - `pushsection(...)`  
    This function is called by ibc_plugin to create a new section, 
    or to add a bunch of continuous headers to an existing section.
 - `rmfirstsctn(...)`  
    This function is called repeatedly by ibc_plugin to delete old section and chaindb data,
    in order to save contract memory consumption.
 - `assert_block_in_lib_and_trx_mroot_in_block(...)`  
    This static function is called directly by the ibc.token contract to validate cross-chain transactions.

### Attack Dimensions and Security Scheme
Hackers may want to attack this contract in a variety of ways,
bellow are some possible ways of attacking they want to try and describe how this contract can defend against these attacks.

 -  One BP do evil with plugin account
    Attack mode: One BP and relay jointly do evil and push continuously headers generated by this BP.
    Defense: Each BP can produce up to 12 blocks continuity, more than 12 headers will assert failed.
    
 -  7 continuous BPs on current schedule do evil with plugin account
    If we set lib_depth >= 85 (12 * 7 + 1), then can defend against such attack.
 
 -  plugin account do evil
    As long as the initial block header of the ibc.chain contract is correct, 
    plugin cannot push headers that are not signed by BPs, 
    so the plugin either chooses push headers signed by BPs or strikes.

