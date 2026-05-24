#include "PCFG_pthread_adv.h"
#include <chrono>
#include <fstream>
#include "md5.h"
#include <iomanip>
#include <unordered_set>

using namespace std;
using namespace chrono;

int main()
{
    double time_hash = 0;  
    double time_guess = 0; 
    double time_train = 0; 
    PriorityQueue q;
    auto start_train = system_clock::now();
    q.m.train("/guessdata/Rockyou-singleLined-full.txt");
    q.m.order();
    auto end_train = system_clock::now();
    auto duration_train = duration_cast<microseconds>(end_train - start_train);
    time_train = double(duration_train.count()) * microseconds::period::num / microseconds::period::den;

    unordered_set<std::string> test_set;
    ifstream test_data("/guessdata/Rockyou-singleLined-full.txt");
    int test_count=0;
    string pw;
    while(test_data>>pw)
    {   
        test_count+=1;
        test_set.insert(pw);
        if (test_count>=1000000) break;
    }
    int cracked=0;

    q.init();
    int curr_num=0;
    int history =0;
    auto start = system_clock::now();

    while (history + q.GetTotalGuesses() < 10000000)
    {
        q.PopNext();
        if (history + q.GetTotalGuesses() >= curr_num + 100000)
        {
            cout << "Guesses generated: " << history + q.GetTotalGuesses() << endl;
            curr_num = q.GetTotalGuesses();

            if (history + q.GetTotalGuesses() > 10000000)
            {
                auto end = system_clock::now();
                auto duration = duration_cast<microseconds>(end - start);
                time_guess = double(duration.count()) * microseconds::period::num / microseconds::period::den;
                cout << "Guess time:" << time_guess - time_hash << "seconds"<< endl;
                cout << "Hash time:" << time_hash << "seconds"<<endl;
                cout << "Train time:" << time_train <<"seconds"<<endl;
                cout<< "Cracked:"<< cracked<<endl;
                break;
            }
        }
        
        if (curr_num > 1000000)
        {
            auto start_hash = system_clock::now();
            bit32 state[4];
            
            // 进阶优化：分别处理 8 个子队列中的口令
            for(int k = 0; k < NUM_QUEUES; k++) {
                for (string pw : q.multi_guesses[k])
                {
                    if (test_set.find(pw) != test_set.end()) {
                        cracked+=1;
                    }
                    MD5Hash(pw, state);
                }
                q.multi_guesses[k].clear();
                q.sub_total_guesses[k] = 0;
            }

            auto end_hash = system_clock::now();
            auto duration = duration_cast<microseconds>(end_hash - start_hash);
            time_hash += double(duration.count()) * microseconds::period::num / microseconds::period::den;
            
            history += curr_num;
            curr_num = 0;
        }
    }
    return 0;
}