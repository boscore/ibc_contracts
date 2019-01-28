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

   struct transfer_action_type {
      name    from;
      name    to;
      asset   quantity;
      string  memo;

      EOSLIB_SERIALIZE( transfer_action_type, (from)(to)(quantity)(memo) )
   };

   struct transfer_action_info {
      name    contract;
      name    from;
      asset   quantity;

      EOSLIB_SERIALIZE( transfer_action_info, (contract)(from)(quantity) )
   };

   struct cash_action_type {
      uint64_t                               seq_num;
      uint32_t                               orig_trx_block_num;
      std::vector<char>                      orig_trx_packed_trx_receipt;
      std::vector<capi_checksum256>          orig_trx_merkle_path;
      transaction_id_type                    orig_trx_id;
      name                                   to;
      asset                                  quantity;
      string                                 memo;
      name                                   relay;

      EOSLIB_SERIALIZE( cash_action_type, (seq_num)(orig_trx_block_num)(orig_trx_packed_trx_receipt)(orig_trx_merkle_path)(orig_trx_id)(to)(quantity)(memo)(relay) )
   };

   const static uint32_t default_max_trx_per_minute_per_token = 100;

   class [[eosio::contract("ibc.token")]] token : public contract {
      public:
      token( name s, name code, datastream<const char*> ds );
      ~token();

      [[eosio::action]]
      void setglobal( name       ibc_chain_contract,
                      name       peerchain_name,
                      name       peerchain_ibc_token_contract,
                      uint32_t   max_origtrxs_table_records,
                      uint32_t   cache_cashtrxs_table_records,
                      uint32_t   max_original_trxs_per_block,
                      bool       active );


      [[eosio::action]]
      void regacpttoken( name        original_contract,
                         asset       max_accept,
                         name        administrator,
                         asset       min_once_transfer,
                         asset       max_once_transfer,
                         asset       max_daily_transfer,
                         uint32_t    max_tfs_per_minute, // 0 means the default value defined by default_max_trx_per_minute_per_token
                         string      organization,
                         string      website,
                         name        service_fee_mode,
                         asset       service_fee_fixed,
                         double      service_fee_ratio,
                         name        failed_fee_mode,
                         asset       failed_fee_fixed,
                         double      failed_fee_ratio,
                         bool        active, // when non active, transfer not allowed, but cash trigger by peerchain withdraw can still execute
                         symbol      peerchain_sym );

      [[eosio::action]]
      void setacptasset( name contract, string which, asset quantity );

      [[eosio::action]]
      void setacptstr( name contract, string which, string value );

      [[eosio::action]]
      void setacptint( name contract, string which, uint64_t value );

      [[eosio::action]]
      void setacptbool( name contract, string which, bool value );

      [[eosio::action]]
      void setacptfee( name   contract,
                       name   kind,   // "success"_n or "failed"_n
                       name   fee_mode,
                       asset  fee_fixed,
                       double fee_ratio );


      [[eosio::action]]
      void regpegtoken( asset       max_supply,
                        asset       min_once_withdraw,
                        asset       max_once_withdraw,
                        asset       max_daily_withdraw,
                        uint32_t    max_wds_per_minute, // 0 means the default value defined by default_max_trx_per_minute_per_token
                        name        administrator,
                        name        peerchain_contract,
                        symbol      peerchain_sym,
                        name        failed_fee_mode,
                        asset       failed_fee_fixed,
                        double      failed_fee_ratio,
                        bool        active ); // when non active, withdraw not allowed, but cash which trigger by peerchain transfer can still execute

      [[eosio::action]]
      void setpegasset( symbol_code symcode, string which, asset quantity );

      [[eosio::action]]
      void setpegint( symbol_code symcode, string which, uint64_t value );

      [[eosio::action]]
      void setpegbool( symbol_code symcode, string which, bool value );

      [[eosio::action]]
      void setpegtkfee( symbol_code symcode,
                        name        fee_mode,
                        asset       fee_fixed,
                        double      fee_ratio );


      // called in C apply function
      void transfer_notify( name    code,
                            name    from,
                            name    to,
                            asset   quantity,
                            string  memo );

      [[eosio::action]]
      void transfer( name    from,
                     name    to,
                     asset   quantity,
                     string  memo );

      // called by ibc plugin
      [[eosio::action]]
      void cash( uint64_t                               seq_num,
                 const uint32_t                         orig_trx_block_num,
                 const std::vector<char>&               orig_trx_packed_trx_receipt,
                 const std::vector<capi_checksum256>&   orig_trx_merkle_path,
                 transaction_id_type                    orig_trx_id,    // redundant, facilitate indexing and checking
                 name                                   to,             // redundant, facilitate indexing and checking
                 asset                                  quantity,       // redundant, facilitate indexing and checking
                 string                                 memo,
                 name                                   relay );

      // called by ibc plugin
      [[eosio::action]]
      void cashconfirm( const uint32_t                         cash_trx_block_num,
                        const std::vector<char>&               cash_trx_packed_trx_receipt,
                        const std::vector<capi_checksum256>&   cash_trx_merkle_path,
                        transaction_id_type                    cash_trx_id,   // redundant, facilitate indexing and checking
                        transaction_id_type                    orig_trx_id ); // redundant, facilitate indexing and checking

      // called by ibc plugin repeatedly
      [[eosio::action]]
      void rollback( const transaction_id_type trx_id, name relay );   // check if any orignal transactions should be rolled back, rollback them if have

      // called by ibc plugin repeatedly when there are unrollbackable original transactions
      [[eosio::action]]
      void rmunablerb( const transaction_id_type trx_id, name relay );   // force to remove unrollbackable transaction

      // this action maybe needed when repairing the ibc system manually
      [[eosio::action]]
      void fcrollback( const std::vector<transaction_id_type> trxs, string memo );   // force rollback

      // this action maybe needed when can not rollback (because eosio account can refuse transfer token to it)
      [[eosio::action]]
      void fcrmorigtrx( const std::vector<transaction_id_type> trxs, string memo );   // force remove original transaction records, the parameter must be trx_id, in order to query the original transaction conveniently in the later period.

      // this action maybe needed when repairing the ibc system manually
      [[eosio::action]]
      void trxbls( string action, const std::vector<transaction_id_type> trxs );   // original transfer or withdraw transaction blanklist operation

      [[eosio::action]]
      void acntbls( string action, const std::vector<name> accounts );

      [[eosio::action]]
      void lockall();   // when locked, transfer, withdraw and cash action will not allowed to execute for all token

      [[eosio::action]]
      void unlockall();   // when unlocked, the restrictions caused by execute lockall function will be removed

      [[eosio::action]]
      void tmplock( uint32_t minutes );   // when executed, transfer, withdraw and cash action will not allowed to execute for all token for a period of time

      [[eosio::action]]
      void rmtmplock();   // when executed,  the restrictions caused by execute tmplock function will be removed

      [[eosio::action]]
      void open( name owner, const symbol_code& symcode, name ram_payer );

      [[eosio::action]]
      void close( name owner, const symbol_code& symcode );

      // this action maybe needed when repairing the ibc system manually
      [[eosio::action]]
      void fcinit( ); //force init

      static asset get_supply( name token_contract_account, symbol_code sym_code )
      {
         stats statstable( token_contract_account, token_contract_account.value );
         const auto& st = statstable.get( sym_code.raw() );
         return st.supply;
      }

      static asset get_balance( name token_contract_account, name owner, symbol_code sym_code )
      {
         accounts accountstable( token_contract_account, owner.value );
         const auto& ac = accountstable.get( sym_code.raw() );
         return ac.balance;
      }

      struct [[eosio::table("globals")]] global_state {
         global_state(){}

         name              ibc_chain_contract;
         name              peerchain_name;
         name              peerchain_ibc_token_contract;
         uint32_t          max_origtrxs_table_records = 0;
         uint32_t          cache_cashtrxs_table_records = 0;
         uint32_t          max_original_trxs_per_block = 0;

         bool              active = true;  // use as global lock
         uint32_t          lock_start_time = 0;
         uint32_t          lock_minutes = 0;

         // explicit serialization macro is necessary, without this, error "Exceeded call depth maximum" will occur when call state_singleton.set(state)
         EOSLIB_SERIALIZE( global_state, (ibc_chain_contract)(peerchain_name)(peerchain_ibc_token_contract)(max_origtrxs_table_records)
               (cache_cashtrxs_table_records)(max_original_trxs_per_block)(active)(lock_start_time)(lock_minutes) )
      };

   private:
      eosio::singleton< "globals"_n, global_state >   _global_state;
      global_state                                    _gstate;

      bool is_global_active();


      struct [[eosio::table("globalm")]] global_mutable {
         global_mutable(){}

         uint64_t    cash_seq_num = 0;    // set by seq_num in cash action from cashconfirm action, and must be increase one by one, and start from one
         uint32_t    last_confirmed_orig_trx_block_time_slot = 0; // used to determine which failed original transactions should be rolled back
         uint32_t    current_block_time_slot = 0;
         uint32_t    current_block_trxs = 0;
         uint64_t    origtrxs_tb_next_id = 1; // used to retain an incremental id for table origtrxs

         EOSLIB_SERIALIZE( global_mutable, (cash_seq_num)(last_confirmed_orig_trx_block_time_slot)(current_block_time_slot)(current_block_trxs)(origtrxs_tb_next_id) )
      };
      eosio::singleton< "globalm"_n, global_mutable > _global_mutable;
      global_mutable                                  _gmutable;


      struct [[eosio::table]] currency_accept {
         name        original_contract;
         asset       accept;
         asset       max_accept;
         asset       min_once_transfer;
         asset       max_once_transfer;
         asset       max_daily_transfer;
         uint32_t    max_tfs_per_minute;   // max transfer transactions per minute
         symbol      peg_token_symbol;
         name        administrator;
         string      organization;
         string      website;
         name        service_fee_mode;    // "fixed"_n or "ratio"_n
         asset       service_fee_fixed;
         double      service_fee_ratio;
         name        failed_fee_mode;     // "fixed"_n or "ratio"_n
         asset       failed_fee_fixed;
         double      failed_fee_ratio;
         asset       total_transfer;
         uint64_t    total_transfer_times;
         asset       total_cash;
         uint64_t    total_cash_times;
         bool        active;

         struct currency_accept_mutables {
            uint32_t    minute_trx_start;
            uint32_t    minute_trxs;
            uint32_t    daily_tf_start;
            asset       daily_tf_sum;
            uint32_t    daily_wd_start;
            asset       daily_wd_sum;
         } mutables;

         uint64_t  primary_key()const { return original_contract.value; }
         uint64_t  by_symbol()const { return accept.symbol.code().raw(); }
         uint64_t  by_peg_token_symbol()const { return peg_token_symbol.code().raw(); }
      };
      eosio::multi_index< "accepts"_n, currency_accept,
         indexed_by<"symbol"_n, const_mem_fun<currency_accept, uint64_t, &currency_accept::by_symbol> >,
         indexed_by<"pegtokensym"_n, const_mem_fun<currency_accept, uint64_t, &currency_accept::by_peg_token_symbol> >
      > _accepts;

      const currency_accept& get_currency_accept( name contract );
      const currency_accept& get_currency_accept_by_symbol( symbol_code symcode );
      const currency_accept& get_currency_accept_by_peg_token_symbol( symbol_code symcode );


      struct [[eosio::table]] currency_stats {
         asset       supply;
         asset       max_supply;
         asset       min_once_withdraw;
         asset       max_once_withdraw;
         asset       max_daily_withdraw;
         uint32_t    max_wds_per_minute;   // max withdraw transactions per minute
         name        peerchain_contract;
         symbol      orig_token_sym;
         name        administrator;
         name        failed_fee_mode;      // "fixed"_n or "ratio"_n
         asset       failed_fee_fixed;
         double      failed_fee_ratio;
         asset       total_issue;
         uint64_t    total_issue_times;
         asset       total_withdraw;
         uint64_t    total_withdraw_times;
         bool        active;

         struct currency_stats_mutables {
            uint32_t    minute_trx_start;
            uint32_t    minute_trxs;
            uint32_t    daily_isu_start;
            asset       daily_isu_sum;
            uint32_t    daily_wd_start;
            asset       daily_wd_sum;
         } mutables;

         uint64_t primary_key()const { return supply.symbol.code().raw(); }
         uint64_t  by_orig_token_sym()const { return orig_token_sym.code().raw(); }
      };
      typedef eosio::multi_index< "stats"_n, currency_stats,
         indexed_by<"origtokensym"_n, const_mem_fun<currency_stats, uint64_t, &currency_stats::by_orig_token_sym> > > stats;
      stats _stats;

      const currency_stats& get_currency_stats( symbol_code symcode );
      const currency_stats& get_currency_stats_by_orig_token_symbol( symbol_code symcode );


      struct [[eosio::table]] account {
         asset    balance;

         uint64_t primary_key()const { return balance.symbol.code().raw(); }
      };
      typedef eosio::multi_index< "accounts"_n, account > accounts;


      // use to record accepted transfer and withdraw transactions
      struct [[eosio::table]] original_trx_info {
         uint64_t                id; // auto-increment
         uint64_t                block_time_slot; // new record must not decrease time slot,
         transaction_id_type     trx_id;
         transfer_action_info    action; // very important infomation, used when execute rollback


         uint64_t primary_key()const { return id; }
         uint64_t by_time_slot()const { return block_time_slot; }
         fixed_bytes<32> by_trx_id()const { return fixed_bytes<32>(trx_id.hash); }
      };
      eosio::multi_index< "origtrxs"_n, original_trx_info,
         indexed_by<"tslot"_n, const_mem_fun<original_trx_info, uint64_t,        &original_trx_info::by_time_slot> >,  // used by ibc plugin
         indexed_by<"trxid"_n, const_mem_fun<original_trx_info, fixed_bytes<32>, &original_trx_info::by_trx_id> >
      >  _origtrxs;

      void origtrxs_emplace( transfer_action_info action );
      void rollback_trx( transaction_id_type trx_id );
      bool is_trx_id_exist_in_origtrxs_tb( transaction_id_type trx_id );
      void erase_record_in_origtrxs_tb_by_trx_id_for_confirmed( transaction_id_type trx_id );


      /**
       * Note:
       * "orig_trx_block_num" is a very important parameter, in order to prevent replay attacks:
       * first, new record's block_num must not less then the highest block_num in the table,
       *        (so, the ibc plugin is required to take a mechanism to ensure that the original transaction is sent to this contract in the order in which it occured)
       * second, when delete old records, it is important to ensure that the records of heighest two block number must retain.
       * The above two features must be satisfied at the same time. Breaking any one of them will lead to serious replay attacks.
       * set cache_cashtrxs_table_records parameter, when above feature satified, this parameter will take effect
       */
      struct [[eosio::table]] cash_trx_info {
         uint64_t              seq_num; // set by seq_num in cash action, and must be increase one by one, and start from 1
         uint64_t              block_time_slot;
         capi_checksum256      trx_id;
         transfer_action_type  action;                // redundant, facilitate indexing and checking
         capi_checksum256      orig_trx_id;           // redundant, facilitate indexing and checking
         uint64_t              orig_trx_block_num;    // very important!

         uint64_t primary_key()const { return seq_num; }
         uint64_t by_time_slot()const { return block_time_slot; }
         fixed_bytes<32> by_orig_trx_id()const { return fixed_bytes<32>(orig_trx_id.hash); }
         uint64_t by_orig_trx_block_num()const { return orig_trx_block_num; }
      };
      eosio::multi_index< "cashtrxs"_n, cash_trx_info,
         indexed_by<"tslot"_n,    const_mem_fun<cash_trx_info, uint64_t,        &cash_trx_info::by_time_slot> >,  // used by ibc plugin
         indexed_by<"trxid"_n,    const_mem_fun<cash_trx_info, fixed_bytes<32>, &cash_trx_info::by_orig_trx_id> >,
         indexed_by<"blocknum"_n, const_mem_fun<cash_trx_info, uint64_t,        &cash_trx_info::by_orig_trx_block_num> >
      > _cashtrxs;

      void trim_cashtrxs_table_or_not();
      uint64_t get_cashtrxs_tb_max_seq_num();
      uint64_t get_cashtrxs_tb_min_orig_trx_block_num();
      uint64_t get_cashtrxs_tb_max_orig_trx_block_num();
      bool is_orig_trx_id_exist_in_cashtrxs_tb( transaction_id_type orig_trx_id );


      struct [[eosio::table]] account_blacklist {
         name    account;

         uint64_t primary_key()const { return account.value; }
      };
      eosio::multi_index< "acntbls"_n, account_blacklist > _acntbls;

      bool is_in_acntbls( name account );


      struct [[eosio::table]] trx_blacklist {
         uint64_t             id;
         transaction_id_type  trx_id;

         uint64_t primary_key()const { return id; }
         fixed_bytes<32> by_trx_id()const { return fixed_bytes<32>(trx_id.hash); }
      };
      eosio::multi_index< "trxbls"_n, trx_blacklist,
         indexed_by<"trxid"_n, const_mem_fun<trx_blacklist, fixed_bytes<32>, &trx_blacklist::by_trx_id> >
      > _trxbls;

      bool is_in_trxbls( transaction_id_type trx_id );


      // use to record removed unrollbackable transactions
      struct [[eosio::table]] deleted_unrollbackable_trx_info {
         uint64_t                id; // auto-increment
         transaction_id_type     trx_id;

         uint64_t primary_key()const { return id; }
      };
      eosio::multi_index< "rmdunrbs"_n, deleted_unrollbackable_trx_info>  _rmdunrbs;


      void withdraw( name from, name peerchain_receiver, asset quantity, string memo );
      void sub_balance( name owner, asset value );
      void add_balance( name owner, asset value, name ram_payer );
      void verify_merkle_path( const std::vector<capi_checksum256>& merkle_path, digest_type check );
   };

} /// namespace eosio

