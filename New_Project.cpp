#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <chrono>
#include <thread>

int Coins = 5;

using namespace std;

template<typename... Args>
class Event
{
public:
    using Handler = std::function<void(Args...)>;
    using SubscriptionId = uint64_t;

private:
    struct ICallback
    {
        virtual ~ICallback() = default;
        virtual bool IsAlive() const = 0;
        virtual void Invoke(Args... args) = 0;
    };

    struct LambdaCallback : ICallback
    {
        Handler HandlerFunc;

        explicit LambdaCallback(Handler handler)
            : HandlerFunc(std::move(handler))
        {
        }

        bool IsAlive() const override
        {
            return true;
        }

        void Invoke(Args... args) override
        {
            HandlerFunc(args...);
        }
    };

    template<typename TObject>
    struct WeakObjectCallback : ICallback
    {
        std::weak_ptr<TObject> WeakObject;
        std::function<void(TObject&, Args...)> Method;

        WeakObjectCallback(
            std::weak_ptr<TObject> weakObject,
            std::function<void(TObject&, Args...)> method)
            : WeakObject(std::move(weakObject))
            , Method(std::move(method))
        {
        }

        bool IsAlive() const override
        {
            return !WeakObject.expired();
        }

        void Invoke(Args... args) override
        {
            if (auto object = WeakObject.lock())
            {
                Method(*object, args...);
            }
        }
    };

public:
    SubscriptionId Subscribe(Handler handler)
    {
        const SubscriptionId id = ++NextId;
        Callbacks.emplace(id, std::make_unique<LambdaCallback>(std::move(handler)));
        return id;
    }

    template<typename TObject>
    SubscriptionId SubscribeWeak(
        const std::shared_ptr<TObject>& object,
        std::function<void(TObject&, Args...)> method)
    {
        const SubscriptionId id = ++NextId;

        Callbacks.emplace(
            id,
            std::make_unique<WeakObjectCallback<TObject>>(
                object,
                std::move(method)));

        return id;
    }

    void Unsubscribe(SubscriptionId id)
    {
        Callbacks.erase(id);
    }

    void Notify(Args... args)
    {
        std::vector<SubscriptionId> deadSubscriptions;

        for (auto& [id, callback] : Callbacks)
        {
            if (!callback->IsAlive())
            {
                deadSubscriptions.push_back(id);
                continue;
            }

            callback->Invoke(args...);
        }

        for (SubscriptionId id : deadSubscriptions)
        {
            Callbacks.erase(id);
        }
    }

    void Clear()
    {
        Callbacks.clear();
    }

private:
    std::unordered_map<SubscriptionId, std::unique_ptr<ICallback>> Callbacks;
    SubscriptionId NextId = 0;
};


class Enemy;


class Player : public std::enable_shared_from_this<Player>
{
    std::string Name;
    int Health = 125;
    int AttackDamage = 20;

public:
    Event<int> DamageEvent;

    void SetName(const std::string& name)
    {
        Name = name;
    }

    std::string GetName() const
    {
        return Name;
    }

    int GetHealth() const
    {
        return Health;
    }

    void Damage(int damage)
    {
        DamageEvent.Notify(damage);
    }

    void Attack(Enemy& enemy);

    virtual void OnDamage(int damage)
    {
        Health -= damage;

        if (Health < 0)
        {
            Health = 0;
        }

        std::cout << Name << " took " << damage << " damage\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        std::cout << Name << " HP: " << Health << "\n";
        std::cout << "    " << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (Health < 1)
        {
            std::cout << Name << " is dead\n";
        }
    }
};


class Enemy : public std::enable_shared_from_this<Enemy>
{
    std::string Name;
    int MaxHealth = 50;
    int Health = MaxHealth;
    int AttackDamage = 25;
    int CoinReward = 10;
    bool IsDead = false;

public:
    Event<int> DamageEvent;

    Enemy(std::string name, int coinReward)
        : Name(std::move(name))
        , CoinReward(coinReward)
    {
    }

    std::string GetName() const
    {
        return Name;
    }

    int GetHealth() const
    {
        return Health;
    }

    bool GetIsDead() const
    {
        return IsDead;
    }

    void Reset()
    {
        Health = MaxHealth;
        IsDead = false;
    }

    void Damage(int damage)
    {
        DamageEvent.Notify(damage);
    }

    void Attack(Player& player)
    {
        if (IsDead)
        {
            return;
        }

        std::cout << Name << " attacks " << player.GetName() << "\n";
        player.Damage(AttackDamage);
    }

    void OnDamage(int damage)
    {
        if (IsDead)
        {
            return;
        }

        Health -= damage;

        if (Health < 0)
        {
            Health = 0;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << Name << " took " << damage << " damage\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        std::cout << Name << " HP: " << Health << "\n";
        std::cout << "    " << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));


        if (Health < 1)
        {
            IsDead = true;

            std::cout << Name << " is dead\n";

            Coins += CoinReward;

            std::cout << "    " << "\n";
            std::cout << "Player received " << CoinReward << " coins\n";
            std::cout << "Coins: " << Coins << "\n";
        }
    }
};


void Player::Attack(Enemy& enemy)
{
    std::cout << Name << " attacks " << enemy.GetName() << "\n";
    enemy.Damage(AttackDamage);
}
    

int main()
{
    auto player = std::make_shared<Player>();
    player->SetName("Player");

    auto enemy = std::make_shared<Enemy>("Enemy", 10);

    std::cout << "Current bank account: " << Coins << "\n";
    std::cout << "Player enters a dark forest" << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Prepare for the fight" << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "    " << "\n";

    player->DamageEvent.SubscribeWeak<Player>(
        player,
        [](Player& p, int damage)
        {
            p.OnDamage(damage);
        });

    enemy->DamageEvent.SubscribeWeak<Enemy>(
        enemy,
        [](Enemy& e, int damage)
        {
            e.OnDamage(damage);
        });

    while (player->GetHealth() > 0)
    {
        enemy->Attack(*player);

        if (player->GetHealth() < 1)
        {
            break;
        }

        player->Attack(*enemy);

        if (enemy->GetIsDead())
        {
            std::cout << "A new enemy appears\n";
            std::cout << "    " << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            enemy->Reset();
        }
    }

    std::cout << "Game over\n";
    std::cout << "Final coins: " << Coins << "\n";

    return 0;
}