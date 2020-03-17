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
      int a;
   };


} /// namespace eosio
