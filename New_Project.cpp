#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

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

class Player : public std::enable_shared_from_this<Player>
{
    std::string Name;

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

    void Damage(int damage)
    {
        DamageEvent.Notify(damage);
    }

    virtual void OnDamage(int damage)
    {
        std::cout << Name << " took " << damage << " damage\n";
    }
};

int main()
{
    Event<int> OnDamage;

    auto lambdaId = OnDamage.Subscribe(
        [](int damage)
        {
            std::cout << "Lambda damage: " << damage << "\n";
        });

    auto player = std::make_shared<Player>();
    player->SetName("Knight");

    auto weakId = OnDamage.SubscribeWeak<Player>(
        player,
        [](Player& p, int damage)
        {
            p.OnDamage(damage);
        });

    OnDamage.Notify(25);

    std::cout << "---- destroy player ----\n";
    player.reset();

    OnDamage.Notify(50);

    OnDamage.Unsubscribe(lambdaId);

    OnDamage.Notify(100);

    return 0;
}