/**
 *  @file
 *  @copyright defined in bos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>

namespace eosio {

   using std::string;

   struct transfer_action_type {
      name    from;
      name    to;
      asset   quantity;
      string  memo;

      EOSLIB_SERIALIZE( transfer_action_type, (from)(to)(quantity)(memo) )
   };

   class [[eosio::contract("proxytest")]] proxytest : public contract {
      public:
      proxytest( name s, name code, datastream<const char*> ds );
      ~proxytest();

      [[eosio::action]]
      void setglobal( name ibc_proxy_account );

      [[eosio::action]]
      void call( name token_contract, asset quantity, string memo );

      struct [[eosio::table("globals")]] global_state {
         global_state(){}
         name              ibc_proxy_account;
         EOSLIB_SERIALIZE( global_state, (ibc_proxy_account))
      };

   private:
      eosio::singleton< "globals"_n, global_state >   _global_state;
      global_state                                    _gstate;
   };

} /// namespace eosio
