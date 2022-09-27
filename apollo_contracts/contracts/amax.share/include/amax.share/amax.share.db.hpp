#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <utils.hpp>

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>



namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

#define TBL struct [[eosio::table, eosio::contract("amax.share")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.share")]]

NTBL("global") global_t {
    name admin                              = "armoniaadmin"_n;

    EOSLIB_SERIALIZE( global_t, (admin) )

};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

//scope: self
TBL share_pool_t {
    uint64_t            id;                         //PK
    name                share_admin;                //contract or admin user
    extended_symbol     share_token;                //E.g. 8,AMAX@amax.token
    asset               total_share;                //deposited by users
    asset               total_claimed;
    time_point_sec      created_at;

    share_pool_t() {}
    share_pool_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t scope()const { return 0; }

    uint64_t by_share_admin()const { return share_admin.value; }

    typedef multi_index<"sharepools"_n, share_pool_t,
        indexed_by<"shareadmin"_n, const_mem_fun<share_pool_t, uint64_t, &share_pool_t::by_share_admin> >
     > tbl_t;

    EOSLIB_SERIALIZE( share_pool_t, (id)(share_admin)(share_token)(total_share)(total_claimed)(created_at) )
    
};

//Scope: account
//Note: record will be deleted upon claimshare
TBL share_account_t {
    uint64_t            pool_id;                  //PK
    asset               share;
    time_point_sec      created_at;

    share_account_t() {}
    share_account_t(const uint64_t& pid): pool_id(pid) {}

    uint64_t primary_key()const { return pool_id; }

    typedef multi_index<"shareaccts"_n, share_account_t> tbl_t;

    EOSLIB_SERIALIZE( share_account_t,   (pool_id)(share)(created_at) )

};

} //namespace amax