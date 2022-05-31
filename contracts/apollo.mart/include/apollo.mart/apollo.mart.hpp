#pragma once

#include "apollo.mart/apollo.mart.db.hpp"


using namespace db;

class [[eosio::contract("apollo.mart")]] mart : public eosio::contract
{

    public:
        using contract::contract;
        
        mart(eosio::name receiver, eosio::name code, datastream<const char*> ds):contract(receiver, code, ds){}

        [[eosio::action]] void updatetoken(const uint64_t& id,const asset& electricity_price,const asset& token_price,const uint8_t& status,const uint8_t& sell_type,const uint8_t& discount);
        
        [[eosio::action]] void updatestatus(const uint64_t& id,const uint8_t& status);

        [[eosio::on_notify("cnyd.token::transfer")]] void buynft(name& from,name& to,asset& quantity,string& memo);

        [[eosio::on_notify("apollo.token::transfer")]] void ontransfer(const name& from,const name& to, const token_asset& quantity, const string& memo);
};
