#include "PCFG_pthread_adv.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <pthread.h>

using namespace std;

// 合并局部模型的数据到全局模型
void model::MergeFrom(model& local_m) {
    this->total_preterm += local_m.total_preterm;

    // 1. 合并 Letters
    for (int i = 0; i < local_m.letters.size(); i++) {
        segment& local_seg = local_m.letters[i];
        int global_id = FindLetter(local_seg);
        if (global_id == -1) {
            global_id = GetNextLettersID();
            letters.emplace_back(segment(local_seg.type, local_seg.length));
            letters_freq[global_id] = 0;
        }
        letters_freq[global_id] += local_m.letters_freq[i];
        
        auto& target_seg = letters[global_id];
        for (auto& val_pair : local_seg.values) {
            string val = val_pair.first;
            int count = local_seg.freqs[val_pair.second];
            if (target_seg.values.find(val) == target_seg.values.end()) {
                int new_idx = target_seg.values.size();
                target_seg.values[val] = new_idx;
                target_seg.freqs[new_idx] = count;
            } else {
                target_seg.freqs[target_seg.values[val]] += count;
            }
        }
    }

    // 2. 合并 Digits
    for (int i = 0; i < local_m.digits.size(); i++) {
        segment& local_seg = local_m.digits[i];
        int global_id = FindDigit(local_seg);
        if (global_id == -1) {
            global_id = GetNextDigitsID();
            digits.emplace_back(segment(local_seg.type, local_seg.length));
            digits_freq[global_id] = 0;
        }
        digits_freq[global_id] += local_m.digits_freq[i];
        
        auto& target_seg = digits[global_id];
        for (auto& val_pair : local_seg.values) {
            string val = val_pair.first;
            int count = local_seg.freqs[val_pair.second];
            if (target_seg.values.find(val) == target_seg.values.end()) {
                int new_idx = target_seg.values.size();
                target_seg.values[val] = new_idx;
                target_seg.freqs[new_idx] = count;
            } else {
                target_seg.freqs[target_seg.values[val]] += count;
            }
        }
    }

    // 3. 合并 Symbols
    for (int i = 0; i < local_m.symbols.size(); i++) {
        segment& local_seg = local_m.symbols[i];
        int global_id = FindSymbol(local_seg);
        if (global_id == -1) {
            global_id = GetNextSymbolsID();
            symbols.emplace_back(segment(local_seg.type, local_seg.length));
            symbols_freq[global_id] = 0;
        }
        symbols_freq[global_id] += local_m.symbols_freq[i];
        
        auto& target_seg = symbols[global_id];
        for (auto& val_pair : local_seg.values) {
            string val = val_pair.first;
            int count = local_seg.freqs[val_pair.second];
            if (target_seg.values.find(val) == target_seg.values.end()) {
                int new_idx = target_seg.values.size();
                target_seg.values[val] = new_idx;
                target_seg.freqs[new_idx] = count;
            } else {
                target_seg.freqs[target_seg.values[val]] += count;
            }
        }
    }

    // 4. 合并 PTs (Preterminals)
    for (int i = 0; i < local_m.preterminals.size(); i++) {
        PT& local_pt = local_m.preterminals[i];
        int global_id = FindPT(local_pt);
        if (global_id == -1) {
            global_id = GetNextPretermID();
            preterminals.emplace_back(local_pt);
            preterm_freq[global_id] = local_m.preterm_freq[i];
        } else {
            preterm_freq[global_id] += local_m.preterm_freq[i];
        }
    }
}

// 线程传参结构体
struct ThreadData {
    model* m;
    const vector<string>* pw_list;
    int start;
    int end;
};

// 线程执行函数
void* train_worker(void* arg) {
    ThreadData* d = (ThreadData*)arg;
    for (int i = d->start; i < d->end; ++i) {
        d->m->parse((*d->pw_list)[i]);
    }
    return NULL;
}

void model::train(string path)
{
    string pw;
    ifstream train_set(path);
    vector<string> passwords;
    
    cout << "Training..." << endl;
    cout << "Training phase 1: loading passwords into memory..." << endl;
    
    int lines = 0;
    while (train_set >> pw)
    {
        passwords.push_back(pw);
        lines += 1;
        if (lines % 10000 == 0)
        {
            cout << "Lines processed: " << lines << "\r" << std::flush;
            if (lines >= 3000000)
            {
                break;
            }
        }
    }
    cout << "\nLines loaded: " << passwords.size() << ". Starting parallel parse..." << endl;

    int num_threads = 8;
    vector<model> local_models(num_threads);
    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];

    // 计算每个线程处理的数据块大小
    int block_size = passwords.size() / num_threads;
    int remainder = passwords.size() % num_threads;
    int current_start = 0;

    // 创建 Pthread 执行解析
    for(int i = 0; i < num_threads; ++i) {
        int current_end = current_start + block_size + (i < remainder ? 1 : 0);
        thread_data[i] = {&local_models[i], &passwords, current_start, current_end};
        pthread_create(&threads[i], NULL, train_worker, &thread_data[i]);
        current_start = current_end;
    }

    // 等待所有线程完成
    for(int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
    
    cout << "Parsing complete. Merging models..." << endl;

    // 单线程聚合结果
    for(int i = 0; i < num_threads; i++) {
        this->MergeFrom(local_models[i]);
    }
}

int model::FindPT(PT pt)
{
    int size = preterminals.size();
    for (int i = 0; i < size; i++)
    {
        if (preterminals[i] == pt) return i;
    }
    return -1;
}

int model::FindLetter(segment letter)
{
    int size = letters.size();
    for (int i = 0; i < size; i++)
    {
        if (letters[i] == letter) return i;
    }
    return -1;
}

