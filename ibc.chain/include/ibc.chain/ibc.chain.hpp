/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <ibc.chain/block_header.hpp>
#include <ibc.chain/pbft.hpp>
#include <ibc.chain/merkle.hpp>

namespace eosio {

   const static uint32_t section_max_length = 1000;   // used under pipeline consensus algorithm
   const static uint32_t lib_depth = 325;             // don't modify, used under pipeline consensus algorithm
   const static uint32_t prodsches_max_records = 5;
   const static uint32_t sections_max_records = 5;
   const static uint32_t producer_repetitions = 12;   // don't modify
   const static uint32_t chaindb_max_history_length = 60;   // uint: minutes

   const static bool     check_relay_auth = true;

   struct [[eosio::table("global"), eosio::contract("ibc.chain")]] global_state {
      name              chain_name;       // the original chain name, ibc_plugin uses this name to interact with the ibc.token contract
      chain_id_type     chain_id;
      name              consensus_algo;   // pipeline or batch

      EOSLIB_SERIALIZE( global_state, (chain_name)(chain_id)(consensus_algo) )
   };
   typedef eosio::singleton< "global"_n, global_state > global_state_singleton;

   struct [[eosio::table("globalm"), eosio::contract("ibc.chain")]] global_mutable {
      global_mutable(){}
      uint32_t    last_anchor_block_num = 0;

      EOSLIB_SERIALIZE( global_mutable, (last_anchor_block_num) )
   };
   typedef eosio::singleton< "globalm"_n, global_mutable > global_mutable_singleton;

   struct [[eosio::table("chaindb"), eosio::contract("ibc.chain")]] block_header_state {
      uint64_t                   block_num;
      block_id_type              block_id;
      signed_block_header        header;
      uint32_t                   active_schedule_id;
      uint32_t                   pending_schedule_id;
      incremental_merkle         blockroot_merkle;
      capi_public_key            block_signing_key;   // redundant, used for make signature verification faster
      bool                       is_anchor_block = false;

      uint64_t primary_key()const { return block_num; }

      EOSLIB_SERIALIZE( block_header_state, (block_num)(block_id)(header)(active_schedule_id)(pending_schedule_id)
                                            (blockroot_merkle)(block_signing_key)(is_anchor_block) )
   };
   typedef eosio::multi_index< "chaindb"_n, block_header_state > chaindb;

   struct [[eosio::table("prodsches"), eosio::contract("ibc.chain")]] producer_schedule_type {
      uint64_t                      id;
      producer_schedule             schedule;
      digest_type                   schedule_hash;

      uint64_t primary_key()const { return id; }

      EOSLIB_SERIALIZE( producer_schedule_type, (id)(schedule)(schedule_hash) )
   };
   typedef eosio::multi_index< "prodsches"_n, producer_schedule_type >  prodsches;

   struct [[eosio::table("sections"), eosio::contract("ibc.chain")]] section_type {
      uint64_t                first;
      uint64_t                last;
      uint64_t                newprod_block_num = 0;  /// 0 means all headers' header.new_producers in this section is empty
      bool                    valid = false;
      std::vector<name>       producers;
      std::vector<uint32_t>   block_nums;

      uint64_t primary_key()const { return first; }
      
      /// important function, used to prevent attack
      void add( name producer, uint32_t num, uint32_t tslot = 0, const producer_schedule& sch = producer_schedule() ); 
      void clear_from( uint32_t num );

      EOSLIB_SERIALIZE( section_type, (first)(last)(newprod_block_num)(valid)(producers)(block_nums) )
   };
   typedef eosio::multi_index< "sections"_n, section_type >  sections;

   struct [[eosio::table("relays"), eosio::contract("ibc.chain")]] relay_account {
      name    relay;

      uint64_t primary_key()const { return relay.value; }

      EOSLIB_SERIALIZE( relay_account, (relay) )
   };
   typedef eosio::multi_index< "relays"_n, relay_account > relays;

   class [[eosio::contract("ibc.chain")]] chain : public contract {
   private:
      global_state_singleton     _global_state;
      global_state               _gstate;
      global_mutable_singleton   _global_mutable;
      global_mutable             _gmutable;
      chaindb                    _chaindb;
      prodsches                  _prodsches;
      sections                   _sections;
      relays                     _relays;

   public:
      chain( name s, name code, datastream<const char*> ds );
      ~chain();

