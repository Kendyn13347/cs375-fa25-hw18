#include <fstream>
#include <random>
#include <iostream>

using namespace std;

int main() {
    ofstream fout("addresses.txt");
    if (!fout) {
        cerr << "Error: Could not create addresses.txt\n";
        return 1;
    }

    mt19937 rng(42);
    uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    // Hot region: 100 pages (shows TLB benefit with locality)
    uniform_int_distribution<uint32_t> hot(0x70000000, 0x700FFFFF);

    for (int i = 0; i < 1000; i++) {
        // 70% hot region, 30% random (simulates locality)
        uint32_t va = (i < 700) ? hot(rng) : dist(rng);
        fout << hex << "0x" << va << "\n";
    }

    fout.close();
    cout << "Generated 1000 addresses in addresses.txt\n";
    return 0;
}