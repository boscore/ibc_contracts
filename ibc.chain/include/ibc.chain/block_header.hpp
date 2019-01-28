/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/time.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/producer_schedule.hpp>
#include <eosiolib/transaction.hpp>
#include <ibc.chain/types.hpp>

namespace eosio {

   struct block_header {
      block_timestamp                           timestamp;
      name                                      producer;
      uint16_t                                  confirmed;
      block_id_type                             previous;
      capi_checksum256                          transaction_mroot;
      capi_checksum256                          action_mroot;
      uint32_t                                  schedule_version;
      std::optional<eosio::producer_schedule>   new_producers;
      extensions_type                           header_extensions;

      capi_checksum256     digest()const;
      block_id_type        id() const;
      uint32_t             block_num() const { return num_from_id(previous) + 1; }
      static uint32_t      num_from_id(const capi_checksum256& id);

      EOSLIB_SERIALIZE(block_header, (timestamp)(producer)(confirmed)(previous)(transaction_mroot)(action_mroot)
         (schedule_version)(new_producers)(header_extensions))
   };

   struct signed_block_header : public block_header {
      capi_signature     producer_signature;

      EOSLIB_SERIALIZE_DERIVED( signed_block_header, block_header, (producer_signature) )
   };

} /// namespace eosio