int model::FindDigit(segment digit)
{
    int size = digits.size();
    for (int i = 0; i < size; i++)
    {
        if (digits[i] == digit) return i;
    }
    return -1;
}

int model::FindSymbol(segment symbol)
{
    int size = symbols.size();
    for (int i = 0; i < size; i++)
    {
        if (symbols[i] == symbol) return i;
    }
    return -1;
}

void model::parse(string pw)
{
    vector<segment> segments = segmentation(pw);
    if (segments.size() > 5) return;
    PT pt;
    pt.length = segments.size();
    for (int i = 0; i < segments.size(); i++)
    {
        segment seg = segments[i];
        pt.types.push_back(seg.type);
        pt.lengths.push_back(seg.length);
        if (seg.type == "L")
        {
            int index = FindLetter(seg);
            if (index != -1)
            {
                if (letters[index].values.find(seg.value) == letters[index].values.end())
                {
                    letters[index].values[seg.value] = letters[index].values.size();
                }
                letters_freq[index]++;
                letters[index].freqs[letters[index].values[seg.value]]++;
            }
            else
            {
                index = GetNextLettersID();
                letters.push_back(segment("L", seg.length));
                letters[index].values[seg.value] = letters[index].values.size();
                letters_freq[index] = 1;
                letters[index].freqs[letters[index].values[seg.value]] = 1;
            }
        }
        else if (seg.type == "D")
        {
            int index = FindDigit(seg);
            if (index != -1)
            {
                if (digits[index].values.find(seg.value) == digits[index].values.end())
                {
                    digits[index].values[seg.value] = digits[index].values.size();
                }
                digits_freq[index]++;
                digits[index].freqs[digits[index].values[seg.value]]++;
            }
            else
            {
                index = GetNextDigitsID();
                digits.push_back(segment("D", seg.length));
                digits[index].values[seg.value] = digits[index].values.size();
                digits_freq[index] = 1;
                digits[index].freqs[digits[index].values[seg.value]] = 1;
            }
        }
        else if (seg.type == "S")
        {
            int index = FindSymbol(seg);
            if (index != -1)
            {
                if (symbols[index].values.find(seg.value) == symbols[index].values.end())
                {
                    symbols[index].values[seg.value] = symbols[index].values.size();
                }
                symbols_freq[index]++;
                symbols[index].freqs[symbols[index].values[seg.value]]++;
            }
            else
            {
                index = GetNextSymbolsID();
                symbols.push_back(segment("S", seg.length));
                symbols[index].values[seg.value] = symbols[index].values.size();
                symbols_freq[index] = 1;
                symbols[index].freqs[symbols[index].values[seg.value]] = 1;
            }
        }
    }
    int index = FindPT(pt);
    if (index != -1)
    {
        preterm_freq[index]++;
    }
    else
    {
        index = GetNextPretermID();
        preterminals.push_back(pt);
        preterm_freq[index] = 1;
    }
    total_preterm++;
}

bool cmp_pt(pair<PT, int> a, pair<PT, int> b)
{
    return a.second > b.second;
}

bool cmp_seg(pair<string, int> a, pair<string, int> b)
{
    return a.second > b.second;
}

void model::order()
{
    for (int i = 0; i < preterminals.size(); i++)
    {
        ordered_preterminals.push_back(make_pair(preterminals[i], preterm_freq[i]));
    }
    sort(ordered_preterminals.begin(), ordered_preterminals.end(), cmp_pt);
    
    for (int i = 0; i < letters.size(); i++)
    {
        vector<pair<string, int>> ordered_segment;
        for (map<string, int>::iterator it = letters[i].values.begin(); it != letters[i].values.end(); it++)
        {
            ordered_segment.push_back(make_pair(it->first, letters[i].freqs[it->second]));
        }
        sort(ordered_segment.begin(), ordered_segment.end(), cmp_seg);
        ordered_letters.push_back(ordered_segment);
    }

    for (int i = 0; i < digits.size(); i++)
    {
        vector<pair<string, int>> ordered_segment;
        for (map<string, int>::iterator it = digits[i].values.begin(); it != digits[i].values.end(); it++)
        {
            ordered_segment.push_back(make_pair(it->first, digits[i].freqs[it->second]));
        }
        sort(ordered_segment.begin(), ordered_segment.end(), cmp_seg);
        ordered_digits.push_back(ordered_segment);
    }

    for (int i = 0; i < symbols.size(); i++)
    {
        vector<pair<string, int>> ordered_segment;
        for (map<string, int>::iterator it = symbols[i].values.begin(); it != symbols[i].values.end(); it++)
        {
            ordered_segment.push_back(make_pair(it->first, symbols[i].freqs[it->second]));
        }
        sort(ordered_segment.begin(), ordered_segment.end(), cmp_seg);
        ordered_symbols.push_back(ordered_segment);
    }
}

void model::print()
{
    int count = 0;
    cout << "Preterminals: " << ordered_preterminals.size() << endl;
    for (int i = 0; i < ordered_preterminals.size(); i++)
    {
        for (int j = 0; j < ordered_preterminals[i].first.length; j++)
        {
            cout << ordered_preterminals[i].first.types[j] << ordered_preterminals[i].first.lengths[j];
        }
        cout << "\t" << ordered_preterminals[i].second << endl;
        count++;
        if (count == 10) break;
    }
    
    count = 0;
    for (int i = 0; i < letters.size(); i++)
    {
        if (letters[i].length == 4)
        {
            cout << "\nL4 segments: " << ordered_letters[i].size() << endl;
            for (int j = 0; j < ordered_letters[i].size(); j++)
            {
                cout << ordered_letters[i][j].first << "\t" << ordered_letters[i][j].second << endl;
                count++;
                if (count == 10) break;
            }
            break;
        }
    }
}