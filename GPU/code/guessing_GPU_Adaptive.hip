#include "PCFG.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <hip/hip_runtime.h>
using namespace std;

#ifndef GPU_PT_THRESHOLD
#define GPU_PT_THRESHOLD 8192
#endif
static long long adaptive_cpu_pts = 0;
static long long adaptive_gpu_pts = 0;
static long long adaptive_cpu_guesses = 0;
static long long adaptive_gpu_guesses = 0;

#define HIP_CHECK_GPU(call)                                                            \
    do                                                                                 \
    {                                                                                  \
        hipError_t err = (call);                                                       \
        if (err != hipSuccess)                                                         \
        {                                                                              \
            cerr << "HIP error: " << hipGetErrorString(err)                            \
                 << " at " << __FILE__ << ":" << __LINE__ << endl;                   \
            throw runtime_error("HIP runtime failure");                                \
        }                                                                              \
    } while (0)

__global__ void build_guesses_kernel(const char *values,
                                     const int *offsets,
                                     const int *lengths,
                                     int value_count,
                                     const char *prefix,
                                     int prefix_len,
                                     char *output,
                                     int output_stride)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= value_count)
    {
        return;
    }

    char *dst = output + tid * output_stride;
    for (int i = 0; i < prefix_len; ++i)
    {
        dst[i] = prefix[i];
    }

    int value_len = lengths[tid];
    int value_offset = offsets[tid];
    for (int i = 0; i < value_len; ++i)
    {
        dst[prefix_len + i] = values[value_offset + i];
    }
    dst[prefix_len + value_len] = '\0';
}

static void AppendGuessesGPU(const string &prefix,
                             const vector<string> &values,
                             int begin,
                             int end,
                             vector<string> &guesses,
                             int &total_guesses)
{
    int value_count = end - begin;
    if (value_count <= 0)
    {
        return;
    }

    vector<int> offsets(value_count);
    vector<int> lengths(value_count);
    size_t flat_size = 0;
    int max_value_len = 0;

    for (int i = 0; i < value_count; ++i)
    {
        const string &value = values[begin + i];
        offsets[i] = static_cast<int>(flat_size);
        lengths[i] = static_cast<int>(value.size());
        flat_size += value.size();
        max_value_len = max(max_value_len, static_cast<int>(value.size()));
    }

    vector<char> flat_values(flat_size == 0 ? 1 : flat_size);
    for (int i = 0; i < value_count; ++i)
    {
        const string &value = values[begin + i];
        if (!value.empty())
        {
            memcpy(flat_values.data() + offsets[i], value.data(), value.size());
        }
    }

    int prefix_len = static_cast<int>(prefix.size());
    int output_stride = prefix_len + max_value_len + 1;
    vector<char> host_output(static_cast<size_t>(value_count) * output_stride);

    char *d_values = nullptr;
    int *d_offsets = nullptr;
    int *d_lengths = nullptr;
    char *d_prefix = nullptr;
    char *d_output = nullptr;

    HIP_CHECK_GPU(hipMalloc(&d_values, flat_values.size() * sizeof(char)));
    HIP_CHECK_GPU(hipMalloc(&d_offsets, value_count * sizeof(int)));
    HIP_CHECK_GPU(hipMalloc(&d_lengths, value_count * sizeof(int)));
    HIP_CHECK_GPU(hipMalloc(&d_output, host_output.size() * sizeof(char)));

    HIP_CHECK_GPU(hipMemcpy(d_values, flat_values.data(), flat_values.size() * sizeof(char), hipMemcpyHostToDevice));
    HIP_CHECK_GPU(hipMemcpy(d_offsets, offsets.data(), value_count * sizeof(int), hipMemcpyHostToDevice));
    HIP_CHECK_GPU(hipMemcpy(d_lengths, lengths.data(), value_count * sizeof(int), hipMemcpyHostToDevice));

    if (prefix_len > 0)
    {
        HIP_CHECK_GPU(hipMalloc(&d_prefix, prefix_len * sizeof(char)));
        HIP_CHECK_GPU(hipMemcpy(d_prefix, prefix.data(), prefix_len * sizeof(char), hipMemcpyHostToDevice));
    }

    const int threads = 256;
    int blocks = (value_count + threads - 1) / threads;
    build_guesses_kernel<<<blocks, threads>>>(d_values,
                                              d_offsets,
                                              d_lengths,
                                              value_count,
                                              d_prefix,
                                              prefix_len,
                                              d_output,
                                              output_stride);
    HIP_CHECK_GPU(hipGetLastError());
    HIP_CHECK_GPU(hipDeviceSynchronize());
    HIP_CHECK_GPU(hipMemcpy(host_output.data(), d_output, host_output.size() * sizeof(char), hipMemcpyDeviceToHost));

    guesses.reserve(guesses.size() + value_count);
    for (int i = 0; i < value_count; ++i)
    {
        guesses.emplace_back(host_output.data() + static_cast<size_t>(i) * output_stride);
    }
    total_guesses += value_count;

    if (d_prefix != nullptr)
    {
        HIP_CHECK_GPU(hipFree(d_prefix));
    }
    HIP_CHECK_GPU(hipFree(d_output));
    HIP_CHECK_GPU(hipFree(d_lengths));
    HIP_CHECK_GPU(hipFree(d_offsets));
    HIP_CHECK_GPU(hipFree(d_values));
}

