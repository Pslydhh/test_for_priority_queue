#include <iostream>
#include <sys/sysinfo.h>
#include <sys/time.h>

#include <chrono>
#include <set>
#include <thread>
#include <functional>
#include <limits.h>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "scalable_bounded_priority_queue.h"

typedef std::function<void()> WorkFunction;

struct Task {
public:
    int priority = 0;
    WorkFunction work_function;
    bool operator<(const Task& o) const { return priority < o.priority; }

    Task& operator++() {
        priority += 2;
        return *this;
    }
};

int64_t new_time = 0;
int64_t old_time = 0;

static int n_const = 10000000;
static int nthreads_const = 8;

static constexpr int queue_size = 21;
int result1[queue_size];
void test_scalable_bounded_queue_priority(int count, int num, int num2, int scores) {
    memset(&result1[0], 0, queue_size * sizeof(int));
    std::cout << "scalable bounded priority:" << std::endl;
    ScalableBoundedPriorityQueue<Task> queue;
    std::vector<std::thread> threads;

    {
        auto beginTime = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num; ++i) {
            threads.emplace_back([&queue, count, num, scores]() -> void {
                int bigger_than = 0;
                int smaller_than = 0;
                for (int j = 0; j < (count / num); ++j) {
                    for (int k = 0; k < scores; ++k) {
                        Task task;
                        task.priority = 20 - compute_priority(k);
                        auto beginTime1 = std::chrono::high_resolution_clock::now();
                        if (queue.try_put(task, task.priority)) {

                        } else if (queue.blocking_put(task, task.priority)) {

                        }
                        auto endTime1 = std::chrono::high_resolution_clock::now();
                        auto elapsedTime1 = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                endTime1 - beginTime1);
                    }
                }
            });
        }

        for (int i = 0; i < num2; ++i) {
            threads.emplace_back([&queue, count, num2, scores, &result1]() -> void {
                for (int j = 0; j < (count / num2); ++j) {
                    for (int k = 0; k < scores; ++k) {
                        Task task;
                        Task* out = &task;
                        if (queue.non_blocking_get(out)) {

                        } else if (queue.blocking_get(out)) {
                            
                        }
                        FAA(&result1[out->priority], 1);
                    }
                }
            });
        }

        for (std::thread& th : threads) th.join();
        threads.clear();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime);
        std::cout << "scalable priority time is " << elapsedTime.count() << " milliseconds"
                  << std::endl;
        new_time += elapsedTime.count();
    }
}

int result2[queue_size];
void test_lock_spmc_queue_deque_wait_seperate_write_and_read_priority(int count, int num, int num2,
                                                                      int scores) {
    memset(&result2[0], 0, queue_size * sizeof(int));
    std::cout << "mutex based priority queue:" << std::endl;
    std::vector<Task> queue;
    std::mutex mutex;
    std::condition_variable not_empty;
    std::vector<std::thread> threads;

    {
        auto beginTime = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num; ++i) {
            threads.emplace_back([&queue, count, num, &mutex, &not_empty, scores]() -> void {
                for (int j = 0; j < (count / num); ++j) {
                    for (int k = 0; k < scores; ++k) {
                        Task task;
                        task.priority = compute_priority(k);
                        std::unique_lock<std::mutex> unique_lock(mutex);
                        queue.emplace_back(task);
                        std::push_heap(queue.begin(), queue.end());
                        unique_lock.unlock();
                        not_empty.notify_one();
                    }
                }
            });
        }
       
        for (int i = 0; i < num2; ++i) {
            threads.emplace_back(
                    [&queue, count, num2, &mutex, &not_empty, scores, &result2]() -> void {
                        for (int j = 0; j < (count / num2); ++j) {
                            for (int k = 0; k < scores; ++k) {
                                Task task;
                                Task* out = &task;
                                std::unique_lock<std::mutex> unique_lock(mutex);
                                not_empty.wait(unique_lock, [&queue]() { return !queue.empty(); });
                                //_adjust_priority_if_needed();
                                std::pop_heap(queue.begin(), queue.end());
                                if constexpr (std::is_move_assignable_v<Task>) {
                                    *out = std::move(queue.back());
                                } else {
                                    *out = queue.back();
                                }
                                queue.pop_back();
                                unique_lock.unlock();
                                FAA(&result2[out->priority], 1);
                            }
                        }
                    });
        }

        for (std::thread& th : threads) th.join();
        threads.clear();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - beginTime);
        std::cout << "mutex based priority queue time is " << elapsedTime.count()
                  << " milliseconds" << std::endl;
        old_time += elapsedTime.count();
    }
}

int main(int argc, char* argv[]) {
    int times = atoi(argv[1]);
    n_const = atoi(argv[2]);
    nthreads_const = atoi(argv[3]);
    int nthreads_const2;
    if (argc >= 5) {
        nthreads_const2 = atoi(argv[4]);
    }

    int scores;
    if (argc >= 6) {
        scores = atoi(argv[5]);
    } else {
        std::cout << "argc is less than 6: " << std::endl;
        exit(-1);
    }

    for (int i = 0; i < times; ++i) {
        std::thread thread([i, nthreads_const2, scores]() -> void {
            std::cout << "" << i << " times: " << std::endl;
            test_scalable_bounded_queue_priority(n_const, nthreads_const, nthreads_const2, scores);
            test_lock_spmc_queue_deque_wait_seperate_write_and_read_priority(
                    n_const, nthreads_const, nthreads_const2, scores);
            
            for (int i = 0; i < queue_size; ++i) {
                if (result1[20 - i] != result2[i]) {
                    std::cout << "result1: " << result1[20 - i] << " result2: " << result2[i] << std::endl;
                    std::cout << "ERRROR!\n";
                    exit(-1);
                }
            }
            
           
            std::cout << "\n";
        });
        thread.join();
    }

    std::cout << "new avg: " << (new_time / times) << " old avg: " << (old_time/times) << " X: " << (static_cast<double>(old_time) / new_time) << std::endl;
    return 0;
}
