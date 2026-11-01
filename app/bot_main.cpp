// #include <chrono>
// #include <condition_variable>
// #include <iostream>
// #include <mutex>
// #include <thread>
//
// std::mutex mutex;
// std::condition_variable cv;
// int a = 0;
//
// namespace {
//     // Explication des cv.
//     class Worker {
//     public:
//         explicit Worker(std::mutex &mutex);
//
//         void work(int& a, int its_me) {
//             while (true) {
//                 std::unique_lock<std::mutex> lock(m_mutex);
//                 cv.wait(lock, [&]() { return (a % 2) == its_me || a >= 10; });
//                 if (a >= 10) {
//                     cv.notify_all();
//                     break;
//                 }
//                 a += 1;
//                 std::cout << std::format("Worker {} (parity {} ) a = {} ", std::this_thread::get_id(), its_me, a) << std::endl;
//
//                 cv.notify_all();
//             }
//         }
//
//     private:
//         std::mutex& m_mutex;
//     };
// }
//
// Worker::Worker(std::mutex &mutex) : m_mutex(mutex) {}
//
// int main() {
//
//     Worker w1(mutex);
//
//     std::thread t1([&] {w1.work(a, 0); });
//     std::thread t2([&] {w1.work(a, 1); });
//
//     t1.join();
//     t2.join();
//
//     std::cout << "Finished!" << std::endl;
//     return EXIT_SUCCESS;
// }

#include <iostream>
#include <ostream>
#include <vector>
#include <variant>

namespace {
    template<class... Ts>
    struct overloaded : Ts... { using Ts::operator()...; };

    struct Died {
        std::size_t agentId;
    };

    struct Message {
        std::size_t agentId;
        std::string message;
    };

    using Event = std::variant<Died, Message>;
}


struct App {

    void handler(const Event& e) {
        std::visit(overloaded{
            [](const Died& d)  { std::cout << std::format("agent {} die \n", d.agentId); },
                [](const Message& m)  { std::cout << std::format("message {} \n", m.agentId); },
        }, e);
    }

};

int main() {
    App app;

    const std::vector<Event> events = {
        Died{.agentId = 10},
        Message{.agentId = 10, .message = "Super test"},
    };

    for (const auto& e : events) {
        app.handler(e);
    }

    return EXIT_SUCCESS;
}










