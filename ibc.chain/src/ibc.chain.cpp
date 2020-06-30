/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <ibc.chain/ibc.chain.hpp>
#include "merkle.cpp"
#include "block_header.cpp"

namespace eosio {

   chain::chain( name s, name code, datastream<const char*> ds ) :contract(s,code,ds),
            _global_state(_self, _self.value),
            _admin_sg(_self, _self.value),
            _wtmsig_sg(_self, _self.value),
            _global_mutable(_self, _self.value),
            _chaindb(_self, _self.value),
            _prodsches(_self, _self.value),
            _sections(_self, _self.value),
            _relays(_self, _self.value)
   {
      _gstate = _global_state.exists() ? _global_state.get() : global_state{};
      _gmutable = _global_mutable.exists() ? _global_mutable.get() : global_mutable{};
      _admin_st = _admin_sg.exists() ? _admin_sg.get() : admin_struct{};
      _wtmsig_st = _wtmsig_sg.exists() ? _wtmsig_sg.get() : wtmsig_struct{};
   }

   chain::~chain() {
      _global_state.set( _gstate, _self );
      _global_mutable.set( _gmutable, _self );
      _admin_sg.set( _admin_st , _self );
      _wtmsig_sg.set( _wtmsig_st , _self );
   }

   void chain::setglobal( name              chain_name,
                          chain_id_type     chain_id,
                          name              consensus_algo,
                          bool              wtmsig_activated,
                          uint32_t          wtmsig_ext_id  ){
      require_auth( _self );
      eosio_assert( chain_name != ""_n, "chain_name can not be empty");
      eosio_assert( ! is_equal_capi_checksum256(chain_id, chain_id_type()), "chain_id can not be empty");
      eosio_assert( consensus_algo == "pipeline"_n || consensus_algo == "batch"_n, "consensus_algo must be pipeline or batch" );
      _gstate.chain_name      = chain_name;
      _gstate.chain_id        = chain_id;
      _gstate.consensus_algo  = consensus_algo;

      _wtmsig_st.activated = wtmsig_activated;
      _wtmsig_st.ext_id = wtmsig_ext_id;
   }

   void chain::setadmin( name admin ){
      require_auth( _self );
      _admin_st.admin = admin;
   }

   // init for both pipeline and batch light client
   void chain::chaininit( const std::vector<char>&      header_data,
                          const producer_schedule&      active_schedule,
                          const incremental_merkle&     blockroot_merkle,
                          const name&                   relay ) {
      if ( has_auth(_self) ){
         while ( _chaindb.begin() != _chaindb.end() ){ _chaindb.erase(_chaindb.begin()); }
         while ( _prodsches.begin() != _prodsches.end() ){ _prodsches.erase(_prodsches.begin()); }
         while ( _sections.begin() != _sections.end() ){ _sections.erase(_sections.begin()); }
         _gmutable = global_mutable{};
      } else {
         eosio_assert( _chaindb.begin() == _chaindb.end() &&
                       _prodsches.begin() == _prodsches.end() &&
                       _sections.begin() == _sections.end() &&
                       _gmutable.last_anchor_block_num == 0, "the light client has already been initialized" );
         require_relay_auth( _self, relay );
      }

      const signed_block_header& header = unpack<signed_block_header>( header_data );

      auto active_schedule_id = 1;
      _prodsches.emplace( _self, [&]( auto& r ) {
         r.id              = active_schedule_id;
         r.schedule        = active_schedule;
         r.schedule_hash   = get_schedule_hash( active_schedule );
      });

      auto block_signing_key = get_public_key_by_producer( active_schedule_id, header.producer );
      auto header_block_num = header.block_num();

      block_header_state bhs;
      bhs.block_num             = header_block_num;
      bhs.block_id              = header.id();
      bhs.header                = header;

      /** In this function, block_header_state's pending_schedule version must equal to active_schedule version
      for this block cannot be a block in the process of BPs replacement. */
      bhs.active_schedule_id    = active_schedule_id;
      bhs.pending_schedule_id   = active_schedule_id;
      bhs.blockroot_merkle      = blockroot_merkle;
      bhs.block_signing_key     = block_signing_key;
      bhs.is_anchor_block       = true;

      auto dg = bhs_sig_digest( bhs );
      assert_producer_signature( dg, header.producer_signature, block_signing_key );

      _chaindb.emplace( _self, [&]( auto& r ) {
         r = std::move( bhs );
      });

      section_type sct;
      sct.first              = header_block_num;
      sct.last               = header_block_num;
      sct.valid              = false;

      if ( _gstate.consensus_algo == "batch"_n )
         sct.valid = true;

      sct.add( header.producer, header_block_num );

      _sections.emplace( _self, [&]( auto& r ) {
         r = std::move( sct );
      });
   }


