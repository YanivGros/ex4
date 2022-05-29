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

uint32_t get_base_width() {
    int ret = VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH;
    if (ret == 0) {
        return OFFSET_WIDTH;
    } else {
        return ret;
    }
}

// function add by us

int getPhysicalAddress(uint64_t address);


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
    if (virtualAddress < 0 || (virtualAddress >> VIRTUAL_ADDRESS_WIDTH) > 0) {
        return 0;
    }
    word_t physicalAddress = getPhysicalAddress(virtualAddress);
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
    if (virtualAddress
            < 0 || (virtualAddress >> VIRTUAL_ADDRESS_WIDTH) > 0) {
        return 0;
    }
    word_t physicalAddress = getPhysicalAddress(virtualAddress);
    PMwrite(physicalAddress, value);
    return 1;
}

/* find the physical address of a given logic address
 *
 * returns 1 on success.
 * returns 0 on failure
 *
 */

void dfsZero(word_t cur_frame, word_t father_cur_adr, word_t original_father, uint64_t depth, word_t *zero_frame) {
    if (depth == TABLES_DEPTH) {
        return;
    }
    bool is_zero = true;
    for (int i = 0; i < PAGE_SIZE; ++i) {
        uint64_t son_adr = cur_frame * PAGE_SIZE + i;
        word_t son_frame;
        PMread(son_adr, &son_frame);
        if (son_frame != 0) {
            is_zero = false;
            dfsZero(son_frame, (word_t) son_adr, original_father, depth + 1, zero_frame);
        }
        if (*zero_frame != 0) {
            return;
        }
    }
    if (is_zero && cur_frame != original_father) {
        *zero_frame = cur_frame;
        PMwrite(father_cur_adr, 0);
    }
}
void dfsFindMaxFrame(word_t cur_frame, word_t* max_frame, uint64_t depth){
    *max_frame = (*max_frame) < cur_frame?cur_frame:(*max_frame);
    if (depth == TABLES_DEPTH) {
        return;
    }
    for (int i = 0; i < PAGE_SIZE; ++i) {
        uint64_t son_adr = cur_frame * PAGE_SIZE + i;
        word_t son_frame;
        PMread(son_adr, &son_frame);
        dfsFindMaxFrame(son_frame, max_frame,depth + 1);
    }
}
uint64_t calc_cycle_dist(word_t page_path, word_t page_in) {
    int dist = page_path - page_in;
    uint64_t abs_dist = dist > 0 ? dist : -dist;
    uint64_t res = abs_dist < (NUM_PAGES - abs_dist) ? abs_dist : NUM_PAGES - abs_dist;
    return res;
}

void dfsFindMaxCycle(word_t cur_frame, word_t father_cur_adr, uint64_t depth, word_t path,word_t page_in_number,
                     word_t *max_cycle_dist_adr, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist) {
    if (depth == TABLES_DEPTH) {
        uint64_t res = calc_cycle_dist(path, page_in_number);
        if (res > *max_cycle_dist) {
            *max_cycle_dist = res;
            *max_cycle_dist_adr = father_cur_adr;
            *max_cycle_dist_page = path;
            
        }
        return;
    }
    for (int i = 0; i < PAGE_SIZE; ++i) {
        uint64_t son_adr = cur_frame * PAGE_SIZE + i;
        word_t son_frame;
        PMread(son_adr, &son_frame);
        if (son_frame != 0) {
            dfsFindMaxCycle(son_frame, (word_t) son_adr, depth + 1, (path << OFFSET_WIDTH) + i, page_in_number,
                            max_cycle_dist_adr, max_cycle_dist_page, max_cycle_dist);
        }
    }
}
word_t find_new_frame(word_t father_frame, uint64_t page_number ) {
    word_t zero_frame = 0;
    dfsZero(0, 0, father_frame, 0, &zero_frame);
    if (zero_frame > 0) {
        return zero_frame;
    }
    
    word_t max_frame = 0;
    dfsFindMaxFrame(0, &max_frame, 0);
    if (max_frame + 1  < NUM_FRAMES) {
        return max_frame + 1 ;
    }
    word_t max_cycle_dist_adr = 0;
    word_t max_cycle_dist_page = 0;
    uint64_t max_cycle_dist = 0;
    dfsFindMaxCycle(0, 0, 0, 0, (word_t) page_number, &max_cycle_dist_adr, &max_cycle_dist_page, &max_cycle_dist);
    word_t frame_to_evict;
    PMread(max_cycle_dist_adr, &frame_to_evict);
    PMevict(frame_to_evict, max_cycle_dist_page);
    PMwrite(max_cycle_dist_adr, 0);
    return frame_to_evict;
}

