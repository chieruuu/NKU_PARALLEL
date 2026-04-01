#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cmath>
#include "calculative.cpp"

using namespace std;

int main(int argc, char* argv[]) {
    system("chcp 65001 > nul");
    string label = (argc > 1) ? argv[1] : "Unknown";

    vector<long long> test_sizes = {
        1LL << 20, 
        1LL << 22, 
        1LL << 24, 
        1LL << 26, 
        1LL << 28  
    };

    ofstream csv_file("compiler_results.csv", ios::app);

    for (long long n : test_sizes) {
        vector<double> a(n, 0.1);
        int repeat = (n >= (1 << 26)) ? 30 : 100;
        
        HighResTimer timer;
        double total_time = 0;

        for (int r = 0; r < repeat; r++) {
            timer.start();
            double sum = 0;

            for (long long i = 0; i < n; i++) {
                sum += a[i];
            }
            timer.stop();
            total_time += timer.getElapsedMs();
            
            if (sum < 0) cout << sum; 
        }

        double avg_time = total_time / repeat;
        
        csv_file << label << "," << n << "," << avg_time << "\n";
        cout << "版本 [" << label << "] N=" << n << " 完成，平均耗时: " << avg_time << " ms" << endl;
    }

    csv_file.close();
    return 0;
}