static void AppendGuessesGPU(const string &prefix,
                             const vector<string> &values,
                             int count,
                             vector<string> &guesses,
                             int &total_guesses)
{
    const int batch_size = 1 << 20;
    for (int begin = 0; begin < count; begin += batch_size)
    {
        int end = min(begin + batch_size, count);
        AppendGuessesGPU(prefix, values, begin, end, guesses, total_guesses);
    }
}

static void AppendGuessesCPU(const string &prefix,
                             const vector<string> &values,
                             int count,
                             vector<string> &guesses,
                             int &total_guesses)
{
    guesses.reserve(guesses.size() + count);
    for (int i = 0; i < count; i += 1)
    {
        guesses.emplace_back(prefix + values[i]);
        total_guesses += 1;
    }
}

static void AppendGuessesAdaptive(const string &prefix,
                                  const vector<string> &values,
                                  int count,
                                  vector<string> &guesses,
                                  int &total_guesses)
{
    if (count < GPU_PT_THRESHOLD)
    {
        adaptive_cpu_pts += 1;
        adaptive_cpu_guesses += count;
        AppendGuessesCPU(prefix, values, count, guesses, total_guesses);
    }
    else
    {
        adaptive_gpu_pts += 1;
        adaptive_gpu_guesses += count;
        AppendGuessesGPU(prefix, values, count, guesses, total_guesses);
    }
}

struct AdaptiveStatsPrinter
{
    ~AdaptiveStatsPrinter()
    {
        cout << "Adaptive threshold:" << GPU_PT_THRESHOLD << endl;
        cout << "Adaptive CPU PTs:" << adaptive_cpu_pts << endl;
        cout << "Adaptive GPU PTs:" << adaptive_gpu_pts << endl;
        cout << "Adaptive CPU guesses:" << adaptive_cpu_guesses << endl;
        cout << "Adaptive GPU guesses:" << adaptive_gpu_guesses << endl;
    }
};

static AdaptiveStatsPrinter adaptive_stats_printer;

void PriorityQueue::CalProb(PT &pt)
{
    // 计算PriorityQueue里面一个PT的流程如下：
    // 1. 首先需要计算一个PT本身的概率。例如，L6S1的概率为0.15
    // 2. 需要注意的是，Queue里面的PT不是“纯粹的”PT，而是除了最后一个segment以外，全部被value实例化的PT
    // 3. 所以，对于L6S1而言，其在Queue里面的实际PT可能是123456S1，其中“123456”为L6的一个具体value。
    // 4. 这个时候就需要计算123456在L6中出现的概率了。假设123456在所有L6 segment中的概率为0.1，那么123456S1的概率就是0.1*0.15

    // 计算一个PT本身的概率。后续所有具体segment value的概率，直接累乘在这个初始概率值上
    pt.prob = pt.preterm_prob;

    // index: 标注当前segment在PT中的位置
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
    // 用所有可能的PT，按概率降序填满整个优先队列
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

        // 计算当前pt的概率
        CalProb(pt);
        // 将PT放入优先队列
        priority.emplace_back(pt);
    }
}

