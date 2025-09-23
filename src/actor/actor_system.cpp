#include <oxherdcpp/actor/actor_system.h>

#include <oxherdcpp/actor/actor_registry.h>
#include <oxherdcpp/actor/events.h>

namespace oxherdcpp
{
ActorSystem::ActorSystem(std::string name, const std::size_t thread_count)
    : name_(std::move(name)), work_guard_{boost::asio::make_work_guard(io_context_)},
      thread_count_{thread_count > 0 ? thread_count : 1}
{
    InitRuntime();
    InitServices();
}

ActorSystem::~ActorSystem()
{
    if (is_running_.load())
    {
        Stop();
    }
}

auto ActorSystem::GetExecutor() -> boost::asio::any_io_executor
{
    return io_context_.get_executor();
}

auto ActorSystem::GetActorRegistry() -> ActorRef
{
    return ActorRef{actor_registry_, this->weak_from_this()};
}

auto ActorSystem::DispatchMessage(const ActorId actor_id, MPtr<BaseMessage> message) -> void
{
    auto find_request = MakeMessage<FindActorWithCallbackMessage>();

    auto cb{[msg = std::move(message)](ActorRef ref) { ref.Tell(msg); }};
    find_request->actor_id = actor_id;
    find_request->callback = std::move(cb);

    actor_registry_->Receive(std::move(find_request));
}

auto ActorSystem::Stop() -> void
{
    if (!is_running_.exchange(false))
    {
        return;
    }
    work_guard_.reset();
    thread_pool_.clear();

    if (!io_context_.stopped())
    {
        io_context_.stop();
    }
}

auto ActorSystem::InitRuntime() -> void
{
    thread_pool_.reserve(thread_count_);
    for (std::size_t i = 0; i < thread_count_; ++i)
    {
        thread_pool_.emplace_back([this] { io_context_.run(); });
    }
    is_running_ = true;
}

auto ActorSystem::InitServices() -> void
{
    actor_registry_ = CreateActor<ActorRegistry>("system/actor-registry");
    actor_registry_->Receive(MakeMessage<GoStartActor>());
}
} // namespace oxherdcpp