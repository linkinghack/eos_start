/**
 * hodl 模拟一个定期存款合约
 * cleos push action eosio.token transfer '[ "alice", "han", "100.0000 SYS", "Slecht geld verdrijft goed" ]' -p alice@active
 * cleos transfer han hodl '0.0001 SYS' 'Hodl!' -p han@active  // han 存进hodl银行0.0001 SYS
 *  cleos push action hodl party '["han"]' -p han@active  // 取款
*/
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>

using namespace eosio;

class [[eosio::contract("hodl")]] hodl : public eosio::contract {
    private:
        // static const uint32_t the_party = 1645525342; // 2022, Feb, 22, 10:22:22PM
        static const uint32_t the_party = 1528549200; // 9 June 2018 01:00:00
        const eosio::symbol hodl_symbol;
        
        // multi_index table
        struct [[eosio::table]] balance
        {
            eosio::asset funds;
            uint64_t primary_key() const { return funds.symbol.raw(); }
        };
    
        uint32_t now() {
            return current_time_point().sec_since_epoch();
        }

        using balance_table = eosio::multi_index<"balance"_n, balance>;

    public:
        using contract::contract;
        hodl(name receiver, name code, datastream<const char*> ds):contract(receiver, code, ds), 
                                                                    hodl_symbol("SYS", 4) {}
                                                                    // symbol=SYS, precision=4

        // on_notify attribute 保证仅仅来自eosio.token合约的transfer action 
        //   的notification会被指向deposit
        //  即此action
        [[eosio::on_notify("eosio.token::transfer")]]
        void deposit(name holder, name to, eosio::asset quantity, std::string memo)
        {
            // get_self() 获取合约部署账户
            if (to != get_self() || holder == get_self())
            {
                print("These are not the droids you are looking for.");
                return;
            }

            check(now() < the_party, "You're way late");
            check(quantity.amount > 0, "When pigs fly");
            check(quantity.symbol == hodl_symbol, "These are not the droids you are looking for.");

            balance_table balance(get_self(), holder.value);
            auto hodl_it = balance.find(hodl_symbol.raw());

            if (hodl_it != balance.end())
            {
                balance.modify(hodl_it, get_self(), [&](auto &row) {
                    row.funds += quantity;
                });
            } else {
                balance.emplace(get_self(), [&](auto &row) {
                    row.funds = quantity;
                });
            }
        }

        [[eosio::action]]
        void party(name hodler)
        {
            require_auth(hodler);

            check(now() > the_party, "Hold you horses");

            balance_table balance(get_self(), hodler.value);
            auto hodl_it = balance.find(hodl_symbol.raw());

            check(hodl_it != balance.end(), "You're not allowed to party");

            action {
                permission_level{get_self(), "active"_n},
                "eosio.token"_n,
                "transfer"_n,
                std::make_tuple(get_self(), hodler, hodl_it->funds, 
                std::string("Party! Your hodl is free"))
            }.send();

            balance.erase(hodl_it);
        }

};

