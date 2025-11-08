#include <cstdint>
#include <iostream>

struct PageTableEntry {
    bool valid = false;
    uint32_t frame = 0;
};

struct PageDirectoryEntry {
    bool present = false;
    PageTableEntry* pt = nullptr;
};

// Global page directory (1K entries)
PageDirectoryEntry page_directory[1024];

uint32_t translate_2level(uint32_t va) {
    uint32_t pd_index = (va >> 22) & 0x3FF;  // top 10 bits
    uint32_t pt_index = (va >> 12) & 0x3FF;  // next 10 bits
    uint32_t offset   = va & 0xFFF;          // bottom 12 bits

    PageDirectoryEntry& pd = page_directory[pd_index];
    if (!pd.present || pd.pt == nullptr) {
        std::cerr << "Page directory entry not present!\n";
        return 0;
    }

    PageTableEntry& pt = pd.pt[pt_index];
    if (!pt.valid) {
        std::cerr << "Page table entry invalid!\n";
        return 0;
    }

    return (pt.frame << 12) | offset;  // combine frame + offset
}

int main() {
    // Example setup
    page_directory[1].present = true;
    page_directory[1].pt = new PageTableEntry[1024];
    page_directory[1].pt[2].valid = true;
    page_directory[1].pt[2].frame = 0x12345;

    uint32_t va = (1 << 22) | (2 << 12) | 0xABC;
    uint32_t pa = translate_2level(va);

    std::cout << "VA: 0x" << std::hex << va
              << " -> PA: 0x" << pa << std::endl;
    return 0;
}
