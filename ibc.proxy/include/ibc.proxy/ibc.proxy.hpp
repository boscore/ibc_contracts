/**
 *  @file
 *  @copyright defined in bos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>

namespace eosio {



   class [[eosio::contract("ibc.proxy")]] token : public contract {
      public:
      token( name s, name code, datastream<const char*> ds );
      ~token();

      [[eosio::action]]
      void setglobal( name this_chain, bool active );

     
   private:
      eosio::singleton< "globals"_n, global_state >   _global_state;
      global_state                                    _gstate;
      
      
   };

} /// namespace eosio
