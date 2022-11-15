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

// Change to 1 for std::reduce to use parallel exec policy, to 0 to use sequential
#define PARRALEL 0
#if PARRALEL
    #include <execution>
    #define EXEC_POLICY std::execution::par,
#else
    #define EXEC_POLICY
#endif
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
    generatorTimeLimit = std::chrono::seconds(10);
    // vector size distribution
    std::uniform_int_distribution<int> sizeRange(1, 1000);

    // value distribution
    std::uniform_int_distribution<int> valueRange(-1000, 1000);

    // Restart timer to precisely measure 10 seconds
    start = std::chrono::system_clock::now();
    std::cout << "Generator will work for " << generatorTimeLimit.count() << "seconds" << std::endl;

    // debug
    int count {0};

    // For random time from 1 to 10 seconds
    while (std::chrono::system_clock::now() - start < generatorTimeLimit) {
        // Generate random sized vector
        //auto vecSize = sizeRange(ranEng);
        int vecSize = 10;
        std::vector<int> vec(vecSize);

        // Generate random ints
        std::generate_n(vec.begin(), vecSize, [&valueRange, &ranEng]() -> int {return valueRange(ranEng);});

        procQueue->push(vec);
        ++count;

        std::this_thread::sleep_for(std::chrono::microseconds(4000));
    }

    auto stop =std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - st);
    std::cout << "No more time left. Generated "<< count << " vectors." << std::endl;
    std::cout << "Generating took: " << duration.count() << std::endl;
    genFinished = true;
}



void processor(SharedQueue<std::vector<int>> * const procQueue, SharedQueue<float> * const aggrQueue) {
    auto st = std::chrono::high_resolution_clock::now();
    std::cout << "Hi from processor" << std::endl;
    procFinished = false;
    auto data = procQueue->pop();

    // Wait for data
    while(!data)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
        data = procQueue->pop();
    }
    std::cout << "Processor got data" << std::endl;

    while (!genFinished || data){
        if (data){

            // auto vec = procQueue->front();
            auto count = static_cast<float>(data->size());
            float avg = static_cast<float>(std::reduce(EXEC_POLICY data->begin(), data->end())) / static_cast<float>(count);

            aggrQueue->push(avg);
            std::this_thread::sleep_for(std::chrono::microseconds(10000));
            data = procQueue->pop();
        }
        else{ // Data has not been added quickly enough, wait for data again
            std::this_thread::sleep_for(std::chrono::microseconds(77));
            data = procQueue->pop();
        }
    }
    procFinished = true;
    std::cout << "Processor FINISHED" << std::endl;
    auto stop =std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - st);
    std::cout << duration.count() << std::endl;
}

void aggregator(SharedQueue<float> * const aggrQueue, float * const result) {
    auto st = std::chrono::high_resolution_clock::now();
    std::cout<<"hi from aggregator" << std::endl;
    float aggr = {0.0};
    int count = {0};
    std::this_thread::sleep_for(std::chrono::microseconds(100000));
    auto data = aggrQueue->pop();

    // Wait for data
    while(!data){
        std::this_thread::sleep_for(std::chrono::microseconds(100000));
        data = aggrQueue->pop();
    }
    std::cout<<"Aggregator got data" << std::endl;

    while(!procFinished || data) {
        if (data) {
            aggr += data.value();
            data = aggrQueue->pop();
            ++count;
        } else { // Data has not been added quickly enough, wait for data again
            std::this_thread::sleep_for(std::chrono::microseconds(10000));
            data = aggrQueue->pop();
        }
    }
    std::cout << "aggr = " << aggr << " count " << count << std::endl;
    float average = aggr / static_cast<float>(count);
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

    std::cout<<"Program finished, total average: " << result << std::endl;
    return 0;
}
