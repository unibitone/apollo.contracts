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
class [[eosio::contract("apollo.sttle")]] sttle : public contract {
private:
   dbc                 _db;
   global_singleton    _global;
   global_t            _gstate;

public:
   using contract::contract;

   sttle(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        _db(_self), contract(receiver, code, ds), _global(_self, _self.value) {
        if (_global.exists()) {
            _gstate = _global.get();

        } else { // first init
            _gstate = global_t{};
            _gstate.admin = _self;
        }
    }

    ~sttle() { _global.set( _gstate, get_self() ); }


    [[eosio::action]]
    void setconf(const time& check_begin, const time& check_end, const time& sttle_begin, const time& sttle_end, const asset&  quantity, const hashrate_t&  hashrate);

    [[eosio::action]]
    void startmine(const String& nft_id, const time_point& mine_at, const time_point& start_at);

    [[eosio::action]]
    void deposits(const name& owner, const asset& quantity);

    [[eosio::action]]
    void destory(const string& nft_id);
};
} //namespace apollo
