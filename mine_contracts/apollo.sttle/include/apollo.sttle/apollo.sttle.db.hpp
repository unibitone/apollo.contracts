 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

#include "apollo.sttle/apollo.sttle.external.db.hpp"

using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size());

#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

namespace apollo {

using namespace std;
using namespace eosio;
using namespace wasm;

static constexpr eosio::name active_perm{"active"_n};
static constexpr uint64_t start_time_since_epoch   = 1655569098;
static constexpr uint64_t seconds_per_day       = 24 * 3600;

#define STTLE_TBL [[eosio::table, eosio::contract("apollo.sttle")]]

static uint128_t get_union_id( const name& o, const uint64_t& t_id, const uint64_t& s_t_id ) {
    return ( (uint128_t)o.value ) << 64 | ((uint64_t)t_id << 32 | s_t_id);

}

struct STTLE_TBL sttle_t {
    uint64_t       pk_id;
    name           owner;
    name           beneficiary;
    name           status;
    time_point     last_settled_at;
    uint32_t       token_id;
    uint32_t       sub_token_id;
    token_asset    earning;
    uint16_t       sttle_times = 0;

    uint64_t    primary_key()const { return pk_id; }
    uint128_t   by_union_id()const {
        return ( (uint128_t)owner.value ) << 64 | ((uint64_t)token_id << 32 | sub_token_id);

    }
    uint64_t raw()const { return (uint64_t) token_id << 32 | sub_token_id; }

    // uint64_t    by_sub_token_id()const { return (uint64_t) sub_token_id << 32; }

    sttle_t() {}
    sttle_t( const uint64_t& id ): pk_id(id){}
    typedef eosio::multi_index
            <"sttles"_n, sttle_t,
             indexed_by<"unionid"_n, const_mem_fun<sttle_t, uint128_t, &sttle_t::by_union_id> >
            // indexed_by<"sub_token_id"_n, const_mem_fun<sttle_t, uint64_t, &sttle_t::by_sub_token_id> >
     > idx_t;

     EOSLIB_SERIALIZE( sttle_t, (pk_id)(owner)(beneficiary)(status)(last_settled_at)(token_id)(sub_token_id)(earning)(sttle_times) )
};

} // apollo
