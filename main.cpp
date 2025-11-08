#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <chrono>
#include <random>

using namespace std;
using namespace std::chrono;

// === Configuration ===
const int PAGE_SIZE = 4096;
const int PAGE_OFFSET_BITS = 12;
const int VPN_BITS = 20;
const int L1_BITS = 10, L2_BITS = 10;
const int PD_ENTRIES = 1 << L1_BITS;  // 1024
const int PT_ENTRIES = 1 << L2_BITS;  // 1024

// === Page Table Structures ===
struct PageTableEntry {
    bool valid = false;
    uint32_t frame = 0;
};

struct PageDirectoryEntry {
    bool present = false;
    PageTableEntry* pt = nullptr;
    
    PageDirectoryEntry() : present(false), pt(nullptr) {}
    
    ~PageDirectoryEntry() { 
        if (pt) delete[] pt; 
    }
};

PageDirectoryEntry page_directory[PD_ENTRIES];

// === Physical Memory Simulation ===
const int NUM_FRAMES = 256;
bool frame_allocated[NUM_FRAMES] = {false};
vector<uint32_t> allocated_frames;

// === TLB Implementation ===
struct TLBEntry {
    uint32_t vpn = 0;
    uint32_t pfn = 0;
    uint64_t timestamp = 0;
    bool valid = false;
};

class TLB {
    TLBEntry entries[16];
    uint64_t clock = 0;
    int hits = 0;
    int misses = 0;

public:
    // Lookup VPN in TLB, return true if hit
    bool lookup(uint32_t vpn, uint32_t& pfn) {
        for (auto& e : entries) {
            if (e.valid && e.vpn == vpn) {
                e.timestamp = ++clock;  // Update for LRU
                pfn = e.pfn;
                hits++;
                return true;
            }
        }
        misses++;
        return false;
    }

    // Insert new mapping using LRU replacement
    void insert(uint32_t vpn, uint32_t pfn) {
        // Find LRU entry (oldest timestamp)
        int lru_idx = 0;
        uint64_t oldest = entries[0].timestamp;
        
        for (int i = 1; i < 16; i++) {
            if (!entries[i].valid) {
                // Found empty slot
                lru_idx = i;
                break;
            }
            if (entries[i].timestamp < oldest) {
                oldest = entries[i].timestamp;
                lru_idx = i;
            }
        }
        
        entries[lru_idx] = {vpn, pfn, ++clock, true};
    }

    void print_stats() {
        int total = hits + misses;
        cout << "TLB Hits: " << hits << ", Misses: " << misses;
        if (total > 0) {
            cout << ", Hit Rate: " << fixed << setprecision(2)
                 << (hits * 100.0 / total) << "%\n";
        } else {
            cout << "\n";
        }
    }

    void reset_stats() {
        hits = 0;
        misses = 0;
    }
};

TLB tlb;

// === Helper Functions ===
// Extract L1 index (bits 31-22)
uint32_t get_l1_index(uint32_t va) { 
    return (va >> 22) & 0x3FF; 
}

// Extract L2 index (bits 21-12)
uint32_t get_l2_index(uint32_t va) { 
    return (va >> 12) & 0x3FF; 
}

// Extract page offset (bits 11-0)
uint32_t get_offset(uint32_t va) { 
    return va & 0xFFF; 
}

// Extract full VPN (bits 31-12)
uint32_t get_vpn(uint32_t va) { 
    return va >> 12; 
}

// === Physical Frame Allocation ===
uint32_t allocate_frame() {
    static mt19937 rng(random_device{}());
    
    // If frames available, find free one
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!frame_allocated[i]) {
            frame_allocated[i] = true;
            allocated_frames.push_back(i);
            return i;
        }
    }
    
    // No free frames - random replacement
    int victim_idx = rng() % allocated_frames.size();
    uint32_t victim_frame = allocated_frames[victim_idx];
    
    return victim_frame;
}

// === Translation: 2-Level Page Table + TLB + Page Faults ===
bool translate(uint32_t va, uint32_t& pa, bool& fault) {
    uint32_t vpn = get_vpn(va);
    uint32_t pfn;

    // Step 1: TLB lookup
    if (tlb.lookup(vpn, pfn)) {
        pa = (pfn << 12) | get_offset(va);
        fault = false;
        return true;
    }

    // Step 2: Page table walk (2-level)
    uint32_t l1 = get_l1_index(va);
    uint32_t l2 = get_l2_index(va);

    // Check if page directory entry exists
    if (!page_directory[l1].present) {
        page_directory[l1].present = true;
        page_directory[l1].pt = new PageTableEntry[PT_ENTRIES];
    }

    PageTableEntry& pte = page_directory[l1].pt[l2];

    // Step 3: Check if page is valid (page fault handling)
    if (!pte.valid) {
        fault = true;
        cout << "PAGE FAULT: VA 0x" << hex << setw(8) << setfill('0') 
             << va << " → allocating frame\n";
        
        pfn = allocate_frame();
        pte.valid = true;
        pte.frame = pfn;
    } else {
        pfn = pte.frame;
        fault = false;
    }

    // Step 4: Update TLB
    tlb.insert(vpn, pfn);

    // Step 5: Construct physical address
    pa = (pfn << 12) | get_offset(va);
    return true;
}

// === Main Program ===
int main() {
    vector<uint32_t> addresses;
    ifstream fin("addresses.txt");
    
    if (!fin) {
        cerr << "Error: Could not open addresses.txt\n";
        cerr << "Please run: make generate && ./generate\n";
        return 1;
    }

    // Read virtual addresses from file
    uint32_t addr;
    while (fin >> hex >> addr) {
        addresses.push_back(addr);
    }
    fin.close();

    if (addresses.empty()) {
        cerr << "Error: No addresses found in addresses.txt\n";
        return 1;
    }

    cout << "=======================================================\n";
    cout << "  TLB and Page Table Translation Simulator\n";
    cout << "=======================================================\n";
    cout << "Configuration:\n";
    cout << "  - 2-Level Page Table (10-bit L1, 10-bit L2, 12-bit offset)\n";
    cout << "  - TLB: 16 entries, fully associative, LRU replacement\n";
    cout << "  - Physical Memory: " << NUM_FRAMES << " frames\n";
    cout << "  - Total Addresses: " << addresses.size() << "\n";
    cout << "=======================================================\n\n";

    auto start = high_resolution_clock::now();

    int faults = 0;
    int count = 0;

    for (uint32_t va : addresses) {
        uint32_t pa;
        bool fault = false;
        translate(va, pa, fault);
        
        if (fault) faults++;

        // Print first 10 and last 10 translations
        if (count < 10 || count >= (int)addresses.size() - 10) {
            cout << "VA: 0x" << hex << setw(8) << setfill('0') << va
                 << " → PA: 0x" << setw(8) << setfill('0') << pa;
            if (fault) {
                cout << " [FAULT]";
            }
            cout << "\n";
        } else if (count == 10) {
            cout << "... (translations " << dec << 11 << " to " 
                 << addresses.size() - 10 << " omitted) ...\n";
        }
        
        count++;
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    // Summary statistics
    cout << "\n=======================================================\n";
    cout << "  SUMMARY STATISTICS\n";
    cout << "=======================================================\n";
    cout << "Total Addresses Translated: " << dec << addresses.size() << "\n";
    cout << "Page Faults: " << faults << "\n";
    cout << "Translation Time: " << duration.count() << " μs\n";
    cout << "Avg Time per Translation: " << fixed << setprecision(3)
         << (duration.count() / (double)addresses.size()) << " μs\n";
    tlb.print_stats();
    cout << "=======================================================\n";

    return 0;
}