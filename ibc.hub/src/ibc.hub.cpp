/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosiolib/action.hpp>
#include <eosiolib/transaction.hpp>
#include <ibc.hub/ibc.hub.hpp>

namespace eosio {


   hub::hub( name s, name code, datastream<const char*> ds ):
         contract( s, code, ds )
   {
   }
   void hub::setglobal( name this_chain, bool active ){

   }



   /**
  * memo string format specification:
  * memo string must start with "local" or meet the 'ibc transfer action's memo string format' described above
  * any other prefix or empty memo string will assert failed
  *
  * when start with "local", means this is a local chain transaction, do not any process and return directly
  */
   void hub::transfer_notify( name original_contract, name from, name to, asset quantity, string memo ) {


      print_f("*---- %   %  %  %  %----*", original_contract, from, to , quantity, memo);


   }





} /// namespace eosio

extern "C" {
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
      if( code == receiver ) {
         switch( action ) {
            EOSIO_DISPATCH_HELPER( eosio::hub, (setglobal))
         }
         return;
      }
      if (code != receiver && action == eosio::name("transfer").value) {
         auto args = eosio::unpack_action_data<eosio::transfer_action_type>();
         if( args.to == eosio::name(receiver) && args.quantity.amount > 0 && args.memo.length() > 0 ){
            eosio::hub thiscontract(eosio::name(receiver), eosio::name(code), eosio::datastream<const char*>(nullptr, 0));
            thiscontract.transfer_notify(eosio::name(code), args.from, args.to, args.quantity, args.memo);
         }
      }
   }
}

