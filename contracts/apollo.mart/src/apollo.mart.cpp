#include <apollo.mart/apollo.mart.hpp>
#include <apollo.token.hpp>
#include <cnyd.token.hpp>
#include <utils.hpp>

using namespace db;
using namespace apollo;
using namespace cnyd;

static constexpr eosio::name active_permission{"active"_n};


[[eosio::on_notify("apollo.token::transfer")]]
void mart::ontransfer(const name& from, const name& to, const token_asset& quantity,const string& memo){
    if(from == APOLLO_BANK && to == MART_CONTRACT){
        apollo_token_t::tbl_t token_tbl(get_self(), to.value);
        auto apollo_token_itr = token_tbl.find(quantity.symbol.token_id);
        if(apollo_token_itr == token_tbl.end()){
            token_tbl.emplace(get_self(),[&](auto& item){
                item.id =  quantity.symbol.token_id;
                item.sell_token = quantity;
                item.electricity_price = asset(0, symbol("CNYD",4));
                item.token_price = asset(0, symbol("CNYD",4));
                item.created_at = current_time_point();
                item.updated_at = current_time_point();
            });
        }else{
            token_tbl.modify(apollo_token_itr, get_self(), [&](auto& item){
                item.sell_token.amount += quantity.amount;
            });
        }
    }
}

[[eosio::on_notify("cnyd.token::transfer")]] 
void mart::buynft(name& from,name& to,asset& quantity,string& memo){
     if (from == get_self() || to != get_self()) return;
     check(quantity.amount > 0, "quantity must be positive");
     //buy nft eg:"nft:1(nft id):50(nft quantity)"
     vector<string_view> memo_params = split(memo, ":");
     if (memo_params[0] == "nft") {
        auto param_nft_id = memo_params[1];
        auto param_quantity = memo_params[2];
        check(!param_nft_id.empty() , "param nft id is missing");
        check(!param_quantity.empty(), "param nft quantity is missing");
        apollo_token_t::tbl_t token_tbl(get_self(), get_self().value);
        uint64_t nft_id = 0;
        nft_id = to_uint64(param_nft_id.data(),"nft_id");
        check( nft_id != 0, "nft id can not be 0" );
        auto nft_token_itr = token_tbl.find(nft_id);
        check(nft_token_itr != token_tbl.end(),"nft token not found:"+to_string(nft_id));

        int64_t nft_quantity = 0;
        nft_quantity = to_int64(param_quantity.data(),"nft_quantity");
        check(nft_quantity > 0, "nft quantity can not be 0" );
        check(quantity.symbol == CNYD_SYMBOL , "quantity symbol mismatch with cnyd symbol");
        check(nft_token_itr->sell_token.amount >= nft_quantity,"Insufficient stock of nft("+to_string(nft_id)+"), remaining quantity is "+to_string(nft_token_itr->sell_token.amount));
        
        int64_t total_electricity_price = multiply_i64(nft_quantity,nft_token_itr->electricity_price.amount);
        int64_t total_token_price = multiply_i64(nft_quantity,nft_token_itr->token_price.amount);
        int64_t total_amount = total_electricity_price + total_token_price;
        check(quantity.amount == total_amount , "insufficient amount");
        
        token_order_t::tbl_t order_tbl(get_self(), get_self().value);
        auto order_id = order_tbl.available_primary_key();
        order_tbl.emplace(get_self(),[&](auto& item){
            item.id = order_id;
            item.sell_token = nft_token_itr->sell_token;
            item.electricity_price = nft_token_itr->electricity_price;
            item.token_price = nft_token_itr->token_price;
            item.buy_quantity = nft_quantity;
            item.pay_amount = quantity;
            item.buyer = from;
            item.discount = nft_token_itr->discount;
            item.created_at = current_time_point();
        });

        token_tbl.modify(nft_token_itr,get_self(),[&](auto& item){
            item.sell_token.amount -= nft_quantity;
            if(item.sell_token.amount == 0){
                item.status = APOLLO_STATUS_SELLOUT;
            }
        });
        //transfer nft
        auto transfer_nft = token_asset(nft_token_itr->sell_token.symbol);
        transfer_nft.amount = nft_quantity;
        apollotoken::transfer_action(APOLLO_BANK, {{get_self(), active_permission}}).send(get_self(), from, transfer_nft, "nft transfer");
        //recharge
        asset recharge_asset = asset(total_electricity_price,CNYD_SYMBOL);
        string recharge_memo = "topup:"+from.to_string();
        cnydtoken::transfer_action(CNYD_BANK, {{get_self(), active_permission}}).send(get_self(), VCOIN_CONTRACT, recharge_asset, recharge_memo);
     }
}

[[eosio::action]] 
void mart::updatetoken(const uint64_t& id,const asset& electricity_price,const asset& token_price,const uint8_t& status,const uint8_t& sell_type,const uint8_t& discount){
    require_auth(get_self());
    apollo_token_t::tbl_t token_tbl(get_self(),get_self().value);
    auto apollo_token_itr = token_tbl.find(id);
    check(apollo_token_itr != token_tbl.end(),"token not found:"+to_string(id));
    check(electricity_price.symbol == CNYD_SYMBOL , "electricity_price symbol mismatch with cnyd symbol ");
    check(electricity_price.amount >0 , "electricity_price must be positive");
    check(token_price.symbol == CNYD_SYMBOL , "token_price symbol mismatch with cnyd symbol ");
    check(token_price.amount > 0 , "token_price must be positive");
    check(discount > 0 && discount <= 100,"discount must be positive");
    token_tbl.modify(apollo_token_itr,get_self(),[&](auto& item){
        item.electricity_price = electricity_price;
        item.token_price = token_price;
        item.status = status;
        item.sell_type = sell_type;
        item.discount = discount;
        item.updated_at = current_time_point();
    });
}

[[eosio::action]]
void mart::updatestatus(const uint64_t& id,const uint8_t& status){
    require_auth(get_self());
    apollo_token_t::tbl_t token_tbl(get_self(),get_self().value);
    auto apollo_token_itr = token_tbl.find(id);
    check(apollo_token_itr != token_tbl.end(),"token not found:"+to_string(id));
    token_tbl.modify(apollo_token_itr,get_self(),[&](auto& item){
        item.status = status;
    });
}




