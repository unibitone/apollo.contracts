#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <string>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

using namespace eosio;
using namespace std;
using std::string;

#define MART_TBL [[eosio::table, eosio::contract("apollo.mart")]]
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)
static constexpr symbol CNYD_SYMBOL              = SYMBOL("CNYD", 4);
static constexpr name CNYD_BANK                  { "cnyd.token"_n };
static constexpr name APOLLO_BANK                { "apollo.token"_n };
static constexpr name MART_CONTRACT              { "apollo.mart"_n };
static constexpr name VCOIN_CONTRACT             { "apolloevcoin"_n };

namespace db{

struct asset_symbol {
    uint32_t token_id;      // 1:1 with NFT invars hash
    uint32_t sub_token_id;   // can be token receiving date
    //uint8_t precision = 0;

    asset_symbol() {}
    asset_symbol(const uint64_t raw) {
        token_id = raw >> 32;
        sub_token_id = (raw << 32) >> 32;
    }

    uint64_t raw()const { return (uint64_t) token_id << 32 | sub_token_id; }

    friend bool operator==(const asset_symbol&, const asset_symbol&);

    EOSLIB_SERIALIZE(asset_symbol, (token_id)(sub_token_id) )
};

bool operator==(const asset_symbol& symb1, const asset_symbol& symb2) { 
    return( symb1.token_id == symb2.token_id && symb1.sub_token_id == symb2.sub_token_id ); 
}

struct token_asset {
    int64_t         amount;
    asset_symbol    symbol;

    token_asset() {}
    token_asset(const asset_symbol& symb): amount(0), symbol(symb) {}

    token_asset& operator+=(const token_asset& quantity) { 
        check( quantity.symbol == this->symbol, "symbol mismatch");
        this->amount += quantity.amount; return *this;
    } 
    token_asset& operator-=(const token_asset& quantity) { 
        check( quantity.symbol == this->symbol, "symbol mismatch");
        this->amount -= quantity.amount; return *this; 
    }

    EOSLIB_SERIALIZE(token_asset, (amount)(symbol) )
};    



static constexpr name ORDER_STATUS_NONE     = "none"_n;
static constexpr name ORDER_STATUS_SHELF    = "shelf"_n;
static constexpr name ORDER_STATUS_OFFSHELF = "offshelf"_n;
static constexpr name ORDER_STATUS_SELLOUT  = "sellout"_n;



static constexpr name SELL_TYPE_NONE = "none"_n;
static constexpr name SELL_TYPE_ONSALE = "onsale"_n;

struct MART_TBL sell_order_t{
    token_asset quantity;
    asset price;
    asset electricity_fee;
    time_point created_at;
    time_point updated_at;
    name status = ORDER_STATUS_NONE;
    name sell_type = SELL_TYPE_NONE;
    uint8_t discount = 100;

    uint64_t primary_key() const { return quantity.symbol.token_id; }

    typedef multi_index<"selltokens"_n, sell_order_t
        > tbl_t;
    EOSLIB_SERIALIZE(sell_order_t,(quantity)(electricity_fee)(price)(created_at)(updated_at)(status)(sell_type)(discount))
};

//todo table name deal
struct MART_TBL deal_t{
    uint64_t id;
    token_asset sell_token;
    asset electricity_fee;
    asset token_price;
    int64_t buy_quantity;
    asset pay_amount;
    name buyer;
    uint8_t discount;
    time_point created_at;
    uint8_t status = 9;

    uint64_t primary_key() const { return id; }

    typedef multi_index<"tokenorders"_n, deal_t
        > tbl_t;
    EOSLIB_SERIALIZE(deal_t,(id)(sell_token)(electricity_fee)(token_price)(buy_quantity)(pay_amount)(buyer)(discount)(created_at)(status))
};
}