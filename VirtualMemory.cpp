//
// Created by qywoe on 16/05/2022.
//
#include "VirtualMemory.h"
#include "PhysicalMemory.h"

/*
 * return the bits from begin (include) to end (not including)
 * starting to count from bit zero
 */
uint64_t get_bytes(uint64_t number, uint32_t begin, uint32_t end) {
    uint64_t res = number >> begin;
    uint64_t ones = 1ul << (end - begin);
    ones--;
    res = res & ones;
    return res;
}

uint32_t get_base_width() {
    int ret = VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH;
    if (ret == 0) {
        return OFFSET_WIDTH;
    } else {
        return ret;
    }
}

// function add by us

uint64_t getPhysicalAddress(uint64_t address, uint64_t *retAddress);

uint64_t get_offset(uint64_t i, uint64_t address);

void clean_page(word_t adr);

bool find_unused(word_t *frame_adr);

/*
 * Initialize the virtual memory.
 */
void VMinitialize() {
    for (int i = 0; i < get_base_width(); ++i) {
        PMwrite(i, 0);
    }
}

/* Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t *value) {
    uint64_t physicalAddress;
    getPhysicalAddress(virtualAddress, &physicalAddress);
    PMread(physicalAddress, value);
}


/* Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value) {
    uint64_t physicalAddress;
    getPhysicalAddress(virtualAddress, &physicalAddress);
    PMwrite(physicalAddress, value);
    return 0;
}

/* find the physical address of a given logic address
 *
 * returns 1 on success.
 * returns 0 on failure
 *
 */
uint64_t getPhysicalAddress(uint64_t address, uint64_t *retAddress) {
    

    word_t adr = 0, n_adr;
    PMread(get_offset(0, address), &n_adr);
    for (uint64_t i = 0; i < TABLES_DEPTH - 1; ++i) {
        if (n_adr == 0) {
            if (find_unused(&n_adr)) {
//                error
            }
            PMrestore(n_adr, get_offset(i, address));// we get the page back
            clean_page(n_adr); // might should be if page
            PMwrite(adr * PAGE_SIZE + get_offset(i, address), n_adr);
            adr = n_adr;
        }
        PMread(adr, &n_adr);
        adr = n_adr;
    }
    return n_adr * PAGE_SIZE + get_offset(TABLES_DEPTH - 1, address);
}

bool find_unused(word_t *frame_adr) {
    uint64_t frame_all_zeros, max_in_use_frame,  max_cyclic_distance_page,max_cyclic_distance_frame;

    uint64_t res = traversing(&frame_all_zeros,
                              &max_in_use_frame,
                              &max_cyclic_distance_page,
                              &max_cyclic_distance_frame);
    if (frame_all_zeros) {
        return frame_all_zeros;
    } else if (max_in_use_frame < NUM_FRAMES) {
        return max_in_use_frame;
    } else {
        PMevict(max_cyclic_distance_frame, max_cyclic_distance_page);
        return max_cyclic_distance_frame;
    }
}

/* clean the page from adr to adr + OFFSET_WIDTH
 *
 */
void clean_page(word_t adr) {
    for (int i = 0; i < OFFSET_WIDTH; ++i) {
        PMwrite(adr + i, 0);
    }
}

/*
 *  return the ith offset of address
 */
uint64_t get_offset(uint64_t i, uint64_t address) {
    if (i == 0) {
        return get_bytes(address, 0, get_base_width());
    }
    return get_bytes(address, i * OFFSET_WIDTH, (i + 1) * OFFSET_WIDTH);
}


// wrap if in case of failure