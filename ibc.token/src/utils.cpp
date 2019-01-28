/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

namespace eosio{

   void trim(string &s) {
      if( !s.empty() ) {
         s.erase(0,s.find_first_not_of(" "));
         s.erase(s.find_last_not_of(" ") + 1);
      }
   }

   capi_checksum256 get_trx_id( bool assert_only_one_action = true ) {
      capi_checksum256 trx_id;
      std::vector<char> trx_bytes;
      size_t trx_size = transaction_size();
      trx_bytes.resize(trx_size);
      read_transaction(trx_bytes.data(), trx_size);
      sha256( trx_bytes.data(), trx_size, &trx_id );
      
      if ( assert_only_one_action ) {
         auto trx = unpack<transaction>(trx_bytes.data(), trx_size);
         eosio_assert( trx.actions.size() == 1, "transction contains more then one action");
      }
      return trx_id;
   }

   uint32_t get_block_time_slot() {
      return  ( current_time() / 1000 - 946684800000 ) / 500;
   }

   string to_hex( const uint8_t* d, uint32_t s )
   {
      std::string r;
      const char* to_hex="0123456789abcdef";
      for( uint32_t i = 0; i < s; ++i )
         (r += to_hex[(d[i]>>4)]) += to_hex[(d[i] &0x0f)];
      return r;
   }

   string capi_checksum256_to_string( capi_checksum256 value ){
      return to_hex( value.hash, 32 );
   }

}