   // ------ section related functions ------ //

   void chain::pushsection( const std::vector<char>&    headers_data,
                            const incremental_merkle&   blockroot_merkle,
                            const name&                 relay ) {
      require_relay_auth( _self, relay );

      eosio_assert( _gstate.consensus_algo == "pipeline"_n, "consensus algorithm must be pipeline");

      std::vector<signed_block_header> headers = unpack<std::vector<signed_block_header>>( headers_data );
      eosio_assert( _sections.begin() != _sections.end(), "the light client has not been initialized yet");
      const auto& last_section = *(_sections.rbegin());

      uint32_t front_block_num = headers.front().block_num();
      eosio_assert ( front_block_num >= last_section.first, "front_block_num >= last_section.first must be true");

      bool create_section = false;
      if ( front_block_num > last_section.last + 1 ) {      // create new section
         eosio_assert( last_section.valid , "last section must be completed first");
         create_section = true;
      }
      else if ( front_block_num == last_section.first ) {   // delete old and create new section
         eosio_assert( ! is_equal_capi_checksum256(headers.front().id(), _chaindb.get( front_block_num ).block_id), "first block header repeated");
         if ( ! remove_invalid_last_section()){ return; }
         create_section = true;
      }

      if ( create_section ){
         eosio_assert( headers.size() >= 30, "new section's size must not less then 30");
         new_section( headers.front(), blockroot_merkle );
         headers.erase( headers.begin() );
      }

      for ( const auto & header : headers ){
         append_header( header );
      }

      // mark anchor block
      auto ls = *(_sections.rbegin());
      if ( !ls.valid ){ return; }
      uint32_t anchor_block_num = ls.last - lib_depth;
      auto itr = _chaindb.find( anchor_block_num );
      if ( itr != _chaindb.end() ){
         _chaindb.modify( itr, same_payer, [&]( auto& r ) {
            r.is_anchor_block = true;
         });
         _gmutable.last_anchor_block_num = anchor_block_num;
      }
   }

   /**
    * Notes:
    * the last section must be valid
    * the header should not have new_producers and schedule_version consist with the last valid lwc section
    * the header block number should greater then the last block number of last section
    */
   void chain::new_section( const signed_block_header& header,
                           const incremental_merkle&  blockroot_merkle ){

      auto new_producers = header.new_producers;
      if ( _wtmsig_st.activated ){
         new_producers = header.get_ext_new_producers( _wtmsig_st.ext_id );
      }
      eosio_assert( ! new_producers, "section root header can not contain new_producers" );

      auto header_block_num = header.block_num();

      const auto& last_section = *(_sections.rbegin());
      eosio_assert( last_section.valid, "last_section is not valid" );
      eosio_assert( header_block_num > last_section.last + 1, "header_block_num should larger then last_section.last + 1" );

      auto active_schedule_id = get_section_last_active_schedule_id( last_section );
      auto version = _prodsches.get( active_schedule_id ).schedule.version;
      eosio_assert( header.schedule_version == version, "schedule_version not equal to previous one" );

      auto block_signing_key = get_public_key_by_producer( active_schedule_id, header.producer );

      block_header_state bhs;
      bhs.block_num             = header_block_num;
      bhs.block_id              = header.id();
      bhs.header                = header;
      bhs.active_schedule_id    = active_schedule_id;
      bhs.pending_schedule_id   = active_schedule_id;
      bhs.blockroot_merkle      = blockroot_merkle;
      bhs.block_signing_key     = block_signing_key;

      auto dg = bhs_sig_digest( bhs );
      assert_producer_signature( dg, header.producer_signature, block_signing_key );

      remove_header_if_exist( header_block_num );
      _chaindb.emplace( _self, [&]( auto& r ) {
         r = std::move( bhs );
      });

      section_type sct;
      sct.first              = header_block_num;
      sct.last               = header_block_num;
      sct.valid              = false;
      sct.add( header.producer, header_block_num );

      _sections.emplace( _self, [&]( auto& r ) {
         r = std::move( sct );
      });

      print_f("-- new section block added: % --", header_block_num);
   }


