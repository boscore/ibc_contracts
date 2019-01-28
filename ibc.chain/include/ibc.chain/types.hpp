/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <string>
#include <eosiolib/varint.hpp>
#include <eosiolib/privileged.hpp>

namespace eosio {

   using std::string;

   typedef capi_checksum256   digest_type;
   typedef capi_checksum256   block_id_type;
   typedef capi_checksum256   transaction_id_type;
   typedef capi_signature     signature_type;

   template<typename T>
   void push(T&){}

   template<typename Stream, typename T, typename ... Types>
   void push(Stream &s, T arg, Types ... args){
      s << arg;
      push(s, args...);
   }

   template<class ... Types> capi_checksum256 get_checksum256(const Types & ... args ){
      datastream <size_t> ps;
      push(ps, args...);
      size_t size = ps.tellp();

      std::vector<char> result;
      result.resize(size);

      datastream<char *> ds(result.data(), result.size());
      push(ds, args...);
      capi_checksum256 digest;
      sha256(result.data(), result.size(), &digest);
      return digest;
   }

   inline bool is_equal_capi_checksum256( capi_checksum256 a, capi_checksum256 b ){
      return std::memcmp( a.hash, b.hash, 32 ) == 0;
   }
}
