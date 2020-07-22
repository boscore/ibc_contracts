/**
 *  @file
 *  @copyright defined in bos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <ibc.token/ibc.token.hpp>

namespace eosio {



   class [[eosio::contract("ibc.proxy")]] proxy : public contract {
      public:
      token( name s, name code, datastream<const char*> ds );
      ~token();

      [[eosio::action]]
      void setglobal( name ibc_token_account );

      // called in C apply function
      void transfer_notify( name    code,
                            name    from,
                            name    to,
                            asset   quantity,
                            string  memo );

      [[eosio::action]]
      void transfer( name    from,
                     name    to,
                     asset   quantity,
                     string  memo );

      // called by ibc plugin repeatedly
      [[eosio::action]]
      void rollback( transaction_id_type trx_id );



      struct [[eosio::table("globals")]] global_state {
         global_state(){
         name              ibc_token_account;
         // explicit serialization macro is necessary, without this, error "Exceeded call depth maximum" will occur when call state_singleton.set(state)
         EOSLIB_SERIALIZE( global_state, (ibc_token_account))
      };

   private:
      eosio::singleton< "globals"_n, global_state >   _global_state;
      global_state                                    _gstate;

      // use to record accepted transfer and withdraw transactions
      // code,scope(_self,peerchain_name.value)
      struct [[eosio::table]] original_trx_info {
         uint64_t                id; // auto-increment
         uint64_t                block_time_slot; // new record must not decrease time slot,
         transaction_id_type     trx_id;
         transfer_action_info    action; // very important infomation, used when execute rollback


         uint64_t primary_key()const { return id; }
         uint64_t by_time_slot()const { return block_time_slot; }
         fixed_bytes<32> by_trx_id()const { return fixed_bytes<32>(trx_id.hash); }
      };
      typedef eosio::multi_index< "origtrxs"_n, original_trx_info,
            indexed_by<"tslot"_n, const_mem_fun<original_trx_info, uint64_t,        &original_trx_info::by_time_slot> >,  // used by ibc plugin
      indexed_by<"trxid"_n, const_mem_fun<original_trx_info, fixed_bytes<32>, &original_trx_info::by_trx_id> >
      >  origtrxs_table;

   };

} /// namespace eosio
