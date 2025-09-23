#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include <oxherdcpp/actor/message/message.h>

namespace testing
{

namespace ox = oxherdcpp;

struct SimpleTestMessage final : ox::Message<SimpleTestMessage>
{
};

struct AnotherTestMessage final : ox::Message<AnotherTestMessage>
{
};

// Специальный тип сообщения для теста переиспользования памяти
struct ReuseTestMessage final : ox::Message<ReuseTestMessage>
{
};

// Сообщение большого размера для проверки работы пула с крупными блоками
struct LargeMessage final : ox::Message<LargeMessage>
{
    // 4 КБ — типичный размер страницы памяти, хороший кандидат для теста
    std::array<char, 4096> payload{};
};

// Сообщение для отслеживания вызова деструктора через внешний счётчик.
struct DestructorTrackingMessage final : ox::Message<DestructorTrackingMessage>
{
    // Указатель на счётчик, который принадлежит фикстуре.
    // Это безопасно, так как фикстура живёт дольше, чем любое сообщение внутри теста.
    std::atomic<int> *destructor_counter_{nullptr};

    explicit DestructorTrackingMessage(std::atomic<int> &counter) : destructor_counter_{&counter}
    {
    }
    ~DestructorTrackingMessage() override
    {
        if (destructor_counter_)
        {
            ++*destructor_counter_;
        }
    }
};

// Тестовая фикстура для управления общим состоянием.
class MessageActorTests : public Test
{
  protected:
    auto SetUp() -> void override
    {
        // Сбрасываем состояние пулов для всех используемых типов сообщений
        // перед КАЖДЫМ тестом, чтобы обеспечить их независимость.
        ox::ReleaseMessagePoolMemory<SimpleTestMessage>();
        ox::ReleaseMessagePoolMemory<AnotherTestMessage>();
        ox::ReleaseMessagePoolMemory<ReuseTestMessage>();
        ox::ReleaseMessagePoolMemory<DestructorTrackingMessage>();
        ox::ReleaseMessagePoolMemory<LargeMessage>();
    }

