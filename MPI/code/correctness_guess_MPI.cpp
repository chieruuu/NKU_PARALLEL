#include "PCFG_MPI.h"
#include <chrono>
#include <fstream>
#include "md5.h"
#include <iomanip>
#include <unordered_set>
#include <mpi.h>
using namespace std;
using namespace chrono;

// 编译指令如下
// mpic++ correctness_guess_MPI.cpp train_MPI.cpp guessing_MPI.cpp md5.cpp -o test_mpi -O2

int mpi_rank, mpi_size;

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

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

    // 加载测试数据
    unordered_set<std::string> test_set;
    ifstream test_data("/guessdata/Rockyou-singleLined-full.txt");
    int test_count = 0;
    string pw;
    while (test_data >> pw)
    {
        test_count += 1;
        test_set.insert(pw);
        if (test_count >= 1000000)
        {
            break;
        }
    }
    int cracked = 0;

    q.init();
    if (mpi_rank == 0) cout << "here" << endl;
    int curr_num = 0;
    auto start = system_clock::now();
    int history = 0;

    // 适配多进程的工作量
    int local_generate_target = 10000000 / mpi_size;
    int local_batch_limit = 1000000 / mpi_size;
    int local_print_step = 100000 / mpi_size;

    while (!q.priority.empty())
    {
        q.PopNext();
        q.total_guesses = q.guesses.size();

        if (q.total_guesses - curr_num >= local_print_step)
        {
            if (mpi_rank == 0)
            {
                cout << "Guesses generated (Global Approx): " << (history + q.total_guesses) * mpi_size << endl;
            }
            curr_num = q.total_guesses;

            if (history + q.total_guesses >= local_generate_target)
            {
                auto end = system_clock::now();
                auto duration = duration_cast<microseconds>(end - start);
                time_guess = double(duration.count()) * microseconds::period::num / microseconds::period::den;

                if (mpi_rank == 0)
                {
                    cout << "Guess time:" << time_guess - time_hash << "seconds" << endl;
                    cout << "Hash time:" << time_hash << "seconds" << endl;
                    cout << "Train time:" << time_train << "seconds" << endl;
                }
                break;
            }
        }

        if (curr_num > local_batch_limit)
        {
            auto start_hash = system_clock::now();

            // 哈希并检查正确性
            int total = q.guesses.size();
            for (int idx = 0; idx < total; idx++)
            {
                if (test_set.find(q.guesses[idx]) != test_set.end())
                {
                    cracked += 1;
                }
                bit32 state[4];
                MD5Hash(q.guesses[idx], state);
            }

            auto end_hash = system_clock::now();
            auto duration = duration_cast<microseconds>(end_hash - start_hash);
            time_hash += double(duration.count()) * microseconds::period::num / microseconds::period::den;

            history += curr_num;
            curr_num = 0;
            q.guesses.clear();
        }
    }

    // 汇总各进程的破解数
    int global_cracked = 0;
    MPI_Reduce(&cracked, &global_cracked, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (mpi_rank == 0)
    {
        cout << "Cracked (Global): " << global_cracked << endl;
    }

    MPI_Finalize();
    return 0;
}
