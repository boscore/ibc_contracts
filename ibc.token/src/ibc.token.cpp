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
         _peerchains( _self, _self.value ),
         _peerchainm( _self, _self.value ),
         _accepts( _self, _self.value ),
         _stats( _self, _self.value )
   {
      _gstate = _global_state.exists() ? _global_state.get() : global_state{};
   }

   token::~token(){
      _global_state.set( _gstate, _self );
   }
   
   void token::setglobal( name this_chain, bool active ) {
      require_auth( _self );
      _gstate.this_chain   = this_chain;
      _gstate.active       = active;
   }

   void token::regpeerchain( name           peerchain_name,
                             string         peerchain_info,
                             name           peerchain_ibc_token_contract,
                             name           thischain_ibc_chain_contract,
                             name           thischain_free_account,
                             uint32_t       max_original_trxs_per_block,
                             uint32_t       max_origtrxs_table_records,
                             uint32_t       cache_cashtrxs_table_records,
                             bool           active ){
      require_auth( _self );

      eosio_assert( peerchain_name != name(), "peerchain_name can not be empty");
      eosio_assert( peerchain_info.size() < 256, "peerchain_info has more than 256 bytes");
      eosio_assert( peerchain_ibc_token_contract != name(), "peerchain_ibc_token_contract can not be empty");
      eosio_assert( is_account( thischain_ibc_chain_contract ), "thischain_ibc_chain_contract account does not exist");

      eosio_assert( 1 <= max_original_trxs_per_block && max_original_trxs_per_block <= 10 ,"max_original_trxs_per_block must in range [1,10]");
      eosio_assert( 500 <= max_origtrxs_table_records && max_origtrxs_table_records <= 2000 ,"max_origtrxs_table_records must in range [500,2000]");
      eosio_assert( 1000 <= cache_cashtrxs_table_records && cache_cashtrxs_table_records <= 2000 ,"cache_cashtrxs_table_records must in range [1000,2000]");

      auto itr = _peerchains.find( peerchain_name.value );
      if ( itr != _peerchains.end() ){ _peerchains.erase( itr );}

      _peerchains.emplace( _self, [&]( auto& r ){
         r.peerchain_name                 = peerchain_name;
         r.peerchain_info                 = peerchain_info;
         r.peerchain_ibc_token_contract   = peerchain_ibc_token_contract;
         r.thischain_ibc_chain_contract   = thischain_ibc_chain_contract;
         r.thischain_free_account         = thischain_free_account;
         r.max_original_trxs_per_block    = max_original_trxs_per_block;
         r.max_origtrxs_table_records     = max_origtrxs_table_records;
         r.cache_cashtrxs_table_records   = cache_cashtrxs_table_records;
         r.active                         = active;
      });

      _peerchainm.emplace( _self, [&]( auto& r ){
         r.peerchain_name           = peerchain_name;
         r.cash_seq_num             = 0;
         r.last_confirmed_orig_trx_block_time_slot = 0;
         r.current_block_time_slot  = 0;
         r.current_block_trxs       = 0;
         r.origtrxs_tb_next_id      = 1;
      });
   }

   void token::setchainbool( name peerchain_name, string which, bool value ){
      require_auth( _self );

      auto& chain = _peerchains.get( peerchain_name.value );
      if ( which == "active" ){
         _peerchains.modify( chain, same_payer, [&]( auto& r ) { r.active = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::regacpttoken( name        original_contract,
                             symbol      orig_token_symbol,
                             symbol      peg_token_symbol,
                             asset       max_accept,
                             asset       min_once_transfer,
                             asset       max_once_transfer,
                             asset       max_daily_transfer,
                             uint32_t    max_tfs_per_minute,
                             string      organization,
                             string      website,
                             name        administrator,
                             name        service_fee_mode,
                             asset       service_fee_fixed,
                             double      service_fee_ratio,
                             asset       failed_fee,
                             bool        active ){
      require_auth( _self );

      eosio_assert( is_account( original_contract ), "original_contract account does not exist");
      eosio_assert( is_account( administrator ), "administrator account does not exist");

      eosio_assert( orig_token_symbol.is_valid(), "orig_token_symbol invalid");
      eosio_assert( peg_token_symbol.is_valid(), "peg_token_symbol invalid");
      eosio_assert( orig_token_symbol.precision() == peg_token_symbol.precision(), "precision mismatch");

      eosio_assert( max_accept.is_valid(), "invalid max_accept");
      eosio_assert( max_accept.amount > 0, "max_accept must be positive");
      eosio_assert( max_accept.symbol           == orig_token_symbol &&
                    min_once_transfer.symbol    == orig_token_symbol &&
                    max_once_transfer.symbol    == orig_token_symbol &&
                    max_daily_transfer.symbol   == orig_token_symbol &&
                    service_fee_fixed.symbol    == orig_token_symbol &&
                    failed_fee.symbol     == orig_token_symbol &&
                    min_once_transfer.amount    > 0 &&
                    max_once_transfer.amount    > min_once_transfer.amount &&
                    max_daily_transfer.amount   > max_once_transfer.amount &&
                    service_fee_fixed.amount    >= 0 &&
                    failed_fee.amount     >= 0 , "invalid asset");

      eosio_assert( organization.size() < 256, "organization has more than 256 bytes");
      eosio_assert( website.size() < 256, "website has more than 256 bytes");

      eosio_assert( service_fee_mode == "fixed"_n || service_fee_mode == "ratio"_n, "invalid service_fee_mode");
      eosio_assert( 0 <= service_fee_ratio && service_fee_ratio <= 0.05 , "invalid service_fee_ratio");

      eosio_assert( service_fee_fixed.amount * 5 <= min_once_transfer.amount, "service_fee_fixed.amount * 5 <= min_once_transfer.amount assert failed");
      eosio_assert( failed_fee.amount * 10 <= min_once_transfer.amount, "failed_fee.amount * 10 <= min_once_transfer.amount assert failed");

      auto existing = _accepts.find( original_contract.value );
      eosio_assert( existing == _accepts.end(), "token contract already exist" );

      _accepts.emplace( _self, [&]( auto& r ){
         r.original_contract  = original_contract;
         r.peg_token_symbol   = peg_token_symbol;
         r.accept             = asset{ 0, orig_token_symbol };
         r.max_accept         = max_accept;
         r.min_once_transfer  = min_once_transfer;
         r.max_once_transfer  = max_once_transfer;
         r.max_daily_transfer = max_daily_transfer;
         r.max_tfs_per_minute = max_tfs_per_minute;
         r.organization       = organization;
         r.website            = website;
         r.administrator      = administrator;
         r.service_fee_mode   = service_fee_mode;
         r.service_fee_fixed  = service_fee_fixed;
         r.service_fee_ratio  = service_fee_ratio;
         r.failed_fee   = failed_fee;
         r.total_transfer     =  asset{ 0, max_accept.symbol };
         r.total_transfer_times = 0;
         r.total_cash         =  asset{ 0, max_accept.symbol };
         r.total_cash_times   = 0;
         r.active             = active;
      });
   }

   void token::setacptasset( name contract, string which, asset quantity ) {
      const auto& acpt = get_currency_accept( contract );
      eosio_assert( quantity.symbol == acpt.accept.symbol, "invalid symbol" );

      require_auth( acpt.administrator );

      if ( which == "max_accept" ){
         eosio_assert( quantity.amount >= acpt.accept.amount, "max_accept.amount should not less then accept.amount");
         _accepts.modify( acpt, same_payer, [&]( auto& r ) { r.max_accept = quantity; });
         return;
      }
      if ( which == "min_once_transfer" ){
         eosio_assert( 0 < quantity.amount && quantity.amount < acpt.max_once_transfer.amount, "min_once_transfer invalid");
         _accepts.modify( acpt, same_payer, [&]( auto& r ) { r.min_once_transfer = quantity; });
         return;
      }
      if ( which == "max_once_transfer" ){
         eosio_assert( acpt.min_once_transfer.amount < quantity.amount && quantity.amount < acpt.max_daily_transfer.amount , "max_once_transfer invalid");
         _accepts.modify( acpt, same_payer, [&]( auto& r ) { r.max_once_transfer = quantity; });
         return;
      }
      if ( which == "max_daily_transfer" ){
         eosio_assert( quantity.amount > acpt.max_once_transfer.amount, "max_daily_transfer must be greater then max_once_transfer");
         _accepts.modify( acpt, same_payer, [&]( auto& r ) { r.max_daily_transfer = quantity; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setacptstr( name contract, string which, string value ) {
      const auto& acpt = get_currency_accept( contract );
      eosio_assert( value.size() < 256, "value string has more than 256 bytes");

      require_auth( acpt.administrator );

      if ( which == "organization" ){
         _accepts.modify( acpt, acpt.administrator, [&]( auto& r ) { r.organization = value; });
         return;
      }
      if ( which == "website" ){
         _accepts.modify( acpt, acpt.administrator, [&]( auto& r ) { r.website = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setacptint( name contract, string which, uint64_t value ) {
      const auto& acpt = get_currency_accept( contract );
      require_auth( _self );

      if ( which == "max_tfs_per_minute" ){
         eosio_assert( 1 <= value && value <= 500, "max_tfs_per_minute's value must in range [1,500]");
         _accepts.modify( acpt, _self, [&]( auto& r ) { r.max_tfs_per_minute = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setacptbool( name contract, string which, bool value ) {
      const auto& acpt = get_currency_accept( contract );
      require_auth( acpt.administrator );

      if ( which == "active" ){
         _accepts.modify( acpt, acpt.administrator, [&]( auto& r ) { r.active = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setacptfee( name   contract,
                           name   kind,
                           name   fee_mode,
                           asset  fee_fixed,
                           double fee_ratio ) {
      require_auth( _self );

      const auto& acpt = get_currency_accept( contract );
      eosio_assert( fee_mode == "fixed"_n || fee_mode == "ratio"_n, "mode can only be fixed or ratio");
      eosio_assert( fee_fixed.symbol == acpt.accept.symbol && fee_fixed.amount >= 0, "service_fee_fixed invalid" );
      eosio_assert( 0 <= fee_ratio && fee_ratio <= 0.05 , "service_fee_ratio invalid");

      if ( kind == "success"_n ){
         eosio_assert( fee_fixed.amount * 5 <= acpt.min_once_transfer.amount, "service_fee_fixed.amount * 5 <= min_once_transfer.amount assert failed");
         _accepts.modify( acpt, _self, [&]( auto& r ) {
            r.service_fee_mode  = fee_mode;
            r.service_fee_fixed = fee_fixed;
            r.service_fee_ratio = fee_ratio;
         });
      } else if ( kind == "failed"_n ){
         eosio_assert( fee_fixed.amount * 10 <= acpt.min_once_transfer.amount, "failed_fee.amount * 10 <= min_once_transfer.amount assert failed");
         _accepts.modify( acpt, _self, [&]( auto& r ) {
            r.failed_fee = fee_fixed;
         });
      } else {
         eosio_assert( false, "kind must be \"success\" or \"failed\"");
      }
   }
   
   void token::regpegtoken( name        peerchain_name,
                            name        peerchain_contract,
                            symbol      orig_token_symbol,
                            symbol      peg_token_symbol,
                            asset       max_supply,
                            asset       min_once_withdraw,
                            asset       max_once_withdraw,
                            asset       max_daily_withdraw,
                            uint32_t    max_wds_per_minute,
                            name        administrator,
                            asset       failed_fee,
                            bool        active ){
      require_auth( _self );
      eosio_assert( is_account( administrator ), "administrator account does not exist");

      eosio_assert( orig_token_symbol.is_valid(), "orig_token_symbol invalid");
      eosio_assert( peg_token_symbol.is_valid(), "peg_token_symbol invalid");
      eosio_assert( orig_token_symbol.precision() == peg_token_symbol.precision(), "precision mismatch");

      eosio_assert( max_supply.is_valid(), "invalid max_supply");
      eosio_assert( max_supply.amount > 0, "max_supply must be positive");
      eosio_assert( max_supply.symbol           == peg_token_symbol &&
                    min_once_withdraw.symbol    == peg_token_symbol &&
                    max_once_withdraw.symbol    == peg_token_symbol &&
                    max_daily_withdraw.symbol   == peg_token_symbol &&
                    failed_fee.symbol     == peg_token_symbol &&
                    min_once_withdraw.amount    > 0 &&
                    max_once_withdraw.amount    > min_once_withdraw.amount &&
                    max_daily_withdraw.amount   > max_once_withdraw.amount &&
                    failed_fee.amount     >= 0 , "invalid asset");

      eosio_assert( peerchain_name != name(), "peerchain_name can not be empty");
      eosio_assert( _peerchains.find(peerchain_name.value) != _peerchains.end(), "peerchain not registered");
      eosio_assert( peerchain_contract != name(), "peerchain_contract can not be empty");

      eosio_assert( failed_fee.amount * 10 <= min_once_withdraw.amount, "failed_fee.amount * 10 <= min_once_withdraw.amount assert failed");

      auto existing = _stats.find( max_supply.symbol.code().raw() );
      eosio_assert( existing == _stats.end(), "token contract already exist" );

      _stats.emplace( _self, [&]( auto& r ){
         r.supply             = asset{ 0, max_supply.symbol };
         r.max_supply         = max_supply;
         r.min_once_withdraw  = min_once_withdraw;
         r.max_once_withdraw  = max_once_withdraw;
         r.max_daily_withdraw = max_daily_withdraw;
         r.max_wds_per_minute = max_wds_per_minute;
         r.administrator      = administrator;
         r.peerchain_name     = peerchain_name;
         r.peerchain_contract = peerchain_contract;
         r.orig_token_symbol  = orig_token_symbol;
         r.failed_fee   = failed_fee;
         r.total_issue        = asset{ 0, max_supply.symbol };
         r.total_issue_times  = 0;
         r.total_withdraw     = asset{ 0, max_supply.symbol };
         r.total_withdraw_times = 0;
         r.active             = active;
      });
   }

   void token::setpegasset( symbol_code symcode, string which, asset quantity ) {
      const auto& st = get_currency_stats( symcode );
      eosio_assert( quantity.symbol == st.supply.symbol, "invalid symbol" );
      require_auth( st.administrator );

      if ( which == "max_supply" ){
         eosio_assert( quantity.amount >= st.supply.amount, "max_supply.amount should not less then supply.amount");
         _stats.modify( st, same_payer, [&]( auto& r ) { r.max_supply = quantity; });
         return;
      }
      if ( which == "min_once_withdraw" ){
         eosio_assert( 0 < quantity.amount && quantity.amount < st.max_once_withdraw.amount, "min_once_withdraw invalid");
         _stats.modify( st, same_payer, [&]( auto& r ) { r.min_once_withdraw = quantity; });
         return;
      }
      if ( which == "max_once_withdraw" ){
         eosio_assert( st.min_once_withdraw.amount < quantity.amount && quantity.amount < st.max_daily_withdraw.amount , "max_once_withdraw invalid");
         _stats.modify( st, same_payer, [&]( auto& r ) { r.max_once_withdraw = quantity; });
         return;
      }
      if ( which == "max_daily_withdraw" ){
         eosio_assert( quantity.amount > st.max_once_withdraw.amount, "max_daily_withdraw.amount must be greater then max_once_withdraw.amount");
         _stats.modify( st, same_payer, [&]( auto& r ) { r.max_daily_withdraw = quantity; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setpegint( symbol_code symcode, string which, uint64_t value ) {
      const auto& st = get_currency_stats( symcode );
      require_auth( _self );

      if ( which == "max_wds_per_minute" ){
         eosio_assert( 1 <= value && value <= 500, "max_wds_per_minute's value must in range [1,500]");
         _stats.modify( st, _self, [&]( auto& r ) { r.max_wds_per_minute = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setpegbool( symbol_code symcode, string which, bool value ) {
      const auto& st = get_currency_stats( symcode );
      require_auth( st.administrator );

      if ( which == "active" ){
         _stats.modify( st, same_payer, [&]( auto& r ) { r.active = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setpegtkfee( symbol_code symcode, asset fee) {
      require_auth( _self );

      const auto& st = get_currency_stats( symcode );
      eosio_assert( fee.symbol == st.supply.symbol && fee.amount >= 0, "service_fee_fixed invalid" );
      eosio_assert( fee.amount * 10 <= st.min_once_withdraw.amount, "failed_fee.amount * 10 <= min_once_withdraw.amount assert failed");

      _stats.modify( st, same_payer, [&]( auto& r ) {
         r.failed_fee   = fee;
      });
   }

   /**
    * ---- 'ibc transfer action's memo string format' ----
    * memo string format is '{account_name}@{chain_name} {user-defined string}', user-defined string is optinal
    *
    * examples:
    * 'bosaccount31@bos happy new year 2019'
    * 'bosaccount32@bos'
    */

   struct memo_info_type {
      name     receiver;
      name     peerchain;
      string   notes;
   };

   memo_info_type get_memo_info( const string& memo_str ){
      static const string format = "{receiver}@{chain} {user-defined string}";
      memo_info_type info;

      string memo = memo_str;
      trim( memo );

      // --- get receiver ---
      auto pos = memo.find("@");
      eosio_assert( pos != std::string::npos, ( string("memo format error, didn't find charactor \'@\' in memo, correct format: ") + format ).c_str() );
      string receiver_str = memo.substr( 0, pos );
      trim( receiver_str );
      info.receiver = name( receiver_str );

      // --- trim ---
      memo = memo.substr( pos + 1 );
      trim( memo );

      // --- get chain name and notes ---
      pos = memo.find(" ");
      if ( pos == std::string::npos ){
         info.peerchain = name( memo );
         info.notes = "";
      } else {
         info.peerchain = name( memo.substr(0,pos) );
         info.notes = memo.substr( pos + 1 );
         trim( info.notes );
      }

      eosio_assert( info.receiver != name(), ( string("memo format error, receiver not provided, correct format: ") + format ).c_str() );
      eosio_assert( info.peerchain != name(), ( string("memo format error, chain not provided, correct format: ") + format ).c_str() );
      return info;
   }

   /**
     * memo string format specification:
     * memo string must start with "local" or meet the 'ibc transfer action's memo string format' described above
     * any other prefix or empty memo string will assert failed
     *
     * when start with "local", means this is a local chain transaction, do not any process and return directly
     */
   void token::transfer_notify( name original_contract, name from, name to, asset quantity, string memo ) {
      eosio_assert( to == _self, "to is not this contract");

      if ( memo.find("local") == 0 ){
         return;
      }

      // check global active
      eosio_assert( _gstate.active, "global not active" );

      auto info = get_memo_info( memo );
      eosio_assert( info.receiver != name(),"receiver not provide");
      auto pch = _peerchains.get( info.peerchain.value );

      // check chain active
      eosio_assert( pch.active, "peer chain is not active");

      const auto& acpt = get_currency_accept( original_contract );
      eosio_assert( acpt.active, "not active");

      eosio_assert( quantity.symbol == acpt.accept.symbol, "symbol does not match");
      eosio_assert( quantity.amount >= acpt.min_once_transfer.amount, "quantity less then min_once_transfer");
      eosio_assert( quantity.amount <= acpt.max_once_transfer.amount, "quantity greater then max_once_transfer");

      // accumulate max_tfs_per_minute and check
      auto current_time_sec = now();
      uint32_t limit = acpt.max_tfs_per_minute > 0 ? acpt.max_tfs_per_minute : default_max_trxs_per_minute_per_token;
      if ( current_time_sec > acpt.mutables.minute_trx_start + 60 ){
         _accepts.modify( acpt, same_payer, [&]( auto& r ) {
            r.mutables.minute_trx_start = current_time_sec;
            r.mutables.minute_trxs = 1;
         });
      } else {
         _accepts.modify( acpt, same_payer, [&]( auto& r ) {
            r.mutables.minute_trxs += 1;
         });
      }
      eosio_assert( acpt.mutables.minute_trxs <= limit,"max transactions per minute exceed" );

      // accumulate max_daily_transfer and check
      if ( acpt.max_daily_transfer.amount != 0 ) {
         if ( current_time_sec > acpt.mutables.daily_tf_start + 3600 * 24 ){
            _accepts.modify( acpt, same_payer, [&]( auto& r ) {
               r.mutables.daily_tf_start = current_time_sec;
               r.mutables.daily_tf_sum = quantity;
            });
         } else {
            _accepts.modify( acpt, same_payer, [&]( auto& r ) {
               r.mutables.daily_tf_sum += quantity;
            });
         }
         eosio_assert( acpt.mutables.daily_tf_sum <= acpt.max_daily_transfer,"max daily transfer exceed" );
      }

      // accumulate max_original_trxs_per_block and check
      auto& pchm = _peerchainm.get(info.peerchain.value);
      if ( get_block_time_slot() == pchm.current_block_time_slot ) {
         _peerchainm.modify( pchm, same_payer, [&]( auto& r ) {
            r.current_block_trxs += 1;
         });
         eosio_assert( pchm.current_block_trxs <= pch.max_original_trxs_per_block, "max_original_trxs_per_block exceed" );
      } else {
         _peerchainm.modify( pchm, same_payer, [&]( auto& r ) {
            r.current_block_time_slot = get_block_time_slot();
            r.current_block_trxs = 1;
         });
      }

      auto _chainassets = chainassets_table( _self, info.peerchain.value );
      auto itr = _chainassets.find( quantity.symbol.code().raw() );
      if ( itr == _chainassets.end() ){
         _chainassets.emplace( _self, [&]( auto& chain ){
            chain.balance = quantity;
         });
      } else {
         _chainassets.modify( itr, same_payer, [&]( auto& chain ) {
            chain.balance += quantity;
         });
      }

      _accepts.modify( acpt, same_payer, [&]( auto& r ) {
         r.accept += quantity;
         r.total_transfer += quantity;
         r.total_transfer_times += 1;
      });
      eosio_assert( acpt.accept.amount <= acpt.max_accept.amount, "acpt.accept.amount <= acpt.max_accept.amount assert failed");

      origtrxs_emplace( info.peerchain, transfer_action_info{ original_contract, from, quantity } );
   }

   /**
    * memo string format specification:
    * when to == _self, memo string must start with "local" or meet the 'ibc transfer action's memo string format' described above
    * any other prefix or empty memo string will assert failed
    *
    * when start with "local", means this is a local chain transaction
    * when to != _self, memo string will not be parsed
    */
   void token::transfer( name    from,
                         name    to,
                         asset   quantity,
                         string  memo )
   {
      eosio_assert( from != to, "cannot transfer to self" );
      require_auth( from );

      eosio_assert( memo.size() <= 512, "memo has more than 512 bytes" );

      if (  to == _self && memo.find("local") != 0 ) {
         auto info = get_memo_info( memo );
         eosio_assert( info.receiver != name(), "receiver not provide");
         auto pch = _peerchains.get( info.peerchain.value );
         eosio_assert( pch.active, "peer chain is not active");

         withdraw( from, info.peerchain, info.receiver, quantity, info.notes );
         return;
      }

      eosio_assert( is_account( to ), "to account does not exist");
      auto sym = quantity.symbol.code();
      const auto& st = _stats.get( sym.raw() );

      require_recipient( from );
      require_recipient( to );

      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
      eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

      auto payer = has_auth( to ) ? to : from;

      sub_balance( from, quantity );
      add_balance( to, quantity, payer );
   }

   void token::withdraw( name from, name peerchain_name, name peerchain_receiver, asset quantity, string memo ) {
      require_auth( from );

      // check global state
      eosio_assert( _gstate.active, "global not active" );

      const auto& st = get_currency_stats( quantity.symbol.code() );
      eosio_assert( st.active, "not active");
      eosio_assert( peerchain_name == st.peerchain_name, (string("peerchain_name must be ") + st.peerchain_name.to_string()).c_str());

      eosio_assert( quantity.symbol == st.supply.symbol, "symbol does not match");
      eosio_assert( quantity.amount >= st.min_once_withdraw.amount, "quantity less then min_once_transfer");
      eosio_assert( quantity.amount <= st.max_once_withdraw.amount, "quantity greater then max_once_transfer");

      const auto& balance = get_balance( _self, from, quantity.symbol.code() );
      eosio_assert( quantity.amount <= balance.amount, "overdrawn balance");

      // accumulate max_wds_per_minute and check
      auto current_time_sec = now();
      auto limit = st.max_wds_per_minute > 0 ? st.max_wds_per_minute : default_max_trxs_per_minute_per_token;

      if ( current_time_sec > st.mutables.minute_trx_start + 60 ){
         _stats.modify( st, same_payer, [&]( auto& r ) {
            r.mutables.minute_trx_start = current_time_sec;
            r.mutables.minute_trxs = 1;
         });
      } else {
         _stats.modify( st, same_payer, [&]( auto& r ) {
            r.mutables.minute_trxs += 1;
         });
      }
      eosio_assert( st.mutables.minute_trxs <= limit, "max transactions per minute exceed" );

      // accumulate max_daily_withdraw and check
      if ( st.max_daily_withdraw.amount != 0 ) {
         if ( current_time_sec > st.mutables.daily_wd_start + 3600 * 24 ){
            _stats.modify( st, same_payer, [&]( auto& r ) {
               r.mutables.daily_wd_start = current_time_sec;
               r.mutables.daily_wd_sum = quantity;
            });
         } else {
            _stats.modify( st, same_payer, [&]( auto& r ) {
               r.mutables.daily_wd_sum += quantity;
            });
         }
         eosio_assert( st.mutables.daily_wd_sum <= st.max_daily_withdraw,"max daily withdraw exceed" );
      }

      // accumulate max_original_trxs_per_block and check
      auto pch = _peerchains.get( peerchain_name.value );
      auto& pchm = _peerchainm.get( peerchain_name.value );
      if ( get_block_time_slot() == pchm.current_block_time_slot ) {
         _peerchainm.modify( pchm, same_payer, [&]( auto& r ) {
            r.current_block_trxs += 1;
         });
         eosio_assert( pchm.current_block_trxs <= pch.max_original_trxs_per_block, "max_original_trxs_per_block exceed" );
      } else {
         _peerchainm.modify( pchm, same_payer, [&]( auto& r ) {
            r.current_block_time_slot = get_block_time_slot();
            r.current_block_trxs = 1;
         });
      }

      _stats.modify( st, same_payer, [&]( auto& r ) {
         r.supply -= quantity;
         r.total_withdraw += quantity;
         r.total_withdraw_times += 1;
      });

      sub_balance( from, quantity );
      add_balance( _self, quantity, _self ); // not burn token here, burn at cash_confirm()

      origtrxs_emplace( peerchain_name, transfer_action_info{ _self, from, quantity } );
   }

   void token::verify_merkle_path( const std::vector<digest_type>& merkle_path, digest_type check ) {
      eosio_assert( merkle_path.size() > 0,"merkle_path can not be empty");

      if ( merkle_path.size() == 1 ){
         eosio_assert( is_equal_capi_checksum256(check, merkle_path[0]),"merkle path validate failed");
         return;
      }

      eosio_assert( is_equal_capi_checksum256(check, merkle_path[0]) ||
                    is_equal_capi_checksum256(check, merkle_path[1]), "digest not in merkle tree");

      digest_type result = get_checksum256( make_canonical_pair(merkle_path[0], merkle_path[1]) );

      for( auto i = 0; i < merkle_path.size() - 3; ++i ){
         digest_type left;
         digest_type right;

         if ( is_canonical_left(merkle_path[i+2]) ){
            left = merkle_path[i+2];
            right = make_canonical_right( result );
         } else {
            left = make_canonical_left( result );
            right = merkle_path[i+2];
         }
         result = get_checksum256( std::make_pair(left,right) );
      }
      eosio_assert( is_equal_capi_checksum256(result, merkle_path.back()) ,"merkle path validate failed" );
   }

   void token::cash( const uint64_t&                        seq_num,
                     const name&                            from_chain,
                     const transaction_id_type&             orig_trx_id,         // redundant, facilitate indexing and checking
                     const std::vector<char>&               orig_trx_packed_trx_receipt,
                     const std::vector<capi_checksum256>&   orig_trx_merkle_path,
                     const uint32_t&                        orig_trx_block_num,  // redundant with orig_trx_block_header_data, facilitate indexing and checking
                     const std::vector<char>&               orig_trx_block_header_data,
                     const std::vector<capi_checksum256>&   orig_trx_block_id_merkle_path,
                     const uint32_t&                        anchor_block_num,
                     const name&                            to,                  // redundant, facilitate indexing and checking
                     const asset&                           quantity,            // with the token symbol of the original trx it self. redundant, facilitate indexing and checking
                     const string&                          memo ) {
      // check global state
      eosio_assert( _gstate.active, "global not active" );

      auto sym = quantity.symbol;
      eosio_assert( sym.is_valid(), "invalid symbol name" );
      eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

      eosio_assert( seq_num == get_cashtrxs_tb_max_seq_num(from_chain) + 1, "seq_num not valid");   // seq_num is important, used to enable all successful cash transactions must be successfully returned to the original chain, no one will be lost
      eosio_assert( orig_trx_block_num >= get_cashtrxs_tb_max_orig_trx_block_num(from_chain), "orig_trx_block_num error");  // important! used to prevent replay attack
      eosio_assert( false == is_orig_trx_id_exist_in_cashtrxs_tb(from_chain, orig_trx_id), "orig_trx_id already exist");      // important! used to prevent replay attack

      const transaction_receipt& trx_receipt = unpack<transaction_receipt>( orig_trx_packed_trx_receipt );
      eosio_assert( trx_receipt.status == transaction_receipt::executed, "trx_receipt.status must be executed");
      packed_transaction pkd_trx = std::get<packed_transaction>(trx_receipt.trx);
      transaction trxn = unpack<transaction>( pkd_trx.packed_trx );
      eosio_assert( trxn.actions.size() == 1, "transfer transaction contains more then one action" );

      // validate transaction id
      eosio_assert( std::memcmp(orig_trx_id.hash, pkd_trx.id().hash, 32) == 0, "transaction id mismatch");

      // check transfer action
      action actn = trxn.actions.front();
      transfer_action_type args = unpack<transfer_action_type>( actn.data );

      auto pch = _peerchains.get( from_chain.value );
      eosio_assert( args.to == pch.peerchain_ibc_token_contract, "transfer to account not correct" );
      eosio_assert( args.quantity == quantity, "quantity not equal to quantity within packed transaction" );
      memo_info_type memo_info = get_memo_info( args.memo );

      eosio_assert( to == memo_info.receiver, "to not equal to receiver，which provided in memo string" );
      eosio_assert( is_account( to ), "to account does not exist");
      eosio_assert( memo_info.peerchain == _gstate.this_chain, "peer chain name in orignal trx must be this chain's name");

      // validate merkle path
      verify_merkle_path( orig_trx_merkle_path, trx_receipt.digest() );

      // --- validate with lwc ---
      eosio_assert( orig_trx_block_num <= anchor_block_num, "orig_trx_block_num <= anchor_block_num assert failed");
      if ( orig_trx_block_num < anchor_block_num ){
         block_header orig_trx_block_header = unpack<block_header>( orig_trx_block_header_data );
         eosio_assert( orig_trx_block_header.block_num() == orig_trx_block_num, "orig_trx_block_header.block_num() must equal to orig_trx_block_num");
         eosio_assert( std::memcmp(orig_trx_merkle_path.back().hash, orig_trx_block_header.transaction_mroot.hash, 32) == 0, "transaction_mroot check failed");
         verify_merkle_path( orig_trx_block_id_merkle_path, orig_trx_block_header.id() );
         uint32_t layer = orig_trx_block_id_merkle_path.size() == 1 ? 1 : orig_trx_block_id_merkle_path.size() - 1;
         chain::assert_anchor_block_and_merkle_node( pch.thischain_ibc_chain_contract, anchor_block_num, layer, orig_trx_block_id_merkle_path.back() );
      } else { // orig_trx_block_num < anchor_block_num
         chain::assert_anchor_block_and_transaction_mroot( pch.thischain_ibc_chain_contract, anchor_block_num, orig_trx_merkle_path.back() );
      }

      asset new_quantity;

      if ( actn.account != pch.peerchain_ibc_token_contract ){   // issue peg token to user
         const auto& st = get_currency_stats_by_orig_token_symbol( sym.code() );
         eosio_assert( st.active, "not active");
         eosio_assert( st.peerchain_name == from_chain, "from_chain must equal to st.peerchain_name");
         eosio_assert( st.peerchain_contract == actn.account, " action.account not correct");

         eosio_assert( quantity.is_valid(), "invalid quantity" );
         eosio_assert( quantity.amount > 0, "must issue positive quantity" );
         eosio_assert( quantity.symbol.precision() == st.supply.symbol.precision(), "symbol precision mismatch" );
         eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

         new_quantity = asset( quantity.amount, st.supply.symbol );

         _stats.modify( st, same_payer, [&]( auto& s ) {
            s.supply += new_quantity;
            s.total_issue += new_quantity;
            s.total_issue_times += 1;
         });

         add_balance( _self, new_quantity, _self );
         if( to != _self ) {
            if ( memo_info.notes.size() > 256 ) memo_info.notes.resize( 256 );
            string new_memo = memo_info.notes + " | from peerchain trx_id:" + capi_checksum256_to_string(orig_trx_id) + " "
                              + args.from.to_string() + "(" + quantity.to_string() + ") --ibc-issue--> thischain "
                              + to.to_string() + "(" + new_quantity.to_string() + ")";
            transfer_action_type action_data{ _self, to, new_quantity, new_memo };
            action( permission_level{ _self, "active"_n }, _self, "transfer"_n, action_data ).send();
         }
      } else {  // withdraw accepted token to user
         const auto& acpt = get_currency_accept_by_peg_token_symbol( quantity.symbol.code() );
         eosio_assert( acpt.active, "not active");

         eosio_assert( quantity.is_valid(), "invalid quantity" );
         eosio_assert( quantity.amount > 0, "must issue positive quantity" );
         eosio_assert( quantity.symbol.precision() == acpt.accept.symbol.precision(), "symbol precision mismatch" );
         eosio_assert( quantity.amount <= acpt.accept.amount, "quantity exceeds available accept");

         new_quantity = asset( quantity.amount, acpt.accept.symbol );

         auto _chainassets = chainassets_table( _self, from_chain.value );
         auto itr = _chainassets.find( acpt.accept.symbol.code().raw() );
         eosio_assert( itr != _chainassets.end(), "chain assets not found");
         eosio_assert( itr->balance.amount >= new_quantity.amount, "have no enough chain asset to withdraw");

         _chainassets.modify( itr, same_payer, [&]( auto& chain ) { // calculate before charging fee
            chain.balance -= new_quantity;
         });

         int64_t diff = 0;
         if ( to != pch.thischain_free_account ){
            if ( acpt.service_fee_mode == "fixed"_n ){
               diff = acpt.service_fee_fixed.amount;
            } else {
               diff = int64_t( new_quantity.amount * acpt.service_fee_ratio );
            }
         }

         eosio_assert( diff >= 0, "internal error, service_fee_ratio config error");
         new_quantity.amount = new_quantity.amount > diff ? new_quantity.amount - diff : 1; // 1 is used to avoid withdraw failure
         eosio_assert( new_quantity.amount > 0, "must issue positive quantity" );

         _accepts.modify( acpt, same_payer, [&]( auto& r ) {
            r.accept -= new_quantity;
            r.total_cash += new_quantity;
            r.total_cash_times += 1;
         });

         if( to != _self ) {
            if ( memo_info.notes.size() > 70 ) memo_info.notes.resize( 70 );
            string new_memo = memo_info.notes + " | from peerchain trx_id:" + capi_checksum256_to_string(orig_trx_id) + " "
                              + args.from.to_string() + "(" + quantity.to_string() + ") --ibc-withdraw--> thischain "
                              + to.to_string() + "(" + new_quantity.to_string() + ")";
            if ( new_memo.size() > 250 ) new_memo.resize( 250 );
            transfer_action_type action_data{ _self, to, new_quantity, new_memo };
            action( permission_level{ _self, "active"_n }, acpt.original_contract, "transfer"_n, action_data ).send();
         }
      }

      trim_cashtrxs_table_or_not( from_chain );

      // record to cash table
      auto _cashtrxs = cashtrxs_table( _self, from_chain.value );
      _cashtrxs.emplace( _self, [&]( auto& r ) {
            r.seq_num = seq_num;
            r.block_time_slot = get_block_time_slot();
            r.trx_id = get_trx_id();
            r.action = transfer_action_type{ _self, to, new_quantity, memo };
            r.orig_trx_id = orig_trx_id;
            r.orig_trx_block_num = orig_trx_block_num;
      });
   }

   void token::cashconfirm( const name&                            from_chain,
                            const transaction_id_type&             cash_trx_id,
                            const std::vector<char>&               cash_trx_packed_trx_receipt,
                            const std::vector<capi_checksum256>&   cash_trx_merkle_path,
                            const uint32_t&                        cash_trx_block_num,  // redundant with cash_trx_block_header_data, facilitate indexing and checking
                            const std::vector<char>&               cash_trx_block_header_data,
                            const std::vector<capi_checksum256>&   cash_trx_block_id_merkle_path,
                            const uint32_t&                        anchor_block_num,
                            const transaction_id_type&             orig_trx_id ) {

      auto orig_action_info = get_orignal_action_by_trx_id( from_chain, orig_trx_id );

      const transaction_receipt& trx_receipt = unpack<transaction_receipt>( cash_trx_packed_trx_receipt );
      eosio_assert( trx_receipt.status == transaction_receipt::executed, "trx_receipt.status must be executed");
      packed_transaction pkd_trx = std::get<packed_transaction>(trx_receipt.trx);
      transaction trx = unpack<transaction>( pkd_trx.packed_trx );
      eosio_assert( trx.actions.size() == 1, "cash transaction contains more then one action" );

      // validate cash transaction id
      eosio_assert( std::memcmp(cash_trx_id.hash, pkd_trx.id().hash, 32) == 0, "cash_trx_id mismatch");

      // check issue action
      cash_action_type args = unpack<cash_action_type>( trx.actions.front().data );
      transaction_receipt src_tf_trx_receipt = unpack<transaction_receipt>( args.orig_trx_packed_trx_receipt );
      eosio_assert( src_tf_trx_receipt.status == transaction_receipt::executed, "trx_receipt.status must be executed");
      packed_transaction src_pkd_trx = std::get<packed_transaction>(src_tf_trx_receipt.trx);
      /* transaction src_trx = unpack<transaction>( src_pkd_trx.packed_trx );
      eosio_assert( src_trx.actions.size() == 1, "orignal transaction contains more then one action" ); */
      eosio_assert( std::memcmp(orig_trx_id.hash, src_pkd_trx.id().hash, 32) == 0, "orig_trx_id mismatch" );

      // check cash_seq_num
      auto& pchm = _peerchainm.get( from_chain.value );
      eosio_assert( args.seq_num == pchm.cash_seq_num + 1, "seq_num derived from cash_trx_packed_trx_receipt error");

      // validate merkle path
      verify_merkle_path( cash_trx_merkle_path, trx_receipt.digest());

      // --- validate with lwc ---
      auto pch = _peerchains.get( from_chain.value );
      eosio_assert( cash_trx_block_num <= anchor_block_num, "cash_trx_block_num <= anchor_block_num assert failed");
      if ( cash_trx_block_num < anchor_block_num ){
         block_header cash_trx_block_header = unpack<block_header>( cash_trx_block_header_data );
         eosio_assert( cash_trx_block_header.block_num() == cash_trx_block_num, "cash_trx_block_header.block_num() must equal to cash_trx_block_num");
         eosio_assert( std::memcmp(cash_trx_merkle_path.back().hash, cash_trx_block_header.transaction_mroot.hash, 32) == 0, "transaction_mroot check failed");
         verify_merkle_path( cash_trx_block_id_merkle_path, cash_trx_block_header.id() );
         uint32_t layer = cash_trx_block_id_merkle_path.size() == 1 ? 1 : cash_trx_block_id_merkle_path.size() - 1;
         chain::assert_anchor_block_and_merkle_node( pch.thischain_ibc_chain_contract, anchor_block_num, layer, cash_trx_block_id_merkle_path.back() );
      } else { // cash_trx_block_num < anchor_block_num
         chain::assert_anchor_block_and_transaction_mroot( pch.thischain_ibc_chain_contract, anchor_block_num, cash_trx_merkle_path.back() );
      }
      // if orignal trx is withdraw, burn those token
      if ( orig_action_info.contract == _self ){
         sub_balance( _self, orig_action_info.quantity );
      }

      // remove record in origtrxs table
      erase_record_in_origtrxs_tb_by_trx_id_for_confirmed( from_chain, orig_trx_id );

      _peerchainm.modify( pchm, same_payer, [&]( auto& r ) {
         r.cash_seq_num += 1;
      });
   }

   void token::rollback( name peerchain_name, const transaction_id_type trx_id ){    // notes: if non-rollbackable attacks occurred, such records need to be deleted manually, to prevent RAM consume from being maliciously occupied
      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      auto idx = _origtrxs.get_index<"trxid"_n>();
      auto it = idx.find( fixed_bytes<32>(trx_id.hash) );
      eosio_assert( it != idx.end(), "trx_id not exist");

      eosio_assert( it->block_time_slot + 25 < _peerchainm.get(peerchain_name.value).last_confirmed_orig_trx_block_time_slot, "(block_time_slot + 25 < last_confirmed_orig_trx_block_time_slot) is false");

      transfer_action_info action_info = it->action;
      string memo = "rollback transaction: " + capi_checksum256_to_string(trx_id);
      print( memo.c_str() );

      auto pch = _peerchains.get( peerchain_name.value );

      if ( action_info.contract != _self ){  // rollback ibc transfer
         const auto& acpt = get_currency_accept( action_info.contract );
         _accepts.modify( acpt, same_payer, [&]( auto& r ) {
            r.accept -= action_info.quantity;
            r.total_transfer -= action_info.quantity;
            r.total_transfer_times -= 1;
         });

         if( action_info.from != _self ) {

            asset fee = acpt.failed_fee;
            if ( action_info.from == pch.thischain_free_account )
               fee.amount = 0;
            eosio_assert( fee.amount >= 0, "internal error, service_fee_ratio config error");

            auto final_quantity = asset( action_info.quantity.amount > fee.amount ?  action_info.quantity.amount - fee.amount : 1, action_info.quantity.symbol ); // 1 is used to avoid rollback failure
            transfer_action_type action_data{ _self, action_info.from, final_quantity, memo };
            action( permission_level{ _self, "active"_n }, acpt.original_contract, "transfer"_n, action_data ).send();
         }
      } else { // rollback ibc withdraw
         const auto& st = get_currency_stats( action_info.quantity.symbol.code() );
         _stats.modify( st, same_payer, [&]( auto& r ) {
            r.supply += action_info.quantity;
            r.max_supply += action_info.quantity;
            r.total_withdraw -= action_info.quantity;
            r.total_withdraw_times -= 1;
         });

         if( action_info.from != _self ) {

            asset fee = st.failed_fee;
            if ( action_info.from == pch.thischain_free_account )
               fee.amount = 0;
            eosio_assert( fee.amount >= 0, "internal error, service_fee_ratio config error");

            auto final_quantity = asset( action_info.quantity.amount > fee.amount ?  action_info.quantity.amount - fee.amount : 1, action_info.quantity.symbol ); // 1 is used to avoid rollback failure
            transfer_action_type action_data{ _self, action_info.from, final_quantity, memo };
            action( permission_level{ _self, "active"_n }, _self, "transfer"_n, action_data ).send();
         }
      }

      _origtrxs.erase( _origtrxs.find(it->id) );
   }

   static const uint32_t min_distance = 3600 * 24 * 2;   // one day
   void token::rmunablerb( name peerchain_name, const transaction_id_type trx_id ){
      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      auto idx = _origtrxs.get_index<"trxid"_n>();
      auto it = idx.find( fixed_bytes<32>(trx_id.hash) );
      eosio_assert( it != idx.end(), "trx_id not exist");

      auto pchm = _peerchainm.get( peerchain_name.value );
      eosio_assert( it->block_time_slot + min_distance < pchm.last_confirmed_orig_trx_block_time_slot, "(block_time_slot + min_distance < _gmutable.last_confirmed_orig_trx_block_time_slot) is false");

      _origtrxs.erase( _origtrxs.find(it->id) );

      auto _rmdunrbs = rmdunrbs_table( _self, peerchain_name.value );
      _rmdunrbs.emplace( _self, [&]( auto& r ) {
         r.id        = _rmdunrbs.available_primary_key();
         r.trx_id    = trx_id;
      });
   }

   // this action maybe needed when repairing the ibc system manually
   void token::fcrollback( name peerchain_name, const std::vector<transaction_id_type> trxs ) {
      require_auth( _self );
      eosio_assert( trxs.size() != 0, "no transacton" );
      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );

      for ( const auto& trx_id : trxs ){
         auto idx = _origtrxs.get_index<"trxid"_n>();
         const auto& record = idx.get( fixed_bytes<32>(trx_id.hash) );
         transfer_action_info action_info = record.action;

         if ( action_info.contract != _self ){  // rollback ibc transfer
            const auto& acpt = get_currency_accept( action_info.contract );
            _accepts.modify( acpt, same_payer, [&]( auto& r ) {
               r.accept -= action_info.quantity;
               r.total_transfer -= action_info.quantity;
               r.total_transfer_times -= 1;
            });

            if( action_info.from != _self ) {
               string memo = "rollback transaction: " + capi_checksum256_to_string(trx_id);
               transfer_action_type action_data{ _self, action_info.from, action_info.quantity, memo };
               action( permission_level{ _self, "active"_n }, acpt.original_contract, "transfer"_n, action_data ).send();
            }
         } else { // rollback withdraw
            const auto& st = get_currency_stats( action_info.quantity.symbol.code() );
            _stats.modify( st, same_payer, [&]( auto& r ) {
               r.supply += action_info.quantity;
               r.max_supply += action_info.quantity;
               r.total_withdraw -= action_info.quantity;
               r.total_withdraw_times -= 1;
            });

            if( action_info.from != _self ) {
               string memo = "rollback transaction: " + capi_checksum256_to_string(trx_id);
               transfer_action_type action_data{ _self, action_info.from, action_info.quantity, memo };
               action( permission_level{ _self, "active"_n }, _self, "transfer"_n, action_data ).send();
            }
         }
         _origtrxs.erase( record );
      }
   }

   void token::fcrmorigtrx( name peerchain_name, const std::vector<transaction_id_type> trxs ){
      require_auth( _self );
      eosio_assert( trxs.size() != 0, "no transacton" );

      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      for ( const auto& trx_id : trxs ){
         auto idx = _origtrxs.get_index<"trxid"_n>();
         const auto& record = idx.get( fixed_bytes<32>(trx_id.hash) );
         _origtrxs.erase( record );
      }
   }

   void token::lockall() {
      require_auth( _self );
      eosio_assert( _gstate.active == true, "_gstate.active == false, nothing to do");
      _gstate.active = false;
   }

   void token::unlockall() {
      require_auth( _self );
      eosio_assert( _gstate.active == false,  "_gstate.active == true, nothing to do");
      _gstate.active = true;
   }

   void token::sub_balance( name owner, asset value ) {
      accounts from_acnts( _self, owner.value );

      const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
      eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

      from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
   }

   void token::add_balance( name owner, asset value, name ram_payer )
   {
      accounts to_acnts( _self, owner.value );
      auto to = to_acnts.find( value.symbol.code().raw() );
      if( to == to_acnts.end() ) {
         to_acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = value;
         });
      } else {
         to_acnts.modify( to, same_payer, [&]( auto& a ) {
            a.balance += value;
         });
      }
   }

   void token::open( name owner, const symbol_code& symcode, name ram_payer )
   {
      require_auth( ram_payer );

      auto sym_code_raw = symcode.raw();
      const auto& st = _stats.get( sym_code_raw, "symbol does not exist" );
      accounts acnts( _self, owner.value );
      auto it = acnts.find( sym_code_raw );
      if( it == acnts.end() ) {
         acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = asset{0, st.supply.symbol};
         });
      }
   }

   void token::close( name owner, const symbol_code& symcode )
   {
      require_auth( owner );
      accounts acnts( _self, owner.value );
      auto it = acnts.find( symcode.raw() );
      eosio_assert( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
      eosio_assert( it->balance.amount == 0, "Cannot close because the balance is not zero." );
      acnts.erase( it );
   }

   void token::forceinit( name peerchain_name ) {
      require_auth( _self );

      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      auto _cashtrxs = cashtrxs_table( _self, peerchain_name.value );
      auto _rmdunrbs = rmdunrbs_table( _self, peerchain_name.value );


      eosio_assert( _origtrxs.begin() != _origtrxs.end() ||
                    _cashtrxs.begin() != _cashtrxs.end() ||
                    _rmdunrbs.begin() != _rmdunrbs.end(), "already empty");

      uint32_t count = 0, max_delete_per_time = 200;
      while ( _origtrxs.begin() != _origtrxs.end() && count++ < max_delete_per_time ){
         _origtrxs.erase(_origtrxs.begin());
      }
      while ( _cashtrxs.begin() != _cashtrxs.end() && count++ < max_delete_per_time ){
         _cashtrxs.erase(_cashtrxs.begin());
      }
      while ( _rmdunrbs.begin() != _rmdunrbs.end() && count++ < max_delete_per_time ){
         _rmdunrbs.erase(_rmdunrbs.begin());
      }

      auto itr = _peerchainm.find( peerchain_name.value );
      if ( itr != _peerchainm.end() ){
         _peerchainm.modify( itr, same_payer, [&]( auto& r ) {
            r.cash_seq_num                            = 0;
            r.last_confirmed_orig_trx_block_time_slot = 0;
            r.current_block_time_slot                 = 0;
            r.current_block_trxs                      = 0;
            r.origtrxs_tb_next_id                     = 1;
         });
      }

      if( _origtrxs.begin() == _origtrxs.end() &&
          _cashtrxs.begin() == _cashtrxs.end() &&
          _rmdunrbs.begin() == _rmdunrbs.end() ){
         print( "force initialization complete" );
      } else {
         print( "force initialization not complete" );
      }
   }

   // ---- currency_accept related methods ----
   const token::currency_accept& token::get_currency_accept( name contract ){
      return _accepts.get( contract.value, "token of contract does not support" );
   }

   const token::currency_accept& token::get_currency_accept_by_symbol( symbol_code symcode ){
      auto idx = _accepts.get_index<"origtokensym"_n>();
      return idx.get( symcode.raw(),"token with symbol does not support" );
   }

   const token::currency_accept& token::get_currency_accept_by_peg_token_symbol( symbol_code symcode ){
      auto idx = _accepts.get_index<"pegtokensym"_n>();
      return idx.get( symcode.raw(),"token with symbol does not support" );
   }

   // ---- currency_stats related methods  ----
   const token::currency_stats& token::get_currency_stats( symbol_code symcode ){
      return _stats.get( symcode.raw(), "token with symbol does not exist");
   }

   const token::currency_stats& token::get_currency_stats_by_orig_token_symbol( symbol_code symcode ){
      auto idx = _stats.get_index<"origtokensym"_n>();
      return idx.get( symcode.raw(),"token with symbol does not support" );
   }

   // ---- original_trx_info related methods  ----
   void token::origtrxs_emplace( name peerchain_name, transfer_action_info action ) {
      transaction_id_type trx_id = get_trx_id();
      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      
      auto& pchm = _peerchainm.get( peerchain_name.value );
      _origtrxs.emplace( _self, [&]( auto& r ){
         r.id = pchm.origtrxs_tb_next_id;
         r.block_time_slot = get_block_time_slot();
         r.trx_id = trx_id;
         r.action = action;
      });

      _peerchainm.modify( pchm, same_payer, [&]( auto& r ) {
         r.origtrxs_tb_next_id += 1;
      });
   }

   transfer_action_info token::get_orignal_action_by_trx_id( name peerchain_name, transaction_id_type trx_id ) {
      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      auto idx = _origtrxs.get_index<"trxid"_n>();
      auto itr = idx.find( fixed_bytes<32>(trx_id.hash) );
      eosio_assert( itr != idx.end(), "orig_trx_id not exist");
      return itr->action;
   }

   void token::erase_record_in_origtrxs_tb_by_trx_id_for_confirmed( name peerchain_name, transaction_id_type  trx_id ){
      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      auto idx = _origtrxs.get_index<"trxid"_n>();
      auto it = idx.find( fixed_bytes<32>(trx_id.hash) );
      eosio_assert( it != idx.end(), "trx_id not exit in origtrxs table");

      auto& pchm = _peerchainm.get( peerchain_name.value );
      _peerchainm.modify( pchm, same_payer, [&]( auto& r ) {
         r.last_confirmed_orig_trx_block_time_slot = it->block_time_slot;
      });

      idx.erase(it);
   }

   // ---- cash_trx_info related methods  ----
   void token::trim_cashtrxs_table_or_not( name peerchain_name ) {
      auto _cashtrxs = cashtrxs_table( _self, peerchain_name.value );
      
      uint32_t total = 0;
      if ( _cashtrxs.begin() == _cashtrxs.end()){
         return;
      }
      
      total = (--_cashtrxs.end())->seq_num - _cashtrxs.begin()->seq_num;
      auto pch = _peerchains.get( peerchain_name.value );
      if ( total > pch.cache_cashtrxs_table_records ){
         auto last_orig_trx_block_num = _cashtrxs.rbegin()->orig_trx_block_num;
         int i = 10; // erase 10 records per time
         while ( i-- > 0 ){
            auto first_orig_trx_block_num = _cashtrxs.begin()->orig_trx_block_num;
            if ( last_orig_trx_block_num - first_orig_trx_block_num > 1 ) { // very importand
               _cashtrxs.erase( _cashtrxs.begin() );
            } else {
               break;
            }
         }
      }
   }

   uint64_t token::get_cashtrxs_tb_max_seq_num( name peerchain_name ) {
      auto _cashtrxs = cashtrxs_table( _self, peerchain_name.value );
      if ( _cashtrxs.begin() != _cashtrxs.end() ){
         return _cashtrxs.rbegin()->seq_num;
      }
      return 0;
   }

   uint64_t token::get_cashtrxs_tb_min_orig_trx_block_num( name peerchain_name ) {
      auto _cashtrxs = cashtrxs_table( _self, peerchain_name.value );
      auto idx = _cashtrxs.get_index<"blocknum"_n>();
      if ( idx.begin() != idx.end() ){
         return idx.begin()->orig_trx_block_num;
      }
      return 0;
   }

   uint64_t token::get_cashtrxs_tb_max_orig_trx_block_num( name peerchain_name ) {
      auto _cashtrxs = cashtrxs_table( _self, peerchain_name.value );
      auto idx = _cashtrxs.get_index<"blocknum"_n>();
      if ( idx.begin() != idx.end() ){
         return idx.rbegin()->orig_trx_block_num;
      }
      return 0;
   }

   /**
    * This is a Very Importand Function, Used to Avoid Replay Attack
    */
   bool token::is_orig_trx_id_exist_in_cashtrxs_tb( name peerchain_name, transaction_id_type orig_trx_id ) {
      auto _cashtrxs = cashtrxs_table( _self, peerchain_name.value );
      auto idx = _cashtrxs.get_index<"trxid"_n>();
      auto it = idx.find( fixed_bytes<32>(orig_trx_id.hash) );
      if ( it == idx.end() ) {
         return false;
      }
      return true;
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
            (lockall)(unlockall)(forceinit)(open)(close))
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

