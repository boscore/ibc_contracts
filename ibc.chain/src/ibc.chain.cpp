/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <ibc.chain/ibc.chain.hpp>
#include "merkle.cpp"
#include "block_header.cpp"

namespace eosio {

   chain::chain( name s, name code, datastream<const char*> ds ) :contract(s,code,ds),
            _chaindb(_self, _self.value),
            _prodsches(_self, _self.value),
            _sections(_self, _self.value),
            _relays(_self, _self.value),
            _global(_self, _self.value)
   {
      _gstate = _global.exists() ? _global.get() : global_state{};
   }

   chain::~chain() {
      _global.set( _gstate, _self );
   }

   void chain::setglobal( const global_state& gs ){
      require_auth( _self );
      eosio_assert( 50 <= gs.lib_depth && gs.lib_depth <= 400, "lib_depth range must be between 50 and 400" );
      _gstate = gs;
   }

   /**
    * Notes
    * The chaininit block_header_state's pending_schedule version should equal to active_schedule version"
    * if not, assert_producer_signature will failed
    */

   void chain::chaininit( const std::vector<char>&      header_data,
                          const producer_schedule&      active_schedule,
                          const incremental_merkle&     blockroot_merkle) {
      if ( has_auth(_self) ){
         while ( _chaindb.begin() != _chaindb.end() ){ _chaindb.erase(_chaindb.begin()); }
         while ( _prodsches.begin() != _prodsches.end() ){ _prodsches.erase(_prodsches.begin()); }
         while ( _sections.begin() != _sections.end() ){ _sections.erase(_sections.begin()); }
      } else {
         eosio_assert( _chaindb.begin() == _chaindb.end() &&
                       _prodsches.begin() == _prodsches.end() &&
                       _sections.begin() == _sections.end(), "already initialized" );
         eosio_assert( has_relay_auth(),"can not find any relay authority" );
      }

      const signed_block_header& header = unpack<signed_block_header>( header_data );

      auto active_schedule_id = 1;
      _prodsches.emplace( _self, [&]( auto& r ) {
         r.id              = active_schedule_id;
         r.schedule        = active_schedule;
         r.schedule_hash   = get_checksum256( active_schedule );
      });

      auto block_signing_key = get_producer_capi_public_key( active_schedule_id, header.producer );
      auto header_block_num = header.block_num();

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

      _chaindb.emplace( _self, [&]( auto& r ) {
         r = std::move( bhs );
      });

      section_type sct;
      sct.first              = header_block_num;
      sct.last               = header_block_num;
      sct.valid              = true;
      sct.add( header.producer, header_block_num );

      _sections.emplace( _self, [&]( auto& r ) {
         r = std::move( sct );
      });
   }

   /**
    *
    * @param headers_data The first header of headers_data must not have .new_producers
    * @param blockroot_merkle
    * @param relay
    */
   
   void chain::pushsection( const std::vector<char>&    headers_data,
                            const incremental_merkle&   blockroot_merkle,
                            const name&                 relay ) {
      eosio_assert( is_relay( _self, relay ), "relay not found");
      require_auth( relay );

      std::vector<signed_block_header> headers = unpack<std::vector<signed_block_header>>( headers_data );
      eosio_assert( _sections.begin() != _sections.end(), "fatal error, _sections is empty");
      const auto& last_section = *(_sections.rbegin());
      if ( headers.front().block_num() > last_section.last + 1 ) {
         if ( !last_section.valid ) {
            remove_section( last_section.first );
         }

         newsection( headers.front(), blockroot_merkle );
         headers.erase( headers.begin() );
      }

      for ( const auto & header : headers ){
         pushheader( header );
      }
   }

   void chain::rminvalidls( const name& relay ) {
      eosio_assert( is_relay( _self, relay ), "relay not found");
      require_auth( relay );

      auto it = --_sections.end();
      eosio_assert( false == it->valid, "lwcls is valid, can't remove");

      for( uint64_t num = it->first; num <= it->last; ++num ){
         auto existing = _chaindb.find( num );
         if ( existing != _chaindb.end() ){
            _chaindb.erase( existing );
         }
      }
      _sections.erase( it );
   }

