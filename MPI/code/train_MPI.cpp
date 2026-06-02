// 文件名: train_MPI.cpp
#include "PCFG_MPI.h" // 修改点: 引入新的头文件
#include <fstream>
#include <cctype>
#include <algorithm>

void model::train(string path)
{
    string pw;
    ifstream train_set(path);
    int lines = 0;
    // 修改点: 只有 Rank 0 负责打印
    if (mpi_rank == 0) {
        cout<<"Training..."<<endl;
        cout<<"Training phase 1: reading and parsing passwords..."<<endl;
    }
    while (train_set >> pw)
    {
        lines += 1;
        if (lines % 10000 == 0)
        {
            // 修改点: 只有 Rank 0 负责打印
            if (mpi_rank == 0) {
                cout <<"Lines processed: "<< lines << endl;
            }
            if (lines > 3000000)
            {
                break;
            }
        }
        parse(pw);
    }
}

int model::FindPT(PT pt) { /* 省略原有内容，与源文件完全一致 */ 
    for (int id = 0; id < preterminals.size(); id += 1) {
        if (preterminals[id].content.size() != pt.content.size()) { continue; }
        else {
            bool equal_flag = true;
            for (int idx = 0; idx < preterminals[id].content.size(); idx += 1) {
                if (preterminals[id].content[idx].type != pt.content[idx].type || preterminals[id].content[idx].length != pt.content[idx].length) { equal_flag = false; break; }
            }
            if (equal_flag == true) { return id; }
        }
    }
    return -1;
}

int model::FindLetter(segment seg) { /* 保持原样 */
    for (int id = 0; id < letters.size(); id += 1) { if (letters[id].length == seg.length) { return id; } }
    return -1;
}

int model::FindDigit(segment seg) { /* 保持原样 */
    for (int id = 0; id < digits.size(); id += 1) { if (digits[id].length == seg.length) { return id; } }
    return -1;
}

int model::FindSymbol(segment seg) { /* 保持原样 */
    for (int id = 0; id < symbols.size(); id += 1) { if (symbols[id].length == seg.length) { return id; } }
    return -1;
}

void PT::insert(segment seg) { content.emplace_back(seg); }

void segment::insert(string value) {
    if (values.find(value) == values.end()) {
        values[value] = values.size();
        freqs[values[value]] = 1;
    } else { freqs[values[value]] += 1; }
}

void segment::order() {
    for (pair<string, int> value : values) { ordered_values.emplace_back(value.first); }
    std::sort(ordered_values.begin(), ordered_values.end(),
              [this](const std::string &a, const std::string &b) { return freqs.at(values[a]) > freqs.at(values[b]); });
    for (const std::string &val : ordered_values) {
        ordered_freqs.emplace_back(freqs.at(values[val]));
        total_freq += freqs.at(values[val]);
    }
}

