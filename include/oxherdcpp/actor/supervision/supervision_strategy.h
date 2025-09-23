#pragma once

#include <unordered_map>

#include <oxherdcpp/actor/message/message.h>
#include <oxherdcpp/common/memory.h>

namespace oxherdcpp
{
struct ActorFailureEvent;

enum class Directive
{
    Resume,
    Restart,
    Stop,
    Escalate
};

class SupervisionStrategy
{
  public:
    virtual ~SupervisionStrategy() = default;
    virtual auto Decide(const MPtr<ActorFailureEvent> &failure) -> Directive = 0;
};

class ExceptionHandler
{
  public:
    virtual ~ExceptionHandler() = default;

    virtual auto GetTypeID() -> std::size_t = 0;

    [[nodiscard]] virtual auto Handle(const std::exception_ptr &exception_ptr) const -> Directive = 0;
};

template <typename ExceptionType> class TypedExceptionHandler final : public ExceptionHandler
{
    constexpr static auto GetClassTypeID() -> std::size_t
    {
        return GetTypeHash<ExceptionType>();
    }

  public:
    ~TypedExceptionHandler() override = default;

    TypedExceptionHandler(const Directive success_directive, const Directive default_directive)
        : success_directive_{success_directive}, default_directive_{default_directive}
    {
    }

    auto GetTypeID() -> std::size_t override
    {
        return GetClassTypeID();
    }

    [[nodiscard]] auto Handle(const std::exception_ptr &exception_ptr) const -> Directive override
    {
        try
        {
            std::rethrow_exception(exception_ptr);
        }
        catch (const ExceptionType &)
        {
            return success_directive_;
        }
        catch (...)
        {
            return default_directive_;
        }
    }

  private:
    Directive success_directive_;
    Directive default_directive_;
};

class OneForOneStrategy final : public SupervisionStrategy
{
  public:
    ~OneForOneStrategy() override = default;

    auto Decide(const MPtr<ActorFailureEvent> &failure) -> Directive override;

    template <typename ExceptionType>
    auto HandleException(Directive success_directive, Directive default_directive = Directive::Escalate)
        -> OneForOneStrategy &
    {
        auto handler{MakeUptr<TypedExceptionHandler<ExceptionType>>(success_directive, default_directive)};
        exception_handlers_[handler->GetTypeID()] = std::move(handler);
        return *this;
    }

    auto SetDefaultDirective(Directive default_directive) -> OneForOneStrategy &;

  private:
    std::unordered_map<std::size_t, Uptr<ExceptionHandler>> exception_handlers_;
    Directive default_directive_{Directive::Escalate};
};

} // namespace oxherdcpp