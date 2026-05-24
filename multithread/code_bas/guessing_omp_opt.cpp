#include "PCFG_omp.h" 
#include <omp.h>
using namespace std;

void PriorityQueue::CalProb(PT &pt)
{
    pt.prob = pt.preterm_prob;
    int index = 0;
    for (int idx : pt.curr_indices)
    {
        if (pt.content[index].type == 1) {
            pt.prob *= m.letters[m.FindLetter(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.letters[m.FindLetter(pt.content[index])].total_freq;
        }
        if (pt.content[index].type == 2) {
            pt.prob *= m.digits[m.FindDigit(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.digits[m.FindDigit(pt.content[index])].total_freq;
        }
        if (pt.content[index].type == 3) {
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
            if (seg.type == 1) {
                pt.max_indices.emplace_back(m.letters[m.FindLetter(seg)].ordered_values.size());
            }
            if (seg.type == 2) {
                pt.max_indices.emplace_back(m.digits[m.FindDigit(seg)].ordered_values.size());
            }
            if (seg.type == 3) {
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
            if (iter != priority.end() - 1 && iter != priority.begin()) {
                if (pt.prob <= iter->prob && pt.prob > (iter + 1)->prob) {
                    priority.emplace(iter + 1, pt);
                    break;
                }
            }
            if (iter == priority.end() - 1) {
                priority.emplace_back(pt);
                break;
            }
            if (iter == priority.begin() && iter->prob < pt.prob) {
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
    if (content.size() == 1) {
        return res;
    } else {
        int init_pivot = pivot;
        for (int i = pivot; i < curr_indices.size() - 1; i += 1) {
            curr_indices[i] += 1;
            if (curr_indices[i] < max_indices[i]) {
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

    if (total_tasks == 0) return;

    // 【算法优化核心：动态粒度控制】
    // 使用 if(total_tasks > 5000) 告诉 OpenMP，只有任务量足够庞大时才唤醒线程池
    // 否则直接串行执行该块，避免微小任务引发的线程调度开销
    #pragma omp parallel num_threads(8) if(total_tasks > 5000)
    {
        vector<string> local_guesses;
        
        #pragma omp for nowait
        for (int i = 0; i < total_tasks; i++)
        {
            if (type == 1)
            {
                local_guesses.emplace_back(a->ordered_values[i]);
            }
            else
            {
                local_guesses.emplace_back(prefix + a->ordered_values[i]);
            }
        }

        #pragma omp critical
        {
            guesses.insert(guesses.end(), local_guesses.begin(), local_guesses.end());
            total_guesses += local_guesses.size();
        }
    }
}