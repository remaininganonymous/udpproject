#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;

public:
    void push(T item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(item);
        m_cond.notify_one();
    }

    T pop()
    {
        //блокировка
        std::unique_lock<std::mutex> lock(m_mutex);
        //ожидание, пока не в очереди не появятся элементы
        m_cond.wait(lock, [this]() { return !m_queue.empty(); });
        //извлечение элемента
        T item = m_queue.front();
        m_queue.pop();
        //возврат элемента
        return item;
    }
    //проверить, пуста ли очередь
    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    //получить размер очереди
    size_t size() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
};