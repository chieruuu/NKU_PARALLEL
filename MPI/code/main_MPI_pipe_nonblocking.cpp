#include "PCFG_MPI_pipe_nonblocking.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include "md5.h"
#include <iomanip>
#include <mpi.h>
#include <cstring>

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
        cout << "MD5Hash test passed!" << endl;
    }

    double time_hash = 0;
    double time_guess = 0;
    double time_train = 0;

    double my_guess_time = 0;
    double my_hash_time = 0;

    // ================= 角色划分：平分生产者与消费者 =================
    int num_producers = max(1, mpi_size / 2);
    int num_consumers = mpi_size - num_producers;

    int color = (mpi_rank < num_producers) ? 0 : 1;

    MPI_Comm role_comm;
    MPI_Comm_split(MPI_COMM_WORLD, color, mpi_rank, &role_comm);

    producer_size = num_producers;
    producer_rank = (color == 0) ? mpi_rank : -1;

    PriorityQueue q;

    // ================== 多生产者组 (非阻塞发送) ==================
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

        int local_generate_target = 10000000 / producer_size;
        int local_batch_limit = 1000000 / producer_size;
        int dest_offset = 0;

        // ---- 非阻塞通信相关变量 ----
        MPI_Request pending_send = MPI_REQUEST_NULL;
        string* pending_packed = nullptr;  // 堆分配保持缓冲区存活

        while (!q.priority.empty())
        {
            // 等待上一轮非阻塞发送完成
            if (pending_send != MPI_REQUEST_NULL) {
                MPI_Wait(&pending_send, MPI_STATUS_IGNORE);
                delete pending_packed;
                pending_packed = nullptr;
                pending_send = MPI_REQUEST_NULL;
            }

            q.PopNext();
            q.total_guesses = q.guesses.size();

            if (q.total_guesses - curr_num >= 100000 / producer_size)
            {
                if (producer_rank == 0) {
                    cout << "Guesses generated: " << (history + q.total_guesses) * producer_size << endl;
                }
                curr_num = q.total_guesses;

                int generate_n = 10000000;
                if (history + q.total_guesses >= local_generate_target)
                {
                    break;
                }
            }

            if (curr_num > local_batch_limit)
            {
                string packed;
                packed.reserve(q.guesses.size() * 10);
                for (const auto& pw : q.guesses) {
                    packed.append(pw);
                    packed.push_back('\0');
                }

                int dest = num_producers + (dest_offset % num_consumers);
                dest_offset++;

                // ---- 非阻塞发送：缓冲区需保持存活至 Wait 完成 ----
                pending_packed = new string(std::move(packed));
                MPI_Isend(pending_packed->c_str(), pending_packed->size(),
                          MPI_CHAR, dest, TAG_DATA, MPI_COMM_WORLD, &pending_send);
                // 注意：此时不等待，直接继续生成下一批口令
                // 发送在后台进行，与口令生成重叠

                history += curr_num;
                curr_num = 0;
                q.guesses.clear();
            }
        }

        // 等最后一轮非阻塞发送完成
        if (pending_send != MPI_REQUEST_NULL) {
            MPI_Wait(&pending_send, MPI_STATUS_IGNORE);
            delete pending_packed;
            pending_packed = nullptr;
            pending_send = MPI_REQUEST_NULL;
        }

        // 发送残留数据（非阻塞）
        if (!q.guesses.empty()) {
            string packed;
            packed.reserve(q.guesses.size() * 10);
            for (const auto& pw : q.guesses) { packed.append(pw); packed.push_back('\0'); }
            int dest = num_producers + (dest_offset % num_consumers);
            if (num_consumers > 0) {
                pending_packed = new string(std::move(packed));
                MPI_Isend(pending_packed->c_str(), pending_packed->size(),
                          MPI_CHAR, dest, TAG_DATA, MPI_COMM_WORLD, &pending_send);
                MPI_Wait(&pending_send, MPI_STATUS_IGNORE);
                delete pending_packed;
                pending_packed = nullptr;
                pending_send = MPI_REQUEST_NULL;
            }
        }

        auto end = system_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        my_guess_time = double(duration.count()) * microseconds::period::num / microseconds::period::den;

        MPI_Barrier(role_comm);

        // 发送下班哨（非阻塞），每个 STOP 信号分别发送
        if (mpi_rank == 0) {
            for (int i = num_producers; i < mpi_size; i++) {
                MPI_Request stop_req;
                MPI_Isend(NULL, 0, MPI_CHAR, i, TAG_STOP, MPI_COMM_WORLD, &stop_req);
                MPI_Wait(&stop_req, MPI_STATUS_IGNORE);
            }
        }
    }
    // ================== 多消费者组 (非阻塞接收) ==================
    else if (color == 1 && mpi_size > 1) {
        while (true) {
            MPI_Status status;
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_STOP) {
                // STOP 信号：非阻塞接收
                MPI_Request stop_req;
                MPI_Irecv(NULL, 0, MPI_CHAR, status.MPI_SOURCE, TAG_STOP,
                          MPI_COMM_WORLD, &stop_req);
                MPI_Wait(&stop_req, MPI_STATUS_IGNORE);
                break;
            }

            int count;
            MPI_Get_count(&status, MPI_CHAR, &count);
            char* buf = new char[count];

            // ---- 非阻塞接收 ----
            MPI_Request recv_req;
            MPI_Irecv(buf, count, MPI_CHAR, status.MPI_SOURCE, TAG_DATA,
                      MPI_COMM_WORLD, &recv_req);
            MPI_Wait(&recv_req, MPI_STATUS_IGNORE);
            // ---- 非阻塞操作完成 ----

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
                string inputs[4] = { local_guesses[idx], local_guesses[idx+1],
                                     local_guesses[idx+2], local_guesses[idx+3] };
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
    MPI_Barrier(MPI_COMM_WORLD);

    double global_max_guess = 0;
    double global_max_hash = 0;
    double global_train = 0;

    MPI_Reduce(&my_guess_time, &global_max_guess, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&my_hash_time, &global_max_hash, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_train, &global_train, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (mpi_rank == 0) {
        time_train = global_train;
        time_hash = global_max_hash;
        time_guess = global_max_guess + time_hash;

        cout << "Guess time:" << time_guess - time_hash << "seconds"<< endl;
        cout << "Hash time:" << time_hash << "seconds"<<endl;
        cout << "Train time:" << time_train <<"seconds"<<endl;
    }

    MPI_Comm_free(&role_comm);
    MPI_Finalize();
    return 0;
}