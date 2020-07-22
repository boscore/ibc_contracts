/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosiolib/action.hpp>
#include <eosiolib/transaction.hpp>
#include <ibc.chain/ibc.chain.hpp>
#include <ibc.token/ibc.token.hpp>

#include <merkle.cpp>
#include <block_header.cpp>
#include "utils.cpp"

namespace eosio {

   token::token( name s, name code, datastream<const char*> ds ):
         contract( s, code, ds ),
         _global_state( _self, _self.value )
   {
      _gstate = _global_state.exists() ? _global_state.get() : global_state{};
   }

   token::~token(){
      _global_state.set( _gstate, _self );
    }
   
   void token::setglobal(  name ibc_token_account ) {
      require_auth( _self );
      _gstate.ibc_token_account   = ibc_token_account;
   }

   void transfer_notify( name    code,
                         name    from,
                         name    to,
                         asset   quantity,
                         string  memo ){
      
   }

   void token::transfer( name from,name to, asset quantity, string memo ){

   }

   void token::rollback( transaction_id_type trx_id ){

   }



} /// namespace eosio

extern "C" {
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
      if( code == receiver ) {
         switch( action ) {
            EOSIO_DISPATCH_HELPER( eosio::token, (setglobal)(transfer)(rollback))
         }
         return;
      }
      if (code != receiver && action == eosio::name("transfer").value) {
         auto args = eosio::unpack_action_data<eosio::transfer_action_type>();
         if( args.to == eosio::name(receiver) && args.quantity.amount > 0 && args.memo.length() > 0 ){
            eosio::proxy thiscontract(eosio::name(receiver), eosio::name(code), eosio::datastream<const char*>(nullptr, 0));
            thiscontract.transfer_notify(eosio::name(code), args.from, args.to, args.quantity, args.memo);
         }
      }
   }
}

