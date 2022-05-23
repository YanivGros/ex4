//
// Created by qywoe on 16/05/2022.
//
#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cassert>

#define OFFSET_ONES ((1ul << OFFSET_WIDTH) - 1)

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

void getPhysicalAddress(uint64_t address, uint64_t *retAddress);

void updateCyclic(word_t cur_frame,
                  word_t page_path,
                  word_t page_in, word_t *max_cycle_dist_frame, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist,
                  int i);


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
    return 1;
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
    return 1;
}

/* find the physical address of a given logic address
 *
 * returns 1 on success.
 * returns 0 on failure
 *
 */




uint64_t calc_cycle_dist(word_t page_path, word_t page_in) {
    int dist = page_path - page_in;
    uint64_t abs_dist = dist > 0 ? dist : -dist;
    uint64_t res = abs_dist < (NUM_PAGES - abs_dist) ? abs_dist : NUM_PAGES - abs_dist;
    return res;
}

void traversing(word_t cur_frame, word_t father, uint64_t depth, uint64_t frame_size,
                word_t page_path,
                word_t page_in,
                word_t *max_frame,
                word_t *frame_all_zero,
                word_t *max_cycle_dist_frame, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist) {
    if (*max_frame < cur_frame) {
        *max_frame = cur_frame;
    }
    if (depth == TABLES_DEPTH) {
        return;
    }
    bool is_zero = true;
    page_path = page_path << frame_size;
    for (uint64_t i = 0; i < (1 << frame_size); i++) {
        page_path++;
        word_t son_frame;
        PMread(cur_frame * PAGE_SIZE + i, &son_frame);
        if (son_frame != 0) {
            is_zero = false;

            if (depth == TABLES_DEPTH - 1) {
                updateCyclic(cur_frame, page_path, page_in, max_cycle_dist_frame, max_cycle_dist_page, max_cycle_dist,
                             i);
            }
            traversing(son_frame, cur_frame, depth + 1, OFFSET_WIDTH,
                       page_path,
                       page_in,
                       max_frame,
                       frame_all_zero,
                       max_cycle_dist_frame, max_cycle_dist_page, max_cycle_dist);

        }
    }
    if (is_zero && (cur_frame != father)) {
        *frame_all_zero = cur_frame;
    }
}

void updateCyclic(word_t cur_frame,
                  word_t page_path,
                  word_t page_in, word_t *max_cycle_dist_frame, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist,
                  int i) {
    uint64_t cycle_dist = calc_cycle_dist(page_path >> OFFSET_WIDTH, page_in);
    if (cycle_dist > *max_cycle_dist) {
        *max_cycle_dist = cycle_dist;
        *max_cycle_dist_frame = cur_frame * PAGE_SIZE + i;
        *max_cycle_dist_page = page_path;
    }

}


bool check_if_table_of_pages(uint64_t depth) {
    assert(depth >= TABLES_DEPTH);
    return depth == TABLES_DEPTH - 1;
}

void calc_for_each_max_dist(word_t cur_frame, word_t page_path, uint64_t page_number,
                            word_t *max_cycle_dist_page, word_t *max_cycle_dist_frame, uint64_t *max_cycle_dist) {
    for (int i = 0; i < PAGE_SIZE; ++i) {
        word_t cur_page;
        auto cur_adr = (word_t) (cur_frame * PAGE_SIZE + i);
        PMread(cur_adr, &cur_page);
        if (cur_page != 0) {
            auto res = calc_cycle_dist(page_path, page_number);
            if (res > *max_cycle_dist) {
                *max_cycle_dist_frame = cur_adr;
                *max_cycle_dist_page = page_path;
                *max_cycle_dist = res;
            }
        }
    }
}


void search_tree(word_t father_frame, word_t origin_father_frame, uint64_t depth,
                 word_t page_path,
                 word_t page_number,
                 word_t *max_frame,
                 word_t *frame_all_zero,
                 word_t *max_cycle_dist_frame, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist,
                 word_t *father_max_frame, word_t *father_max_cyc) {
    for (int i = 0; i < OFFSET_WIDTH; ++i) {
        word_t son_frame;
        auto cur_adr = (word_t) (father_frame * PAGE_SIZE + i);
        PMread(cur_adr, &son_frame);
        if (son_frame != 0) {
            if (check_if_table_of_pages(depth)) {
                auto cyc_dist = calc_cycle_dist((page_path << OFFSET_WIDTH) + i, page_number);
                if (cyc_dist > *max_cycle_dist) {
                    *max_cycle_dist = cyc_dist;
                    *max_cycle_dist_frame = cur_adr;
                    *max_cycle_dist_page = (page_path << OFFSET_WIDTH) + i;
                    *father_max_cyc = father_frame;
                }
            } else {
                search_tree(son_frame, origin_father_frame, depth, page_path, page_number, max_frame,
                            frame_all_zero, max_cycle_dist_frame, max_cycle_dist_page, max_cycle_dist,
                            father_max_frame, father_max_cyc);
            }
            if (son_frame > *max_frame) {
                *max_frame = son_frame;
                *father_max_frame = father_frame;
            }
        }
    }


    if (check_if_table_of_pages(depth)) {
        calc_for_each_max_dist(father_frame, page_path, page_number, max_cycle_dist_page, max_cycle_dist_frame,
                               max_cycle_dist);
        return;
    }
    uint64_t son_adr;
    word_t son_frame;
    bool is_all_zero = true;
    for (int i = 0; i < PAGE_SIZE; ++i) {
        son_adr = father_frame * PAGE_SIZE + i;
        page_path = (page_path << OFFSET_WIDTH) + i;
        PMread(son_adr, &son_frame);
        if (son_adr != 0) {
            is_all_zero = false;
            search_tree(son_frame, origin_father_frame, depth + 1, page_path, page_number, max_frame,
                        frame_all_zero,
                        max_cycle_dist_frame, max_cycle_dist_page, max_cycle_dist);
        }
    }
    if (is_all_zero && father_frame) {
        *frame_all_zero = father_frame
    }
}

void find_new_frame(word_t father_frame, uint64_t page_number) {
    word_t max_frame_number, max_page_cyclic_distance, zero_frame;
    search_tree);

}

word_t handle_zero(word_t father_frame, uint64_t son_adr, uint64_t page_number) {
    find_new_frame(father_frame, page_number);

}

void getPhysicalAddress_2(uint64_t address_page, uint64_t *retAddress) {
    word_t father_frame = 0, son_frame;
    uint64_t son_adr;
    uint64_t page_number;
    for (uint64_t i = 0; i < TABLES_DEPTH; ++i) {
        son_adr = (father_frame * PAGE_SIZE) + ((address_page >> (OFFSET_WIDTH * (TABLES_DEPTH - i))) & OFFSET_ONES);
        PMread(son_adr, &son_frame);
        if (son_frame == 0) {
            page_number = address_page >> OFFSET_ONES;
            son_frame = handle_zero(father_frame, son_adr, page_number);
        }
    }
}