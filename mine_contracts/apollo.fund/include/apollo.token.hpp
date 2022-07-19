#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>
#include <apollo.mart/apollo.mart.db.hpp>

namespace apollo {
using std::string;
using namespace eosio;
class [[eosio::contract("apollo.token")]] apollotoken : public contract {
    public:
        using contract::contract;
        [[eosio::action]] void transfer(const name& from, const name& to, token_asset& quantity, const string& memo );
        using transfer_action = eosio::action_wrapper<"transfer"_n, &apollotoken::transfer>;
};
}