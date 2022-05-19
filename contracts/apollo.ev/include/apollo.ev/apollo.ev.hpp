#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>
#include <algorithm>

using namespace std;
using namespace eosio;

#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr symbol   AEV_SYMBOL          = symbol(symbol_code("AEV"), 4);
static constexpr symbol CNYD_SYMBOL = SYMBOL("CNYD", 4);
static constexpr eosio::name CNYD_BANK{"cnyd.token"_n};

namespace apollo {

   using std::string;

   /**
    * The `amax.token` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.token` contract instead of developing their own.
    * 
    * The `amax.token` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
    * 
    * The `amax.token` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
    * 
    * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
    */
   class [[eosio::contract("apollo.ev")]] token : public contract {
      public:
         using contract::contract;

         /**
          * Allows `issuer` account to create a token in supply of `maximum_supply`. If validation is successful a new entry in statstable for token symbol scope gets created.
          *
          * @param issuer - the account that creates the token,
          * @param maximum_supply - the maximum supply set for the token created.
          *
          * @pre Token symbol has to be valid,
          * @pre Token symbol must not be already created,
          * @pre maximum_supply has to be smaller than the maximum supply allowed by the system: 1^62 - 1.
          * @pre Maximum supply must be positive;
          */
         [[eosio::action]]
         void create( const name&   issuer,
                      const name& burner,
                    const name& taker,
                      const asset&  maximum_supply);
         /**
          *  This action issues to `to` account a `quantity` of tokens.
          *
          * @param to - the account to issue tokens to, it must be the same as the issuer,
          * @param quntity - the amount of tokens to be issued,
          * @memo - the memo string that accompanies the token issue transaction.
          */
         [[eosio::action]]
         void issue( const name& to, const asset& quantity, const string& memo );

         /**
          * The opposite for create action, if all validations succeed,
          * it debits the statstable.supply amount.
          *
          * @param quantity - the quantity of tokens to retire,
          * @param memo - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         void retire( const asset& quantity, const string& memo );

         /**
          * The opposite for issue action, if all validations succeed,
          * it debits the statstable.supply amount.
          *
          * @param to - the owner to retire token
          * @param quantity - the quantity of tokens to retire,
          * @param memo - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         void burn(const name& to, const asset& quantity, const string& memo );

         /**
          * Allows `from` account to transfer to `to` account the `quantity` tokens.
          * One account is debited and the other is credited with quantity tokens.
          *
          * @param from - the account to transfer from,
          * @param to - the account to be transferred to,
          * @param quantity - the quantity of tokens to be transferred,
          * @param memo - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         void transfer( const name&    from,
                        const name&    to,
                        const asset&   quantity,
                        const string&  memo );
                        
         /**
          * Allows `ram_payer` to create an account `owner` with zero balance for
          * token `symbol` at the expense of `ram_payer`.
          *
          * @param owner - the account to be created,
          * @param symbol - the token to be payed with by `ram_payer`,
          * @param ram_payer - the account that supports the cost of this action.
          *
          * More information can be read [here](https://github.com/armoniax/amax.contracts/issues/62)
          * and [here](https://github.com/armoniax/amax.contracts/issues/61).
          */
         [[eosio::action]]
         void open( const name& owner, const symbol& symbol, const name& ram_payer );

         /**
          * This action is the opposite for open, it closes the account `owner`
          * for token `symbol`.
          *
          * @param owner - the owner account to execute the close action for,
          * @param symbol - the symbol of the token to execute the close action for.
          *
          * @pre The pair of owner plus symbol has to exist otherwise no action is executed,
          * @pre If the pair of owner plus symbol exists, the balance has to be zero.
          */
         [[eosio::action]]
         void close( const name& owner, const symbol& symbol );

         /**
          * ontransfer, trigger by recipient of transfer()
          * @param memo - memo format:
          * 1. charge:${receiver} Eg: "charge:receiver1234"
          *    @param receiver - charge to receiver, send to &from if empty memo
          *
          *    transfer() params:
          *    @param from - default eletricity receiver
          *    @param to   - must be contract self
          *    @param quantity - issued quantity
          */
         
         [[eosio::on_notify("cnyd.token::transfer")]] 
         void ontransfer(name from, name to, asset quantity, string memo);

         /**
          * @require run by taker only
          * transfer CNYD from contract to taker
          */
         [[eosio::action]]
         void withdraw(asset quantity, string memo);

         static asset get_supply( const name& token_contract_account, const symbol_code& sym_code )
         {
            stats statstable( token_contract_account, sym_code.raw() );
            const auto& st = statstable.get( sym_code.raw() );
            return st.supply;
         }

         static asset get_balance( const name& token_contract_account, const name& owner, const symbol_code& sym_code )
         {
            accounts accountstable( token_contract_account, owner.value );
            const auto& ac = accountstable.get( sym_code.raw() );
            return ac.balance;
         }

         using create_action = eosio::action_wrapper<"create"_n, &token::create>;
         // using issue_action = eosio::action_wrapper<"issue"_n, &token::issue>;
         using retire_action = eosio::action_wrapper<"retire"_n, &token::retire>;
         using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;
         using open_action = eosio::action_wrapper<"open"_n, &token::open>;
         using close_action = eosio::action_wrapper<"close"_n, &token::close>;
         using burn_action = eosio::action_wrapper<"burn"_n, &token::burn>;
         using withdraw_action = eosio::action_wrapper<"withdraw"_n, &token::withdraw>;
      private:
         struct [[eosio::table]] account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };

         struct [[eosio::table]] currency_stats {
            asset    supply;
            asset    max_supply;
            name     issuer;
            name     burner;
            name     taker;

            uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };

         typedef eosio::multi_index< "accounts"_n, account > accounts;
         typedef eosio::multi_index< "stat"_n, currency_stats > stats;

         void sub_balance( const name& owner, const asset& value );
         void add_balance( const name& owner, const asset& value, const name& ram_payer );
         
         string_view trim(string_view sv) {
            sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size())); // left trim
            sv.remove_suffix(std::min(sv.size()-sv.find_last_not_of(" ")-1, sv.size())); // right trim
            return sv;
         }
         vector<string_view> split(string_view str, string_view delims = " ")
         {
            vector<string_view> res;
            std::size_t current, previous = 0;
            current = str.find_first_of(delims);
            while (current != std::string::npos) {
               res.push_back(trim(str.substr(previous, current - previous)));
               previous = current + 1;
               current = str.find_first_of(delims, previous);
            }
            res.push_back(trim(str.substr(previous, current - previous)));
            return res;
         }

   };

}