    // Счётчик деструкторов теперь является членом фикстуры.
    // Для каждого теста создаётся новый экземпляр фикстуры, поэтому счётчик
    // автоматически сбрасывается в 0 перед каждым тестом.
    std::atomic<int> destructed_count_{0};
};

TEST_F(MessageActorTests, CreateSingleMessageWithMakeMessage)
{
    const auto msg = ox::MakeMessage<SimpleTestMessage>();

    ASSERT_TRUE(static_cast<bool>(msg));

    EXPECT_TRUE(msg->IsA<SimpleTestMessage>());
    EXPECT_FALSE(msg->IsA<DestructorTrackingMessage>());
    EXPECT_EQ(msg->GetTypeId(), ox::Message<SimpleTestMessage>::GetClassTypeId());

    EXPECT_EQ(msg->use_count(), 1);
}

TEST_F(MessageActorTests, VerifyReferenceCounter)
{
    auto msg1 = ox::MakeMessage<SimpleTestMessage>();
    EXPECT_EQ(msg1->use_count(), 1);

    const auto msg2 = msg1;
    EXPECT_EQ(msg1->use_count(), 2);
    EXPECT_EQ(msg2->use_count(), 2);

    msg1.reset();
    EXPECT_EQ(msg2->use_count(), 1);
}

// Проверяем, что деструктор сообщения вызывается только при обнулении счётчика ссылок
TEST_F(MessageActorTests, DestructorInvokedOnZeroRefCount)
{
    // Базовая проверка: пока есть хотя бы одна ссылка, деструктор не вызывается
    {
        auto msg1 = ox::MakeMessage<DestructorTrackingMessage>(destructed_count_);
        EXPECT_EQ(msg1->use_count(), 1);
        EXPECT_EQ(destructed_count_.load(), 0);

        // Добавляем вторую ссылку
        const auto msg2 = msg1;
        (void)msg2;
        EXPECT_EQ(msg1->use_count(), 2);
        EXPECT_EQ(destructed_count_.load(), 0);

        // Сбрасываем одну ссылку — объект ещё жив
        msg1.reset();
        EXPECT_EQ(destructed_count_.load(), 0);

        // Вторая ссылка выйдет из области видимости ниже, и деструктор должен сработать ровно один раз
    }

    // После выхода из блока счётчик ссылок стал 0 и деструктор был вызван
    EXPECT_EQ(destructed_count_.load(), 1);
}

// Проверяем, что память из пула освобождается при обнулении счётчика ссылок
TEST_F(MessageActorTests, PoolDeallocatesOnZeroRefCount)
{
    // Создаём «сторожевое» сообщение этого же типа, чтобы получить доступ к статистике пула
    const auto sentinel = ox::MakeMessage<DestructorTrackingMessage>(destructed_count_);
    auto &[allocations, deallocations, bytes_allocated, bytes_deallocated] = sentinel->GetPoolStats();

    const auto base_allocs = allocations.load();
    const auto base_deallocs = deallocations.load();
    const auto base_bytes_alloc = bytes_allocated.load();
    const auto base_bytes_dealloc = bytes_deallocated.load();

    // Выделяем ещё одно сообщение и проверяем изменение статистики
    {
        auto msg = ox::MakeMessage<DestructorTrackingMessage>(destructed_count_);
        EXPECT_EQ(allocations.load(), base_allocs + 1);
        EXPECT_EQ(bytes_allocated.load(), base_bytes_alloc + sizeof(DestructorTrackingMessage));

        // Дополнительные ссылки не влияют на количество аллокаций/деаллокаций
        const auto another_ref = msg;
        (void)another_ref;
        EXPECT_EQ(deallocations.load(), base_deallocs);
        EXPECT_EQ(bytes_deallocated.load(), base_bytes_dealloc);

        // Сбрасываем одну ссылку — память ещё не освобождена
        msg.reset();
        EXPECT_EQ(deallocations.load(), base_deallocs);
        EXPECT_EQ(bytes_deallocated.load(), base_bytes_dealloc);

        // another_ref будет разрушен при выходе из области видимости ниже
    }

    // После выхода из блока последняя ссылка обнулилась — память должна быть освобождена ровно один раз
    EXPECT_EQ(deallocations.load(), base_deallocs + 1);
    EXPECT_EQ(bytes_deallocated.load(), base_bytes_dealloc + sizeof(DestructorTrackingMessage));

    // sentinel оставляем до конца теста, чтобы его деаллокация не влияла на проверку выше
}

TEST_F(MessageActorTests, GetTypeIdCorrectnessForDifferentMessageTypes)
{
    const auto s = ox::MakeMessage<SimpleTestMessage>();
    const auto a = ox::MakeMessage<AnotherTestMessage>();
    const auto d = ox::MakeMessage<DestructorTrackingMessage>(destructed_count_);

    // Для каждого сообщения GetTypeId совпадает со статическим идентификатором класса и хэш-функцией типа
    EXPECT_EQ(s->GetTypeId(), ox::Message<SimpleTestMessage>::GetClassTypeId());
    EXPECT_EQ(s->GetTypeId(), ox::GetTypeHash<SimpleTestMessage>());

    EXPECT_EQ(a->GetTypeId(), ox::Message<AnotherTestMessage>::GetClassTypeId());
    EXPECT_EQ(a->GetTypeId(), ox::GetTypeHash<AnotherTestMessage>());

    EXPECT_EQ(d->GetTypeId(), ox::Message<DestructorTrackingMessage>::GetClassTypeId());
    EXPECT_EQ(d->GetTypeId(), ox::GetTypeHash<DestructorTrackingMessage>());
}

TEST_F(MessageActorTests, IsA_IsExactTypeMatch)
{
    const auto s = ox::MakeMessage<SimpleTestMessage>();
    const auto a = ox::MakeMessage<AnotherTestMessage>();
    const auto d = ox::MakeMessage<DestructorTrackingMessage>(destructed_count_);

    // Положительные проверки
    EXPECT_TRUE(s->IsA<SimpleTestMessage>());
    EXPECT_TRUE(a->IsA<AnotherTestMessage>());
    EXPECT_TRUE(d->IsA<DestructorTrackingMessage>());

    // Отрицательные проверки: другие типы и базовый тип
    EXPECT_FALSE(s->IsA<AnotherTestMessage>());
    EXPECT_FALSE(s->IsA<DestructorTrackingMessage>());
    EXPECT_FALSE(s->IsA<ox::BaseMessage>());

    EXPECT_FALSE(a->IsA<SimpleTestMessage>());
    EXPECT_FALSE(a->IsA<DestructorTrackingMessage>());
    EXPECT_FALSE(a->IsA<ox::BaseMessage>());

    EXPECT_FALSE(d->IsA<SimpleTestMessage>());
    EXPECT_FALSE(d->IsA<AnotherTestMessage>());
    EXPECT_FALSE(d->IsA<ox::BaseMessage>());
}

TEST_F(MessageActorTests, MessageTypeHashesAreUnique)
{
    const auto id_simple = ox::Message<SimpleTestMessage>::GetClassTypeId();
    const auto id_another = ox::Message<AnotherTestMessage>::GetClassTypeId();
    const auto id_destructor = ox::Message<DestructorTrackingMessage>::GetClassTypeId();

    // Уникальность хэшей типов для разных сообщений
    EXPECT_NE(id_simple, id_another);
    EXPECT_NE(id_simple, id_destructor);
    EXPECT_NE(id_another, id_destructor);

    // Также проверим, что прямой хэш соответствует идентификатору класса
    EXPECT_EQ(id_simple, ox::GetTypeHash<SimpleTestMessage>());
    EXPECT_EQ(id_another, ox::GetTypeHash<AnotherTestMessage>());
    EXPECT_EQ(id_destructor, ox::GetTypeHash<DestructorTrackingMessage>());
}

// Проверяем, что сообщения одного типа используют общий пул (per-type pool)
TEST_F(MessageActorTests, SameTypeMessagesSharePool)
{
    // Снимем базовые значения статистики пула для типа SimpleTestMessage без создания экземпляров
    const auto base_allocs = ox::GetMessagePoolStats<SimpleTestMessage>().allocations.load();
    const auto base_bytes_alloc = ox::GetMessagePoolStats<SimpleTestMessage>().bytes_allocated.load();

    // Создаём два сообщения одного типа
    auto m1 = ox::MakeMessage<SimpleTestMessage>();
    auto m2 = ox::MakeMessage<SimpleTestMessage>();

    auto &s1 = SimpleTestMessage::GetPoolStats();
    auto &s2 = SimpleTestMessage::GetPoolStats();
    EXPECT_EQ(&s1, &s2);

    // Проверим, что аллокации и объём байт увеличились ровно на два объекта этого типа
    EXPECT_EQ(s1.allocations.load(), base_allocs + 2);
    EXPECT_EQ(s1.bytes_allocated.load(), base_bytes_alloc + 2 * sizeof(SimpleTestMessage));

    // После разрушения обоих сообщений количество деаллокаций должно вырасти на 2 и совпасть в обоих наблюдателях
    const auto base_deallocs = s1.deallocations.load();
    const auto base_bytes_dealloc = s1.bytes_deallocated.load();
    m1.reset();
    m2.reset();
    EXPECT_EQ(s1.deallocations.load(), base_deallocs + 2);
    EXPECT_EQ(s1.bytes_deallocated.load(), base_bytes_dealloc + 2 * sizeof(SimpleTestMessage));

    // Для другого типа пул должен быть другим (другая область памяти статистики)
    const auto a = ox::MakeMessage<AnotherTestMessage>();
    auto &sa = a->GetPoolStats();
    EXPECT_NE(&s1, &sa);
}

// Стресс-тест: создаём и удаляем большое количество сообщений, проверяем отсутствие утечек
TEST_F(MessageActorTests, NoLeaks_WhenCreatingAndDeletingManyMessages)
{
    constexpr std::size_t N = 10000; // Достаточно большое число, но быстрое для запуска

    // Берём статистику пула для отслеживаемого типа без дополнительной аллокации
    const auto &[allocations, deallocations, bytes_allocated, bytes_deallocated] =
        ox::GetMessagePoolStats<DestructorTrackingMessage>();

    const auto base_allocs = allocations.load();
    const auto base_deallocs = deallocations.load();
    const auto base_bytes_alloc = bytes_allocated.load();
    const auto base_bytes_dealloc = bytes_deallocated.load();
    const auto base_destructed = destructed_count_.load();

    std::vector<ox::MPtr<DestructorTrackingMessage>> vec;
    vec.reserve(N);

    for (std::size_t i = 0; i < N; ++i)
    {
        vec.emplace_back(ox::MakeMessage<DestructorTrackingMessage>(destructed_count_));
    }

    // Проверяем, что аллокаций стало больше на N и байт тоже на N * sizeof(T)
    EXPECT_EQ(allocations.load(), base_allocs + N);
    EXPECT_EQ(bytes_allocated.load(), base_bytes_alloc + N * sizeof(DestructorTrackingMessage));

    // Пока вектор держит ссылки, деаллокаций быть не должно
    EXPECT_EQ(deallocations.load(), base_deallocs);
    EXPECT_EQ(bytes_deallocated.load(), base_bytes_dealloc);
    EXPECT_EQ(destructed_count_.load(), base_destructed);

    // Освобождаем все ссылки
    vec.clear();
    vec.shrink_to_fit();

    // Теперь должно стать ровно на N деаллокаций и вызовов деструкторов больше
    EXPECT_EQ(deallocations.load(), base_deallocs + N);
    EXPECT_EQ(bytes_deallocated.load(), base_bytes_dealloc + N * sizeof(DestructorTrackingMessage));
    EXPECT_EQ(destructed_count_.load(), base_destructed + static_cast<int>(N));

    // Сильные инварианты: когда нет живых сообщений типа, аллокации и деаллокации равны
    EXPECT_EQ(allocations.load(), deallocations.load());
    EXPECT_EQ(bytes_allocated.load(), bytes_deallocated.load());
}

// Дополнительно проверим, что разные типы не мешают статистике друг друга при больших объёмах
TEST_F(MessageActorTests, NoLeaks_WithManyMessagesAcrossDifferentTypes)
{
    constexpr std::size_t N = 8000;

    // Базовая статистика для обоих типов
    const auto s_base_allocs = ox::GetMessagePoolStats<SimpleTestMessage>().allocations.load();
    const auto s_base_deallocs = ox::GetMessagePoolStats<SimpleTestMessage>().deallocations.load();

    const auto a_base_allocs = ox::GetMessagePoolStats<AnotherTestMessage>().allocations.load();
    const auto a_base_deallocs = ox::GetMessagePoolStats<AnotherTestMessage>().deallocations.load();

    std::vector<ox::MPtr<SimpleTestMessage>> v1;
    std::vector<ox::MPtr<AnotherTestMessage>> v2;
    v1.reserve(N);
    v2.reserve(N);

    for (std::size_t i = 0; i < N; ++i)
    {
        v1.emplace_back(ox::MakeMessage<SimpleTestMessage>());
        v2.emplace_back(ox::MakeMessage<AnotherTestMessage>());
    }

    // Проверяем увеличение аллокаций
    EXPECT_EQ(ox::GetMessagePoolStats<SimpleTestMessage>().allocations.load(), s_base_allocs + N);
    EXPECT_EQ(ox::GetMessagePoolStats<AnotherTestMessage>().allocations.load(), a_base_allocs + N);

    // Освобождаем
    v1.clear();
    v2.clear();

    // И проверяем деаллокации
    EXPECT_EQ(ox::GetMessagePoolStats<SimpleTestMessage>().deallocations.load(), s_base_deallocs + N);
    EXPECT_EQ(ox::GetMessagePoolStats<AnotherTestMessage>().deallocations.load(), a_base_deallocs + N);
}

TEST_F(MessageActorTests, MemoryIsReused_FromPool_WhenRecreatingMessages)
{
    // Проверяем, что освобождённые блоки памяти переиспользуются пулом при последующих аллокациях
    // Для этого сравниваем адреса объектов из двух последовательных партий аллокаций одного и того же типа
    constexpr std::size_t K = 256;

    std::vector<const void *> first_ptrs;
    first_ptrs.reserve(K);

    // Первая партия: аллоцируем и запоминаем адреса, затем освобождаем
    {
        std::vector<ox::MPtr<ReuseTestMessage>> batch1;
        batch1.reserve(K);
        for (std::size_t i = 0; i < K; ++i)
        {
            auto m = ox::MakeMessage<ReuseTestMessage>();
            first_ptrs.push_back(m.get());
            batch1.emplace_back(std::move(m));
        }
        // Выход из области видимости — все сообщения освобождены и возвращены в пул
    }

    // Вторая партия: проверяем, что хотя бы часть адресов совпадает с уже освобождёнными
    const std::unordered_set first_set(first_ptrs.begin(), first_ptrs.end());

    std::size_t reused = 0;
    {
        std::vector<ox::MPtr<ReuseTestMessage>> batch2;
        batch2.reserve(K);
        for (std::size_t i = 0; i < K; ++i)
        {
            auto m = ox::MakeMessage<ReuseTestMessage>();
            if (first_set.contains(m.get()))
            {
                ++reused;
            }
            batch2.emplace_back(std::move(m));
        }
        // Держим вторую партию до конца блока, чтобы адреса были валидны во время проверки
    }

    // std::pmr::synchronized_pool_resource обычно переиспользует освобождённые блоки малого размера.
    // Чтобы тест был стабильным между реализациями, требуем минимум одно совпадение адресов.
    EXPECT_GE(reused, static_cast<std::size_t>(1));
}

// Проверяем, что память из пула переиспользуется при создании новых сообщений
TEST_F(MessageActorTests, PoolReusesMemoryBetweenBatches)
{
    using T = ReuseTestMessage;

    constexpr std::size_t N = 2048;

    // Первая партия аллокаций — собираем адреса
    std::vector<ox::MPtr<T>> batch1;
    batch1.reserve(N);
    std::vector<const void *> addrs1;
    addrs1.reserve(N);

    for (std::size_t i = 0; i < N; ++i)
    {
        auto m = ox::MakeMessage<T>();
        addrs1.push_back(m.get());
        batch1.emplace_back(std::move(m));
    }

    // Освобождаем все сообщения первой партии
    batch1.clear();
    batch1.shrink_to_fit();

    // Вторая партия — собираем адреса и сравниваем с первой партией
    std::vector<ox::MPtr<T>> batch2;
    batch2.reserve(N);
    std::size_t reused_count = 0;

    // Для быстрого поиска и удаления адресов первой партии используем unordered_set
    std::unordered_set addrs_set(addrs1.begin(), addrs1.end());

    ASSERT_EQ(addrs_set.size(), N) << "Адреса в первой партии должны быть уникальными";

    for (std::size_t i = 0; i < N; ++i)
    {
        auto m = ox::MakeMessage<T>();
        // Ищем и удаляем адрес из набора. Если удаление успешно, значит, адрес был переиспользован.
        if (auto addr = static_cast<const void *>(m.get()); addrs_set.erase(addr) > 0)
        {
            ++reused_count;
        }
        batch2.emplace_back(std::move(m));
    }

    // // Ожидаем, что пул переиспользует ВСЕ освобождённые ранее блоки для того же типа и размера
    EXPECT_GT(reused_count, 0) << "Пул должен переиспользовать освобождённые блоки для сообщений одинакового типа";

    // Убираем ссылки, чтобы не мешать другим тестам.
    batch2.clear();
    batch2.shrink_to_fit();
}

// Тесты на корректность копирования и перемещения MPtr (boost::intrusive_ptr)
TEST_F(MessageActorTests, MPtr_CopyConstruction_IncrementsRefCount)
{
    const auto m1 = ox::MakeMessage<SimpleTestMessage>();
    ASSERT_TRUE(m1);
    EXPECT_EQ(m1->use_count(), 1);

    const ox::MPtr m2{m1};
    EXPECT_TRUE(m2);
    EXPECT_EQ(m1.get(), m2.get());
    EXPECT_EQ(m1->use_count(), 2);
    EXPECT_EQ(m2->use_count(), 2);
}

TEST_F(MessageActorTests, MPtr_CopyAssignment_ReleasesOldAndIncrementsNew)
{
    auto src = ox::MakeMessage<SimpleTestMessage>();
    EXPECT_EQ(src->use_count(), 1);

    // Подготовим независимый приёмник, чтобы проверить освобождение прежней ссылки
    const auto other = ox::MakeMessage<SimpleTestMessage>();
    EXPECT_EQ(other->use_count(), 1);

    ox::MPtr dst{other};
    EXPECT_EQ(dst->use_count(), 2); // other и dst

    // Присваиваем из src — счётчик у other должен уменьшиться, у src увеличиться
    dst = src;
    EXPECT_EQ(src->use_count(), 2); // src и dst

    // Сбрасываем исходную ссылку — объект всё ещё жив в dst
    src.reset();
    EXPECT_TRUE(dst);
    EXPECT_EQ(dst->use_count(), 1);
}

TEST_F(MessageActorTests, MPtr_SelfCopyAssignment_NoChange)
{
    auto m = ox::MakeMessage<SimpleTestMessage>();
    EXPECT_EQ(m->use_count(), 1);

    auto *p = m.get();
    m = m; // NOLINT // самоприсваивание не должно менять состояние
    EXPECT_EQ(m.get(), p);
    EXPECT_EQ(m->use_count(), 1);
}

TEST_F(MessageActorTests, MPtr_MoveConstruction_TransfersOwnership)
{
    auto m1 = ox::MakeMessage<SimpleTestMessage>();
    auto *raw = m1.get();
    EXPECT_EQ(m1->use_count(), 1);

    const ox::MPtr m2{std::move(m1)};
    EXPECT_FALSE(m1); // перемещённый указатель должен обнулиться
    ASSERT_TRUE(m2);
    EXPECT_EQ(m2.get(), raw);
    EXPECT_EQ(m2->use_count(), 1);
}

TEST_F(MessageActorTests, MPtr_MoveAssignment_TransfersAndReleasesPrevious)
{
    const auto a = ox::MakeMessage<SimpleTestMessage>();
    auto b = ox::MakeMessage<SimpleTestMessage>();

    auto *pb = b.get();

    ox::MPtr dst{a}; // pa: use_count == 2
    EXPECT_EQ(dst->use_count(), 2);

    dst = std::move(b); // теперь dst указывает на pb, а pa: use_count снова 1
    EXPECT_TRUE(dst);
    EXPECT_EQ(dst.get(), pb);
    EXPECT_EQ(dst->use_count(), 1);
    EXPECT_TRUE(a); // 'a' всё ещё держит 'pa'
    EXPECT_EQ(a->use_count(), 1);
}

TEST_F(MessageActorTests, MPtr_CopyToBase_IncrementsRefCountAndKeepsType)
{
    auto d = ox::MakeMessage<SimpleTestMessage>();
    EXPECT_EQ(d->use_count(), 1);

    const ox::MPtr<ox::BaseMessage> base{d};
    EXPECT_TRUE(base);
    EXPECT_EQ(d->use_count(), 2);

    // Оба указывают на один и тот же объект
    EXPECT_EQ(static_cast<void *>(d.get()), static_cast<void *>(base.get()));

    // Динамическая проверка типа через виртуальный API
    EXPECT_TRUE(base->IsA<SimpleTestMessage>());

    // Сбрасываем производный — базовый по‑прежнему владеет
    d.reset();
    EXPECT_TRUE(base);
    EXPECT_EQ(base->use_count(), 1);
}

TEST_F(MessageActorTests, MPtr_MoveToBase_TransfersOwnershipWithoutIncrement)
{
    auto d = ox::MakeMessage<SimpleTestMessage>();
    auto *raw = d.get();
    EXPECT_EQ(d->use_count(), 1);

    // Перемещаем во входящий базовый MPtr — владение должно перейти без инкремента счётчика
    const ox::MPtr<ox::BaseMessage> base{std::move(d)};
    EXPECT_FALSE(d);
    ASSERT_TRUE(base);
    EXPECT_EQ(static_cast<void *>(base.get()), static_cast<void *>(raw));
    EXPECT_EQ(base->use_count(), 1);
    EXPECT_TRUE(base->IsA<SimpleTestMessage>());
}

TEST_F(MessageActorTests, MPtr_MoveAssignToBase_ReleasesOldAndTransfersNew)
{
    const auto a = ox::MakeMessage<SimpleTestMessage>();
    auto b = ox::MakeMessage<SimpleTestMessage>();

    ox::MPtr<ox::BaseMessage> base{a}; // теперь счётчик для a == 2
    EXPECT_EQ(a->use_count(), 2);

    auto *raw_b = b.get();
    base = std::move(b); // базовый теперь указывает на b, a должен освободить одну ссылку

    EXPECT_EQ(a->use_count(), 1);
    EXPECT_FALSE(b);
    ASSERT_TRUE(base);
    EXPECT_EQ(static_cast<void *>(base.get()), static_cast<void *>(raw_b));
    EXPECT_EQ(base->use_count(), 1);
    EXPECT_TRUE(base->IsA<SimpleTestMessage>());
}

TEST_F(MessageActorTests, ConcurrentCreationFromMultipleThreads)
{
    // Базовые значения статистики для контроля аллокаций/деаллокаций
    auto &statsS = ox::GetMessagePoolStats<SimpleTestMessage>();  // NOLINT
    auto &statsA = ox::GetMessagePoolStats<AnotherTestMessage>(); // NOLINT

    const auto s_base_alloc = statsS.allocations.load();
    const auto s_base_dealloc = statsS.deallocations.load();
    const auto s_base_bytes_alloc = statsS.bytes_allocated.load();
    const auto s_base_bytes_dealloc = statsS.bytes_deallocated.load();

    const auto a_base_alloc = statsA.allocations.load();
    const auto a_base_dealloc = statsA.deallocations.load();
    const auto a_base_bytes_alloc = statsA.bytes_allocated.load();
    const auto a_base_bytes_dealloc = statsA.bytes_deallocated.load();

    const unsigned hw = std::thread::hardware_concurrency();
    const unsigned threads = std::max(4u, std::min(16u, hw ? hw : 4u));
    constexpr std::size_t per_thread = 2000; // число итераций на поток

    std::atomic start{false};
    std::vector<std::thread> workers;
    workers.reserve(threads);

    // Держим некоторое количество сообщений «живыми» за пределами потоков, чтобы проверить корректность подсчёта ссылок
    std::vector<ox::MPtr<SimpleTestMessage>> survivorsS;
    std::vector<ox::MPtr<AnotherTestMessage>> survivorsA;
    std::mutex surv_m;

    for (unsigned t = 0; t < threads; ++t)
    {
        workers.emplace_back([&] {
            // Барьер запуска: ждём общего флага
            while (!start.load(std::memory_order_acquire))
            {
                // активное ожидание — короткая секция, чтобы синхронизировать старт
            }

            std::vector<ox::MPtr<SimpleTestMessage>> localS;
            std::vector<ox::MPtr<AnotherTestMessage>> localA;
            localS.reserve(64);
            localA.reserve(64);

            for (std::size_t i = 0; i < per_thread; ++i)
            {
                if ((i & 1u) == 0u)
                {
                    auto m = ox::MakeMessage<SimpleTestMessage>();
                    // Периодически сохраняем ссылку, чтобы продлить время жизни некоторых объектов
                    if (i % 257u == 0u)
                    {
                        localS.emplace_back(m);
                    }
                }
                else
                {
                    auto m = ox::MakeMessage<AnotherTestMessage>();
                    if (i % 251u == 0u)
                    {
                        localA.emplace_back(m);
                    }
                }
            }

            // Переносим часть «долго живущих» сообщений в общий список, чтобы они пережили завершение потока
            if (!localS.empty() || !localA.empty())
            {
                std::lock_guard lk(surv_m);
                for (auto &p : localS)
                {
                    survivorsS.emplace_back(std::move(p));
                }
                for (auto &p : localA)
                {
                    survivorsA.emplace_back(std::move(p));
                }
            }
        });
    }

    // Одновременно запускаем все потоки
    start.store(true, std::memory_order_release);

    for (auto &th : workers)
    {
        th.join();
    }

    const std::size_t total_simple_created = threads * ((per_thread + 1) / 2); // половина итераций — Simple
    const std::size_t total_another_created = threads * (per_thread / 2);      // вторая половина — Another

    // Сначала проверяем только аллокации: часть объектов ещё жива в survivors*
    EXPECT_EQ(statsS.allocations.load(), s_base_alloc + total_simple_created);
    EXPECT_EQ(statsS.bytes_allocated.load(), s_base_bytes_alloc + total_simple_created * sizeof(SimpleTestMessage));

    EXPECT_EQ(statsA.allocations.load(), a_base_alloc + total_another_created);
    EXPECT_EQ(statsA.bytes_allocated.load(), a_base_bytes_alloc + total_another_created * sizeof(AnotherTestMessage));

    // Освобождаем все оставшиеся ссылки и убеждаемся, что деаллокации «догнали» аллокации
    survivorsS.clear();
    survivorsA.clear();
    survivorsS.shrink_to_fit();
    survivorsA.shrink_to_fit();

    EXPECT_EQ(statsS.deallocations.load(), s_base_dealloc + total_simple_created);
    EXPECT_EQ(statsS.bytes_deallocated.load(), s_base_bytes_dealloc + total_simple_created * sizeof(SimpleTestMessage));

    EXPECT_EQ(statsA.deallocations.load(), a_base_dealloc + total_another_created);
    EXPECT_EQ(statsA.bytes_deallocated.load(),
              a_base_bytes_dealloc + total_another_created * sizeof(AnotherTestMessage));

    // Итоговые инварианты: для обоих типов количество аллокаций равно деаллокациям и байты сходятся
    EXPECT_EQ(statsS.allocations.load(), statsS.deallocations.load());
    EXPECT_EQ(statsS.bytes_allocated.load(), statsS.bytes_deallocated.load());
    EXPECT_EQ(statsA.allocations.load(), statsA.deallocations.load());
    EXPECT_EQ(statsA.bytes_allocated.load(), statsA.bytes_deallocated.load());
}

TEST_F(MessageActorTests, LargeMessageAllocationAndDeallocation)
{
    using T = LargeMessage;
    constexpr std::size_t N = 100; // Для больших объектов не нужно много итераций

    const auto &[allocations, deallocations, bytes_allocated, bytes_deallocated] = ox::GetMessagePoolStats<T>();
    const auto base_allocs = allocations.load();
    const auto base_deallocs = deallocations.load();
    const auto base_bytes_alloc = bytes_allocated.load();
    const auto base_bytes_dealloc = bytes_deallocated.load();

    {
        std::vector<ox::MPtr<T>> messages;
        messages.reserve(N);
        for (std::size_t i = 0; i < N; ++i)
        {
            messages.emplace_back(ox::MakeMessage<T>());
        }

        // Проверяем, что все было аллоцировано
        EXPECT_EQ(allocations.load(), base_allocs + N);
        EXPECT_EQ(bytes_allocated.load(), base_bytes_alloc + N * sizeof(T));

        // В этой точке еще ничего не должно быть освобождено
        EXPECT_EQ(deallocations.load(), base_deallocs);

    } // `messages` уничтожается здесь, все ссылки освобождаются

    // Теперь проверяем, что все было корректно освобождено
    EXPECT_EQ(deallocations.load(), base_deallocs + N);
    EXPECT_EQ(bytes_deallocated.load(), base_bytes_dealloc + N * sizeof(T));

    // Финальный инвариант: баланс должен сойтись
    EXPECT_EQ(allocations.load(), deallocations.load());
    EXPECT_EQ(bytes_allocated.load(), bytes_deallocated.load());
}

TEST_F(MessageActorTests, Cast_Success_FromBaseToExactType)
{
    const auto s = ox::MakeMessage<SimpleTestMessage>();
    const ox::MPtr<ox::BaseMessage> base{s};

    const auto casted = ox::Cast<SimpleTestMessage>(base);
    ASSERT_TRUE(casted);
    EXPECT_EQ(static_cast<void *>(casted.get()), static_cast<void *>(s.get()));

    // Прямое приведение того же типа также успешно и возвращает тот же адрес
    const auto casted_same = ox::Cast<SimpleTestMessage>(s);
    ASSERT_TRUE(casted_same);
    EXPECT_EQ(static_cast<void *>(casted_same.get()), static_cast<void *>(s.get()));
}

TEST_F(MessageActorTests, Cast_Fails_ForDifferentTypes)
{
    const auto a = ox::MakeMessage<AnotherTestMessage>();

    const auto wrong = ox::Cast<SimpleTestMessage>(a);
    EXPECT_FALSE(wrong);
}

TEST_F(MessageActorTests, Cast_ConstCorrectness)
{
    const auto s = ox::MakeMessage<SimpleTestMessage>();

    // Из не константного базового в константный производный — должно работать
    const ox::MPtr<ox::BaseMessage> base_nc{s};
    const auto cast_const_from_nc = ox::Cast<const SimpleTestMessage>(base_nc);
    ASSERT_TRUE(cast_const_from_nc);
    EXPECT_EQ(static_cast<const void *>(cast_const_from_nc.get()), static_cast<const void *>(s.get()));

    // Из константного базового в константный производный — тоже должно работать
    const ox::MPtr<const ox::BaseMessage> base_c{s};
    const auto cast_const_from_c = ox::Cast<const SimpleTestMessage>(base_c);
    ASSERT_TRUE(cast_const_from_c);
    EXPECT_EQ(static_cast<const void *>(cast_const_from_c.get()), static_cast<const void *>(s.get()));
}

TEST_F(MessageActorTests, Cast_NullInput_ReturnsNull)
{
    constexpr ox::MPtr<ox::BaseMessage> empty;
    const auto casted = ox::Cast<SimpleTestMessage>(empty);
    EXPECT_FALSE(casted);
}

} // namespace testing
