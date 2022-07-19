#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>

namespace cnyd {
using std::string;
using namespace eosio;
class [[eosio::contract("cnyd.token")]] cnydtoken : public contract {
    public:
        using contract::contract;
        [[eosio::action]] void transfer(const name& from, const name& to, asset& quantity, const string& memo );
        using transfer_action = eosio::action_wrapper<"transfer"_n, &cnydtoken::transfer>;
};
}