   /* active and pending producer schedule change process under pipeline pbft consensus algorithm
    *
    * +------------------+---------+---------+---------+---------+---------+---------+
    * | block num        |   1000  |   1001  |  1002   |   ....  |   1330  |   1331  |
    * +------------------+---------+---------+---------+---------+---------+---------+
    * | schedule_version |   v1    |   v1    |   v1    |    v1   |    v1   |    v2   |
    * +------------------+---------+---------+---------+---------+---------+---------+
    * | new_producers    |   none  |   have  |  none   |  none   |   none  |   none  |
    * +------------------+---------+---------+---------+---------+---------+---------+
    * | active_schedule  |   v1    |   v1    |   v1    |    v1   |  v2 @a  |  v2 (+) |
    * +------------------+---------+---------+---------+---------+---------+---------+
    * | pending_schedule |   =v1   | v2 (+)  |  =v2    |   =v2   |   =v2   |   =v2   |
    * +------------------+---------+---------+---------+---------+---------+---------+
    * @a In a real nodeos client node, the value here is v2, but in the SPV client, we set it v1
    * for convenience of calculation, and doing so does not affect signatures verification
    */


   /**
    * Notes:
    * 1. the header must be linkable to the last section
    * 2. can not push repeated block header
    */
   void chain::append_header( const signed_block_header& header ) {
      auto header_block_num = header.block_num();
      auto header_block_id = header.id();

      const auto& last_section = *(_sections.rbegin());
      auto last_section_first = last_section.first;
      auto last_section_last = last_section.last;
      
      eosio_assert( header_block_num > last_section_first, "new header number must larger then section root number" );
      eosio_assert( header_block_num <= last_section_last + 1, "unlinkable block" );

      // delete old branch
      if ( header_block_num < last_section_last + 1){
         auto result = _chaindb.get( header_block_num );
         eosio_assert( std::memcmp(header_block_id.hash, result.block_id.hash, 32) != 0, ("block repeated: " + std::to_string(header_block_num)).c_str() );

         _sections.modify( last_section, same_payer, [&]( auto& r ) {
            r.valid = header_block_num - last_section_first < lib_depth ? false : last_section.valid;
            r.clear_from( header_block_num );
         });

         while ( _chaindb.rbegin()->block_num != header_block_num - 1 ){
            _chaindb.erase( --_chaindb.end() );
         }

         print_f("-- block deleted: from % back to % --", last_section_last, header_block_num);
      }

      // verify linkable
      auto last_bhs = _chaindb.get( header_block_num - 1 );   // don't make pointer or const, for it needs change
      eosio_assert(std::memcmp(last_bhs.block_id.hash, header.previous.hash, 32) == 0 , "unlinkable block" );

      // verify new block
      block_header_state bhs;
      bhs.block_num           = header_block_num;
      bhs.block_id            = std::move( header_block_id );
      
      last_bhs.blockroot_merkle.append( last_bhs.block_id );
      bhs.blockroot_merkle = std::move( last_bhs.blockroot_merkle );

      // handle bps list replacement
      if ( last_bhs.active_schedule_id == last_bhs.pending_schedule_id ){  // normal circumstances

         // check if last_section valid
         bool valid = false;
         if ( last_section.newprod_block_num != 0 ){
            if ( header_block_num - last_section.newprod_block_num >= lib_depth * 2 ){
               valid = true;
            }
         } else {
            if ( header_block_num - last_section_first >= lib_depth ){
               valid = true;
            }
         }

         if ( valid && ! last_section.valid ){
            _sections.modify( last_section, same_payer, [&]( auto& r ) {
               r.valid = true;
            });
         }

         bhs.active_schedule_id  = last_bhs.active_schedule_id;
         bhs.pending_schedule_id = last_bhs.pending_schedule_id;
      }
      else { // producers replacement interval
         auto last_pending_schedule_version = _prodsches.get( last_bhs.pending_schedule_id ).schedule.version;
         if ( header.schedule_version == last_pending_schedule_version ){  // producers replacement finished
            /* important! infact header_block_num - last_section.newprod_block_num should be approximately equal to 325 */

            if ( ! only_one_eosio_bp() ){
               eosio_assert( header_block_num - last_section.newprod_block_num > 20 * 12, "header_block_num - last_section.newprod_block_num > 20 * 12 failed");
            }

            // replace
            bhs.active_schedule_id  = last_bhs.pending_schedule_id;

            // clear last_section's producers and block_nums
            _sections.modify( last_section, same_payer, [&]( auto& s ) {
               s.producers = std::vector<name>();
               s.block_nums = std::vector<uint32_t>();
            });
         } else { // producers replacement not finished
            bhs.active_schedule_id  = last_bhs.active_schedule_id;
         }
         bhs.pending_schedule_id = last_bhs.pending_schedule_id;
      }

      auto new_producers = header.new_producers;
      if ( _wtmsig_st.activated ){
         new_producers = header.get_ext_new_producers( _wtmsig_st.ext_id );
      }

      // handle new_producers
      if ( new_producers ){  // has new producers
         eosio_assert( new_producers->version == header.schedule_version + 1, "new_producers version invalid" );

         _sections.modify( last_section, same_payer, [&]( auto& r ) {
            r.valid = false;
            r.newprod_block_num = header_block_num;
         });

         auto new_schedule_id = _prodsches.available_primary_key();
         _prodsches.emplace( _self, [&]( auto& r ) {
            r.id              = new_schedule_id;
            r.schedule        = *new_producers;
            r.schedule_hash   = get_schedule_hash( *new_producers );
         });

         if ( _prodsches.rbegin()->id - _prodsches.begin()->id >= prodsches_max_records ){
            _prodsches.erase( _prodsches.begin() );
         }

         bhs.pending_schedule_id = new_schedule_id;
      }

      bhs.header = std::move(header);

      if ( bhs.header.producer == last_bhs.header.producer && bhs.active_schedule_id == last_bhs.active_schedule_id ){
         bhs.block_signing_key = std::move(last_bhs.block_signing_key);
      } else{
         bhs.block_signing_key = get_public_key_by_producer( bhs.active_schedule_id, bhs.header.producer );
      }

      auto dg = bhs_sig_digest( bhs );
      assert_producer_signature( dg, bhs.header.producer_signature, bhs.block_signing_key);

      remove_header_if_exist( header_block_num );
      _chaindb.emplace( _self, [&]( auto& r ) {
         r = std::move(bhs);
      });

      const auto& active_schedule = _prodsches.get( bhs.active_schedule_id ).schedule;

      _sections.modify( last_section, same_payer, [&]( auto& s ) {
         s.last = header_block_num;
         s.add( header.producer, header_block_num, header.timestamp.slot, active_schedule );
      });

      trim_last_section_or_not();

      print_f("-- block added: % --", header_block_num);
   }