   static const uint32_t max_delete = 150; // max delete 150 records per time, in order to avoid exceed cpu limit
   void chain::rmfirstsctn( const name& relay ){
      eosio_assert( is_relay( _self, relay ), "relay not found");
      require_auth( relay );

      auto it = _sections.begin();
      auto next = ++it;
      eosio_assert( next != _sections.end(), "can not delete");
      next = ++it;
      eosio_assert( next != _sections.end(), "can not delete the last section");
      eosio_assert( next->valid == true, "next section must be valid");

      uint32_t count = 0;
      auto begin = _sections.begin();
      if ( begin->last - begin->first + 1 > max_delete ){
         for ( uint32_t num = begin->first; num < begin->first + max_delete; ++num ){
            auto it = _chaindb.find( num );
            if ( it != _chaindb.end() ){
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
            if ( it != _chaindb.end() ){
               print("-- delete block2 --");print( num );
               _chaindb.erase( it );
            }
         }

         count += ( begin->last - begin->first + 1 );
         _sections.erase( begin );
      }

      // do this again to ensure that all old chaindb data is deleted
      uint32_t lwcls_first = _sections.begin()->first;
      while ( count++ <= max_delete ){
         auto begin = _chaindb.begin();
         if ( begin->block_num < lwcls_first ){
            _chaindb.erase( begin );
            print("-- delete block3 --");print(begin->block_num);
         } else {
            break;
         }
      }
   }

   void chain::relay( string action, name relay ) {
      require_auth( _self );
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

   bool chain::has_relay_auth() {
      for ( auto it = _relays.begin(); it != _relays.end(); ++it ){
         if ( has_auth(it->relay) ){
            return true;
         }
      }
      return false;
   }

   void chain::blockmerkle( uint64_t block_num, incremental_merkle merkle, name relay ){
      static constexpr uint32_t range = ( 1 << 10 ) * 4;    // about 30 minutes
      static constexpr uint32_t range_large = range * 6;    // about 3 hours
      static constexpr uint32_t recent = range * 24;        // about 3 days
      static constexpr uint32_t past = recent * 5;          // about 15 days

      eosio_assert( is_relay( _self, relay ), "relay not found");
      require_auth( relay );

      eosio_assert( block_num % range == 0, "the block number should be integral multiple of 1024 * 4");

      blkrtmkls _blkrtmkls( _self, relay.value );
      auto exsiting = _blkrtmkls.find( block_num );
      eosio_assert( exsiting == _blkrtmkls.end(), "the block's blockroot_merkle already exist" );

      _blkrtmkls.emplace( _self, [&]( auto& r ) {
         r.block_num = block_num;
         r.merkle = merkle;
      });

      // reorgnize the list
      if ( block_num <= recent || _blkrtmkls.begin() == _blkrtmkls.end() ){
         return;
      }

      auto it = _blkrtmkls.lower_bound( block_num - recent );
      while ( it != _blkrtmkls.end() && it->block_num < block_num - recent + range * 2 && it->block_num % range_large != 0  ){
         _blkrtmkls.erase( it );
         ++it;
      }

      while ( _blkrtmkls.begin()->block_num < block_num - past ){
         _blkrtmkls.erase( _blkrtmkls.begin() );
      }
   }

   // ---- private methods ----

   /**
    * Notes
    * the last section must be valid
    * the header should not have header.new_producers and schedule_version consist with the last valid lwc section
    * the header block number should greater then the last block number of last section
    */
   void chain::newsection( const signed_block_header& header,
                           const incremental_merkle&  blockroot_merkle ){
      eosio_assert( !header.new_producers, "section root header can not contain new_producers" );

      remove_first_section_or_not();

      auto header_block_num = header.block_num();

      const auto& last_section = *(_sections.rbegin());
      eosio_assert( last_section.valid, "last_section is not valid" );
      eosio_assert( header_block_num > last_section.last + 1, "header_block_num should larger then last_section.last + 1" );

      auto active_schedule_id = get_section_active_schedule_id( last_section );
      auto version = _prodsches.get( active_schedule_id ).schedule.version;
      eosio_assert( header.schedule_version == version, "schedule_version not equal to previous one" );

      auto block_signing_key = get_producer_capi_public_key( active_schedule_id, header.producer );

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

   /**
    * the block header must be linkable to the last section
    * can not push repeated block header
    */
   void chain::pushheader( const signed_block_header& header ) {
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

         if ( header_block_num - last_section_first < _gstate.lib_depth ){
            _sections.modify( last_section, same_payer, [&]( auto& r ) {
               r.valid = false;
            });
         }

         while ( _chaindb.rbegin()->block_num != header_block_num - 1 ){
            _chaindb.erase( --_chaindb.end() );
         }

         _sections.modify( last_section, same_payer, [&]( auto& r ) {
            r.clear_from( header_block_num );
         });

         print_f("-- block deleted: from % back to % --", last_section_last, header_block_num);
      }

      // verify linkable
      auto last_bhs = *(_chaindb.rbegin());   // don't make pointer or const, for it needs change
      eosio_assert(std::memcmp(last_bhs.block_id.hash, header.previous.hash, 32) == 0 , "unlinkable block" );

      // verify new block
      block_header_state bhs;
      bhs.block_num           = header_block_num;
      bhs.block_id            = std::move( header_block_id );
      
      last_bhs.blockroot_merkle.append( last_bhs.block_id );
      bhs.blockroot_merkle = std::move(last_bhs.blockroot_merkle);

      // handle bps list replacement
      if ( last_bhs.active_schedule_id == last_bhs.pending_schedule_id ){  // normal circumstances

         // check if last_section valid
         bool valid = false;
         if ( last_section.newprod_block_num != 0 ){
            if ( header_block_num - last_section.newprod_block_num >= 325 + _gstate.lib_depth ){
               valid = true;
            }
         } else {
            if ( header_block_num - last_section_first >= _gstate.lib_depth ){
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
            #ifndef FEW_BP_NODES_TEST
            eosio_assert( header_block_num - last_section.newprod_block_num > 20 * 12, "header_block_num - last_section.newprod_block_num > 20 * 12 failed");
            #endif

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

      // handle header.new_producers
      if ( header.new_producers ){  // has new producers
         eosio_assert( header.new_producers->version == header.schedule_version + 1, " header.new_producers version invalid" );

         _sections.modify( last_section, same_payer, [&]( auto& r ) {
            r.valid = false;
            r.newprod_block_num = header_block_num;
         });

         auto new_schedule_id = _prodsches.available_primary_key();
         _prodsches.emplace( _self, [&]( auto& r ) {
            r.id              = new_schedule_id;
            r.schedule        = *header.new_producers;
            r.schedule_hash   = get_checksum256( *header.new_producers );
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
         bhs.block_signing_key = get_producer_capi_public_key( bhs.active_schedule_id, bhs.header.producer );
      }

      auto dg = bhs_sig_digest( bhs );
      assert_producer_signature( dg, bhs.header.producer_signature, bhs.block_signing_key);

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

   digest_type chain::bhs_sig_digest( const block_header_state& hs ) const {
      auto it = _prodsches.find( hs.pending_schedule_id );
      eosio_assert( it != _prodsches.end(), "internal error: block_header_state::sig_digest" );
      auto header_bmroot = get_checksum256( std::make_pair( hs.header.digest(), hs.blockroot_merkle.get_root() ));
      return get_checksum256( std::make_pair( header_bmroot, it->schedule_hash ));
   }

   capi_public_key chain::get_producer_capi_public_key( uint64_t id, name producer ) const {
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

   void chain::assert_producer_signature(const digest_type& digest,
                                         const capi_signature& signature,
                                         const capi_public_key& pub_key ) const {
      assert_recover_key( reinterpret_cast<const capi_checksum256*>( digest.hash ),
                          reinterpret_cast<const char*>( signature.data ), 66,
                          reinterpret_cast<const char*>( pub_key.data ), 34 );
   }

   uint32_t chain::get_section_active_schedule_id( const section_type& section ) const {
      return _chaindb.get( section.last ).active_schedule_id;
   }

   void chain::remove_section( uint64_t id ){
      auto it = _sections.find( id );
      if ( it != _sections.end() ){
         for( uint64_t num = it->first; num <= it->last; ++num ){
            auto existing = _chaindb.find( num );
            if ( existing != _chaindb.end() ){
               _chaindb.erase( existing );
            }
         }
         _sections.erase( it );
      }
   }

   const static uint32_t trim_length = 50;

   void chain::trim_last_section_or_not() {
      auto lwcls = *(_sections.rbegin());
      if ( lwcls.last - lwcls.first > section_max_length ){

         // delete first 100 blocks in _chaindb
         for ( uint32_t num = lwcls.first; num < lwcls.first + trim_length; ++num ){
            auto it = _chaindb.find( num );
            if ( it != _chaindb.end() ){
               _chaindb.erase( it );
            }
         }

         // construct new section info
         section_type ls = lwcls;
         ls.first += trim_length;

         // replace old section with new section in _sections
         _sections.erase( --_sections.end() );
         _sections.emplace( _self, [&]( auto& r ) {
            r = std::move( ls );
         });
      }
   }

   void chain::remove_first_section_or_not(){
      uint32_t count = 0;
      for( auto it = _sections.begin(); it != _sections.end(); ++it) {
         ++count;
      }
      if ( count >= sections_max_records ){
         remove_section( _sections.begin()->first );
      }
   }

   void chain::fcinit(){
      require_auth(_self);
      while ( _chaindb.begin() != _chaindb.end() ){ _chaindb.erase(_chaindb.begin()); }
      while ( _prodsches.begin() != _prodsches.end() ){ _prodsches.erase(_prodsches.begin()); }
      while ( _sections.begin() != _sections.end() ){ _sections.erase(_sections.begin()); }
   }

// ---- class: section_type ----

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

#ifdef FEW_BP_NODES_TEST
      return;
#endif

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

} /// namespace eosio

EOSIO_DISPATCH( eosio::chain, (setglobal)(chaininit)(pushsection)(rminvalidls)(rmfirstsctn)(relay)(blockmerkle)(fcinit) )


