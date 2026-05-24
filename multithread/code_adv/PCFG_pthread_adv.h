#ifndef PCFG_PTHREAD_ADV_H
#define PCFG_PTHREAD_ADV_H

#include <string>
#include <iostream>
#include <unordered_map>
#include <queue>
#include <vector>
#include <pthread.h>

using namespace std;

#define NUM_QUEUES 8

class segment
{
public:
    int type; 
    int length; 
    segment(int type, int length)
    {
        this->type = type;
        this->length = length;
    };
    void PrintSeg();
    vector<string> ordered_values;
    vector<int> ordered_freqs;
    int total_freq = 0;
    unordered_map<string, int> values;
    unordered_map<int, int> freqs;
    void insert(string value);
    void order();
    void PrintValues();
};

class PT
{
public:
    vector<segment> content;
    int pivot = 0;
    void insert(segment seg);
    void PrintPT();
    vector<PT> NewPTs();
    vector<int> curr_indices;
    vector<int> max_indices;
    float preterm_prob;
    float prob;
};

class model
{
public:
    int preterm_id = -1;
    int letters_id = -1;
    int digits_id = -1;
    int symbols_id = -1;
    int GetNextPretermID() { preterm_id++; return preterm_id; };
    int GetNextLettersID() { letters_id++; return letters_id; };
    int GetNextDigitsID() { digits_id++; return digits_id; };
    int GetNextSymbolsID() { symbols_id++; return symbols_id; };
    int total_preterm = 0;
    vector<PT> preterminals;
    int FindPT(PT pt);
    vector<segment> letters;
    vector<segment> digits;
    vector<segment> symbols;
    int FindLetter(segment seg);
    int FindDigit(segment seg);
    int FindSymbol(segment seg);
    unordered_map<int, int> preterm_freq;
    unordered_map<int, int> letters_freq;
    unordered_map<int, int> digits_freq;
    unordered_map<int, int> symbols_freq;
    vector<PT> ordered_pts;

    void train(string train_path);
    void store(string store_path);
    void load(string load_path);
    void parse(string pw);
    void order();
    void print();

    void MergeFrom(model& local_m);
};

class PriorityQueue
{
public:
    vector<PT> priority; 
    
    // 进阶优化：多队列与多锁，代替单一的 guesses 和 mutex
    pthread_mutex_t mutexes[NUM_QUEUES];
    vector<string> multi_guesses[NUM_QUEUES];
    int sub_total_guesses[NUM_QUEUES];

    PriorityQueue() {
        for(int i = 0; i < NUM_QUEUES; ++i) {
            pthread_mutex_init(&mutexes[i], NULL);
            sub_total_guesses[i] = 0;
        }
    }
    ~PriorityQueue() {
        for(int i = 0; i < NUM_QUEUES; ++i) {
            pthread_mutex_destroy(&mutexes[i]);
        }
    }

    int GetTotalGuesses() {
        int sum = 0;
        for(int i = 0; i < NUM_QUEUES; ++i) {
            sum += sub_total_guesses[i];
        }
        return sum;
    }

    model m;
    void init();
    void PopNext();
    void Generate(PT pt);
    void CalProb(PT &pt);
};

#endif