/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosiolib/action.hpp>
#include <eosiolib/transaction.hpp>
#include <proxytest/proxytest.hpp>

namespace eosio {

   proxytest::proxytest( name s, name code, datastream<const char*> ds ):
         contract( s, code, ds ),
         _global_state( _self, _self.value )
   {
      _gstate = _global_state.exists() ? _global_state.get() : global_state{};
   }

   proxytest::~proxytest(){
      _global_state.set( _gstate, _self );
    }
   
   void proxytest::setglobal(  name ibc_proxy_account ) {
      require_auth( _self );
      _gstate.ibc_proxy_account   = ibc_proxy_account;
   }

   void proxytest::call( name token_contract, asset quantity, string memo  ){
      transfer_action_type action_data{ _self, _gstate.ibc_proxy_account, quantity, memo };
      action( permission_level{ _self, "active"_n }, token_contract, "transfer"_n, action_data ).send();
   }
} /// namespace eosio

extern "C" {
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
      if( code == receiver ) {
         switch( action ) {
            EOSIO_DISPATCH_HELPER( eosio::proxytest, (setglobal)(call))
         }
      }
   }
}

