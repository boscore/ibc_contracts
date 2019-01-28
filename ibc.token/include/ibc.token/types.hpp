/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/transaction.hpp>
#include <ibc.chain/types.hpp>

namespace eosio {

   struct packed_transaction {
      enum compression_type : uint8_t {
         none = 0,
         zlib = 1,
      };

      std::vector<signature_type>   signatures;
      compression_type              compression;
      std::vector<char>             packed_context_free_data;
      std::vector<char>             packed_trx;

      digest_type packed_digest() const {
         auto digest = get_checksum256( signatures, packed_context_free_data );
         return get_checksum256( compression, packed_trx, digest );
      }

      transaction_id_type id()const {
         capi_checksum256 digest;
         sha256(packed_trx.data(), packed_trx.size(), &digest);
         return digest;
      }

      EOSLIB_SERIALIZE( packed_transaction, (signatures)(compression)(packed_context_free_data)(packed_trx))
   };

   struct transaction_receipt_header {
      enum status_enum : uint8_t {
         executed = 0,      ///< succeed, no error handler executed
         soft_fail = 1,     ///< objectively failed (not executed), error handler executed
         hard_fail = 2,     ///< objectively failed and error handler objectively failed thus no state change
         delayed = 3,       ///< transaction delayed/deferred/scheduled for future execution
         expired = 4        ///< transaction expired and storage space refuned to user
      };

      status_enum    status;
      uint32_t       cpu_usage_us;
      unsigned_int   net_usage_words;

      EOSLIB_SERIALIZE( transaction_receipt_header, (status)(cpu_usage_us)(net_usage_words))
   };

   struct transaction_receipt : public transaction_receipt_header {
      std::variant<transaction_id_type,packed_transaction> trx;
      digest_type digest() const {
         return get_checksum256(status, cpu_usage_us, net_usage_words, std::get<packed_transaction>(trx).packed_digest());
      }
      EOSLIB_SERIALIZE_DERIVED( transaction_receipt, transaction_receipt_header,(trx))
   };

}