   uint32_t chain::get_section_last_active_schedule_id( const section_type& section ) const {
      return _chaindb.get( section.last ).active_schedule_id;
   }

   bool chain::remove_invalid_last_section( ){
      const static uint32_t max_delete = 50;

      auto it = --_sections.end();
      eosio_assert( false == it->valid, "last section is valid, can't remove");

      bool finished = max_delete >= (it->last - it->first + 1) ? true : false;
      for( uint64_t num = std::max(it->first, it->last - max_delete + 1); num <= it->last; ++num ){
         auto existing = _chaindb.find( num );
         if ( existing != _chaindb.end() ){
            _chaindb.erase( existing );
         }
      }

      if ( finished ){
         _sections.erase( it );
      } else {
         _sections.modify( it, same_payer, [&]( auto& r ) {
            r.last = it->last - max_delete;
            r.clear_from( it->last - max_delete + 1 );
         });
      }

      return finished;
   }

   void chain::remove_header_if_exist( uint32_t block_num ){
      auto existing = _chaindb.find( block_num );
      if ( existing != _chaindb.end() ){
         _chaindb.erase( existing );
      }
   }

   const static uint32_t max_trim = 50;

   void chain::trim_last_section_or_not() {
      auto lwcls = *(_sections.rbegin());
      if ( lwcls.last - lwcls.first > section_max_length ){

         // delete blocks in _chaindb
         for ( uint32_t num = lwcls.first; num < lwcls.first + max_trim; ++num ){
            auto it = _chaindb.find( num );
            if ( it != _chaindb.end() && !it->is_anchor_block ){
               _chaindb.erase( it );
            }
         }

         // construct new section info
         section_type ls = lwcls;
         ls.first += max_trim;

         // replace old section with new section in _sections
         _sections.erase( --_sections.end() );
         _sections.emplace( _self, [&]( auto& r ) {
            r = std::move( ls );
         });
      }
   }

   static const uint32_t max_delete = 150; // max delete 150 records per time, in order to avoid exceed cpu limit
   void chain::rmfirstsctn( const name& relay ){
      require_relay_auth( _self, relay );

      auto it = _sections.begin();
      auto next = ++it;
      eosio_assert( next != _sections.end(), "can not delete the last section");
      eosio_assert( next->valid == true, "next section must be valid");

      uint32_t count = 0;
      auto begin = _sections.begin();
      if ( begin->last - begin->first + 1 > max_delete ){
         for ( uint32_t num = begin->first; num < begin->first + max_delete; ++num ){
            auto it = _chaindb.find( num );
            if ( it != _chaindb.end() && !it->is_anchor_block ){
               print("-- delete block1 --");print( num );
               _chaindb.erase( it );
            }
         }

         section_type sctn = *begin;
         sctn.first += max_delete;

         _sections.erase( begin );
         _sections.emplace( _self, [&]( auto& r ) {
            r = std::move( sctn );
         });

         return;
      } else {
         for ( uint32_t num = begin->first; num <= begin->last; ++num ){
            auto it = _chaindb.find( num );
            if ( it != _chaindb.end() && !it->is_anchor_block ){
               print("-- delete block2 --");print( num );
               _chaindb.erase( it );
            }
         }

         count += ( begin->last - begin->first + 1 );
         _sections.erase( begin );
      }

      // do this again to ensure that all old chaindb data is deleted
      uint32_t lwcls_first = _sections.begin()->first;
      uint32_t end_block_num = _chaindb.rbegin()->block_num;

      if ( end_block_num <= chaindb_max_history_length * 120 ){
         return;
      }

      while ( count++ <= max_delete ){
         auto itr = _chaindb.begin();
         if ( itr->block_num < end_block_num - chaindb_max_history_length * 120 ){
            _chaindb.erase( itr );
            print("-- delete block3 --");print(itr->block_num);
         } else { break; }
      }
   }

