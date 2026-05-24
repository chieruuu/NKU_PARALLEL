#include "PCFG_pthread.h"
#include <pthread.h>
using namespace std;

// 定义传递给工作线程的参数结构体
struct ThreadArgs {
    PriorityQueue* pq;
    segment* a;
    string prefix;
    int start_idx;
    int end_idx;
    int type;
};

// 工作线程函数
void* GenerateWorker(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    vector<string> local_guesses;
    local_guesses.reserve(args->end_idx - args->start_idx);

    for (int i = args->start_idx; i < args->end_idx; i += 1) {
        if (args->type == 1) {
            local_guesses.emplace_back(args->a->ordered_values[i]);
        } else {
            local_guesses.emplace_back(args->prefix + args->a->ordered_values[i]);
        }
    }

    pthread_mutex_lock(&args->pq->mutex);
    args->pq->guesses.insert(args->pq->guesses.end(), local_guesses.begin(), local_guesses.end());
    args->pq->total_guesses += local_guesses.size();
    pthread_mutex_unlock(&args->pq->mutex);

    pthread_exit(NULL);
}

void PriorityQueue::CalProb(PT &pt)
{
    pt.prob = pt.preterm_prob;
    int index = 0;
    for (int idx : pt.curr_indices)
    {
        if (pt.content[index].type == 1)
        {
            pt.prob *= m.letters[m.FindLetter(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.letters[m.FindLetter(pt.content[index])].total_freq;
        }
        if (pt.content[index].type == 2)
        {
            pt.prob *= m.digits[m.FindDigit(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.digits[m.FindDigit(pt.content[index])].total_freq;
        }
        if (pt.content[index].type == 3)
        {
            pt.prob *= m.symbols[m.FindSymbol(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.symbols[m.FindSymbol(pt.content[index])].total_freq;
        }
        index += 1;
    }
}

void PriorityQueue::init()
{
    for (PT pt : m.ordered_pts)
    {
        for (segment seg : pt.content)
        {
            if (seg.type == 1)
            {
                pt.max_indices.emplace_back(m.letters[m.FindLetter(seg)].ordered_values.size());
            }
            if (seg.type == 2)
            {
                pt.max_indices.emplace_back(m.digits[m.FindDigit(seg)].ordered_values.size());
            }
            if (seg.type == 3)
            {
                pt.max_indices.emplace_back(m.symbols[m.FindSymbol(seg)].ordered_values.size());
            }
        }
        pt.preterm_prob = float(m.preterm_freq[m.FindPT(pt)]) / m.total_preterm;
        CalProb(pt);
        priority.emplace_back(pt);
    }
}

void PriorityQueue::PopNext()
{
    Generate(priority.front());
    vector<PT> new_pts = priority.front().NewPTs();
    for (PT pt : new_pts)
    {
        CalProb(pt);
        for (auto iter = priority.begin(); iter != priority.end(); iter++)
        {
            if (iter != priority.end() - 1 && iter != priority.begin())
            {
                if (pt.prob <= iter->prob && pt.prob > (iter + 1)->prob)
                {
                    priority.emplace(iter + 1, pt);
                    break;
                }
            }
            if (iter == priority.end() - 1)
            {
                priority.emplace_back(pt);
                break;
            }
            if (iter == priority.begin() && iter->prob < pt.prob)
            {
                priority.emplace(iter, pt);
                break;
            }
        }
    }
    priority.erase(priority.begin());
}

vector<PT> PT::NewPTs()
{
    vector<PT> res;
    if (content.size() == 1)
    {
        return res;
    }
    else
    {
        int init_pivot = pivot;
        for (int i = pivot; i < curr_indices.size() - 1; i += 1)
        {
            curr_indices[i] += 1;
            if (curr_indices[i] < max_indices[i])
            {
                pivot = i;
                res.emplace_back(*this);
            }
            curr_indices[i] -= 1;
        }
        pivot = init_pivot;
        return res;
    }
    return res;
}

void PriorityQueue::Generate(PT pt)
{
    CalProb(pt);

    int num_threads = 8;
    segment* a = nullptr;
    int total_tasks = 0;
    int type = 0;
    string prefix = "";

    if (pt.content.size() == 1)
    {
        type = 1;
        total_tasks = pt.max_indices[0];
        if (pt.content[0].type == 1) a = &m.letters[m.FindLetter(pt.content[0])];
        if (pt.content[0].type == 2) a = &m.digits[m.FindDigit(pt.content[0])];
        if (pt.content[0].type == 3) a = &m.symbols[m.FindSymbol(pt.content[0])];
    }
    else
    {
        type = 2;
        int seg_idx = 0;
        for (int idx : pt.curr_indices)
        {
            if (pt.content[seg_idx].type == 1) prefix += m.letters[m.FindLetter(pt.content[seg_idx])].ordered_values[idx];
            if (pt.content[seg_idx].type == 2) prefix += m.digits[m.FindDigit(pt.content[seg_idx])].ordered_values[idx];
            if (pt.content[seg_idx].type == 3) prefix += m.symbols[m.FindSymbol(pt.content[seg_idx])].ordered_values[idx];
            seg_idx += 1;
            if (seg_idx == pt.content.size() - 1) break;
        }

        total_tasks = pt.max_indices[pt.content.size() - 1];
        if (pt.content[pt.content.size() - 1].type == 1) a = &m.letters[m.FindLetter(pt.content[pt.content.size() - 1])];
        if (pt.content[pt.content.size() - 1].type == 2) a = &m.digits[m.FindDigit(pt.content[pt.content.size() - 1])];
        if (pt.content[pt.content.size() - 1].type == 3) a = &m.symbols[m.FindSymbol(pt.content[pt.content.size() - 1])];
    }

    if (total_tasks < num_threads) num_threads = total_tasks;
    if (num_threads == 0) return;

    // 【算法优化核心：动态粒度控制】
    // 若单次候选口令生成任务量小于预设阈值，直接在主线程串行执行，剥离调度与互斥锁竞争开销
    int threshold = 5000;
    if (total_tasks < threshold) {
        vector<string> local_guesses;
        local_guesses.reserve(total_tasks);
        for (int i = 0; i < total_tasks; i += 1) {
            if (type == 1) {
                local_guesses.emplace_back(a->ordered_values[i]);
            } else {
                local_guesses.emplace_back(prefix + a->ordered_values[i]);
            }
        }
        
        pthread_mutex_lock(&mutex);
        guesses.insert(guesses.end(), local_guesses.begin(), local_guesses.end());
        total_guesses += local_guesses.size();
        pthread_mutex_unlock(&mutex);
        return; 
    }

    pthread_t threads[num_threads];
    ThreadArgs args[num_threads];

    int chunk = total_tasks / num_threads;
    int remainder = total_tasks % num_threads;
    int current_start = 0;

    for (int t = 0; t < num_threads; t++) {
        args[t].pq = this;
        args[t].a = a;
        args[t].prefix = prefix;
        args[t].type = type;
        
        args[t].start_idx = current_start;
        int current_end = current_start + chunk + (t < remainder ? 1 : 0);
        args[t].end_idx = current_end;
        current_start = current_end;

        pthread_create(&threads[t], NULL, GenerateWorker, (void*)&args[t]);
    }

    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }
}