#include "PCFG_MPI_pipe.h" 
#include <chrono>
#include <fstream>
#include <sstream>
#include "md5.h"
#include <iomanip>
#include <mpi.h>

using namespace std;
using namespace chrono;

int mpi_rank, mpi_size;
int producer_rank, producer_size;

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if (mpi_rank == 0) {
        //下面代码用于测试MD5哈希的正确性
        cout << "Testing MD5Hash correctness..." << endl;
    }
    string test_pws[8] = {"123456", "password", "12345678", "qwerty", "123456789", "12345", "1234", "111111"};
    string test_hashes[8] = {
        "e10adc3949ba59abbe56e057f20f883e",
        "5f4dcc3b5aa765d61d8327deb882cf99",
        "25d55ad283aa400af464c76d713c07ad",
        "d8578edf8458ce06fbc5bb76a58c5ca4",
        "25f9e794323b453885f5181f1b624d0b",
        "827ccb0eea8a706c4c34a16891f84e7b",
        "81dc9bdb52d04dc20036dbd8313ed055",
        "96e79218965eb72c92a549dd5a330112"
    };
    for (int i = 0; i < 8; i++) {
        bit32 state[4];
        MD5Hash(test_pws[i], state);
        stringstream ss;
        for (int i1 = 0; i1 < 4; i1 += 1) {
            ss << std::setw(8) << std::setfill('0') << hex << state[i1];
        }
        if (ss.str() != test_hashes[i]) {
            if (mpi_rank == 0) {
                cout << "MD5Hash test failed for " << test_pws[i] << "!" << endl;
                cout << "Expected: " << test_hashes[i] << "\nGot:      " << ss.str() << endl;
            }
            MPI_Finalize();
            return 1;
        }
    }
    if (mpi_rank == 0) {
        cout << "MD5Hash test passed!" << endl; //请不要修改这一行
    }

    double time_hash = 0;  // 用于MD5哈希的时间
    double time_guess = 0; // 哈希和猜测的总时长
    double time_train = 0; // 模型训练的总时长
    
    // 我们额外需要两个变量来记录各进程局部的耗时
    double my_guess_time = 0;
    double my_hash_time = 0;

    // ================= 角色划分：平分生产者与消费者 =================
    int num_producers = max(1, mpi_size / 2); // 至少保证有 1 个生成者
    int num_consumers = mpi_size - num_producers;
    
    // color 为 0 代表生产者，color 为 1 代表消费者
    int color = (mpi_rank < num_producers) ? 0 : 1;
    
    MPI_Comm role_comm;
    MPI_Comm_split(MPI_COMM_WORLD, color, mpi_rank, &role_comm);
    
    producer_size = num_producers;
    producer_rank = (color == 0) ? mpi_rank : -1;

    PriorityQueue q;
    
    // ================== 多生产者组 (进行训练与并行生成) ==================
    if (color == 0) {
        auto start_train = system_clock::now();
        q.m.train("/guessdata/Rockyou-singleLined-full.txt");
        q.m.order();
        auto end_train = system_clock::now();
        auto duration_train = duration_cast<microseconds>(end_train - start_train);
        time_train = double(duration_train.count()) * microseconds::period::num / microseconds::period::den;

        q.init();
        if (producer_rank == 0) cout << "here" << endl;
        
        int curr_num = 0;
        auto start = system_clock::now();
        int history = 0;
        
        // 适应多生产者的均分工作量
        int local_generate_target = 10000000 / producer_size;
        int local_batch_limit = 1000000 / producer_size;
        int dest_offset = 0;

        while (!q.priority.empty())
        {
            q.PopNext();
            q.total_guesses = q.guesses.size();
            
            // 为了让进度打印看起来和串行一模一样，只由 Rank 0 近似打印
            if (q.total_guesses - curr_num >= 100000 / producer_size)
            {
                if (producer_rank == 0) {
                    cout << "Guesses generated: " << (history + q.total_guesses) * producer_size << endl;
                }
                curr_num = q.total_guesses;

                // 在此处更改实验生成的猜测上限
                int generate_n = 10000000;
                if (history + q.total_guesses >= local_generate_target)
                {
                    break;
                }
            }
            
            // 当 q.guesses 中口令达到一定数目时，将其打包发送，而不是本地哈希
            if (curr_num > local_batch_limit)
            {
                string packed;
                packed.reserve(q.guesses.size() * 10);
                for (const auto& pw : q.guesses) { 
                    packed.append(pw); 
                    packed.push_back('\0'); 
                }
                
                // 将数据包发送给消费者组的其中一人（轮询机制）
                int dest = num_producers + (dest_offset % num_consumers);
                dest_offset++;
                MPI_Send(packed.c_str(), packed.size(), MPI_CHAR, dest, TAG_DATA, MPI_COMM_WORLD);
                
                history += curr_num;
                curr_num = 0;
                q.guesses.clear();
            }
        }
        
        // 把最后残留的剩余数据也发出去
        if (!q.guesses.empty()) {
            string packed;
            packed.reserve(q.guesses.size() * 10);
            for (const auto& pw : q.guesses) { packed.append(pw); packed.push_back('\0'); }
            int dest = num_producers + (dest_offset % num_consumers);
            if (num_consumers > 0) {
                MPI_Send(packed.c_str(), packed.size(), MPI_CHAR, dest, TAG_DATA, MPI_COMM_WORLD);
            }
        }
        
        // 记录本进程纯粹的生成猜测耗时
        auto end = system_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        my_guess_time = double(duration.count()) * microseconds::period::num / microseconds::period::den;

        // 所有生产者在这里同步，发完货就可以下班了
        MPI_Barrier(role_comm);
        
        // 让主进程广播下班哨，通知所有消费者退出死循环
        if (mpi_rank == 0) {
            for (int i = num_producers; i < mpi_size; i++) {
                MPI_Send(NULL, 0, MPI_CHAR, i, TAG_STOP, MPI_COMM_WORLD);
            }
        }
    } 
    // ================== 多消费者组 (疯狂接收并进行 SIMD 哈希) ==================
    else if (color == 1 && mpi_size > 1) {
        while (true) {
            MPI_Status status;
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            
            if (status.MPI_TAG == TAG_STOP) {
                MPI_Recv(NULL, 0, MPI_CHAR, status.MPI_SOURCE, TAG_STOP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                break;
            }
            
            int count;
            MPI_Get_count(&status, MPI_CHAR, &count);
            char* buf = new char[count];
            MPI_Recv(buf, count, MPI_CHAR, status.MPI_SOURCE, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            vector<string> local_guesses;
            int ptr = 0;
            while(ptr < count) {
                string pw(buf + ptr);
                local_guesses.push_back(pw);
                ptr += pw.length() + 1;
            }
            delete[] buf;
            
            auto start_hash = system_clock::now();
            int total = local_guesses.size();
            int idx = 0;
            for (; idx <= total - 4; idx += 4) {
                string inputs[4] = { local_guesses[idx], local_guesses[idx+1], local_guesses[idx+2], local_guesses[idx+3] };
                bit32 states[4][4];
                MD5Hash_SIMD(inputs, states);
            }
            for (; idx < total; idx++) {
                bit32 state[4]; MD5Hash(local_guesses[idx], state);
            }
            auto end_hash = system_clock::now();
            auto duration = duration_cast<microseconds>(end_hash - start_hash);
            my_hash_time += double(duration.count()) * microseconds::period::num / microseconds::period::den;
        }
    }

    // ================== 全局时间规约与严格输出 ==================
    // 确保所有进程都到达此处
    MPI_Barrier(MPI_COMM_WORLD);

    double global_max_guess = 0;
    double global_max_hash = 0;
    double global_train = 0;

    // 将大家的时间汇总到 Rank 0，取最大值
    MPI_Reduce(&my_guess_time, &global_max_guess, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&my_hash_time, &global_max_hash, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_train, &global_train, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (mpi_rank == 0) {
        time_train = global_train;
        time_hash = global_max_hash;
        
        // 【核心魔法】巧妙赋值，让接下来的输出代码不需要改动一字一句，就能完美输出真正的生成时间
        time_guess = global_max_guess + time_hash;

        // 完全保留原文件的最终输出代码
        cout << "Guess time:" << time_guess - time_hash << "seconds"<< endl;//请不要修改这一行
        cout << "Hash time:" << time_hash << "seconds"<<endl;//请不要修改这一行
        cout << "Train time:" << time_train <<"seconds"<<endl;//请不要修改这一行
    }

    MPI_Comm_free(&role_comm);
    MPI_Finalize();
    return 0;
}