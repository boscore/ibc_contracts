/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <ibc.chain/block_header.hpp>

namespace eosio {

   inline uint64_t endian_reverse_u64( uint64_t x )
   {
      return (((x >> 0x38) & 0xFF)        )
             | (((x >> 0x30) & 0xFF) << 0x08)
             | (((x >> 0x28) & 0xFF) << 0x10)
             | (((x >> 0x20) & 0xFF) << 0x18)
             | (((x >> 0x18) & 0xFF) << 0x20)
             | (((x >> 0x10) & 0xFF) << 0x28)
             | (((x >> 0x08) & 0xFF) << 0x30)
             | (((x        ) & 0xFF) << 0x38)
         ;
   }

   inline uint32_t endian_reverse_u32( uint32_t x )
   {
      return (((x >> 0x18) & 0xFF)        )
             | (((x >> 0x10) & 0xFF) << 0x08)
             | (((x >> 0x08) & 0xFF) << 0x10)
             | (((x        ) & 0xFF) << 0x18)
         ;
   }

   digest_type block_header::digest()const
   {
      std::vector<char> buf = pack(*this);
      ::capi_checksum256 hash;
      ::sha256( reinterpret_cast<char*>(buf.data()), buf.size(), &hash );
      return hash;
   }

   uint32_t block_header::num_from_id(const block_id_type& id)
   {
      return endian_reverse_u32(*(uint64_t*)(id.hash));
   }

   block_id_type block_header::id()const
   {
      union {
         block_id_type result;
         uint64_t hash64[4];
      }u;

      u.result = digest();
      u.hash64[0] &= 0xffffffff00000000;
      u.hash64[0] += endian_reverse_u32(block_num());
      return u.result;
   }

} /// namespace eosio