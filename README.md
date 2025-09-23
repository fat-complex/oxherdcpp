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

namespace ox = oxherdcpp;

// Пользовательское сообщение
template <typename T> struct PrintMessageBase : ox::Message<T> {
  std::string text; explicit PrintMessageBase(std::string t) : text(std::move(t)) {}
};
struct PrintMessage final : PrintMessageBase<PrintMessage> { using PrintMessageBase::PrintMessageBase; };

class PrinterActor final : public ox::Actor {
public:
  PrinterActor(const ox::Executor &exec, const std::string &name, ox::ActorId id)
    : Actor(exec, name, id) {
      GetMessageDispatcher().RegisterHandler<PrintMessage>(
        [this](const auto &msg){ std::cout << "[" << GetName() << "] got: " << msg->text << std::endl; }
      );
  }
private:
  void Behaviour(const ox::MPtr<ox::BaseMessage> &message) override {
    GetMessageDispatcher().Dispatch(message);
  }
  void OnStarted() override { std::cout << "[" << GetName() << "] started\n"; }
};

int main() {
  auto system = std::make_shared<ox::ActorSystem>("example-system", 1);
  auto printer = system->CreateActor<PrinterActor>("printer");
  ox::ActorRef ref{printer, system};
  ref.Tell(ox::MakeMessage<ox::GoStartActor>());
  ref.Tell(ox::MakeMessage<PrintMessage>("Hello, actors!"));
  system->Stop();
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
- Лицензия может быть указана в файле LICENSE в корне репозитория (если файл присутствует).