   name get_scheduled_producer( uint32_t tslot, const producer_schedule& active_schedule) {
      auto index = tslot % (active_schedule.producers.size() * producer_repetitions);
      index /= producer_repetitions;
      return active_schedule.producers[index].producer_name;
   }

#define BIGNUM  2000
#define MAXSPAN 4
   void section_type::add( name prod, uint32_t num, uint32_t tslot, const producer_schedule& sch ) {
      // one node per chain test model
      if ( sch.producers.size() == 1 && sch.producers.front().producer_name == "eosio"_n ){  // for one node test
         return;
      }

      // section create
      if ( producers.empty() ){
         eosio_assert( block_nums.empty(), "internal error, producers not consistent with block_nums" );
         eosio_assert( prod != name() && num != 0, "internal error, invalid parameters" );
         producers.push_back( prod );
         block_nums.push_back( num );
         return;
      }

      eosio_assert( tslot != 0, "internal error,tslot == 0");
      eosio_assert( sch.producers.size() > 15, "producers.size() must greater then 15" ); // should be equal to 21 infact
      eosio_assert( get_scheduled_producer( tslot, sch ) == prod, "scheduled producer validate failed");

      // same producer, do nothing
      if( prod == producers.back() ){
         return;
      }

      // producer can not repeat within last 15 producers
      int size = producers.size();
      int count = size > 15 ? 15 : size;
      for ( int i = 0; i < count ; ++i){
         eosio_assert( prod != producers[ size - 1 - i ] , "producer can not repeat within last 15 producers" );
      }

      // Check if the distance from producers.back() to prod is not greater then MAXSPAN
      int index_last = BIGNUM;
      int index_this = BIGNUM;
      int i = 0;
      for ( const auto& pk : sch.producers ){
         if ( pk.producer_name == producers.back() ){
            index_last = i;
         }
         if ( pk.producer_name == prod ){
            index_this = i;
         }
         ++i;
      }
      if ( index_this > index_last ){
         eosio_assert( index_this - index_last <= MAXSPAN, "exceed max span" );
      } else {
         eosio_assert( index_last - index_this >= sch.producers.size() - MAXSPAN, "exceed max span" );
      }

      // add
      producers.push_back( prod );
      block_nums.push_back( num );

      // trim
      if( producers.size() > 21 ){
         producers.erase( producers.begin() );
         block_nums.erase( block_nums.begin() );
      }
   }

   void section_type::clear_from( uint32_t num ){
      int pos = 0;
      eosio_assert( first < num && num <= last , "invalid number" );

      while ( num <= block_nums.back() && !producers.empty() && !block_nums.empty() ){
         producers.pop_back();
         block_nums.pop_back();
      }
   }


   // ------ pbft related functions ------ //

