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
         _global_state( _self, _self.value ),
         _admin_sg(_self, _self.value),
         _peerchains( _self, _self.value ),
         _freeaccount( _self, _self.value ),
         _peerchainm( _self, _self.value ),
         _accepts( _self, _self.value ),
         _stats( _self, _self.value )

   {
      _gstate = _global_state.exists() ? _global_state.get() : global_state{};
      _admin_st = _admin_sg.exists() ? _admin_sg.get() : admin_struct{};

   }

   token::~token(){
      _global_state.set( _gstate, _self );
      _admin_sg.set( _admin_st , _self );

   }
   
   void token::setglobal( name this_chain, bool active ) {
      require_auth( _self );
      _gstate.this_chain   = this_chain;
      _gstate.active       = active;
   }

   void token::setadmin( name admin ){
      require_auth( _self );
      _admin_st.admin = admin;
   }

} /// namespace eosio

extern "C" {
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
      if( code == receiver ) {
         switch( action ) {
            EOSIO_DISPATCH_HELPER( eosio::token, (setglobal)(regpeerchain)(setchainbool)
            (regacpttoken)(setacptasset)(setacptstr)(setacptint)(setacptbool)(setacptfee)
            (regpegtoken)(setpegasset)(setpegint)(setpegbool)(setpegtkfee)
            (transfer)(cash)(cashconfirm)(rollback)(rmunablerb)(fcrollback)(fcrmorigtrx)
            (lockall)(unlockall)(forceinit)(open)(close)(unregtoken)(setfreeacnt)(setadmin)
#ifdef HUB
            (hubinit)(feetransfer)(regpegtoken2)
#endif
            )
         }
         return;
      }
      if (code != receiver && action == eosio::name("transfer").value) {
         auto args = eosio::unpack_action_data<eosio::transfer_action_type>();
         if( args.to == eosio::name(receiver) && args.quantity.amount > 0 && args.memo.length() > 0 ){
            eosio::token thiscontract(eosio::name(receiver), eosio::name(code), eosio::datastream<const char*>(nullptr, 0));
            thiscontract.transfer_notify(eosio::name(code), args.from, args.to, args.quantity, args.memo);
         }
      }
   }
}

