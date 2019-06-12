/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <ibc.chain/types.hpp>

namespace eosio {

   struct pbft_commit {
      string            uuid;
      uint32_t          view;
      uint32_t          block_num;
      block_id_type     block_id;
      capi_public_key   pub_key;
      chain_id_type     chain_id;
      signature_type    producer_signature;
      time_point        timestamp;

      digest_type digest() const {
         return get_checksum256( view, block_num, block_id, pub_key, chain_id, timestamp );
      }

      EOSLIB_SERIALIZE(pbft_commit, (uuid)(view)(block_num)(block_id)(pub_key)(chain_id)(producer_signature)(timestamp))
   };

   struct pbft_checkpoint {
      string            uuid;
      uint32_t          block_num;
      block_id_type     block_id;
      capi_public_key   pub_key;
      chain_id_type     chain_id;
      signature_type    producer_signature;
      time_point        timestamp;

      digest_type digest() const {
         return get_checksum256( block_num, block_id, pub_key, chain_id, timestamp );
      }

      EOSLIB_SERIALIZE(pbft_checkpoint, (uuid)(block_num)(block_id)(pub_key)(chain_id)(producer_signature)(timestamp))
   };

} /// namespace eosio


