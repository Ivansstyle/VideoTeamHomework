#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <queue>
#include <mutex>
#include <numeric>
#include <optional>
#include <atomic>

// SharedQueue implementation for inter-thread communication
template<typename T>
class SharedQueue {
    std::queue<T> m_queue;
    mutable std::mutex m_mutex;

public:
    SharedQueue() = default;
    virtual ~SharedQueue() = default;

    void push(const T &_el) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(_el);
    }

    unsigned long size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) { return {}; }
        T temp = m_queue.front();
        m_queue.pop();
        return temp;

    }
};

// Time limit of 10 seconds
constexpr time_t TIME_LIMIT = 10;

// Generator finished flag. Means no more data will be produced for processor
static std::atomic<bool> genFinished {false};
// Processor finished flag. Means no more data will be produced for aggregator
static std::atomic<bool> procFinished {false};

void generator(SharedQueue<std::vector<int>> * procQueue) {
    // Flag if generator finished
    genFinished = false;

    auto start = std::chrono::system_clock::now();
    // random number generator
    std::default_random_engine ranEng(start.time_since_epoch().count());

    // Time random distribution
    std::uniform_int_distribution<int> timeRange(1, TIME_LIMIT);
    auto generatorTimeLimit = std::chrono::seconds(timeRange(ranEng));

    // vector size random distribution
    std::uniform_int_distribution<int> sizeRange(1, 1000);

    // value random distribution
    std::uniform_int_distribution<int> valueRange(-1000, 1000);

    // Restart timer to precisely measure 10 seconds
    start = std::chrono::system_clock::now();

    // For random time from 1 to 10 seconds
    while (std::chrono::system_clock::now() - start < generatorTimeLimit) {
        // Generate random sized vector
        auto vecSize = sizeRange(ranEng);
        std::vector<int> vec(vecSize);

        // Generate random ints and fill the vector
        std::generate_n(vec.begin(), vecSize, [&valueRange, &ranEng]() -> int {return valueRange(ranEng);});

        // Push to processor
        procQueue->push(vec);

        // Sleep between yields
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    std::cout << "Generator time limit reached" << std::endl;
    genFinished = true;
}



void processor(SharedQueue<std::vector<int>> * const procQueue, SharedQueue<float> * const aggrQueue) {
    procFinished = false;

    auto data = procQueue->pop();

    // Wait for data
    while(!data)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(77)); // Sleep a little to minimize collisions
        data = procQueue->pop();
    }

    while (!genFinished || data){
        if (data){

            auto count = static_cast<float>(data->size());
            float avg = static_cast<float>(std::reduce(data->begin(), data->end())) / static_cast<float>(count);

            aggrQueue->push(avg);

            data = procQueue->pop();
        } else { // Data has not been added quickly enough, wait for data again
            std::this_thread::sleep_for(std::chrono::microseconds(77)); // Sleep a little to minimize collisions
            data = procQueue->pop();
        }
    }
    procFinished = true;
}

void aggregator(SharedQueue<float> * const aggrQueue, float * const result) {
    float aggr = {0.0};
    int count = {0};

    auto data = aggrQueue->pop();

    // Wait for data
    while(!data){
        std::this_thread::sleep_for(std::chrono::microseconds(77));
        data = aggrQueue->pop();
    }

    while(!procFinished || data) {
        if (data) {
            aggr += data.value();
            data = aggrQueue->pop();
            ++count;
        } else { // Data has not been added quickly enough, wait for data again
            std::this_thread::sleep_for(std::chrono::microseconds(77));
            data = aggrQueue->pop();
        }
    }
    * result = aggr / static_cast<float>(count);
}


int main() {
    float result {0};

    // Processing queue ( generator -> processor )
    SharedQueue<std::vector<int>> procQueue;

    // Aggregation queue ( processor -> aggregator )
    SharedQueue<float> aggrQueue;

    // Threads
    std::thread genThread(generator, &procQueue);
    std::thread procThread(processor, &procQueue, &aggrQueue);
    std::thread aggrThread(aggregator, &aggrQueue, &result);

    // Waiting for all processes to finish correctly
    genThread.join();
    procThread.join();
    aggrThread.join();

    std::cout<<"Total average: " << result << std::endl;
    return 0;
}
