#include <iostream>
#include <vector>
#include <fstream>   
#include "calculative.cpp"

using namespace std;

void test_matrix_vector(int n, int repeat, ofstream& csv_file) {
    vector<vector<double>> b(n, vector<double>(n));
    vector<double> a(n);
    vector<double> sum_trivial(n, 0.0);
    vector<double> sum_optimized(n, 0.0);

    for (int j = 0; j < n; j++) {
        a[j] = j;
        for (int i = 0; i < n; i++) b[j][i] = i + j; 
    }

    HighResTimer timer;
    double time_trivial = 0.0, time_optimized = 0.0;

    timer.start();
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < n; i++) {
            sum_trivial[i] = 0.0;
            for (int j = 0; j < n; j++) sum_trivial[i] += b[j][i] * a[j]; 
        }
    }
    timer.stop();
    time_trivial = timer.getElapsedMs() / repeat;

    timer.start();
    for (int r = 0; r < repeat; r++) {
        for (int i = 0; i < n; i++) sum_optimized[i] = 0.0;
        for (int j = 0; j < n; j++) {
            for (int i = 0; i < n; i++) sum_optimized[i] += b[j][i] * a[j]; 
        }
    }
    timer.stop();
    time_optimized = timer.getElapsedMs() / repeat;

    double speedup = time_trivial / time_optimized;

    cout << "  矩阵规模 n = " << n << " | 重复: " << repeat << endl;
    cout << "    -> 平凡(逐列): " << time_trivial << " ms" << endl;
    cout << "    -> 优化(逐行): " << time_optimized << " ms" << endl;
    cout << "    -> 加速比    : " << speedup << " 倍\n" << endl;

    csv_file << n << "," << time_trivial << "," << time_optimized << "," << speedup << "\n";
    
    if (sum_trivial[0] != sum_optimized[0] && sum_trivial[0] < 0) cout << "Error" << endl; 
}

void test_sum(int n, int repeat, ofstream& csv_file) {
    vector<double> a(n); 
    for(int i = 0; i < n; i++) a[i] = (double)(i % 100) * 0.01; 
    
    HighResTimer timer;
    double time_1way = 0.0, time_2way = 0.0, time_4way = 0.0, time_8way = 0.0;
    double sum_1 = 0.0, sum_2 = 0.0, sum_4 = 0.0, sum_8 = 0.0;

    timer.start();
    for (int r = 0; r < repeat; r++) {
        sum_1 = 0.0;
        for (int i = 0; i < n; i++) sum_1 += a[i]; 
    }
    timer.stop();
    time_1way = timer.getElapsedMs() / repeat;

    timer.start();
    for (int r = 0; r < repeat; r++) {
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; i += 2) {
            s1 += a[i]; s2 += a[i + 1];   
        }
        sum_2 = s1 + s2; 
    }
    timer.stop();
    time_2way = timer.getElapsedMs() / repeat;

    timer.start();
    for (int r = 0; r < repeat; r++) {
        double s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;
        for (int i = 0; i < n; i += 4) {
            s1 += a[i]; s2 += a[i + 1];   
            s3 += a[i + 2]; s4 += a[i + 3];
        }
        sum_4 = s1 + s2 + s3 + s4; 
    }
    timer.stop();
    time_4way = timer.getElapsedMs() / repeat;

    timer.start();
    for (int r = 0; r < repeat; r++) {
        double s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, s7 = 0, s8 = 0;
        for (int i = 0; i < n; i += 8) {
            s1 += a[i];     s2 += a[i + 1];   
            s3 += a[i + 2]; s4 += a[i + 3];
            s5 += a[i + 4]; s6 += a[i + 5];
            s7 += a[i + 6]; s8 += a[i + 7];
        }
        sum_8 = s1 + s2 + s3 + s4 + s5 + s6 + s7 + s8; 
    }
    timer.stop();
    time_8way = timer.getElapsedMs() / repeat;

    cout << "  数组规模 n = " << n << " | 重复: " << repeat << endl;
    cout << "    -> 单路耗时 : " << time_1way << " ms" << endl;
    cout << "    -> 双路耗时 : " << time_2way << " ms" << endl;
    cout << "    -> 四路耗时 : " << time_4way << " ms" << endl;
    cout << "    -> 八路耗时 : " << time_8way << " ms\n" << endl;
    
    csv_file << n << "," << time_1way << "," << time_2way << "," << time_4way << "," << time_8way << "\n";

    volatile double sink_result = sum_1 + sum_2 + sum_4 + sum_8;
    if (sink_result < 0.0) cout << "Error" << endl; 
}

int main() {
    ofstream matrix_csv("matrix_results.csv");

    matrix_csv << "N,Trivial_Time_ms,Optimized_Time_ms,Speedup\n"; 

    cout << "===== 开始实验一：n*n 矩阵与向量内积 =====" << endl;
    vector<int> matrix_n_values = {100, 500, 1000, 2000, 4000}; 
    for (int n : matrix_n_values) {
        int repeat = (n > 1000) ? 5 : 50; 
        test_matrix_vector(n, repeat, matrix_csv);
    }
    matrix_csv.close(); 

    ofstream sum_csv("sum_results.csv");

    sum_csv << "N,1_Way_ms,2_Way_ms,4_Way_ms,8_Way_ms\n";

    cout << "===== 开始实验二：n 个数求和 (超标量测试) =====" << endl;
    vector<int> sum_n_values = {1024, 65536, 1048576, 16777216}; 
    for (int n : sum_n_values) {
        int repeat = 10000000 / n; 
        if (repeat == 0) repeat = 1;
        test_sum(n, repeat, sum_csv);
    }
    sum_csv.close(); 

    cout << "\n测试完成！数据已成功保存到 matrix_results.csv 和 sum_results.csv 文件中。" << endl;
    return 0;
}