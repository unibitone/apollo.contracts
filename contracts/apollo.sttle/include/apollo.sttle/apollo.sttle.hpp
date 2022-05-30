#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>
#include <wasm_db.hpp>
#include "apollo.sttle/apollo.sttle.db.hpp"

namespace apollo {

using std::string;
using namespace eosio;
using namespace wasm::db;

/**
 * The `apollo.sttle` is settlement contract
 *
 */

namespace sttle_status {
    static constexpr eosio::name PAUSE          = "pause"_n;
    static constexpr eosio::name START          = "start"_n;
    static constexpr eosio::name DEL            = "del"_n;

};

namespace sttle_type {
    static constexpr eosio::name ADMIN          = "admin"_n;
    static constexpr eosio::name OWNER          = "owner"_n;

};

enum class err: uint8_t {
    NONE                = 0,
    RECORD_NOT_FOUND    = 1,
    RECORD_EXISTING     = 2,
    SYMBOL_MISMATCH     = 4,
    PARAM_INCORRECT     = 5,
    PAUSED              = 6,
    NO_AUTH             = 7,
    NOT_POSITIVE        = 8,
    NOT_STARTED         = 9,
    OVERSIZED           = 10,
    RECORD_DEL          = 11,
    RECORD_SETTLED      = 12,
    STRATED             = 13,
    OPERATION_FAILURE   = 14

};

static constexpr eosio::name APOLLO_TOKEN{"apollo.token"_n};
//
static constexpr eosio::name APOLLO_EV{"apollo.ev"_n};
static constexpr eosio::name AM_TOKEN{"aplink"_n};
static constexpr symbol   AM_SYMBOL = symbol(symbol_code("APLINK"), 2);
//

class [[eosio::contract("apollo.sttle")]] sttle : public contract {
private:
    dbc        _db;
    dbc        _apollo_db;
    dbc        _am_db;

public:
    using contract::contract;

     sttle(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        _db(_self), _apollo_db(APOLLO_TOKEN), _am_db(AM_TOKEN), contract(receiver, code, ds), _global(_self, _self.value) {
     }

    [[eosio::action]]
    void startmine(const uint32_t& token_id, const name& owner, const name& beneficiary);

    [[eosio::action]]
    void settlement(const name& owner, const uint32_t& sub_token_id, const uinuint16_tt32_t& type );

    [[eosio::action]]
    void pause(const name& owner, const uint32_t& sub_token_id);

    [[eosio::action]]
    void destory(const uint32_t& sub_token_id);

    [[eosio::action]]
    void start( const name& owner, const uint64_t& id );
};
} //namespace apollo
