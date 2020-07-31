/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosiolib/action.hpp>
#include <eosiolib/transaction.hpp>
//#include <ibc.chain/ibc.chain.hpp>
#include <ibc.proxy/ibc.proxy.hpp>

#include <merkle.cpp>
#include <block_header.cpp>
#include "utils.cpp"

namespace eosio {

   proxy::proxy( name s, name code, datastream<const char*> ds ):
         contract( s, code, ds ),
         _global_state( _self, _self.value ),
         _proxytrxs( _self, _self.value )
   {
      _gstate = _global_state.exists() ? _global_state.get() : global_state{};
   }

   proxy::~proxy(){
      _global_state.set( _gstate, _self );
    }
   
   void proxy::setglobal(  name ibc_token_account ) {
      require_auth( _self );
      _gstate.ibc_token_account   = ibc_token_account;
   }

   void proxy::transfer_notify( name    original_contract,
                         name    from,
                         name    to,
                         asset   quantity,
                         string  memo ){
      eosio_assert( to == _self, "to must be the proxy contract account self");

      auto info = get_memo_info( memo );
      eosio_assert( info.receiver != name(),"receiver not provide");
      eosio_assert( info.peerchain != name(),"peerchain not provide");
      eosio_assert( info.notes.length() <= 64,"memo string too long");

      _proxytrxs.emplace( _self, [&]( auto& r ){
         r.id              = _proxytrxs.available_primary_key();
         r.orig_trx_id     = get_trx_id();
         r.block_time_slot = get_block_time_slot();
         r.token_contract  = original_contract;
         r.orig_from       = from;
         r.to              = to;
         r.quantity        = quantity;
         r.orig_memo       = memo;
      });
   }

   void proxy::transfer( name from, name to, asset quantity, string memo ){

      eosio_assert( from == _self, "from must be _self");

      string orig_trxid_str = get_value_str_by_key_str( memo, key_orig_trxid );
      capi_checksum256 orig_trx_id = string_to_capi_checksum256( orig_trxid_str );

      auto idx = _proxytrxs.get_index<"trxid"_n>();
      const auto& trx_p = idx.find(fixed_bytes<32>(orig_trx_id.hash));
      eosio_assert( trx_p != idx.end(), "transaction not found.");

      if( to == trx_p->orig_from ){
         transfer_action_type action_data{ from, to, quantity, memo };
         action( permission_level{ _self, "active"_n }, trx_p->token_contract, "transfer"_n, action_data ).send();
         _proxytrxs.erase( *trx_p );
         return;
      }

      eosio_assert( to == _gstate.ibc_token_account, "to must be ibc_token_account");
      eosio_assert( quantity == trx_p->quantity, "quantity == trx_p->quantity assert failed");

      name orig_from = name( get_value_str_by_key_str( memo, key_orig_from ));
      name orig_token_contract = name( get_value_str_by_key_str( memo, key_token_contract ));

      eosio_assert( orig_from == trx_p->orig_from, "orig_from == trx_p->from assert failed");
      eosio_assert( orig_token_contract == trx_p->token_contract, "orig_token_contract == trx_p->token_contract assert failed");

      string correct_memo_str = trx_p->orig_memo + " " +
            key_token_contract + "=" + trx_p->token_contract.to_string() + " " +
            key_orig_from + "=" + trx_p->orig_from.to_string() + " " +
            key_orig_trxid + "=" + capi_checksum256_to_string(trx_p->orig_trx_id);

      eosio_assert( memo == correct_memo_str, "memo != correct_memo_str");

      transfer_action_type action_data{ from, to, quantity, memo };
      action( permission_level{ _self, "active"_n }, trx_p->token_contract, "transfer"_n, action_data ).send();

      _proxytrxs.erase( *trx_p );
   }

} /// namespace eosio

extern "C" {
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
      if( code == receiver ) {
         switch( action ) {
            EOSIO_DISPATCH_HELPER( eosio::proxy, (setglobal)(transfer))
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

