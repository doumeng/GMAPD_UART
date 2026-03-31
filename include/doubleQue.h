#ifndef _DOUBLE_QUE_H_
#define _DOUBLE_QUE_H_

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

static constexpr int DQUE_GET_TIMEOUT = -2;

template <typename T>
class ThreadSafeDoubleQueue {
public:
    ThreadSafeDoubleQueue() {
        exit_flag = false;
        lost_count = 0;
    }

    enum class DQ_RESULT{
        TIMEOUT = DQUE_GET_TIMEOUT,
        ERROR,
        SUCCESS,
        DQ_EXIT,
    };

    // 从空闲队列强制获取一帧元素, 不论数据队列获取之后是否归还，我们强制给他归还回来
    DQ_RESULT get_from_free_force(T &data){
        std::lock_guard<std::mutex> lock(mutex);

        if(exit_flag)
            return DQ_RESULT::DQ_EXIT;

        ///无空闲节点可用
        if (free_queue.empty()) {
            ///从数据队列获取一帧, 如果获取成功, 则直接释放掉数据队列中的一个元素，并将此元素给到用户使用
            if(!data_queue.empty()){

                data = data_queue.front(); ///将获取的元素给客户用, 并且数据队列减一元素, 空闲队列无需任何操作

                data_queue.pop();

                lost_count++;

                return DQ_RESULT::SUCCESS;
            }else{
                ///如果获取失败, 则证明元素全被用户占用且未归还。
                return DQ_RESULT::ERROR;
            }
        }

        data = free_queue.front();

        free_queue.pop();

        return DQ_RESULT::SUCCESS;
    };

    size_t get_lost_data()
    {
        return lost_count;
    }

////////////修改为阻塞版本, 减少CPU轮询时间, 减少CPU资源占用, 提高效率
    ///条件变量阻塞等待元素
    DQ_RESULT get_from_data(T &data, int timeout_ms = 20) {
        std::unique_lock<std::mutex> lock(mutex);

        if(exit_flag){
            return DQ_RESULT::DQ_EXIT;
        }

        if( !data_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return !data_queue.empty() || exit_flag; }) ){
            return DQ_RESULT::TIMEOUT;
        }

        if(exit_flag){
            return DQ_RESULT::DQ_EXIT;
        }

        data = data_queue.front();

        data_queue.pop();

        return DQ_RESULT::SUCCESS;
    }

    DQ_RESULT get_from_free(T &data, int timeout_ms = 20) {
        std::unique_lock<std::mutex> lock(mutex);

        if(exit_flag){
            return DQ_RESULT::DQ_EXIT;
        }

        if(!free_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return !free_queue.empty() || exit_flag; }) ){
            return DQ_RESULT::TIMEOUT;
        }

        if(exit_flag){
            return DQ_RESULT::DQ_EXIT;
        }

        data = free_queue.front();

        free_queue.pop();

        return DQ_RESULT::SUCCESS;
    }

    void push_to_free(const T &data) {
        std::lock_guard<std::mutex> lock(mutex);
        if (exit_flag)
            return;
        free_queue.push(data);
        free_cv.notify_one();
    }

    void push_to_data(const T &element) {
        std::lock_guard<std::mutex> lock(mutex);
        if (exit_flag)
            return;
        data_queue.push(element);
        data_cv.notify_one();
    }

    // 获取当前队列大小（线程安全）
    size_t data_size(){
        std::lock_guard<std::mutex> lock(mutex);
        return data_queue.size();
    };

    size_t free_size(){
        std::lock_guard<std::mutex> lock(mutex);
        return free_queue.size();
    };

    void close_dque()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            exit_flag = true;
        }
        data_cv.notify_all();
        free_cv.notify_all();
    }

private:
    // size_t _depth;                      //队深 无关紧要，用户自行决定队深，申请及释放
    size_t lost_count;                   //丢帧数量                   
    std::atomic<bool> exit_flag{false};         //结束标志,这个。。。
    std::queue<T> free_queue;               // 空闲元素队列
    std::queue<T> data_queue;               // 数据元素队列
    mutable std::mutex mutex;               // 互斥锁
    std::condition_variable free_cv;        // 空闲资源可用信号
    std::condition_variable data_cv;        // 数据可用信号
};

#endif