void PriorityQueue::PopNext()
{

    // 对优先队列最前面的PT，首先利用这个PT生成一系列猜测
    Generate(priority.front());

    // 然后需要根据即将出队的PT，生成一系列新的PT
    vector<PT> new_pts = priority.front().NewPTs();
    for (PT pt : new_pts)
    {
        // 计算概率
        CalProb(pt);
        // 接下来的这个循环，作用是根据概率，将新的PT插入到优先队列中
        for (auto iter = priority.begin(); iter != priority.end(); iter++)
        {
            // 对于非队首和队尾的特殊情况
            if (iter != priority.end() - 1 && iter != priority.begin())
            {
                // 判定概率
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

    // 现在队首的PT善后工作已经结束，将其出队（删除）
    priority.erase(priority.begin());
}

// 这个函数你就算看不懂，对并行算法的实现影响也不大
// 当然如果你想做一个基于多优先队列的并行算法，可能得稍微看一看了
vector<PT> PT::NewPTs()
{
    // 存储生成的新PT
    vector<PT> res;

    // 假如这个PT只有一个segment
    // 那么这个segment的所有value在出队前就已经被遍历完毕，并作为猜测输出
    // 因此，所有这个PT可能对应的口令猜测已经遍历完成，无需生成新的PT
    if (content.size() == 1)
    {
        return res;
    }
    else
    {
        // 最初的pivot值。我们将更改位置下标大于等于这个pivot值的segment的值（最后一个segment除外），并且一次只更改一个segment
        int init_pivot = pivot;

        // 开始遍历所有位置值大于等于init_pivot值的segment
        // 注意i < curr_indices.size() - 1，也就是除去了最后一个segment（这个segment的赋值预留给并行环节）
        for (int i = pivot; i < curr_indices.size() - 1; i += 1)
        {
            // curr_indices: 标记各segment目前的value在模型里对应的下标
            curr_indices[i] += 1;

            // max_indices：标记各segment在模型中一共有多少个value
            if (curr_indices[i] < max_indices[i])
            {
                // 更新pivot值
                pivot = i;
                res.emplace_back(*this);
            }

            // 这个步骤对于你理解pivot的作用、新PT生成的过程而言，至关重要
            curr_indices[i] -= 1;
        }
        pivot = init_pivot;
        return res;
    }

    return res;
}

// 这个函数是PCFG并行化算法的主要载体
// Adaptive版本根据PT候选数量动态选择CPU或GPU，避免小PT的GPU启动和数据传输开销。
void PriorityQueue::Generate(PT pt)
{
    // 计算PT的概率，这里主要是给PT的概率进行初始化
    CalProb(pt);

    // 对于只有一个segment的PT，直接遍历生成其中的所有value即可
    if (pt.content.size() == 1)
    {
        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a = nullptr;
        // 在模型中定位到这个segment
        if (pt.content[0].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[0])];
        }
        if (pt.content[0].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[0])];
        }
        if (pt.content[0].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[0])];
        }

        AppendGuessesAdaptive("", a->ordered_values, pt.max_indices[0], guesses, total_guesses);
    }
    else
    {
        string guess;
        int seg_idx = 0;
        // 这个for循环的作用：给当前PT的所有segment赋予实际的值（最后一个segment除外）
        // segment值根据curr_indices中对应的值加以确定
        for (int idx : pt.curr_indices)
        {
            if (pt.content[seg_idx].type == 1)
            {
                guess += m.letters[m.FindLetter(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 2)
            {
                guess += m.digits[m.FindDigit(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 3)
            {
                guess += m.symbols[m.FindSymbol(pt.content[seg_idx])].ordered_values[idx];
            }
            seg_idx += 1;
            if (seg_idx == pt.content.size() - 1)
            {
                break;
            }
        }

        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a = nullptr;
        if (pt.content[pt.content.size() - 1].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[pt.content.size() - 1])];
        }

        AppendGuessesAdaptive(guess,
                              a->ordered_values,
                              pt.max_indices[pt.content.size() - 1],
                              guesses,
                              total_guesses);
    }
}
