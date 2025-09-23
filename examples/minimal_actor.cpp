#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <oxherdcpp/actor/actor.h>
#include <oxherdcpp/actor/actor_ref.h>
#include <oxherdcpp/actor/actor_system.h>
#include <oxherdcpp/actor/events.h>

using namespace std::chrono_literals;

namespace ox = oxherdcpp;

// User-defined message
template <typename T> struct PrintMessageBase : ox::Message<T>
{
    std::string text;
    explicit PrintMessageBase(std::string t) : text(std::move(t))
    {
    }
};

struct PrintMessage final : PrintMessageBase<PrintMessage>
{
    using PrintMessageBase::PrintMessageBase;
};

// Minimal actor that prints received a message
class PrinterActor final : public ox::Actor
{
  public:
    PrinterActor(const ox::Executor &exec, const std::string &name, const ox::ActorId id) : Actor(exec, name, id)
    {
        // Register handler for user message
        GetMessageDispatcher().RegisterHandler<PrintMessage>(
            [this](const auto &msg) { std::cout << "[" << GetName() << "] got: " << msg->text << std::endl; });
    }

  private:
    void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override
    {
        // Delegate to dispatcher with previously registered handlers
        GetMessageDispatcher().Dispatch(message);
    }

    void OnStarted() override
    {
        std::cout << "[" << GetName() << "] started" << std::endl;
    }

    void OnStopped() override
    {
        std::cout << "[" << GetName() << "] stopped" << std::endl;
    }
};

int main()
{
    // ActorSystem must be managed by shared_ptr (used internally)
    const auto system = std::make_shared<ox::ActorSystem>("example-system", 1);

    // Create an actor instance
    const auto printer = system->CreateActor<PrinterActor>("printer");

    // Build ActorRef to talk to our actor via the system
    ox::ActorRef printer_ref{printer, system};

    // Start the actor and send a user message
    printer_ref.Tell(ox::MakeMessage<ox::GoStartActor>());
    printer_ref.Tell(ox::MakeMessage<PrintMessage>("Hello, actors!"));

    // Give it a moment to process
    std::this_thread::sleep_for(200ms);

    // Ask the actor to stop (optional) and stop the system
    printer_ref.Tell(ox::MakeMessage<ox::GoStopActor>());
    std::this_thread::sleep_for(100ms);

    system->Stop();
    return 0;
}
