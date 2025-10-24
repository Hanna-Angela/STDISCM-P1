#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>

using namespace std;

mutex print_mutex;

// ---------- Helper: Timestamp ----------
string timestamp() {
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    tm tm_info;
    localtime_s(&tm_info, &t);
    ostringstream oss;
    oss << put_time(&tm_info, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ---------- Helper: Prime check ----------
bool isPrime(int n) {
    if (n <= 1) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    int limit = static_cast<int>(sqrt(n));
    for (int i = 3; i <= limit; i += 2)
        if (n % i == 0) return false;
    return true;
}

// ---------- Helper: Divisibility Check (for B2) ----------
bool isPrime_parallel(int n, int num_threads) {
    int max_threads = thread::hardware_concurrency();
    if (max_threads == 0) max_threads = 8;

    // Limit per-number threads
    int active_threads = min(num_threads, max_threads);

    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;

    bool is_prime = true;
    int maxDiv = static_cast<int>(sqrt(n));
    vector<thread> threads;
    mutex mtx;

    auto worker = [&](int start, int end) {
        for (int i = start; i <= end && is_prime; ++i) {
            if (n % i == 0) {
                lock_guard<mutex> lock(mtx);
                is_prime = false;
                break;
            }
        }
        };

    int range = maxDiv - 3 + 1;
    if (range < 1) return true;

    int chunk = range / num_threads;
    int start = 3;

    for (int i = 0; i < num_threads; ++i) {
        int end = (i == num_threads - 1) ? maxDiv : start + chunk - 1;
        threads.emplace_back(worker, start, end);
        start = end + 1;
    }

    for (auto& t : threads) t.join();
    return is_prime;
}

// ---------- Variant 1 A1 B1 ----------
void variant1_worker(int id, int start, int end) {
    for (int i = start; i <= end; ++i) {
        if (isPrime(i)) {
            lock_guard<mutex> lock(print_mutex);
            cout << "[" << timestamp() << "] Thread " << id
                << " found prime: " << i << endl;
        }
    }
}

// ---------- Variant 2 A2 B1 ----------
void variant2_worker(int id, int start, int end, vector<int>& results) {
    for (int i = start; i <= end; ++i)
        if (isPrime(i))
            results.push_back(i);
}

// ---------- Variant 3 A1 B2 ----------
void variant3_worker(int id, int start, int end, int num_threads) {
    for (int n = start; n <= end; ++n) {
        // Each thread checks different divisors for the same number
        bool prime = true;
        int limit = static_cast<int>(sqrt(n));
        vector<thread> div_threads;
        mutex mtx;

        auto check_divisor = [&](int start_div) {
            for (int d = start_div; d <= limit; d += num_threads) {
                if (n % d == 0) {
                    lock_guard<mutex> lock(mtx);
                    prime = false;
                    break;
                }
            }
            };

        int active_threads = min(num_threads, (int)thread::hardware_concurrency());

        for (int t = 0; t < active_threads; ++t)
            div_threads.emplace_back(check_divisor, 2 + t);

        for (auto& t : div_threads)
            t.join();

        if (prime && n > 1) {
            lock_guard<mutex> lock(print_mutex);
            cout << "[" << timestamp() << "] Main thread found prime: " << n << endl;
        }
    }
}


// ---------- Variant 4 A2 B2 ----------
void variant4_worker(int id, int limit, int num_threads, vector<int>& local_primes) {
    for (int n = 2 + (id - 1); n <= limit; n += num_threads) {
        bool prime = isPrime_parallel(n, num_threads);
        if (prime) local_primes.push_back(n);
    }
}

// ---------- CONFIG + MAIN ----------
int main() {
    int num_threads = 4, limit = 50, variant = 1;

    ifstream config("config.txt");
    if (config.is_open()) {
        string line;
        while (getline(config, line)) {
            if (line.find("threads=") == 0)
                num_threads = stoi(line.substr(8));
            else if (line.find("limit=") == 0)
                limit = stoi(line.substr(6));
            else if (line.find("mode=") == 0)
                variant = stoi(line.substr(5));
        }
        config.close();
    }
    else {
        cout << "Config file not found. Using defaults.\n";
    }

    cout << "[START] Run started at: " << timestamp() << endl;
    auto start_time = chrono::steady_clock::now();

    vector<thread> threads;

    if (variant == 1) {
        cout << "[A1 B1]\n";
        int range = limit / num_threads;
        for (int i = 0; i < num_threads; ++i) {
            int s = 2 + i * range;
            int e = (i == num_threads - 1) ? limit : (2 + (i + 1) * range - 1);
            threads.emplace_back(variant1_worker, i + 1, s, e);
        }
    }

    else if (variant == 2) {
        cout << "[A2 B1]\n";
        int range = limit / num_threads;
        vector<vector<int>> results(num_threads);

        for (int i = 0; i < num_threads; ++i) {
            int s = i * range + 1;
            int e = (i == num_threads - 1) ? limit : (i + 1) * range;
            threads.emplace_back(variant2_worker, i + 1, s, e, ref(results[i]));
        }

        for (auto& th : threads) th.join();
        threads.clear();

        cout << "\nAll threads completed.\n\nCollected results:\n";
        size_t total = 0;
        for (int i = 0; i < num_threads; ++i) {
            cout << "Thread " << i + 1 << " -> [";
            for (size_t j = 0; j < results[i].size(); ++j) {
                cout << results[i][j];
                if (j != results[i].size() - 1) cout << ", ";
            }
            cout << "]\n";
            total += results[i].size();
        }
        cout << "Total primes found: " << total << endl;
    }

    else if (variant == 3) {
        cout << "[A1 B2]\n";
        int current = 2;
        mutex mtx;
        bool done = false;

        auto worker = [&](int id) {
            while (true) {
                int n;
                {
                    lock_guard<mutex> lock(mtx);
                    if (current > limit) break;
                    n = current++;
                }

                bool prime = true;
                int maxDiv = static_cast<int>(sqrt(n));
                for (int d = 2; d <= maxDiv; ++d) {
                    if (n % d == 0) {
                        prime = false;
                        break;
                    }
                }

                if (prime) {
                    lock_guard<mutex> lock(print_mutex);
                    cout << "[" << timestamp() << "] Main thread found prime: " << n << endl;
                }
            }
            };

        vector<thread> threads;
        for (int i = 0; i < num_threads; ++i)
            threads.emplace_back(worker, i + 1);

        for (auto& t : threads)
            t.join();
    }


    else if (variant == 4) {
        cout << "[A2 B2]\n";

        vector<int> all_primes;
        mutex mtx;          // protects shared variables
        int current = 2;

        auto worker = [&]() {
            while (true) {
                int n;
                {
                    lock_guard<mutex> lock(mtx);
                    if (current > limit) break;
                    n = current++;
                }

                bool prime = true;
                int maxDiv = static_cast<int>(sqrt(n));
                for (int d = 2; d <= maxDiv; ++d) {
                    if (n % d == 0) {
                        prime = false;
                        break;
                    }
                }

                if (prime) {
                    lock_guard<mutex> lock(mtx);
                    all_primes.push_back(n);
                }
            }
            };

        vector<thread> threads;
        for (int i = 0; i < num_threads; ++i)
            threads.emplace_back(worker);

        for (auto& t : threads)
            t.join();

        sort(all_primes.begin(), all_primes.end());

        cout << "All threads completed.\nPrimes found: [";
        for (size_t i = 0; i < all_primes.size(); ++i) {
            cout << all_primes[i];
            if (i != all_primes.size() - 1) cout << ", ";
        }
        cout << "]\nTotal primes: " << all_primes.size() << endl;
    }


    else {
        cout << "Invalid mode number in config file." << endl;
    }

    for (auto& th : threads)
        if (th.joinable()) th.join();

    auto end_time = chrono::steady_clock::now();
    cout << "[END] Run finished at: " << timestamp() << endl;
    cout << "Elapsed: "
        << chrono::duration_cast<chrono::seconds>(end_time - start_time).count()
        << " seconds" << endl;

    return 0;
}