void model::parse(string pw)
{
    PT pt;
    string curr_part = "";
    int curr_type = 0; // 0: 未设置, 1: 字母, 2: 数字, 3: 特殊字符
    for (char ch : pw)
    {
        if (isalpha(ch))
        {
            if (curr_type != 1)
            {
                if (curr_type == 2)
                {
                    segment seg(curr_type, curr_part.length());
                    if (FindDigit(seg) == -1)
                    {
                        int id = GetNextDigitsID();
                        digits.emplace_back(seg);
                        digits[id].insert(curr_part);
                        digits_freq[id] = 1;
                    }
                    else
                    {
                        int id = FindDigit(seg);
                        digits_freq[id] += 1;
                        digits[id].insert(curr_part);
                    }
                    curr_part.clear();
                    pt.insert(seg);
                }
                else if (curr_type == 3)
                {
                    segment seg(curr_type, curr_part.length());
                    if (FindSymbol(seg) == -1)
                    {
                        int id = GetNextSymbolsID();
                        symbols.emplace_back(seg);
                        symbols_freq[id] = 1;
                        symbols[id].insert(curr_part);
                    }
                    else
                    {
                        int id = FindSymbol(seg);
                        symbols_freq[id] += 1;
                        symbols[id].insert(curr_part);
                    }
                    curr_part.clear();
                    pt.insert(seg);
                }
            }
            curr_type = 1;
            curr_part += ch;
        }
        else if (isdigit(ch))
        {
            if (curr_type != 2)
            {
                if (curr_type == 1)
                {
                    segment seg(curr_type, curr_part.length());
                    if (FindLetter(seg) == -1)
                    {
                        int id = GetNextLettersID();
                        letters.emplace_back(seg);
                        letters_freq[id] = 1;
                        letters[id].insert(curr_part);
                    }
                    else
                    {
                        int id = FindLetter(seg);
                        letters_freq[id] += 1;
                        letters[id].insert(curr_part);
                    }
                    curr_part.clear();
                    pt.insert(seg);
                }
                else if (curr_type == 3)
                {
                    segment seg(curr_type, curr_part.length());
                    if (FindSymbol(seg) == -1)
                    {
                        int id = GetNextSymbolsID();
                        symbols.emplace_back(seg);
                        symbols_freq[id] = 1;
                        symbols[id].insert(curr_part);
                    }
                    else
                    {
                        int id = FindSymbol(seg);
                        symbols_freq[id] += 1;
                        symbols[id].insert(curr_part);
                    }
                    curr_part.clear();
                    pt.insert(seg);
                }
            }
            curr_type = 2;
            curr_part += ch;
        }
        else
        {
            if (curr_type != 3)
            {
                if (curr_type == 1)
                {
                    segment seg(curr_type, curr_part.length());
                    if (FindLetter(seg) == -1)
                    {
                        int id = GetNextLettersID();
                        letters.emplace_back(seg);
                        letters_freq[id] = 1;
                        letters[id].insert(curr_part);
                    }
                    else
                    {
                        int id = FindLetter(seg);
                        letters_freq[id] += 1;
                        letters[id].insert(curr_part);
                    }
                    curr_part.clear();
                    pt.insert(seg);
                }
                else if (curr_type == 2)
                {
                    segment seg(curr_type, curr_part.length());
                    if (FindDigit(seg) == -1)
                    {
                        int id = GetNextDigitsID();
                        digits.emplace_back(seg);
                        digits_freq[id] = 1;
                        digits[id].insert(curr_part);
                    }
                    else
                    {
                        int id = FindDigit(seg);
                        digits_freq[id] += 1;
                        digits[id].insert(curr_part);
                    }
                    curr_part.clear();
                    pt.insert(seg);
                }
            }
            curr_type = 3;
            curr_part += ch;
        }
    }
    if (!curr_part.empty())
    {
        if (curr_type == 1)
        {
            segment seg(curr_type, curr_part.length());
            if (FindLetter(seg) == -1)
            {
                int id = GetNextLettersID();
                letters.emplace_back(seg);
                letters_freq[id] = 1;
                letters[id].insert(curr_part);
            }
            else
            {
                int id = FindLetter(seg);
                letters_freq[id] += 1;
                letters[id].insert(curr_part);
            }
            curr_part.clear();
            pt.insert(seg);
        }
        else if (curr_type == 2)
        {
            segment seg(curr_type, curr_part.length());
            if (FindDigit(seg) == -1)
            {
                int id = GetNextDigitsID();
                digits.emplace_back(seg);
                digits_freq[id] = 1;
                digits[id].insert(curr_part);
            }
            else
            {
                int id = FindDigit(seg);
                digits_freq[id] += 1;
                digits[id].insert(curr_part);
            }
            curr_part.clear();
            pt.insert(seg);
        }
        else
        {
            segment seg(curr_type, curr_part.length());
            if (FindSymbol(seg) == -1)
            {
                int id = GetNextSymbolsID();
                symbols.emplace_back(seg);
                symbols_freq[id] = 1;
                symbols[id].insert(curr_part);
            }
            else
            {
                int id = FindSymbol(seg);
                symbols_freq[id] += 1;
                symbols[id].insert(curr_part);
            }
            curr_part.clear();
            pt.insert(seg);
        }
    }
    total_preterm += 1;
    if (FindPT(pt) == -1)
    {
        for (int i = 0; i < pt.content.size(); i += 1)
        {
            pt.curr_indices.emplace_back(0);
        }
        int id = GetNextPretermID();
        preterminals.emplace_back(pt);
        preterm_freq[id] = 1;
    }
    else
    {
        int id = FindPT(pt);
        preterm_freq[id] += 1;
    }
}

void segment::PrintSeg()
{
    if (type == 1)
    {
        cout << "L" << length;
    }
    if (type == 2)
    {
        cout << "D" << length;
    }
    if (type == 3)
    {
        cout << "S" << length;
    }
}

void segment::PrintValues()
{
    for (string iter : ordered_values)
    {
        cout << iter << " freq:" << freqs[values[iter]] << endl;
    }
}

void PT::PrintPT()
{
    for (auto iter : content)
    {
        iter.PrintSeg();
    }
}

void model::print()
{
    cout << "preterminals:" << endl;
    for (int i = 0; i < preterminals.size(); i += 1)
    {
        preterminals[i].PrintPT();
        cout << " freq:" << preterm_freq[i];
        cout << endl;
    }
    for (auto iter : ordered_pts)
    {
        iter.PrintPT();
        cout << " freq:" << preterm_freq[FindPT(iter)];
        cout << endl;
    }
    cout << "segments:" << endl;
    for (int i = 0; i < letters.size(); i += 1)
    {
        letters[i].PrintSeg();
        cout << " freq:" << letters_freq[i];
        cout << endl;
    }
    for (int i = 0; i < digits.size(); i += 1)
    {
        digits[i].PrintSeg();
        cout << " freq:" << digits_freq[i];
        cout << endl;
    }
    for (int i = 0; i < symbols.size(); i += 1)
    {
        symbols[i].PrintSeg();
        cout << " freq:" << symbols_freq[i];
        cout << endl;
    }
}

bool compareByPretermProb(const PT& a, const PT& b) { return a.preterm_prob > b.preterm_prob; }

void model::order()
{
    // 修改点: 只有 Rank 0 负责打印
    if (mpi_rank == 0) cout << "Training phase 2: Ordering segment values and PTs..." << endl;
    for (PT pt : preterminals)
    {
        pt.preterm_prob = float(preterm_freq[FindPT(pt)]) / total_preterm;
        ordered_pts.emplace_back(pt);
    }
    if (mpi_rank == 0) cout << "total pts" << ordered_pts.size() << endl;
    std::sort(ordered_pts.begin(), ordered_pts.end(), compareByPretermProb);
    
    if (mpi_rank == 0) cout << "Ordering letters" << endl;
    for (int i = 0; i < letters.size(); i += 1) { letters[i].order(); }
    
    if (mpi_rank == 0) cout << "Ordering digits" << endl;
    for (int i = 0; i < digits.size(); i += 1) { digits[i].order(); }
    
    if (mpi_rank == 0) cout << "ordering symbols" << endl;
    for (int i = 0; i < symbols.size(); i += 1) { symbols[i].order(); }
}