   void chain::pushblkcmits( const std::vector<char>&    headers_data,
                             const incremental_merkle&   blockroot_merkle,
                             const std::vector<char>&    proof_data,
                             const name&                 proof_type,
                             const name&                 relay ) {
      require_relay_auth( _self, relay );

      eosio_assert( _gstate.consensus_algo == "batch"_n, "consensus algorithm must be batch");
      eosio_assert( _chaindb.begin() != _chaindb.end(), "the light client has not been initialized yet");

      // unpack and make basic assert
      std::vector<signed_block_header> headers = unpack<std::vector<signed_block_header>>( headers_data );
      eosio_assert( _chaindb.find( headers.front().block_num() ) == _chaindb.end(), "the first block header aready exist");

      std::vector<pbft_commit> commits;
      std::vector<pbft_checkpoint> checkpoints;
      if ( proof_type == "commit"_n ){
         commits = unpack<std::vector<pbft_commit>>( proof_data );
      } else if ( proof_type == "checkpoint"_n ){
         checkpoints = unpack<std::vector<pbft_checkpoint>>( proof_data );
      } else { eosio_assert( false, "invalid proof_type name"); }

      eosio_assert( headers.size() > 0, "headers can not be empty");
      eosio_assert( blockroot_merkle._node_count != 0 && blockroot_merkle._active_nodes.size() != 0, "blockroot_merkle can not be empty");

      if ( ! only_one_eosio_bp() ){
         eosio_assert( commits.size() >= 15 || checkpoints.size() >= 15, "size of proof must not less then 15");
      }

      eosio_assert( commits.size() <= 40 && checkpoints.size() <= 40, "size of proof too large");

      uint32_t first_num = headers.front().block_num();
      uint32_t last_num = headers.back().block_num();

      // push headers
      push_header( headers.front(), blockroot_merkle );
      headers.erase( headers.begin() );
      for ( const auto & header : headers ){
         eosio_assert( !header.new_producers,"only the first block header can contain new_producers"); // bos chain
         remove_header_if_exist( header.block_num() );
         push_header( header );
      }

      // assert signatures
      std::set<name> producers;
      if ( proof_type == "commit"_n ){
         uint32_t first_view = commits.front().view;

         for ( auto commit : commits ){
            eosio_assert( commit.view == first_view, "assert commit.view == first_view failed");
            eosio_assert( commit.common.type == 1, "not commit message");

            uint32_t block_num = commit.block_num();
            eosio_assert( first_num <= block_num && block_num <= last_num, "invalid commit block_num");

            auto bhs = _chaindb.get( block_num );
            eosio_assert( is_equal_capi_checksum256(commit.block_id(), bhs.block_id), "invalid block_id");

            auto pub_key = get_public_key_form_signature( commit.digest(_gstate.chain_id), commit.sender_signature );
            auto producer = get_producer_by_public_key(bhs.active_schedule_id, pub_key);
            if( producer == name()) continue;

            producers.insert( producer );
         }
      } else if ( proof_type == "checkpoint"_n ) {
         for (auto checkpoint : checkpoints) {
            eosio_assert( checkpoint.common.type == 2, "not checkpoint message");

            uint32_t block_num = checkpoint.block_num();
            eosio_assert(first_num <= block_num && block_num <= last_num, "invalid checkpoint block_num");

            auto bhs = _chaindb.get(block_num);
            eosio_assert(is_equal_capi_checksum256(checkpoint.block_id(), bhs.block_id), "invalid block_id");

            auto pub_key = get_public_key_form_signature( checkpoint.digest(_gstate.chain_id), checkpoint.sender_signature );
            auto producer = get_producer_by_public_key(bhs.active_schedule_id, pub_key);
            if( producer == name()) continue;

            producers.insert(producer);
         }
      }

      if ( ! only_one_eosio_bp() ){
         eosio_assert( producers.size() >= 15, "assert producers.size() >= 15 failed");
      }

      // delete headers [first_num + 1, last_num]
      for( uint32_t n = first_num + 1; n <= last_num; ++n ){
         auto itr = _chaindb.find( n );
         if ( itr != _chaindb.end() ){ _chaindb.erase( itr ); }
      }

      // create new section and delete old section
      _sections.emplace( _self, [&]( auto& r ) {
         r.first  = first_num;
         r.last   = first_num;
         r.valid  = true;
      });

      if( _sections.begin()->first != _sections.rbegin()->first ){
         _sections.erase( _sections.begin() );
      }

      // delete old chaindb data
      uint32_t end_block_num = _chaindb.rbegin()->block_num;
      for ( uint32_t i = 0; i < 50; ++i ){
         auto itr = _chaindb.begin();
         uint32_t range_length = chaindb_max_history_length * 120;
         if ( end_block_num > range_length && itr->block_num < end_block_num - range_length ){
            _chaindb.erase( itr );
            print("-- delete block --");print(itr->block_num);
         } else { break; }
      }
   }

   /*  active and pending producer schedule change process under batch pbft consensus algorithm
    *
    * +------------------+---------+---------+---------+---------+
    * | block num        |   1000  |   1001  |  1002   |   1003  |
    * +------------------+---------+---------+---------+---------+
    * | schedule_version |   v1    |   v1    |   v1    |    v2   |
    * +------------------+---------+---------+---------+---------+
    * | new_producers    |   none  |   have  |  none   |  none   |
    * +------------------+---------+---------+---------+---------+
    * | active_schedule  |   v1    |   v1    |  v2 @a  |  v2 (+) |
    * +------------------+---------+---------+---------+---------+
    * | pending_schedule |   =v1   | v2 (+)  |  =v2    |    =v2  |
    * +------------------+---------+---------+---------+---------+
    *
    * @a In a real nodeos client node, the value here is v2, but in the SPV client, we set it v1
    * for convenience of calculation, and doing so does not affect signatures verification
    */

