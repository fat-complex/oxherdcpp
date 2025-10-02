# oxherdcpp — акторная библиотека на C++

oxherdcpp — это фановая компактная библиотека на C++ для построения конкурентных приложений по акторной модели. Основана на Boost.Asio (any_io_executor) и предоставляет базовые примитивы: Actor, ActorSystem, ActorRef и типобезопасные сообщения.

Основные возможности
- ActorSystem — управление жизненным циклом акторов и диспетчеризацией сообщений.
- Actor — базовый класс со стадиями жизненного цикла (Initialize/Start/Stop/Pause/Resume/Terminate) и пользовательским поведением.
- ActorRef — безопасная «ссылка» для общения с актором из внешнего мира/других акторов.
- Сообщения — система типизированных сообщений и диспетчер (MessageDispatcher) для обработки пользовательских типов.
- События жизненного цикла (GoStartActor, GoStopActor и т.д.).
- Совместимость с современным C++ (C++20) и Boost.Asio executors.

Требования
- Компилятор с поддержкой C++20 (GCC/Clang/MSVC).
- CMake 3.20+.
- Boost (Asio и др.) — загружается автоматически через CPM при конфигурации проекта, дополнительной ручной установки обычно не требуется.

Сборка
Ниже показана типичная последовательность команд. Параметры CMake можно менять по необходимости.

Опции CMake
- ACTOR_BUILD_TESTS=ON/OFF — собирать модульные тесты (по умолчанию OFF).
- ACTOR_BUILD_INTEGRATION_TESTS=ON/OFF — интеграционные тесты (по умолчанию OFF).
- ACTOR_BUILD_EXAMPLES=ON/OFF — собирать примеры (по умолчанию OFF).
- ACTOR_BUILD_SHARED=ON/OFF — собирать общую библиотеку (SHARED) вместо статической.
- ACTOR_ENABLE_INSTALL=ON/OFF — включить цели установки и генерации package config.

Пример сборки из командной строки (Linux/macOS)
```
cmake -S . -B build -DACTOR_BUILD_EXAMPLES=ON -DACTOR_BUILD_TESTS=ON
cmake --build build --target oxherdcpp
```

Сборка примеров и запуск минимального примера
Если включить ACTOR_BUILD_EXAMPLES=ON, будет доступна цель minimal-actor.
```
cmake --build build --target minimal-actor && ./build/examples/minimal-actor
```

Запуск тестов
При включённом ACTOR_BUILD_TESTS=ON доступна цель unit-tests.
```
cmake --build build --target unit-tests && ./build/tests/unit-tests
```

Установка (необязательно)
Включите ACTOR_ENABLE_INSTALL=ON, затем:
```
cmake -S . -B build -DACTOR_ENABLE_INSTALL=ON
cmake --build build --target install
```
После установки вы можете подключать библиотеку из другого проекта так:
```
find_package(oxherdcpp REQUIRED)
...
target_link_libraries(your_app PRIVATE oxherdcpp::oxherdcpp)
```

Быстрый старт
Ниже — сокращённая версия примера examples/minimal_actor.cpp, демонстрирующая создание системы акторов, актора и отправку сообщения.
```cpp
#include <oxherdcpp/actor/actor.h>
#include <oxherdcpp/actor/actor_ref.h>
#include <oxherdcpp/actor/actor_system.h>
#include <oxherdcpp/actor/events.h>

using namespace std::chrono_literals;

namespace ox = oxherdcpp;

struct PrintMessage final : ox::Message<PrintMessage>
{
    std::string text;
    explicit PrintMessage(std::string  text): text{std::move(text)} {}
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
```

Структура проекта (важные директории)
- include/oxherdcpp — публичные заголовки (Actor, ActorSystem, ActorRef, сообщения и др.).
- src/ — реализация библиотеки, цель oxherdcpp.
- examples/ — примеры (minimal_actor.cpp).
- tests/ — модульные и другие тесты (цель unit-tests).

Цели CMake (основное)
- oxherdcpp — библиотека.
- minimal-actor — пример (при ACTOR_BUILD_EXAMPLES=ON).
- unit-tests — тестовый исполняемый файл (при ACTOR_BUILD_TESTS=ON).

Поддерживаемые платформы и компиляторы
- Linux/macOS/Windows при наличии компилятора с C++20 и поддержки Boost.Asio executors.

Лицензия
- MIT.
