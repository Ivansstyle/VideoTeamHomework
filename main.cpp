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
std::atomic<bool> genFinished;
// Processor finished flag. Means no more data will be produced for aggregator
std::atomic<bool> procFinished;

void generator(SharedQueue<std::vector<int>> * procQueue) {
    auto st = std::chrono::high_resolution_clock::now();
    std::cout << "Hi from generator" << std::endl;
    genFinished = false;

    auto start = std::chrono::system_clock::now();

    // random number generator
    std::default_random_engine ranEng(start.time_since_epoch().count());

    // Time distribution
    std::uniform_int_distribution<int> timeRange(1, TIME_LIMIT);
    auto generatorTimeLimit = std::chrono::seconds(timeRange(ranEng));

    // vector size distribution
    std::uniform_int_distribution<int> sizeRange(1, 1000);

    // value distribution
    std::uniform_int_distribution<int> valueRange(-1000, 1000);

    // Restart timer to precisely measure 10 seconds
    start = std::chrono::system_clock::now();
    std::cout << "Generator will work for " << generatorTimeLimit.count() << "seconds" << std::endl;

    while (std::chrono::system_clock::now() - start < generatorTimeLimit) {
        std::vector<int> vec;
        // Generate random vector size
        auto vecSize = sizeRange(ranEng);
        // Generate random size vectors of random ints for random time
        for (auto i = vecSize; i > 0; --i){
            vec.push_back(valueRange(ranEng));

            // If no time left, stop work
            if (std::chrono::system_clock::now() - start < generatorTimeLimit)
            {
                // Save the last vector
                procQueue->push(vec);
                break;
            }
        }
        procQueue->push(vec);
        std::this_thread::sleep_for(std::chrono::microseconds(200));

    }

    auto stop =std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - st);
    std::cout << "No more time left." << std::endl;
    std::cout << duration.count() << std::endl;
    genFinished = true;
}



void processor(SharedQueue<std::vector<int>> * procQueue, SharedQueue<float> * aggrQueue) {
    auto st = std::chrono::high_resolution_clock::now();
    std::cout << "Hi from processor" << std::endl;
    procFinished = false;
    auto data = procQueue->pop();

    // Wait for data
    while(!data)
    {
        data = procQueue->pop();
    }
    std::cout << "Processor got data" << std::endl;

    while (!genFinished || data){
        if (data){

            // auto vec = procQueue->front();
            auto count = static_cast<float>(data->size());
            float avg = static_cast<float>(std::reduce(data->begin(), data->end())) / static_cast<float>(count);

            aggrQueue->push(avg);
            data = procQueue->pop();
        }
    }
    procFinished = true;
    std::cout << "Processor FINISHED" << std::endl;
    auto stop =std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - st);
    std::cout << duration.count() << std::endl;
}

void aggregator(SharedQueue<float> * aggrQueue, float * result) {
    auto st = std::chrono::high_resolution_clock::now();
    std::cout<<"hi from aggregator" << std::endl;
    float aggr = {0.0};
    int count = {0};
    auto data = aggrQueue->pop();

    // Wait for data
    while(!data){
        data = aggrQueue->pop();
    }
    std::cout<<"Aggregator got data" << std::endl;

    while(!procFinished || data) {
        if (data) {
            // change to data TODO
            aggr += data.value();
            data = aggrQueue->pop();
            ++count;
        }
    }
    std::cout << "aggr = " << aggr << " count " << count << std::endl;
    float average = aggr / count;
    //std::cout << "aggregator FINISHED" << std::endl;
    * result = average;
    auto stop =std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - st);
    std::cout << duration.count() << std::endl;
}



int main() {
    float result = {0};

    // Processing queue ( generator -> processor )
    SharedQueue<std::vector<int>> procQueue;

    // Aggregation queue ( processor -> aggregator )
    SharedQueue<float> aggrQueue;

    std::thread genThread(generator, &procQueue);
    std::thread procThread(processor, &procQueue, &aggrQueue);
    std::thread aggrThread(aggregator, &aggrQueue, &result);

    // Waiting for all processes to finish correctly
    genThread.join();
    procThread.join();
    aggrThread.join();

    std::cout<<"Program finished, result: " << result << std::endl;
    return 0;
}