   void chain::push_header( const signed_block_header& header, const incremental_merkle& blockroot_merkle ) {
      auto header_block_num = header.block_num();
      auto header_block_id = header.id();

      auto last_bhs = *(_chaindb.rbegin());  // don't make pointer or const, for it needs change
      eosio_assert( header_block_num > last_bhs.block_num, "invalid header_block_num" );

      /**
       * construct bhs and verify signature
       */
      block_header_state bhs;
      bhs.block_num           = header_block_num;
      bhs.block_id            = std::move( header_block_id );
      bhs.header              = std::move( header );

      // update active_schedule_id and pending_schedule_id
      if ( header.schedule_version == last_bhs.header.schedule_version ){
         bhs.active_schedule_id  = last_bhs.active_schedule_id;
      } else if ( header.schedule_version == last_bhs.header.schedule_version + 1 ){
         bhs.active_schedule_id  = last_bhs.pending_schedule_id;
      } else {
         eosio_assert( false, "invalid schedule_version");
      }
      bhs.pending_schedule_id = last_bhs.pending_schedule_id;

      auto new_producers = header.new_producers;
      if ( _wtmsig_st.activated ){
         new_producers = header.get_ext_new_producers( _wtmsig_st.ext_id );
      }

      if ( new_producers ){
         eosio_assert( new_producers->version == header.schedule_version + 1, "new_producers version invalid" );

         auto new_schedule_id = _prodsches.available_primary_key();
         _prodsches.emplace( _self, [&]( auto& r ) {
            r.id              = new_schedule_id;
            r.schedule        = *new_producers;
            r.schedule_hash   = get_schedule_hash( *new_producers );
         });

         if ( _prodsches.rbegin()->id - _prodsches.begin()->id >= prodsches_max_records ){
            _prodsches.erase( _prodsches.begin() );
         }

         bhs.pending_schedule_id = new_schedule_id;
      }

      // handle blockroot_merkle
      if ( blockroot_merkle._node_count != 0 && blockroot_merkle._active_nodes.size() != 0 ){
         bhs.blockroot_merkle = blockroot_merkle;
         bhs.is_anchor_block = true;

         // update last_anchor_block_num
         eosio_assert( header_block_num > _gmutable.last_anchor_block_num, "assert header_block_num > _gmutable.last_anchor_block_num failed");
         _gmutable.last_anchor_block_num = header_block_num;
      } else {
         eosio_assert( header_block_num == last_bhs.block_num + 1, "assert header_block_num == last_bhs.block_num + 1 failed");
         last_bhs.blockroot_merkle.append( last_bhs.block_id );
         bhs.blockroot_merkle = std::move(last_bhs.blockroot_merkle);
      }

      // handle block_signing_key
      if ( bhs.header.producer == last_bhs.header.producer && bhs.active_schedule_id == last_bhs.active_schedule_id ){
         bhs.block_signing_key = std::move(last_bhs.block_signing_key);
      } else{
         bhs.block_signing_key = get_public_key_by_producer( bhs.active_schedule_id, bhs.header.producer );
      }

      // verify signature
      auto dg = bhs_sig_digest( bhs );
      assert_producer_signature( dg, bhs.header.producer_signature, bhs.block_signing_key );

      /**
       * add to chaindb
       */
      _chaindb.emplace( _self, [&]( auto& r ) {
         r = std::move(bhs);
      });

      print_f("-- block added: % --", header_block_num);
   }

   // ------ common functions ------ //

   digest_type chain::bhs_sig_digest( const block_header_state& hs ) const {
      auto it = _prodsches.find( hs.pending_schedule_id );
      eosio_assert( it != _prodsches.end(), "internal error: block_header_state::sig_digest" );
      auto header_bmroot = get_checksum256( std::make_pair( hs.header.digest(), hs.blockroot_merkle.get_root() ));
      return get_checksum256( std::make_pair( header_bmroot, it->schedule_hash ));
   }

   capi_public_key chain::get_public_key_form_signature( digest_type digest, signature_type sig ) const {
      capi_public_key pub_key;
      size_t pubkey_size = recover_key( reinterpret_cast<const capi_checksum256*>(digest.hash),
                                        reinterpret_cast<const char*>(sig.data), 66, pub_key.data, 34 );
      eosio_assert( pubkey_size == 34, "pubkey_size != 34");
      return pub_key;
   }

