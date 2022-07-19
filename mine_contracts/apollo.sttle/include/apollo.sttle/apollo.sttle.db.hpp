 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

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


struct STTLE_TBL sttle_t {
    name        owner;
    name        beneficiary;
    uint32_t    token_id;
    uint32_t    sub_token_id;
    asset       profit;
    time_point  last_mine_at;
    uint16_t    sttle_times = 0;
    uint16_t    status = 0;
    uint64_t    primary_key()const { 
        string str =    owner.to_string() + "\n" + 
                        to_string(token_id) + "\n" +
                        to_string(sub_token_id);

        return static_cast<uint64_t> ( HASH256(str) );

    }

    uint64_t    scope() const { return 0; }
    // uint64_t    by_token_id()const { return (uint64_t) token_id << 32; }
    // uint64_t    by_sub_token_id()const { return (uint64_t) sub_token_id << 32; }

    sttle_t() {}
    sttle_t(const name& owner,const uint32_t& t_id,const uint32_t& s_t_id): owner(c), token_id(t_id), sub_token_id(s_t_id) {}

    typedef eosio::multi_index
            <"sttles"_n, sttle_t,
            // indexed_by<"token_id"_n, const_mem_fun<sttle_t, uint64_t, &sttle_t::by_token_id> >,
            // indexed_by<"sub_token_id"_n, const_mem_fun<sttle_t, uint64_t, &sttle_t::by_sub_token_id> >
     > tbl_t;

     EOSLIB_SERIALIZE( sttle_t, (owner)(beneficiary)(sub_token_id)(profit)(last_mine_at)(sttle_times)(status) )
};

} // apollo
