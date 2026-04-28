/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2023 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

template<typename T> class SafeQueue {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
    int maxSize;

public:
    SafeQueue()
        : maxSize(-1)
    {
    }

    explicit SafeQueue(int maxSize)
        : maxSize(maxSize)
    {
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(mtx);
        while (queue.empty()) {
            cv.wait(lock);
        }
        T item = std::move(queue.front());
        queue.pop();
        cv.notify_all();
        return item;
    }

    void push(const T& item)
    {
        std::unique_lock<std::mutex> lock(mtx);
        while (maxSize != -1 && static_cast<int>(queue.size()) >= maxSize) {
            cv.wait(lock);
        }
        queue.push(item);
        lock.unlock();
        cv.notify_all();
    }

    std::optional<T> try_pop()
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (queue.empty()) {
            return {};
        }
        T item = std::move(queue.front());
        queue.pop();
        cv.notify_all();
        return item;
    }

    bool try_push(const T& item)
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (maxSize != -1 && static_cast<int>(queue.size()) >= maxSize) {
            return false;
        }
        queue.push(item);
        lock.unlock();
        cv.notify_all();
        return true;
    }

    bool isEmpty()
    {
        std::unique_lock<std::mutex> lock(mtx);
        return queue.empty();
    }
};

class ThreadPool {
public:
    ThreadPool(size_t threads, size_t maxQueueSize)
        : stop(false)
        , working(0)
        , tasks_count(0)
        , maxQueueSize(maxQueueSize)
    {
        if (threads == 0) {
            threads = 1;
        }
        if (this->maxQueueSize == 0) {
            this->maxQueueSize = 1;
        }

        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

                        if (this->stop && this->tasks.empty()) {
                            return;
                        }

                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                        ++this->working;
                        this->condition.notify_all();
                    }

                    task();

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        --this->working;
                        --this->tasks_count;
                        this->done_condition.notify_all();
                        this->condition.notify_all();
                    }
                }
            });
        }
    }

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() mutable -> return_type {
                return std::apply(f, std::move(args));
            });

        std::future<return_type> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            condition.wait(lock, [this] { return this->tasks_count < this->maxQueueSize || this->stop; });

            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace([task]() { (*task)(); });
            ++tasks_count;
        }

        condition.notify_one();
        return res;
    }

    bool isIdle()
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return tasks.empty() && (working == 0);
    }

    void waitForAllTasks()
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        done_condition.wait(lock, [this] { return tasks_count == 0; });
    }

    void setWritePoolLimitation(size_t limit)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        maxQueueSize = (limit == 0) ? 1 : limit;
        condition.notify_all();
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable done_condition;

    bool stop;
    size_t working;
    size_t tasks_count;
    size_t maxQueueSize;
};
