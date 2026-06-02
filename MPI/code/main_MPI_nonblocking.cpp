#include "PCFG_MPI.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include "md5.h"
#include <iomanip>
#include <mpi.h>

using namespace std;
using namespace chrono;

// 定义全局变量，供其他文件使用
int mpi_rank, mpi_size;

int main(int argc, char *argv[])
{
    // 初始化 MPI 环境
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if (mpi_rank == 0) {
        cout << "Testing MD5Hash correctness..." << endl;
    }
    string test_pws[8] = {"123456", "password", "12345678", "qwerty", "123456789", "12345", "1234", "111111"};
    string test_hashes[8] = {
        "e10adc3949ba59abbe56e057f20f883e", "5f4dcc3b5aa765d61d8327deb882cf99", "25d55ad283aa400af464c76d713c07ad", "d8578edf8458ce06fbc5bb76a58c5ca4",
        "25f9e794323b453885f5181f1b624d0b", "827ccb0eea8a706c4c34a16891f84e7b", "81dc9bdb52d04dc20036dbd8313ed055", "96e79218965eb72c92a549dd5a330112"
    };
    for (int i = 0; i < 8; i++) {
        bit32 state[4];
        MD5Hash(test_pws[i], state);
        stringstream ss;
        for (int i1 = 0; i1 < 4; i1 += 1) { ss << std::setw(8) << std::setfill('0') << hex << state[i1]; }
        if (ss.str() != test_hashes[i]) {
            if (mpi_rank == 0) cout << "MD5Hash test failed for " << test_pws[i] << "!" << endl;
            MPI_Finalize();
            return 1;
        }
    }
    if (mpi_rank == 0) cout << "MD5Hash test passed!" << endl;

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

    q.init();
    if (mpi_rank == 0) cout << "here" << endl;
    int curr_num = 0;
    auto start = system_clock::now();
    int history = 0;

    // 适配多进程的工作量。整体要跑 1000 万，那么分摊到每个进程的工作量为 1000 万 / size。
    int local_generate_target = 10000000 / mpi_size;
    int local_batch_limit = 1000000 / mpi_size;
    int local_print_step = 100000 / mpi_size;

    // 非阻塞通信相关变量
    MPI_Request send_request = MPI_REQUEST_NULL;
    MPI_Request recv_request = MPI_REQUEST_NULL;
    bool send_pending = false;
    bool recv_pending = false;

    while (!q.priority.empty())
    {
        q.PopNext();
        q.total_guesses = q.guesses.size();

        if (q.total_guesses - curr_num >= local_print_step)
        {
            if (mpi_rank == 0) {
                // Rank 0 打印近似的全局总进度
                cout << "Guesses generated (Global Approx): " << (history + q.total_guesses) * mpi_size << endl;
            }
            curr_num = q.total_guesses;

            if (history + q.total_guesses >= local_generate_target)
            {
                auto end = system_clock::now();
                auto duration = duration_cast<microseconds>(end - start);
                time_guess = double(duration.count()) * microseconds::period::num / microseconds::period::den;

                // 等待所有未完成的非阻塞操作
                if (send_pending) {
                    MPI_Wait(&send_request, MPI_STATUS_IGNORE);
                    send_pending = false;
                }
                if (recv_pending) {
                    MPI_Wait(&recv_request, MPI_STATUS_IGNORE);
                    recv_pending = false;
                }

                // 只有 Rank 0 进行最终时间汇总输出
                if (mpi_rank == 0) {
                    cout << "Guess time:" << time_guess - time_hash << "seconds" << endl;
                    cout << "Hash time:" << time_hash << "seconds" << endl;
                    cout << "Train time:" << time_train << "seconds" << endl;
                }
                break;
            }
        }

        if (curr_num > local_batch_limit)
        {
            // 等待之前的非阻塞发送完成
            if (send_pending) {
                MPI_Wait(&send_request, MPI_STATUS_IGNORE);
                send_pending = false;
            }

            auto start_hash = system_clock::now();

            // SIMD 哈希保持原样
            int total = q.guesses.size();
            int idx = 0;
            for (; idx <= total - 4; idx += 4)
            {
                string inputs[4] = { q.guesses[idx], q.guesses[idx+1], q.guesses[idx+2], q.guesses[idx+3] };
                bit32 states[4][4];
                MD5Hash_SIMD(inputs, states);
            }
            for (; idx < total; idx++)
            {
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

    // 释放 MPI 资源
    MPI_Finalize();
    return 0;
}
