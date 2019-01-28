/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

// #define FEW_BP_NODES_TEST // comment this define in release version

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <ibc.chain/block_header.hpp>
#include <ibc.chain/merkle.hpp>

namespace eosio {
   const static uint32_t section_max_length = 1000;
   const static uint32_t prodsches_max_records = 10;
   const static uint32_t sections_max_records = 5;
   const static uint32_t producer_repetitions = 12; // don't change this parameter

   struct [[eosio::table("chaindb"), eosio::contract("ibc.chain")]] block_header_state {
      uint64_t                   block_num;
      block_id_type              block_id;
      signed_block_header        header;
      uint32_t                   active_schedule_id;
      uint32_t                   pending_schedule_id;
      incremental_merkle         blockroot_merkle;
      capi_public_key            block_signing_key;   // redundant, used for make signature verification faster

      uint64_t primary_key()const { return block_num; }

      EOSLIB_SERIALIZE( block_header_state, (block_num)(block_id)(header)(active_schedule_id)(pending_schedule_id)(blockroot_merkle)(block_signing_key) )
   };
   typedef eosio::multi_index< "chaindb"_n, block_header_state >  chaindb;


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


   struct [[eosio::table("global"), eosio::contract("ibc.chain")]] global_state {
      uint32_t    lib_depth = 50;

      EOSLIB_SERIALIZE( global_state, (lib_depth) )
   };
   typedef eosio::singleton< "global"_n, global_state > global_singleton;


   struct [[eosio::table("relays"), eosio::contract("ibc.chain")]] relay_account {
      name    relay;

      uint64_t primary_key()const { return relay.value; }

      EOSLIB_SERIALIZE( relay_account, (relay) )
   };
   typedef eosio::multi_index< "relays"_n, relay_account > relays;


   struct [[eosio::table("blkrtmkls"), eosio::contract("ibc.chain")]] blockroot_merkle_type {
      uint64_t            block_num;
      incremental_merkle  merkle;

      uint64_t primary_key()const { return block_num; }

      EOSLIB_SERIALIZE( blockroot_merkle_type, (block_num)(merkle) )
   };
   typedef eosio::multi_index< "blkrtmkls"_n, blockroot_merkle_type > blkrtmkls;


   class [[eosio::contract("ibc.chain")]] chain : public contract {
   private:
      chaindb                 _chaindb;
      prodsches               _prodsches;
      sections                _sections;
      relays                  _relays;
      global_singleton        _global;
      global_state            _gstate;

   public:
      chain( name s, name code, datastream<const char*> ds );
      ~chain();

      [[eosio::action]]
      void setglobal( const global_state& gs );

      [[eosio::action]]
      void chaininit( const std::vector<char>&     header,
                      const producer_schedule&     active_schedule,
                      const incremental_merkle&    blockroot_merkle );

      // called by ibc plugin
      [[eosio::action]]
      void pushsection( const std::vector<char>&    headers,
                        const incremental_merkle&   blockroot_merkle,
                        const name&                 relay );

      // called by ibc plugin
      [[eosio::action]]
      void rminvalidls( const name& relay );

      // called by ibc plugin repeatedly
      [[eosio::action]]
      void rmfirstsctn( const name& relay ); // first section and old data

      [[eosio::action]]
      void relay( string action, name relay );

      // called by ibc plugin
      [[eosio::action]]
      void blockmerkle( uint64_t block_num, incremental_merkle merkle, name relay );

      static void assert_block_in_lib_and_trx_mroot_in_block( name          ibc_contract_account,
                                                              uint32_t      block_num,
                                                              digest_type   transaction_mroot) {
         sections _sections( ibc_contract_account, ibc_contract_account.value );
         global_singleton _global( ibc_contract_account, ibc_contract_account.value );
         auto _gstate = _global.get();

         bool block_in_lib = false;
         for ( auto it = _sections.rbegin(); it != _sections.rend(); ++it ){
            if ( it->first <= block_num && block_num <= it->last ){
               eosio_assert( it->valid, "section not yet valid");
               eosio_assert( it->last - it->first >= _gstate.lib_depth, "section internal error, not valid yet");
               eosio_assert( it->first <= block_num && block_num <= it->last - _gstate.lib_depth + 2, "block not in section lib");
               block_in_lib = true;
            }
         }
         eosio_assert( block_in_lib,  (string("no section contains this block ") + std::to_string( block_num)).c_str());

         chaindb _chaindb( ibc_contract_account, ibc_contract_account.value );
         auto bhs = _chaindb.get( block_num );
         eosio_assert( is_equal_capi_checksum256( bhs.header.transaction_mroot, transaction_mroot ), "provided transaction_mroot not correct");
      }

      static bool is_relay( name ibc_contract_account, name check ) {
         relays _relays( ibc_contract_account, ibc_contract_account.value );
         auto it = _relays.find( check.value );
         return it != _relays.end();
      }

      // this action maybe needed when repairing the ibc system manually
      [[eosio::action]]
      void fcinit( );

   private:
      void pushheader( const signed_block_header& header );
      void newsection( const signed_block_header& header, const incremental_merkle& blockroot_merkle );

      digest_type       bhs_sig_digest( const block_header_state& hs ) const;
      capi_public_key   get_producer_capi_public_key( uint64_t id, name producer ) const;
      void              assert_producer_signature( const digest_type& digest,
                                                   const capi_signature& signature,
                                                   const capi_public_key& pub_key ) const;
      uint32_t          get_section_active_schedule_id( const section_type& section ) const;

      void trim_last_section_or_not();
      void remove_first_section_or_not();
      void remove_section( uint64_t id );

      bool has_relay_auth();
   };

} /// namespace eosio