   capi_public_key chain::get_public_key_by_producer( uint64_t id, const name& producer ) const {
      auto it = _prodsches.find(id);
      eosio_assert( it != _prodsches.end(), "producer schedule id not found" );
      const producer_schedule& ps = it->schedule;
      for( auto pk : ps.producers){
         if( pk.producer_name == producer){
            capi_public_key cpk;
            eosio::datastream<char*> pubkey_ds( reinterpret_cast<char*>(cpk.data), sizeof(capi_signature) );
            pubkey_ds << pk.block_signing_key;
            return cpk;
         }
      }
      eosio_assert(false, (string("producer not found: ") + producer.to_string()).c_str() );
      return capi_public_key(); //never excute, just used to suppress "no return" warning
   }

   name chain::get_producer_by_public_key( uint64_t id, const capi_public_key& pub_key ) const {
      auto it = _prodsches.find(id);
      eosio_assert( it != _prodsches.end(), "producer schedule id not found" );
      const producer_schedule& ps = it->schedule;

      auto key = unpack<public_key>(pack(pub_key));
      for( auto pk : ps.producers){
         if( pk.block_signing_key == key ){
            return pk.producer_name;
         }
      }
      return name();
   }

   void chain::assert_producer_signature(const digest_type& digest,
                                         const capi_signature& signature,
                                         const capi_public_key& pub_key ) const {
      assert_recover_key( reinterpret_cast<const capi_checksum256*>( digest.hash ),
                          reinterpret_cast<const char*>( signature.data ), 66,
                          reinterpret_cast<const char*>( pub_key.data ), 34 );
   }

   // ------ force init ------ //

   void chain::forceinit(){
      check_admin_auth();
      while ( _prodsches.begin() != _prodsches.end() ){ _prodsches.erase(_prodsches.begin()); }
      while ( _sections.begin() != _sections.end() ){ _sections.erase(_sections.begin()); }
      _gmutable = global_mutable{};

      for ( uint32_t i = 0; i < 150; i++ ){
         auto itr = _chaindb.begin();
         if ( itr != _chaindb.end() ){
            _chaindb.erase( itr );
         } else { break; }
      }

      if( _chaindb.begin() == _chaindb.end() ){
         print_f("force initialization completed");
      } else {
         print_f("force initialization is not complete, please call forceinit() again");
      }
   }

   bool chain::only_one_eosio_bp(){
      eosio_assert( _sections.begin() != _sections.end(), "table sections is empty");
      const auto& last_section = *(_sections.rbegin());
      auto active_schedule_id = _chaindb.get( last_section.last ).active_schedule_id;
      const auto& active_schedule = _prodsches.get( active_schedule_id ).schedule;
      const auto& pds = active_schedule.producers;
      return pds.size() == 1 && pds.front().producer_name == "eosio"_n;
   }

   void chain::relay( string action, name relay ) {
      check_admin_auth();
      auto existing = _relays.find( relay.value );

      if ( action == "add" ) {
         eosio_assert( existing == _relays.end(), "relay already exist" );
         _relays.emplace( _self, [&]( auto& r ){ r.relay = relay; } );
         return;
      }

      if ( action == "remove" ) {
         eosio_assert( existing != _relays.end(), "relay not exist" );
         _relays.erase( existing );
         return;
      }

      eosio_assert(false,"unknown action");
   }

   void chain::reqrelayauth( ){
      if ( check_relay_auth ){
         eosio_assert( false, "check_relay_auth == true" );
      } else {
         eosio_assert( false, "check_relay_auth == false" );
      }
   }

   void chain::check_admin_auth(){
      if ( ! has_auth(_self) ){
         eosio_assert( _admin_st.admin != name() && is_account( _admin_st.admin ),"admin account not exist");
         require_auth( _admin_st.admin );
      }
   }

   digest_type chain::get_schedule_hash( producer_schedule schedule ){
      if ( _wtmsig_st.activated ){
         producer_authority_schedule auth_schedule;
         auth_schedule.version = schedule.version;

         for ( auto& producer : schedule.producers ){
            producer_authority prod_auth;
            prod_auth.producer_name = producer.producer_name;
            block_signing_authority_v0 sign_auth;
            sign_auth.threshold = 1;
            sign_auth.keys.emplace_back(key_weight{producer.block_signing_key,1});
            prod_auth.authority = sign_auth;
            auth_schedule.producers.emplace_back(prod_auth);
         }

         return get_checksum256( auth_schedule );
      } else {
         return get_checksum256( schedule );
      }

   }

} /// namespace eosio

EOSIO_DISPATCH( eosio::chain, (setglobal)(chaininit)(pushsection)(rmfirstsctn)(pushblkcmits)(forceinit)(relay)(reqrelayauth)(setadmin) )
