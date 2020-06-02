#include <eosio/eosio.hpp>
#include "abcounter.cpp"

using namespace eosio;
class [[eosio::contract("addressbook")]] addressbook : public eosio::contract
{
public:
    // 构造函数，需要满足父类 contract的构造
    addressbook(eosio::name receiver, eosio::name code, eosio::datastream<const char *> ds) : contract(receiver, code, ds) {}

    // update 和 insert的组合。提升智能合约的可用性
    [[eosio::action]] void upsert(
        eosio::name user, // name类型实际上是 uint64_t
        std::string first_name,
        std::string last_name,
        uint64_t age,
        std::string street,
        std::string city,
        std::string state)
    {
        // eosio.cdt 提供的用户校验工具。
        // 每个用户只能修改自己的记录, require_auth()会断言执行合约时的用户权限里包含当前name
        require_auth(user);

        // multi_index 构造函数:
        //   code: owner of this table. Use get_self() to get this contract name (assumes same with account name).
        //   scope: table 在scope中唯一。get_first_receiver()获取合约部署的账户
        address_index address(get_self(), get_first_receiver().value);
        auto iterator = address.find(user.value);
        if (iterator == address.end())
        {
            // The user isn't in the table
            //   arg0: payer; the payer of this record who pays the storage usage
            //   arg1: callback: the lambda function to create a reference.
            // [&]: lambda捕获，以引用隐式捕获被使用的自动变量
            address.emplace(user, [&](auto &row) {
                row.key = user;
                row.first_name = first_name;
                row.last_name = last_name;
                row.age = age;
                row.street = street;
                row.city = city;
                row.state = state;
            }  // lambda end
            ); // emplace end

            // notify user
            send_summary(user, " successfully emplaced record to addressbook");
            // count
            increment_counter(user, "emplace");
        }
        else
        {
            std::string changes;

            // The user is in the table
            // arg0: A reference to the object to be updated
            // arg1: payer
            // arg2: callback
            address.modify(iterator, user, [&](auto &row) {
                row.key = user;

                if (row.first_name != first_name)
                {
                    row.first_name = first_name;
                    changes += "first name ";
                }

                if (row.last_name != last_name)
                {
                    row.last_name = last_name;
                    changes += "last name ";
                }

                if (row.age != age)
                {
                    row.age = age;
                    changes += "age ";
                }

                if (row.street != street)
                {
                    row.street = street;
                    changes += "street ";
                }

                if (row.city != city)
                {
                    row.city = city;
                    changes += "city ";
                }

                if (row.state != state)
                {
                    row.state = state;
                    changes += "state ";
                }
            }); // modify end

            if (changes.length() > 0)
            {
                // notify user
                send_summary(user, " successfully modified record to addressbook");
                // count
                increment_counter(user, "modify");
            }
            else
            {
                // inline action
                send_summary(user, " called upsert, but request resulted in no changes.");
            }
        }
    }

    [[eosio::action]] void erase(name user)
    {
        require_auth(user);

        // arg0: code AKA account belongs to
        // arg1: scope
        address_index addresses(get_self(), get_first_receiver().value);

        auto iterator = addresses.find(user.value);
        check(iterator != addresses.end(), "Record does not exist");
        addresses.erase(iterator);

        // notify user
        send_summary(user, " successfully erased record from addressbook");
        // count
        increment_counter(user, "erase");
    }

    [[eosio::action]] void notify(name user, std::string msg)
    {
        // 确保调用此方法的账户是合约账户本身
        require_auth(get_self());
        // 通知user action的执行
        require_recipient(user);
    }

private:
    struct [[eosio::table]] person
    {
        name key;
        std::string first_name;
        std::string last_name;
        std::string street;
        std::string city;
        std::string state;
        uint64_t age;

        // 默认主键, 提供给multi_index调用
        uint64_t primary_key() const { return key.value; }
        // 候选键1, 可在定义multi_index时显示指定
        uint64_t get_secondary_1() const { return age; }
    };

    // use _n operator to define an eosio::name type and uses that to name the table
    typedef eosio::multi_index<"people"_n,
                               person,
                               // indexed_by<> 数据结构提供了multi_index的索引
                               indexed_by<"byage"_n, const_mem_fun<person, uint64_t, &person::get_secondary_1>>>
        address_index;

    void send_summary(name user, std::string message)
    {
        action(
            permission_level{get_self(), "active"_n}, // permission level
            get_self(),                               // code AKA "account where contract is deployed"
            "notify"_n,                               // action name, defined in public:
            std::make_tuple(user, name{user}.to_string() + message))
            .send();
    }

    void increment_counter(name user, std::string type)
    {
        // use count wrapper instead of calling a function
        abcounter::count_action count("abcounter"_n, {get_self(), "active"_n});
        count.send(user, type);
    }
};
