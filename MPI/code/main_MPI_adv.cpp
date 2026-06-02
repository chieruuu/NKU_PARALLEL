#include "PCFG_MPI.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include "md5.h"
#include <iomanip>
#include <mpi.h>

using namespace std;
using namespace chrono;

int mpi_rank, mpi_size;

// PT序列化结构：用于MPI通信
struct PTTask {
    int last_seg_type;      // 最后一个segment的类型
    int last_seg_index;     // 最后一个segment在模型中的索引
    int max_index;          // 候选值数量
    string prefix;          // 公共前缀
};

// 序列化PT为任务结构
PTTask serializePT(PT pt, model& m) {
    PTTask task;
    task.last_seg_type = pt.content.back().type;

    if (task.last_seg_type == 1) {
        task.last_seg_index = m.FindLetter(pt.content.back());
    } else if (task.last_seg_type == 2) {
        task.last_seg_index = m.FindDigit(pt.content.back());
    } else {
        task.last_seg_index = m.FindSymbol(pt.content.back());
    }

    task.max_index = pt.max_indices.back();

    // 计算公共前缀
    string prefix = "";
    for (int i = 0; i < pt.curr_indices.size() - 1; i++) {
        if (pt.content[i].type == 1) {
            prefix += m.letters[m.FindLetter(pt.content[i])].ordered_values[pt.curr_indices[i]];
        } else if (pt.content[i].type == 2) {
            prefix += m.digits[m.FindDigit(pt.content[i])].ordered_values[pt.curr_indices[i]];
        } else {
            prefix += m.symbols[m.FindSymbol(pt.content[i])].ordered_values[pt.curr_indices[i]];
        }
    }
    task.prefix = prefix;
    return task;
}

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
        "e10adc3949ba59abbe56e057f20f883e", "5f4dcc3b5aa765d61d8327deb882cf99",
        "25d55ad283aa400af464c76d713c07ad", "d8578edf8458ce06fbc5bb76a58c5ca4",
        "25f9e794323b453885f5181f1b624d0b", "827ccb0eea8a706c4c34a16891f84e7b",
        "81dc9bdb52d04dc20036dbd8313ed055", "96e79218965eb72c92a549dd5a330112"
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

    // 只有Rank 0初始化优先队列
    if (mpi_rank == 0) {
        q.init();
        cout << "here" << endl;
    }

    int curr_num = 0;
    auto start = system_clock::now();
    int history = 0;
    int local_generate_target = 10000000 / mpi_size;
    int local_batch_limit = 1000000 / mpi_size;
    bool all_done = false;

    // 进阶任务：PT层面并行，主进程负责分发PT，工作进程负责执行
    while (true) {
        if (mpi_rank == 0) {
            // 主进程：检查是否完成
            if (q.priority.empty() || history + q.total_guesses >= local_generate_target) {
                all_done = true;
            }
            MPI_Bcast(&all_done, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
            if (all_done) break;

            // 从优先队列取出一个PT
            PT current_pt = q.priority.front();
            q.PopNext();

            // 序列化并广播任务
            PTTask task = serializePT(current_pt, q.m);
            int prefix_len = task.prefix.length();
            MPI_Bcast(&prefix_len, 1, MPI_INT, 0, MPI_COMM_WORLD);

            int task_data[3] = {task.last_seg_type, task.last_seg_index, task.max_index};
            MPI_Bcast(task_data, 3, MPI_INT, 0, MPI_COMM_WORLD);

            vector<char> prefix_buf(prefix_len + 1);
            if (prefix_len > 0) strcpy(prefix_buf.data(), task.prefix.c_str());
            MPI_Bcast(prefix_buf.data(), prefix_len + 1, MPI_CHAR, 0, MPI_COMM_WORLD);

            // 主进程也执行生成
            segment* a = nullptr;
            if (task.last_seg_type == 1) a = &q.m.letters[task.last_seg_index];
            else if (task.last_seg_type == 2) a = &q.m.digits[task.last_seg_index];
            else a = &q.m.symbols[task.last_seg_index];

            string prefix(task.prefix);
            int total_items = task.max_index;
            int chunk = total_items / mpi_size;
            int rem = total_items % mpi_size;
            int start_idx = mpi_rank * chunk + min(mpi_rank, rem);
            int end_idx = start_idx + chunk + (mpi_rank < rem ? 1 : 0);

            vector<string> local_guesses;
            for (int i = start_idx; i < end_idx; i++) {
                local_guesses.push_back(prefix + a->ordered_values[i]);
            }

            // 收集各进程结果
            int local_size = local_guesses.size();
            vector<int> all_sizes(mpi_size);
            MPI_Gather(&local_size, 1, MPI_INT, all_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

            // 序列化本地结果
            string local_serialized;
            for (const auto& s : local_guesses) {
                local_serialized += s;
                local_serialized += '\0';
            }
            int local_serial_size = local_serialized.length();

            vector<int> all_serial_sizes(mpi_size);
            MPI_Gather(&local_serial_size, 1, MPI_INT, all_serial_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

            vector<int> serial_displacements(mpi_size, 0);
            int total_serial_recv = 0;
            for (int i = 0; i < mpi_size; i++) {
                serial_displacements[i] = total_serial_recv;
                total_serial_recv += all_serial_sizes[i];
            }

            vector<char> recv_buffer(total_serial_recv);
            MPI_Gatherv(local_serialized.data(), local_serial_size, MPI_CHAR,
                       recv_buffer.data(), all_serial_sizes.data(), serial_displacements.data(),
                       MPI_CHAR, 0, MPI_COMM_WORLD);

            // 反序列化并合并结果
            size_t pos = 0;
            while (pos < recv_buffer.size()) {
                string s(&recv_buffer[pos]);
                q.guesses.push_back(s);
                q.total_guesses++;
                pos += s.length() + 1;
            }

            curr_num = q.total_guesses;
            if (curr_num > 0 && curr_num % 100000 < 1000) {
                cout << "Guesses generated (Global Approx): " << history + curr_num << endl;
            }

            // 哈希处理
            if (curr_num > local_batch_limit) {
                auto start_hash = system_clock::now();
                int total = q.guesses.size();
                int idx = 0;
                for (; idx <= total - 4; idx += 4) {
                    string inputs[4] = { q.guesses[idx], q.guesses[idx+1], q.guesses[idx+2], q.guesses[idx+3] };
                    bit32 states[4][4];
                    MD5Hash_SIMD(inputs, states);
                }
                for (; idx < total; idx++) {
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
        else {
            // 工作进程：接收任务并执行
            MPI_Bcast(&all_done, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
            if (all_done) break;

            int prefix_len;
            MPI_Bcast(&prefix_len, 1, MPI_INT, 0, MPI_COMM_WORLD);

            int task_data[3];
            MPI_Bcast(task_data, 3, MPI_INT, 0, MPI_COMM_WORLD);

            vector<char> prefix_buf(prefix_len + 1);
            MPI_Bcast(prefix_buf.data(), prefix_len + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
            string prefix(prefix_buf.data());

            segment* a = nullptr;
            if (task_data[0] == 1) a = &q.m.letters[task_data[1]];
            else if (task_data[0] == 2) a = &q.m.digits[task_data[1]];
            else a = &q.m.symbols[task_data[1]];

            int total_items = task_data[2];
            int chunk = total_items / mpi_size;
            int rem = total_items % mpi_size;
            int start_idx = mpi_rank * chunk + min(mpi_rank, rem);
            int end_idx = start_idx + chunk + (mpi_rank < rem ? 1 : 0);

            vector<string> local_guesses;
            for (int i = start_idx; i < end_idx; i++) {
                local_guesses.push_back(prefix + a->ordered_values[i]);
            }

            // 发送结果
            int local_size = local_guesses.size();
            MPI_Gather(&local_size, 1, MPI_INT, nullptr, 1, MPI_INT, 0, MPI_COMM_WORLD);

            string local_serialized;
            for (const auto& s : local_guesses) {
                local_serialized += s;
                local_serialized += '\0';
            }
            int local_serial_size = local_serialized.length();

            MPI_Gather(&local_serial_size, 1, MPI_INT, nullptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Gatherv(local_serialized.data(), local_serial_size, MPI_CHAR,
                       nullptr, nullptr, nullptr, MPI_CHAR, 0, MPI_COMM_WORLD);
        }
    }

    if (mpi_rank == 0) {
        auto end = system_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        time_guess = double(duration.count()) * microseconds::period::num / microseconds::period::den;
        cout << "Guess time:" << time_guess - time_hash << "seconds" << endl;
        cout << "Hash time:" << time_hash << "seconds" << endl;
        cout << "Train time:" << time_train << "seconds" << endl;
    }

    MPI_Finalize();
    return 0;
}