      [[eosio::action]]
      void setglobal( name              chain_name,
                      chain_id_type     chain_id,
                      name              consensus_algo );

      [[eosio::action]]
      void chaininit( const std::vector<char>&     header,
                      const producer_schedule&     active_schedule,
                      const incremental_merkle&    blockroot_merkle,
                      const name&                  relay );

      // push a batch of blocks, called by ibc plugin, used under pipeline consensus algorithm
      [[eosio::action]]
      void pushsection( const std::vector<char>&    headers,
                        const incremental_merkle&   blockroot_merkle,
                        const name&                 relay );

      // remove first section data, called by ibc plugin repeatedly, used under pipeline consensus algorithm
      [[eosio::action]]
      void rmfirstsctn( const name& relay );

      [[eosio::action]]
      void relay( string action, name relay );

      // push a small batch of blocks and related commits, called by ibc plugin, used under batch consensus algorithm
      [[eosio::action]]
      void pushblkcmits( const std::vector<char>&    headers,
                         const incremental_merkle&   blockroot_merkle,
                         const std::vector<char>&    proof_data,
                         const name&                 proof_type,
                         const name&                 relay );

      /**
       * Very important function, used by other contracts to verifying transactions
       */
      static void assert_anchor_block_and_merkle_node( const name&          ibc_chain_contract,
                                                       const uint32_t&      block_num,
                                                       const uint32_t&      layer,
                                                       const digest_type&   digest ) {
         chaindb _chaindb( ibc_chain_contract, ibc_chain_contract.value );
         eosio_assert( _chaindb.begin() != _chaindb.end(), (string("_chaindb of scpoe: ") + ibc_chain_contract.to_string() + " not exist").c_str());
         auto bhs = _chaindb.get( block_num );
         eosio_assert( bhs.is_anchor_block, (string("block ") + std::to_string(block_num) + " is not anchor block").c_str());
         eosio_assert( is_equal_capi_checksum256( get_inc_merkle_node_by_layer(bhs.blockroot_merkle,layer), digest ), "checksum256 not equal");
      }

      /**
       * Very important function, used by other contracts to verifying transactions
       */
      static void assert_anchor_block_and_transaction_mroot( const name&          ibc_chain_contract,
                                                             const uint32_t&      block_num,
                                                             const digest_type&   transaction_mroot ) {
         chaindb _chaindb( ibc_chain_contract, ibc_chain_contract.value );
         eosio_assert( _chaindb.begin() != _chaindb.end(), (string("_chaindb of scpoe: ") + ibc_chain_contract.to_string() + " not exist").c_str());
         auto bhs = _chaindb.get( block_num );
         eosio_assert( bhs.is_anchor_block, (string("block ") + std::to_string(block_num) + " is not anchor block").c_str());
         eosio_assert( is_equal_capi_checksum256( bhs.header.transaction_mroot, transaction_mroot ), "provided transaction_mroot not correct");
      }

      static bool is_relay( name ibc_contract_account, name check ) {
         relays _relays( ibc_contract_account, ibc_contract_account.value );
         auto it = _relays.find( check.value );
         return it != _relays.end();
      }

      // this action maybe needed when repairing the ibc system manually
      [[eosio::action]]
      void forceinit( );

   private:
      // pipeline pbft related
      void new_section( const signed_block_header& header, const incremental_merkle& blockroot_merkle );
      void append_header( const signed_block_header& header );
      uint32_t get_section_last_active_schedule_id( const section_type& section ) const;
      bool remove_invalid_last_section( );
      void trim_last_section_or_not( );

      // batch pbft related
      void push_header( const signed_block_header& header,
                        const incremental_merkle& blockroot_merkle = incremental_merkle() );

      // common
      void remove_header_if_exist( uint32_t block_num );

      // producer schedule related
      capi_public_key   get_public_key_by_producer( uint64_t id, const name& producer ) const;
      name              get_producer_by_public_key( uint64_t id, const capi_public_key& public_key ) const;

      // producer signature related
      digest_type       bhs_sig_digest( const block_header_state& hs ) const;
      void              assert_producer_signature( const digest_type& digest,
                                                   const capi_signature& signature,
                                                   const capi_public_key& pub_key ) const;
      capi_public_key   get_public_key_form_signature( digest_type digest, signature_type sig ) const;

      bool only_one_eosio_bp();

      void require_relay_auth( const name& relay );
   };

} /// namespace eosio
