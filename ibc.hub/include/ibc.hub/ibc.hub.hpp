/**
 *  @file
 *  @copyright defined in bos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <ibc.token/types.hpp>

namespace eosio {


   class [[eosio::contract("ibc.hub")]] hub : public contract {
   public:
      hub(name s, name code, datastream<const char *> ds){};

      ~hub(){};

      // called in C apply function
      void transfer_notify( name    code,
                            name    from,
                            name    to,
                            asset   quantity,
                            string  memo );

      [[eosio::action]]
      void setglobal(name this_chain, bool active);

   private:

      struct [[eosio::table("globals")]] global_state {
         global_state(){}

         name              this_chain;
         bool              active = true; // use as global locks

         EOSLIB_SERIALIZE( global_state, (this_chain)(active))
      };


      // code,scope(_self,peerchain_name.value)
      struct [[eosio::table]] hub_trx_info {
         uint64_t                id; // auto-increment


         uint64_t primary_key()const { return balance.symbol.code().raw(); }
      };
      typedef eosio::multi_index< "chainassets"_n, peer_chain_asset > chainassets_table;








      int a;
   };


} /// namespace eosio
