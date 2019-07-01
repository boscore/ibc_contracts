/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <ibc.chain/types.hpp>
#include <ibc.chain/block_header.hpp>

namespace eosio {

   enum class pbft_message_type : uint8_t {
      prepare,
      commit,
      checkpoint,
      view_change,
      new_view
   };

   struct pbft_message_common {
      pbft_message_type   type;
      time_point          timestamp;

      EOSLIB_SERIALIZE(pbft_message_common, (type)(timestamp))
   };

   struct block_info_type {
      block_id_type   block_id;

      EOSLIB_SERIALIZE(block_info_type, (block_id))
   };

   struct pbft_commit {
      pbft_message_common  common;
      uint32_t             view;
      block_info_type      block_info;
      signature_type       sender_signature;

      digest_type digest( chain_id_type chain_id ) const {
         return get_checksum256( chain_id, common, view, block_info );
      }

      block_id_type  block_id(){ return block_info.block_id; }
      uint32_t       block_num(){ return block_header::num_from_id(block_id()); }

      EOSLIB_SERIALIZE(pbft_commit,  (common)(view)(block_info)(sender_signature))
   };

   struct pbft_checkpoint {
      pbft_message_common common;
      block_info_type     block_info;
      signature_type      sender_signature;

      digest_type digest( chain_id_type chain_id ) const {
         return get_checksum256( chain_id, common, block_info );
      }

      block_id_type  block_id(){ return block_info.block_id; }
      uint32_t       block_num(){ return block_header::num_from_id(block_id()); }

      EOSLIB_SERIALIZE(pbft_checkpoint, (common)(block_info)(sender_signature))
   };

} /// namespace eosio