void initialiseFrame(word_t frame) {
    for (int i = 0; i < PAGE_SIZE; i++){
        PMwrite(frame * PAGE_SIZE + i, 0);
    }
}

word_t handleZero(word_t father_frame, uint64_t son_adr, uint64_t page_number,bool is_table) {
    word_t new_frame = find_new_frame(father_frame, page_number);
    if (is_table) {
        initialiseFrame(new_frame);
    } else {
        PMrestore(new_frame, page_number);
    }
    PMwrite(son_adr, new_frame);
    return new_frame;
}


word_t getPhysicalAddress(uint64_t address_page) {
    word_t father_frame = 0, son_frame = 0;
    uint64_t son_adr = 0 ;
    uint64_t page_number = 0 ;
    for (uint64_t i = 0; i < TABLES_DEPTH; ++i) {
        son_adr = (father_frame * PAGE_SIZE) + ((address_page >> (OFFSET_WIDTH * (TABLES_DEPTH - i))) & OFFSET_ONES);
        PMread(son_adr, &son_frame);
        if (son_frame == 0) {
            page_number = address_page >> OFFSET_WIDTH;
            son_frame = handleZero(father_frame, son_adr, page_number, i < TABLES_DEPTH - 1);
        }
        father_frame = son_frame;
    }
    word_t ret_adr = ((word_t)son_frame) * PAGE_SIZE + (OFFSET_ONES & address_page);
    return ret_adr;
}







//
//void traversing(word_t cur_frame, word_t father, uint64_t depth, uint64_t frame_size,
//                word_t page_path,
//                word_t page_in,
//                word_t *max_frame,
//                word_t *frame_all_zero,
//                word_t *max_cycle_dist_frame, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist) {
//    if (*max_frame < cur_frame) {
//        *max_frame = cur_frame;
//    }
//    if (depth == TABLES_DEPTH) {
//        return;
//    }
//    bool is_zero = true;
//    page_path = page_path << frame_size;
//    for (uint64_t i = 0; i < (1 << frame_size); i++) {
//        page_path++;
//        word_t son_frame;
//        PMread(cur_frame * PAGE_SIZE + i, &son_frame);
//        if (son_frame != 0) {
//            is_zero = false;
//
//            if (depth == TABLES_DEPTH - 1) {
//                updateCyclic(cur_frame, page_path, page_in, max_cycle_dist_frame, max_cycle_dist_page, max_cycle_dist,
//                             i);
//            }
//            traversing(son_frame, cur_frame, depth + 1, OFFSET_WIDTH,
//                       page_path,
//                       page_in,
//                       max_frame,
//                       frame_all_zero,
//                       max_cycle_dist_frame, max_cycle_dist_page, max_cycle_dist);
//
//        }
//    }
//    if (is_zero && (cur_frame != father)) {
//        *frame_all_zero = cur_frame;
//    }
//}
//
//void updateCyclic(word_t cur_frame,
//                  word_t page_path,
//                  word_t page_in, word_t *max_cycle_dist_frame, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist,
//                  int i) {
//    uint64_t cycle_dist = calc_cycle_dist(page_path >> OFFSET_WIDTH, page_in);
//    if (cycle_dist > *max_cycle_dist) {
//        *max_cycle_dist = cycle_dist;
//        *max_cycle_dist_frame = cur_frame * PAGE_SIZE + i;
//        *max_cycle_dist_page = page_path;
//    }
//
//}
//
//
//bool check_if_table_of_pages(uint64_t depth) {
//    assert(depth >= TABLES_DEPTH);
//    return depth == TABLES_DEPTH - 1;
//}
