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
         #ifdef HUB
         , _hub_globals( _self, _self.value )
         #endif
   {
      _gstate = _global_state.exists() ? _global_state.get() : global_state{};
      _admin_st = _admin_sg.exists() ? _admin_sg.get() : admin_struct{};
      #ifdef HUB
      _hubgs = _hub_globals.exists() ? _hub_globals.get() : hub_globals{};
      #endif
   }

   token::~token(){
      _global_state.set( _gstate, _self );
      _admin_sg.set( _admin_st , _self );
      #ifdef HUB
      _hub_globals.set( _hubgs, _self );
      #endif
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

   void token::regpeerchain( name           peerchain_name,
                             string         peerchain_info,
                             name           peerchain_ibc_token_contract,
                             name           thischain_ibc_chain_contract,
                             name           thischain_free_account,
                             uint32_t       max_original_trxs_per_block,
                             uint32_t       max_origtrxs_table_records,
                             uint32_t       cache_cashtrxs_table_records,
                             bool           active ){
      check_admin_auth();

      eosio_assert( peerchain_name != name(), "peerchain_name can not be empty");
      eosio_assert( peerchain_info.size() < 256, "peerchain_info has more than 256 bytes");
      eosio_assert( peerchain_ibc_token_contract != name(), "peerchain_ibc_token_contract can not be empty");
      eosio_assert( is_account( thischain_ibc_chain_contract ), "thischain_ibc_chain_contract account does not exist");
      eosio_assert( is_account( thischain_free_account ), "thischain_free_account does not exist");

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

      auto itr_m = _peerchainm.find( peerchain_name.value );
      if ( itr_m != _peerchainm.end() ){ _peerchainm.erase( itr_m );}

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
      check_admin_auth();

      auto& chain = _peerchains.get( peerchain_name.value, "peerchain not registered");
      if ( which == "active" ){
         _peerchains.modify( chain, same_payer, [&]( auto& r ) { r.active = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::regacpttoken( name        original_contract,
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
      check_admin_auth();

      eosio_assert( is_account( original_contract ), "original_contract account does not exist");
      eosio_assert( is_account( administrator ), "administrator account does not exist");

      /// When multiple chains make up a ibc hub system, to avoid unnecessary confusion, symbols should be unique in
      /// the whole multi-chain IBC system
      eosio_assert( _stats.find(max_accept.symbol.code().raw()) == _stats.end() || original_contract == _self,
            "token symbol conflict in table 'stats' and 'accepts'");

      eosio_assert( max_accept.is_valid(), "invalid max_accept");
      eosio_assert( max_accept.amount > 0, "max_accept must be positive");
      auto symbol = max_accept.symbol;
      eosio_assert( min_once_transfer.symbol    == symbol &&
                    max_once_transfer.symbol    == symbol &&
                    max_daily_transfer.symbol   == symbol &&
                    service_fee_fixed.symbol    == symbol &&
                    failed_fee.symbol           == symbol &&
                    min_once_transfer.amount    >  0 &&
                    max_once_transfer.amount    > min_once_transfer.amount &&
                    max_daily_transfer.amount   > max_once_transfer.amount &&
                    service_fee_fixed.amount    >= 0 &&
                    failed_fee.amount           >= 0 , "invalid parameters");

      eosio_assert( organization.size() < 256, "organization has more than 256 bytes");
      eosio_assert( website.size() < 256, "website has more than 256 bytes");

      eosio_assert( service_fee_mode == "fixed"_n || service_fee_mode == "ratio"_n, "invalid service_fee_mode");
      eosio_assert( 0 <= service_fee_ratio && service_fee_ratio <= 0.05 , "invalid service_fee_ratio");

      eosio_assert( service_fee_fixed.amount * 5 <= min_once_transfer.amount, "service_fee_fixed.amount * 5 <= min_once_transfer.amount assert failed");
      eosio_assert( failed_fee.amount * 10 <= min_once_transfer.amount, "failed_fee.amount * 10 <= min_once_transfer.amount assert failed");

      _accepts.emplace( _self, [&]( auto& r ){
         r.original_contract  = original_contract;
         r.accept             = asset{ 0, symbol };
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
         r.failed_fee         = failed_fee;
         r.total_transfer     =  asset{ 0, max_accept.symbol };
         r.total_transfer_times = 0;
         r.total_cash         =  asset{ 0, max_accept.symbol };
         r.total_cash_times   = 0;
         r.active             = active;
      });
   }

   void token::setacptasset( symbol_code symcode, string which, asset quantity ) {
      const auto& acpt = get_currency_accept( symcode );
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

   void token::setacptstr( symbol_code symcode, string which, string value ) {
      const auto& acpt = get_currency_accept( symcode );
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

   void token::setacptint( symbol_code symcode, string which, uint64_t value ) {
      const auto& acpt = get_currency_accept( symcode );
      check_admin_auth();

      if ( which == "max_tfs_per_minute" ){
         eosio_assert( 1 <= value && value <= 500, "max_tfs_per_minute's value must in range [1,500]");
         _accepts.modify( acpt, _self, [&]( auto& r ) { r.max_tfs_per_minute = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setacptbool( symbol_code symcode, string which, bool value ) {
      const auto& acpt = get_currency_accept( symcode );
      require_auth( acpt.administrator );

      if ( which == "active" ){
         _accepts.modify( acpt, acpt.administrator, [&]( auto& r ) { r.active = value; });
         return;
      }
      eosio_assert( false, "unkown config item" );
   }

   void token::setacptfee( symbol_code symcode,
                           name   kind,
                           name   fee_mode,
                           asset  fee_fixed,
                           double fee_ratio ) {
      check_admin_auth();

      const auto& acpt = get_currency_accept( symcode );
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
                            asset       max_supply,
                            asset       min_once_withdraw,
                            asset       max_once_withdraw,
                            asset       max_daily_withdraw,
                            uint32_t    max_wds_per_minute,
                            name        administrator,
                            asset       failed_fee,
                            bool        active ){
      check_admin_auth();
      eosio_assert( is_account( administrator ), "administrator account does not exist");

      eosio_assert( _accepts.find(max_supply.symbol.code().raw()) == _accepts.end(), "token symbol conflict in table 'stats' and 'accepts'");

      eosio_assert( max_supply.is_valid(), "invalid max_supply");
      eosio_assert( max_supply.amount > 0, "max_supply must be positive");
      auto symbol = max_supply.symbol;
      eosio_assert( max_supply.symbol           == symbol &&
                    min_once_withdraw.symbol    == symbol &&
                    max_once_withdraw.symbol    == symbol &&
                    max_daily_withdraw.symbol   == symbol &&
                    failed_fee.symbol           == symbol &&
                    min_once_withdraw.amount    > 0 &&
                    max_once_withdraw.amount    > min_once_withdraw.amount &&
                    max_daily_withdraw.amount   > max_once_withdraw.amount &&
                    failed_fee.amount           >= 0 , "invalid parameters");

      eosio_assert( peerchain_name != name(), "peerchain_name can not be empty");
      eosio_assert( _peerchains.find(peerchain_name.value) != _peerchains.end(), "peerchain has not registered");
      eosio_assert( peerchain_contract != name(), "peerchain_contract can not be empty");

      eosio_assert( failed_fee.amount * 10 <= min_once_withdraw.amount, "failed_fee.amount * 10 <= min_once_withdraw.amount assert failed");

      auto existing = _stats.find( max_supply.symbol.code().raw() );
      eosio_assert( existing == _stats.end(), "token already exist" );

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
         r.failed_fee         = failed_fee;
         r.total_issue        = asset{ 0, max_supply.symbol };
         r.total_issue_times  = 0;
         r.total_withdraw     = asset{ 0, max_supply.symbol };
         r.total_withdraw_times = 0;
         r.active             = active;
      });

      update_stats2( max_supply.symbol.code() );
   }

   void token::setpegasset( symbol_code symcode, string which, asset quantity ) {
      const auto& st = get_currency_stats( symcode );
      eosio_assert( quantity.symbol == st.supply.symbol, "invalid symbol" );
      require_auth( st.administrator );

      if ( which == "max_supply" ){
         eosio_assert( quantity.amount >= st.supply.amount, "max_supply.amount should not less then supply.amount");
         _stats.modify( st, same_payer, [&]( auto& r ) { r.max_supply = quantity; });
         update_stats2( quantity.symbol.code() );
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
      check_admin_auth();

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
      check_admin_auth();

      const auto& st = get_currency_stats( symcode );
      eosio_assert( fee.symbol == st.supply.symbol && fee.amount >= 0, "service_fee_fixed invalid" );
      eosio_assert( fee.amount * 10 <= st.min_once_withdraw.amount, "failed_fee.amount * 10 <= min_once_withdraw.amount assert failed");

      _stats.modify( st, same_payer, [&]( auto& r ) {
         r.failed_fee   = fee;
      });
   }

   void token::unregtoken( name table, symbol_code sym_code ){
      check_admin_auth();

      if ( table == "all"_n ){
         auto ptr1 = _accepts.find( sym_code.raw() );
         if ( ptr1 != _accepts.end() ){
            _accepts.erase( ptr1 );
         }

         auto ptr2 = _stats.find( sym_code.raw() );
         if ( ptr2 != _stats.end() ){
            _stats.erase( ptr2 );
         }
      }

      if ( table == "accepts"_n ){
         const auto& acpt = get_currency_accept( sym_code );
         _accepts.erase( acpt );
         return;
      }

      if ( table == "stats"_n ){
         const auto& st = get_currency_stats( sym_code );
         _stats.erase( st );
         return;
      }

      eosio_assert(false, "parameter table must be empty string or accepts or stats");
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
      pos = memo.find_first_not_of("abcdefghijklmnopqrstuvwxyz012345");
      if ( pos == std::string::npos ){
         info.peerchain = name( memo );
         info.notes = "";
      } else {
         info.peerchain = name( memo.substr(0,pos) );
         info.notes = memo.substr( pos ); // important: not + 1
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
      auto pch = _peerchains.get( info.peerchain.value, "peerchain not registered");

      // check chain active
      eosio_assert( pch.active, "peer chain is not active");

      const auto& acpt = get_currency_accept( quantity.symbol.code() );
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
    *
    *
    * To support ibc-hub feature, a new transfer()->cash()->cashconfirm() protocol must be added.
    * for one token which is originally issued on the hub-chain in one contract.
    * let's assume:
    *   there are three blockchain:chaina chainb and chainc, and chaina be the hub chain
    *   one token of chaina is registered to use ibc-hub, the token contract is 'eosio.token' and symbol is 'TOA'
    *   the ibc.token contract on chaina is 'bosibc.io', and the hub account is 'hub.io'
    * the new protocol must accept the following two transfer action be the same.
    *
    * $cleos_a push action bosibc.io   transfer [ hub.io <to> <quantity> <memo>] -p <any account's authority>
    * $cleos_a push action eosio.token transfer [ hub.io <to> <quantity> <memo>] -p hub.io
    *
    *
    * doing this is because the first action doesn't need the authority of hub.io, but the second one need it,
    * this will allown any one can push the second stage of a hub transaction.
    */
   void token::transfer( name    from,
                         name    to,
                         asset   quantity,
                         string  memo )
   {
      eosio_assert( is_account( to ), "to account does not exist");
      eosio_assert( from != to, "cannot transfer to self" );
      eosio_assert( memo.size() <= 512, "memo has more than 512 bytes" );
      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
      auto sym = quantity.symbol.code();

      bool transfered = false;

#ifndef HUB
      require_auth( from );
      const auto& st = _stats.get( sym.raw(), "symbol(token) not registered");
      eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
#else
      if ( ! _hubgs.is_open ){
         require_auth( from );
         const auto& st = _stats.get( sym.raw(), "symbol(token) not registered");
         eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
      } else {
         eosio_assert( _hubgs.hub_account != _self, "hub_account cannot be _self" );
         if ( from != _hubgs.hub_account ){
            require_auth( from );
         }else {
            eosio_assert( to == _self, "the to account of hub transfer must be _self");
            ibc_transfer_from_hub( to, quantity, memo );

            auto itr = _accepts.find(sym.raw());
            if( itr != _accepts.end() && _stats.find(sym.raw()) == _stats.end() ) {
               eosio_assert( quantity.symbol == itr->accept.symbol, "symbol precision mismatch" );
               transfered = true;
            }
         }
      }
#endif
      /// --- ibc related logic ---
      if (  to == _self && memo.find("local") != 0 ) {   /// @tag 1: important 'to == _self' logic, avoid inline invoke action 'transfer_notify' or 'withdraw'
         auto info = get_memo_info( memo );
         eosio_assert( info.receiver != name(), "receiver not provide");

         if ( info.peerchain == _gstate.this_chain ){ // The purpose of this logic is to unify the action call format
            eosio_assert( _stats.find(sym.raw()) != _stats.end(), "token symbol not found in table stats");
            require_recipient( from );
            require_recipient( info.receiver );

            auto payer = has_auth( info.receiver ) ? info.receiver : from;
            sub_balance( from, quantity );
            add_balance( info.receiver, quantity, payer );
            return;
         }

         auto pch = _peerchains.get( info.peerchain.value, "peerchain not registered");
         eosio_assert( pch.active, "peer chain is not active");

         auto itr = _stats.find(sym.raw());
         if ( itr != _stats.end() && info.peerchain == itr->peerchain_name ){
            withdraw( from, info.peerchain, info.receiver, quantity, info.notes );
         } else {
            transfer_notify( _self, from, _self, quantity, memo );
         }
      }

      if ( transfered == false ) {
         /// --- original logic ---
         require_recipient( from );
         require_recipient( to );

         auto payer = has_auth( to ) ? to : from;
         sub_balance( from, quantity );
         add_balance( to, quantity, payer );
      }
   }

   void token::withdraw( name from, name peerchain_name, name peerchain_receiver, asset quantity, string memo ) {
      // check global state
      eosio_assert( _gstate.active, "global not active" );

      const auto& st = get_currency_stats( quantity.symbol.code() );
      eosio_assert( st.active, "not active");
      eosio_assert( peerchain_name == st.peerchain_name, (string("peerchain_name must be ") + st.peerchain_name.to_string()).c_str());

      eosio_assert( quantity.symbol == st.supply.symbol, "symbol does not match");
      eosio_assert( quantity.amount >= st.min_once_withdraw.amount, "quantity less then min_once_transfer");
      eosio_assert( quantity.amount <= st.max_once_withdraw.amount, "quantity greater then max_once_transfer");

      const auto& balance = get_balance( _self, from, quantity.symbol.code() );
      eosio_assert( quantity.amount <= balance.amount, "overdrawn balance1");

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

      origtrxs_emplace( peerchain_name, transfer_action_info{ _self, from, quantity } );

      update_stats2( quantity.symbol.code() );
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
                     const string&                          memo,
                     const name&                            relay ) {
      auto pch = _peerchains.get( from_chain.value, "from_chain not registered");
      chain::require_relay_auth( pch.thischain_ibc_chain_contract, relay );

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

      // check action parameters
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
      /**
       * 'ibc_transfer' means send a token from its original issued chain to its peg-token chain.
       * 'ibc_withdraw' means send a token from its  peg-token chain to its original issued chain.
       *
       * If the symbol code is not registered in table '_stats', this is a ibc_withdraw.
       * If the symbol code is registered in table '_stats', means that it must be a pegtoken, then check whether the
       * original chain of the symbol recorded in table '_stats' is same with parameter 'from_chain' of the
       * action 'cash(...)', if they are the same, it's ibc_transfer, otherwise, it's ibc_withdraw.
       */
      bool ibc_transfer = false;
      {
         auto itr = _stats.find( sym.code().raw() );
         if ( itr != _stats.end() && from_chain == itr->peerchain_name ){
            ibc_transfer = true;
         }
      }

      bool from_free_account = false;
      auto itr2 = _freeaccount.find( from_chain.value);
      if ( itr2 != _freeaccount.end() && args.from == itr2->peerchain_account ){
         from_free_account = true;
      }
      
      if ( ibc_transfer ){   // issue peg token to user
         const auto& st = get_currency_stats( sym.code() );
         eosio_assert( st.active, "not active");
         eosio_assert( st.peerchain_name == from_chain, "from_chain must equal to st.peerchain_name");
         eosio_assert( actn.account == st.peerchain_contract || actn.account == pch.peerchain_ibc_token_contract,
               "action.account not equal to st.peerchain_contract or pch.peerchain_ibc_token_contract.");

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
         if( to != _self ) {  /// @tag 1: important 'to != _self' logic, avoid inline invoke action 'transfer_notify' or 'withdraw'
            if ( memo_info.notes.size() > 250 ) memo_info.notes.resize( 250 );
            transfer_action_type action_data{ _self, to, new_quantity, memo_info.notes };
            action( permission_level{ _self, "active"_n }, _self, "transfer"_n, action_data ).send();
         }

         update_stats2( st.supply.symbol.code() );
      } else {  // withdraw accepted token to user
         const auto& acpt = get_currency_accept( quantity.symbol.code() );
         eosio_assert( acpt.active, "not active");
         eosio_assert( pch.peerchain_ibc_token_contract == actn.account, "action.account not equal to pch.peerchain_ibc_token_contract.");

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
         if ( to != pch.thischain_free_account && (! from_free_account) ){
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

         if( to != _self ) {  /// @tag 1: important 'to != _self' logic, avoid inline invoke action 'transfer_notify' or 'withdraw'
            bool jump = false;
            #ifdef HUB
            if ( _hubgs.is_open && to == _hubgs.hub_account && acpt.original_contract != _self ){ jump = true; }
            #endif

            if ( ! jump ){
               if ( memo_info.notes.size() > 250 ) memo_info.notes.resize( 250 );
               transfer_action_type action_data{ _self, to, new_quantity, memo_info.notes };
               action( permission_level{ _self, "active"_n }, acpt.original_contract, "transfer"_n, action_data ).send();
            }
         }
      }

      #ifdef HUB
      if ( _hubgs.is_open && to == _hubgs.hub_account ){
         ibc_cash_to_hub( seq_num, from_chain, args.from, orig_trx_id, new_quantity, memo_info.notes, from_free_account );
      }
      #endif

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

      auto pch = _peerchains.get( from_chain.value, "from_chain not registered");
      eosio_assert( trx.actions.front().account == pch.peerchain_ibc_token_contract, "trx.actions.front().account == pch.peerchain_ibc_token_contract assert failed");

      // validate cash transaction id
      eosio_assert( std::memcmp(cash_trx_id.hash, pkd_trx.id().hash, 32) == 0, "cash_trx_id mismatch");

      // check issue action
      cash_action_type args = unpack<cash_action_type>( trx.actions.front().data );
      transaction_receipt src_tf_trx_receipt = unpack<transaction_receipt>( args.orig_trx_packed_trx_receipt );
      eosio_assert( src_tf_trx_receipt.status == transaction_receipt::executed, "trx_receipt.status must be executed");
      packed_transaction src_pkd_trx = std::get<packed_transaction>(src_tf_trx_receipt.trx);
      transaction src_trx = unpack<transaction>( src_pkd_trx.packed_trx );
      eosio_assert( src_trx.actions.size() == 1, "orignal transaction contains more then one action" );
      eosio_assert( std::memcmp(orig_trx_id.hash, src_pkd_trx.id().hash, 32) == 0, "orig_trx_id mismatch" );

      transfer_action_type src_trx_args = unpack<transfer_action_type>( src_trx.actions.front().data );

      // check cash_seq_num
      auto& pchm = _peerchainm.get( from_chain.value, "from_chain not registered");
      eosio_assert( args.seq_num == pchm.cash_seq_num + 1, "seq_num derived from cash_trx_packed_trx_receipt error");

      // validate merkle path
      verify_merkle_path( cash_trx_merkle_path, trx_receipt.digest());

      // --- validate with lwc ---
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

      /**
       * If the symbol code is not registered in table '_stats', the orig_trx must be a ibc_transfer.
       * If the symbol code is registered in table '_stats', means that it must be a pegtoken, then check whether the
       * original chain of the symbol recorded in table '_stats' is same with to_chain of the orig_trx's, to_chain is
       * record in the orig_trx's first action's memo string. if they are the same, it's ibc_withdraw, otherwise, it's ibc_transfer.
       */

      /// if orignal trx is withdraw, burn those token
      auto memo_info = get_memo_info( src_trx_args.memo );
      bool ibc_withdraw = false;
      auto sym_code_raw = src_trx_args.quantity.symbol.code().raw();
      auto itr = _stats.find( sym_code_raw );
      if ( itr != _stats.end() && memo_info.peerchain == itr->peerchain_name ){
         ibc_withdraw = true;
      }

      if ( itr == _stats.end()){
         auto acpt = _accepts.get( sym_code_raw );
         auto account = src_trx.actions.front().account;
         eosio_assert( account == _self || account == acpt.original_contract, "account == _self || account == acpt.original_contract assert failed");
      }

      if ( ibc_withdraw ){
         sub_balance( _self, orig_action_info.quantity );
      }

      // remove record in origtrxs table
      erase_record_in_origtrxs_tb_by_trx_id_for_confirmed( from_chain, orig_trx_id );

      _peerchainm.modify( pchm, same_payer, [&]( auto& r ) {
         r.cash_seq_num += 1;
      });

      #ifdef HUB
      if ( _hubgs.is_open && src_trx_args.from == _hubgs.hub_account ){
         delete_by_hub_trx_id( orig_trx_id );
      }
      #endif
   }

   void token::rollback( name peerchain_name, const transaction_id_type trx_id, name relay ){    // notes: if non-rollbackable attacks occurred, such records need to be deleted manually, to prevent RAM consume from being maliciously occupied
      auto pch = _peerchains.get( peerchain_name.value );
      chain::require_relay_auth( pch.thischain_ibc_chain_contract, relay );

      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      auto idx = _origtrxs.get_index<"trxid"_n>();
      auto it = idx.find( fixed_bytes<32>(trx_id.hash) );
      eosio_assert( it != idx.end(), "trx_id not exist");

      eosio_assert( it->block_time_slot + 25 < _peerchainm.get(peerchain_name.value).last_confirmed_orig_trx_block_time_slot, "(block_time_slot + 25 < last_confirmed_orig_trx_block_time_slot) is false");

      transfer_action_info action_info = it->action;
      string memo = "rollback transaction: " + capi_checksum256_to_string(trx_id);
      print( memo.c_str() );

      asset final_quantity(0,action_info.quantity.symbol);

      bool ibc_withdraw = false;
      auto sym_code_raw = action_info.quantity.symbol.code().raw();
      auto itr = _stats.find( sym_code_raw );
      if ( itr != _stats.end() && peerchain_name == itr->peerchain_name ){
         ibc_withdraw = true;
      }

      if ( ! ibc_withdraw ){  // rollback ibc transfer
         const auto& acpt = get_currency_accept(action_info.quantity.symbol.code());
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

            final_quantity = asset( action_info.quantity.amount > fee.amount ?  action_info.quantity.amount - fee.amount : 1, action_info.quantity.symbol ); // 1 is used to avoid rollback failure
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

            final_quantity = asset( action_info.quantity.amount > fee.amount ?  action_info.quantity.amount - fee.amount : 1, action_info.quantity.symbol ); // 1 is used to avoid rollback failure
            transfer_action_type action_data{ _self, action_info.from, final_quantity, memo };
            action( permission_level{ _self, "active"_n }, _self, "transfer"_n, action_data ).send();
         }

         update_stats2( st.supply.symbol.code() );
      }

      _origtrxs.erase( _origtrxs.find(it->id) );

      #ifdef HUB
      if ( _hubgs.is_open ){
         rollback_hub_trx( trx_id, final_quantity );
      }
      #endif
   }

   static const uint32_t min_distance = 3600 * 24 * 2 * 7;   // one day * 7 = one week
   void token::rmunablerb( name peerchain_name, const transaction_id_type trx_id, name relay ){
      auto pch = _peerchains.get( peerchain_name.value );
      chain::require_relay_auth( pch.thischain_ibc_chain_contract, relay );

      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      auto idx = _origtrxs.get_index<"trxid"_n>();
      auto it = idx.find( fixed_bytes<32>(trx_id.hash) );
      eosio_assert( it != idx.end(), "trx_id not exist");

      auto pchm = _peerchainm.get( peerchain_name.value, "peerchain not found");
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
      check_admin_auth();
      eosio_assert( trxs.size() != 0, "no transacton" );
      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );

      for ( const auto& trx_id : trxs ){
         auto idx = _origtrxs.get_index<"trxid"_n>();
         const auto& record = idx.get( fixed_bytes<32>(trx_id.hash), "trx_id not found");
         transfer_action_info action_info = record.action;

         bool ibc_withdraw = false;
         auto sym_code_raw = action_info.quantity.symbol.code().raw();
         auto itr = _stats.find( sym_code_raw );
         if ( itr != _stats.end() && peerchain_name == itr->peerchain_name ){
            ibc_withdraw = true;
         }

         if ( ! ibc_withdraw ){  // rollback ibc transfer
            const auto& acpt = get_currency_accept(action_info.quantity.symbol.code());
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

            update_stats2( st.supply.symbol.code() );
         }
         _origtrxs.erase( record );
      }
   }

   void token::fcrmorigtrx( name peerchain_name, const std::vector<transaction_id_type> trxs ){
      check_admin_auth();
      eosio_assert( trxs.size() != 0, "no transacton" );

      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      for ( const auto& trx_id : trxs ){
         auto idx = _origtrxs.get_index<"trxid"_n>();
         const auto& record = idx.get( fixed_bytes<32>(trx_id.hash), "trx_id not found");
         _origtrxs.erase( record );
      }
   }

   void token::lockall() {
      check_admin_auth();
      eosio_assert( _gstate.active == true, "_gstate.active == false, nothing to do");
      _gstate.active = false;
   }

   void token::unlockall() {
      check_admin_auth();
      eosio_assert( _gstate.active == false,  "_gstate.active == true, nothing to do");
      _gstate.active = true;
   }

   void token::sub_balance( name owner, asset value ) {
      accounts from_acnts( _self, owner.value );

      const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
      eosio_assert( from.balance.amount >= value.amount, "overdrawn balance2" );
      auto payer = owner;
      #ifdef HUB
      if ( _hubgs.is_open && payer == _hubgs.hub_account ){ payer = _self; }
      #endif
      from_acnts.modify( from, payer, [&]( auto& a ) {
         a.balance -= value;
      });
   }

   void token::add_balance( name owner, asset value, name ram_payer )
   {
      accounts to_acnts( _self, owner.value );
      auto to = to_acnts.find( value.symbol.code().raw() );
      if( to == to_acnts.end() ) {
         #ifdef HUB
         if ( _hubgs.is_open && ram_payer == _hubgs.hub_account ){ ram_payer = _self; }
         #endif
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

   void token::setfreeacnt( name peerchain_name, name account ){
      check_admin_auth();

      auto itr = _freeaccount.find( peerchain_name.value );
      if ( itr != _freeaccount.end() ) {
         _freeaccount.modify( itr, same_payer, [&]( auto& r ) {
            r.peerchain_account = account;
         });
      } else {
         _freeaccount.emplace( _self, [&]( auto& r ){
            r.peerchain_name = peerchain_name;
            r.peerchain_account = account;
         });
      }
   }

   void token::forceinit( name peerchain_name ) {
      check_admin_auth();

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
   const token::currency_accept& token::get_currency_accept( symbol_code symcode ){
      return _accepts.get( symcode.raw(), "token with symbol does not support" );
   }

   // ---- currency_stats related methods  ----
   const token::currency_stats& token::get_currency_stats( symbol_code symcode ){
      return _stats.get( symcode.raw(), "token with symbol does not exist");
   }

   // ---- original_trx_info related methods  ----
   void token::origtrxs_emplace( name peerchain_name, transfer_action_info action ) {
      transaction_id_type trx_id = get_trx_id();
      auto _origtrxs = origtrxs_table( _self, peerchain_name.value );
      
      auto& pchm = _peerchainm.get( peerchain_name.value, "peerchain not found");
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

   void token::update_stats2( symbol_code sym_code ){
      const auto& st1 = get_currency_stats( sym_code );

      stats2 _stats2( _self, sym_code.raw() );
      auto itr = _stats2.find( sym_code.raw() );

      if ( itr == _stats2.end() ){
         _stats2.emplace( _self, [&]( auto& s ) {
            s.supply        = st1.supply;
            s.max_supply    = st1.max_supply;
            s.issuer        = _self;
         });
      } else {
         _stats2.modify( itr, same_payer, [&]( auto& s ) {
            s.supply        = st1.supply;
            s.max_supply    = st1.max_supply;
         });
      }
   }

#ifdef HUB
   /// ----- hub related functions -----
   const string error_info = "for the transfer action to the hub accout,it's memo string format "
                       "must be: <account>@<hub_chain_name> >> <accout>@<dest_chain_name> [optional user defined string]";

   void token::ibc_cash_to_hub( const uint64_t&                 cash_seq_num,
                                const name&                     from_chain,
                                const name&                     from_account,
                                const transaction_id_type&      orig_trx_id,
                                const asset&                    quantity,
                                const string&                   memo,
                                bool                            from_free_account){

      /// parse memo string
      string tmp_memo = memo;
      trim( tmp_memo );
      eosio_assert( tmp_memo.find(">>") == 0, error_info.c_str() );
      tmp_memo = tmp_memo.substr(2);
      trim( tmp_memo );
      auto memo_info = get_memo_info( tmp_memo );

      /// assert ...
      eosio_assert(memo_info.receiver != name(),"receiver not provide");
      eosio_assert(memo_info.peerchain != from_chain, "can not hub transfer to it's original chain");
      eosio_assert(memo_info.peerchain != _gstate.this_chain, "can not hub transfer to the hub-chain itself");
      auto pch = _peerchains.get( memo_info.peerchain.value, "dest chain has not registered");
      eosio_assert(pch.active, "dest chain is not active");

      /// --- get mini_to_quantity ---
      const auto& acpt = get_currency_accept( quantity.symbol.code() );
      eosio_assert( acpt.active, "not active");

      int64_t diff = 0;
      if ( ! from_free_account ){
         if ( acpt.service_fee_mode == "fixed"_n ){
            diff = acpt.service_fee_fixed.amount;
         } else {
            diff = int64_t( quantity.amount * acpt.service_fee_ratio );
         }
      }

      eosio_assert( diff >= 0 && diff < quantity.amount, "diff >= 0 && diff < quantity.amount assert failed");
      asset mini_to_quantity{quantity.amount - diff, quantity.symbol};

      eosio_assert( mini_to_quantity >= acpt.min_once_transfer, "mini_to_quantity >= acpt.min_once_transfer assert failed" );

      auto ptr = _stats.find(quantity.symbol.code().raw());
      if ( ptr != _stats.end() ){
         eosio_assert( mini_to_quantity >= ptr->min_once_withdraw, "mini_to_quantity >= ptr->min_once_withdraw assert failed" );
      }

      /// record to hub table
      auto _hubtrxs = hubtrxs_table( _self, _self.value );
      auto p_id = _hubtrxs.available_primary_key();
      p_id = p_id == 0 ? 1 : p_id;
      _hubtrxs.emplace( _self, [&]( auto& r ) {
         r.id                 = p_id; /// can not use cash_seq_num,
         r.cash_time_slot     = get_block_time_slot();
         r.from_chain         = from_chain;
         r.from_account       = from_account;
         r.from_quantity      = quantity;
         r.mini_to_quantity   = mini_to_quantity;
         r.orig_trx_id        = orig_trx_id;
         r.to_chain           = memo_info.peerchain ;
         r.to_account         = memo_info.receiver;
         r.orig_pure_memo     = memo_info.notes;
         r.to_quantity        = asset{0,quantity.symbol};
         r.fee_receiver       = name();
         r.hub_trx_id         = capi_checksum256();
         r.hub_trx_time_slot  = 0;
         r.forward_times      = 0;
         r.backward_times     = 0;
      });

      /// check max unfinished hub trxs
      eosio_assert(_hubgs.unfinished_trxs < max_hub_unfinished_trxs, "to much unfinished hub trxs");
      _hubgs.unfinished_trxs += 1;
   }

   string get_value_str_by_key_str(const string& src, const string& key_str ){
      string src_str = src;
      string value_str;
      auto pos = src_str.find( key_str );
      if ( pos == std::string::npos ){ return string(); }

      src_str = src_str.substr(pos + key_str.length());
      trim( src_str );
      pos = src_str.find("=");
      if ( pos == std::string::npos ){ return string(); }

      src_str = src_str.substr(1);
      trim( src_str );
      pos = src_str.find(' ');
      if ( pos == std::string::npos ){
         value_str = src_str;
      } else {
         value_str = src_str.substr(0,pos);
      }

      return value_str;
   }

   const string error_info2 = "for the transfer action from the hub accout,it's memo string format "
                             "must be: <account>@<dest_chain_name> orig_trx_id=<trx_id> [worker=account] [optional user defined string]";

   void token::ibc_transfer_from_hub( const name& to, const asset& quantity, const string& memo  ){
      /// --- check to ---
      eosio_assert( to == _self, "the to account of hub transfer must be _self");

      /// --- check memo string ---
      /// 1. parse memo string
      string tmp_memo = memo;
      auto memo_info = get_memo_info( tmp_memo );

      /// 2. get orig_trx_id
      string value_str = get_value_str_by_key_str( memo, "orig_trx_id");
      eosio_assert( value_str.size() != 0, error_info2.c_str());
      eosio_assert( value_str.size() == 64, "orig_trx_id value not valid");
      capi_checksum256 orig_trx_id = string_to_capi_checksum256( value_str );

      /// 3. get hubtrxs table recored
      auto _hubtrxs = hubtrxs_table( _self, _self.value );
      auto idx = _hubtrxs.get_index<"origtrxid"_n>();
      const auto& hub_trx_p = idx.find(fixed_bytes<32>(orig_trx_id.hash));
      eosio_assert( hub_trx_p != idx.end(), "original transaction not found with the specified id");

      /// 4. check ...
      eosio_assert( std::memcmp(hub_trx_p->hub_trx_id.hash, capi_checksum256().hash, 32) == 0 &&
                    hub_trx_p->hub_trx_time_slot == 0, "hub trx can not double spend!");

      if ( memo_info.peerchain == hub_trx_p->to_chain ) {
         eosio_assert( memo_info.receiver == hub_trx_p->to_account, "hub trx must transfer to it's dest account");
         _hubtrxs.modify( *hub_trx_p, same_payer, [&]( auto& r ) { r.forward_times += 1; });
      } else if ( memo_info.peerchain == hub_trx_p->from_chain ) {
         eosio_assert( memo_info.receiver == hub_trx_p->from_account, "hub trx must transfer to it's original account");
         auto slot = get_block_time_slot();
         eosio_assert( slot - hub_trx_p->cash_time_slot > 240, "you can't transfer hub trx back to it's original chain within two minutes");
         _hubtrxs.modify( *hub_trx_p, same_payer, [&]( auto& r ) { r.backward_times += 1; });
      } else {
         eosio_assert(false, "hub trx must transfer to it's dest chain or original chain");
      }

      /// --- check quantity ---
      eosio_assert(quantity.symbol == hub_trx_p->from_quantity.symbol, "quantity.symbol == hub_trx_p->from_quantity.symbol assert failed");
      eosio_assert(hub_trx_p->from_quantity >= quantity && quantity >= hub_trx_p->mini_to_quantity, "quantity must in range [from_quantity,mini_to_quantity]");

      /// transfer fee to receiver
      string worker = get_value_str_by_key_str( memo, "worker");
      name receiver = name();
      if ( worker.size() ){
         receiver = name(worker);
         eosio_assert(is_account(receiver), "worker account does not exist");
      }

      /// recored to hubtrxs table
      _hubtrxs.modify( *hub_trx_p, same_payer, [&]( auto& r ) {
         r.to_quantity        = quantity;
         r.fee_receiver       = receiver;
         r.hub_trx_id         = get_trx_id();
         r.hub_trx_time_slot  = get_block_time_slot();
      });
   }

   void token::rollback_hub_trx( const transaction_id_type& hub_trx_id, asset quantity ){
      auto _hubtrxs = hubtrxs_table( _self, _self.value );
      auto idx = _hubtrxs.get_index<"hubtrxid"_n>();
      const auto& hub_trx_p = idx.find(fixed_bytes<32>(hub_trx_id.hash));
      if( hub_trx_p != idx.end()){
         auto diff = hub_trx_p->from_quantity - hub_trx_p->mini_to_quantity;
         auto mini_to_quantity = quantity;
         if ( quantity.amount > diff.amount ){
            mini_to_quantity = quantity - diff;
         }

         _hubtrxs.modify( *hub_trx_p, same_payer, [&]( auto& r ) {
            r.from_quantity      = quantity;
            r.mini_to_quantity   = mini_to_quantity;
            r.to_quantity.amount = 0;
            r.fee_receiver       = name();
            r.hub_trx_id         = capi_checksum256();
            r.hub_trx_time_slot  = 0;
         });
      }
   }

   void token::rbkdiehubtrx( const transaction_id_type& hub_trx_id ){
      check_admin_auth();

      auto _hubtrxs = hubtrxs_table( _self, _self.value );
      auto idx = _hubtrxs.get_index<"hubtrxid"_n>();
      const auto& hub_trx_p = idx.find(fixed_bytes<32>(hub_trx_id.hash));
      eosio_assert(hub_trx_p != idx.end(), "hub_trx_id not exist!");

      auto _origtrxs = origtrxs_table( _self, hub_trx_p->to_chain.value );
      auto idx2 = _origtrxs.get_index<"trxid"_n>();
      auto it = idx2.find( fixed_bytes<32>(hub_trx_id.hash) );
      eosio_assert( it == idx2.end(), "original trx still exist!");

      string memo = "rollback hub transaction: " + capi_checksum256_to_string(hub_trx_id);

      bool ibc_withdraw = false;
      auto sym_code_raw = hub_trx_p->to_quantity.symbol.code().raw();
      auto itr = _stats.find( sym_code_raw );
      if ( itr != _stats.end() && hub_trx_p->to_chain == itr->peerchain_name ){
         ibc_withdraw = true;
      }

      if ( ! ibc_withdraw ){  // rollback ibc transfer
         const auto& acpt = get_currency_accept(hub_trx_p->to_quantity.symbol.code());
         _accepts.modify( acpt, same_payer, [&]( auto& r ) {
            r.accept -= hub_trx_p->to_quantity;
            r.total_transfer -= hub_trx_p->to_quantity;
            r.total_transfer_times -= 1;
         });

         if ( acpt.original_contract == _self ){
            transfer_action_type action_data{ _self, _hubgs.hub_account, hub_trx_p->to_quantity, memo };
            action( permission_level{ _self, "active"_n }, _self, "transfer"_n, action_data ).send();
         }
      } else { // rollback ibc withdraw
         const auto& st = get_currency_stats( hub_trx_p->to_quantity.symbol.code() );
         _stats.modify( st, same_payer, [&]( auto& r ) {
            r.supply += hub_trx_p->to_quantity;
            r.max_supply += hub_trx_p->to_quantity;
            r.total_withdraw -= hub_trx_p->to_quantity;
            r.total_withdraw_times -= 1;
         });

         transfer_action_type action_data{ _self, _hubgs.hub_account, hub_trx_p->to_quantity, memo };
         action( permission_level{ _self, "active"_n }, _self, "transfer"_n, action_data ).send();

         update_stats2( st.supply.symbol.code() );
      }

      _hubtrxs.modify( *hub_trx_p, same_payer, [&]( auto& r ) {
         r.to_quantity.amount = 0;
         r.fee_receiver       = name();
         r.hub_trx_id         = capi_checksum256();
         r.hub_trx_time_slot  = 0;
      });
   }

   void token::delete_by_hub_trx_id( const transaction_id_type& hub_trx_id ){
      auto _hubtrxs = hubtrxs_table( _self, _self.value );
      auto idx = _hubtrxs.get_index<"hubtrxid"_n>();
      auto hub_trx_p = idx.find(fixed_bytes<32>(hub_trx_id.hash));
      if( hub_trx_p == idx.end()){
         return;
      }

      /// transfer fee to receiver
      const auto& acpt = get_currency_accept( hub_trx_p->from_quantity.symbol.code() );
      auto fee = hub_trx_p->from_quantity - hub_trx_p->to_quantity;
      auto receiver = hub_trx_p->fee_receiver;

      if ( receiver == name() || (! is_account(receiver))){
         receiver = _self;
      }

      if ( fee.amount > 0 ){
         if ( acpt.original_contract == _self ){
            const auto& balance = get_balance( _self, _hubgs.hub_account, hub_trx_p->from_quantity.symbol.code() );
            if ( balance >= fee ){
               transfer_action_type action_data{ _hubgs.hub_account, receiver, fee, "hub trx fee"};
               action( permission_level{ _self, "active"_n }, _self, "feetransfer"_n, action_data ).send();
            }
         } else {
            if ( receiver != _self && receiver != _hubgs.hub_account ){
               transfer_action_type action_data{ _self, receiver, fee, "hub trx fee"};
               action( permission_level{ _self, "active"_n }, acpt.original_contract, "transfer"_n, action_data ).send();
            }
         }
      }

      /// delete
      _hubtrxs.erase( *hub_trx_p );
      _hubgs.unfinished_trxs -= 1;
   }

   void token::hubinit( name hub_account ){
      check_admin_auth();
      eosio_assert( _hubgs.is_open == false, "already init");
      _hubgs.is_open = true;
      _hubgs.hub_account = hub_account;
   }

   void token::feetransfer( name from, name to, asset quantity, string memo ){
      require_auth( _self );
      eosio_assert( from == _hubgs.hub_account, "from == _hubgs.hub_account assert failed");
      sub_balance( _hubgs.hub_account, quantity );
      add_balance( to, quantity, _self );
   }

    void token::regpegtoken2( name        peerchain_name,
                              name        peerchain_contract,  // the original token contract on peer chain
                              asset       max_supply,
                              asset       min_once_transfer,
                              asset       max_once_transfer,
                              asset       max_daily_transfer,
                              uint32_t    max_tfs_per_minute,  // 0 means the default value defined by default_max_trxs_per_minute_per_token
                              string      organization,
                              string      website,
                              name        administrator,
                              name        service_fee_mode,
                              asset       service_fee_fixed,
                              double      service_fee_ratio,
                              asset       failed_fee,
                              bool        active ) {
      check_admin_auth();

      /// --- register peg token ---
      regpegtoken(   peerchain_name,
                     peerchain_contract,
                     max_supply,
                     min_once_transfer,
                     max_once_transfer,
                     max_daily_transfer,
                     max_tfs_per_minute,
                     administrator,
                     failed_fee,
                     active );

      regacpttoken(  _self,
                     max_supply,
                     min_once_transfer,
                     max_once_transfer,
                     max_daily_transfer,
                     max_tfs_per_minute,
                     organization,
                     website,
                     administrator,
                     service_fee_mode,
                     service_fee_fixed,
                     service_fee_ratio,
                     failed_fee,
                     active );
   }

#endif

   void token::check_admin_auth(){
      if ( ! has_auth(_self) ){
         eosio_assert( _admin_st.admin != name() && is_account( _admin_st.admin ),"admin account not exist");
         require_auth( _admin_st.admin );
      }
   }

   void token::mvunrtotbl2( uint64_t id, const transfer_action_info transfer_para ){

   }

   void token::rbkunrbktrx( const transaction_id_type trx_id ){

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
            (mvunrtotbl2)(rbkunrbktrx)
#ifdef HUB
            (hubinit)(feetransfer)(regpegtoken2)(rbkdiehubtrx)
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

