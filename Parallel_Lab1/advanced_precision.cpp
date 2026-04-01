#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <fstream> 
#include "calculative.cpp"

using namespace std;

float recursive_sum_float(const vector<float>& a, int left, int right) {
    if (left == right) {
        return a[left];
    }
    int mid = left + (right - left) / 2;
    return recursive_sum_float(a, left, mid) + recursive_sum_float(a, mid + 1, right);
}

int main() {
    ofstream csv_file("precision_results.csv");
    csv_file << "N,Error_1way,Error_2way,Error_Rec,Time_1way,Time_2way,Time_Rec\n";

    cout << "===== 进阶探索一：计算次序与浮点数精度 (Swamping效应) =====" << endl;
    cout << "正在运行测试，请稍候..." << endl;

    for (int k = 10; k <= 24; k += 2) {
        int n = 1 << k; 
        
        vector<float> a(n, 0.01f); 
        HighResTimer timer;

        timer.start();
        float sum_1 = 0.0f;
        for (int i = 0; i < n; i++) sum_1 += a[i];
        timer.stop();
        double time_1 = timer.getElapsedMs();

        timer.start();
        float s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < n; i += 2) {
            s1 += a[i]; s2 += a[i + 1];
        }
        float sum_2 = s1 + s2;
        timer.stop();
        double time_2 = timer.getElapsedMs();

        timer.start();
        float sum_rec = recursive_sum_float(a, 0, n - 1);
        timer.stop();
        double time_rec = timer.getElapsedMs();

        double exact_sum = (double)n * 0.01;
        double err_1 = abs(exact_sum - sum_1);
        double err_2 = abs(exact_sum - sum_2);
        double err_rec = abs(exact_sum - sum_rec);

        cout << "N = " << n << "\t| 1路误差: " << err_1 << "\t| 递归误差: " << err_rec << endl;

        csv_file << n << "," << err_1 << "," << err_2 << "," << err_rec << ","
                 << time_1 << "," << time_2 << "," << time_rec << "\n";
    }

    csv_file.close();
    cout << "\n[成功] 数据已成功保存到 precision_results.csv" << endl; 
    return 0;
}