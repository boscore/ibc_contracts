/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#pragma once

#include <ibc.chain/types.hpp>

namespace eosio {
   digest_type make_canonical_left(const digest_type& val);
   digest_type make_canonical_right(const digest_type& val);
   bool is_canonical_left(const digest_type& val);
   bool is_canonical_right(const digest_type& val);

   inline auto make_canonical_pair(const digest_type& l, const digest_type& r) {
      return std::make_pair(make_canonical_left(l), make_canonical_right(r));
   }

   inline auto sha256hash(std::pair<digest_type,digest_type> pair_data){
      std::vector<char> buf = pack(pair_data);
      ::capi_checksum256 hash;
      ::sha256( reinterpret_cast<char*>(buf.data()), buf.size(), &hash );
      return hash;
   }

   namespace detail {
      constexpr uint64_t next_power_of_2(uint64_t value) {
         value -= 1;
         value |= value >> 1;
         value |= value >> 2;
         value |= value >> 4;
         value |= value >> 8;
         value |= value >> 16;
         value |= value >> 32;
         value += 1;
         return value;
      }

      constexpr int clz_power_2(uint64_t value) {
         int lz = 64;

         if (value) lz--;
         if (value & 0x00000000FFFFFFFFULL) lz -= 32;
         if (value & 0x0000FFFF0000FFFFULL) lz -= 16;
         if (value & 0x00FF00FF00FF00FFULL) lz -= 8;
         if (value & 0x0F0F0F0F0F0F0F0FULL) lz -= 4;
         if (value & 0x3333333333333333ULL) lz -= 2;
         if (value & 0x5555555555555555ULL) lz -= 1;

         return lz;
      }

      constexpr int calcluate_max_depth(uint64_t node_count) {
         if (node_count == 0) {
            return 0;
         }
         auto implied_count = next_power_of_2(node_count);
         return clz_power_2(implied_count) + 1;
      }

      template<typename ContainerA, typename ContainerB>
      inline void move_nodes(ContainerA& to, const ContainerB& from) {
         to.clear();
         to.insert(to.begin(), from.begin(), from.end());
      }

      template<typename Container>
      inline void move_nodes(Container& to, Container&& from) {
         to = std::forward<Container>(from);
      }
   } /// detail

   struct incremental_merkle {
      const digest_type& append(const digest_type& digest) {
         bool partial = false;
         auto max_depth = detail::calcluate_max_depth(_node_count + 1);
         auto current_depth = max_depth - 1;
         auto index = _node_count;
         auto top = digest;
         auto active_iter = _active_nodes.begin();
         auto updated_active_nodes = std::vector<digest_type>();
         updated_active_nodes.reserve(max_depth);

         while (current_depth > 0) {
            if (!(index & 0x1)) {
               // we are collapsing from a "left" value and an implied "right" creating a partial node

               // we only need to append this node if it is fully-realized and by definition
               // if we have encountered a partial node during collapse this cannot be
               // fully-realized
               if (!partial) {
                  updated_active_nodes.emplace_back(top);
               }

               // calculate the partially realized node value by implying the "right" value is identical
               // to the "left" value
               top = sha256hash(make_canonical_pair(top, top));
               partial = true;
            } else {
               // we are collapsing from a "right" value and an fully-realized "left"

               // pull a "left" value from the previous active nodes
               const auto& left_value = *active_iter;
               ++active_iter;
               // if the "right" value is a partial node we will need to copy the "left" as future appends still need it
               // otherwise, it can be dropped from the set of active nodes as we are collapsing a fully-realized node
               if (partial) {
                  updated_active_nodes.emplace_back(left_value);
               }

               // calculate the node
               top = sha256hash(make_canonical_pair(left_value, top));
            }

            // move up a level in the tree
            current_depth--;
            index = index >> 1;
         }

         // append the top of the collapsed tree (aka the root of the merkle)
         updated_active_nodes.emplace_back(top);

         // store the new active_nodes
         detail::move_nodes(_active_nodes, std::move(updated_active_nodes));

         // update the node count
         _node_count++;

         return _active_nodes.back();

      }

      digest_type get_root() const {
         if (_node_count > 0) {
            return _active_nodes.back();
         } else {
            return digest_type();
         }
      }

      std::vector<digest_type>   _active_nodes;
      uint64_t                   _node_count;

      // Keep the same member order as defined at libraries/chain/include/eosio/chain/incremental_merkle.hpp:251
      // and membership order above needs to be same as serialization order below in contract.
      EOSLIB_SERIALIZE(incremental_merkle, (_active_nodes)(_node_count))
   };
} /// eosio

