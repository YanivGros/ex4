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

void getPhysicalAddress(uint64_t address, uint64_t *retAddress);
void updateCyclic(word_t cur_frame,
                  word_t page_path,
                  word_t page_in, word_t *max_cycle_dist_frame, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist, int i);


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
        page_path ++;
        word_t son_frame;
        PMread(cur_frame * PAGE_SIZE + i, &son_frame);
        if (son_frame != 0) {
            is_zero = false;

            if (depth == TABLES_DEPTH - 1){
                updateCyclic(cur_frame, page_path, page_in, max_cycle_dist_frame, max_cycle_dist_page, max_cycle_dist, i);
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
                  word_t page_in, word_t *max_cycle_dist_frame, word_t *max_cycle_dist_page, uint64_t *max_cycle_dist, int i){
    uint64_t cycle_dist = calc_cycle_dist(page_path >>OFFSET_WIDTH , page_in);
    if (cycle_dist > *max_cycle_dist) {
        *max_cycle_dist = cycle_dist;
        *max_cycle_dist_frame = cur_frame * PAGE_SIZE + i;
        *max_cycle_dist_page = page_path;
    }

}



void initialiseFrame(word_t frame) {
    for (int i = 0; i < PAGE_SIZE; i++){
        PMwrite(frame * PAGE_SIZE + i, 0);
    }

}

void find_new_frame(uint64_t cur_adr, uint64_t address, word_t frame_adr) {
    word_t max_frame = 0, all_zero = 0, max_cycle_dist_frame = 0, max_cycle_dist_page = 0;
    uint64_t max_cycle_dist = 0;
    traversing(0, frame_adr, 0,
               get_base_width(),
               0, address >> OFFSET_WIDTH,
               &max_frame,
               &all_zero,
               &max_cycle_dist_frame, &max_cycle_dist_page, &max_cycle_dist);
    if (all_zero) { //case

        PMwrite(cur_adr, all_zero);
        return;
    }
    if (max_frame + 1 < NUM_FRAMES) { //case2

        PMwrite(cur_adr, max_frame + 1);
        initialiseFrame(max_frame + 1);
        return;
    }
    // case 3
    PMevict(max_cycle_dist_frame, max_cycle_dist_page);
    PMwrite(cur_adr, max_cycle_dist_frame);
    initialiseFrame(max_cycle_dist_frame);

}

void getPhysicalAddress(uint64_t address, uint64_t *retAddress) {
//  101 0001 0110
    word_t frame_adr = 0, new_frame_adr;
    uint64_t start = VIRTUAL_ADDRESS_WIDTH - get_base_width(), end = VIRTUAL_ADDRESS_WIDTH;
    uint64_t offset = get_bytes(address, start, end);
    uint64_t cur_adr = frame_adr * PAGE_SIZE + offset;
    PMread(cur_adr, &new_frame_adr);
    for (int i = 0; i < TABLES_DEPTH; ++i) {
        if (new_frame_adr == 0) {
            find_new_frame(cur_adr, address, frame_adr);
            PMread(cur_adr, &new_frame_adr);
            if (i == TABLES_DEPTH - 1) {
                PMrestore(new_frame_adr, address >> OFFSET_WIDTH);
            }
        }

        frame_adr = new_frame_adr;
        end = start;
        start = start - OFFSET_WIDTH;
        offset = get_bytes(address, start, end);
        cur_adr = frame_adr * PAGE_SIZE + offset;
        PMread(cur_adr, &new_frame_adr);
    }
    *retAddress = (frame_adr << OFFSET_WIDTH) + offset;
}



//    address = 983040;
//    1111 0000 0000 0000 0000
//    uint32_t res_1 = get_base_width();
//    uint32_t  res_2 = VIRTUAL_ADDRESS_WIDTH - get_base_width();
//    uint32_t res_3 = read_MSB(address, 0, get_base_width());

//    address = 983040;
//    1111 0000 0000 0000 0000
//    uint32_t res_1 = get_base_width();
//    uint32_t  res_2 = VIRTUAL_ADDRESS_WIDTH - get_base_width();
//    uint32_t res_3 = get_bytes(address, VIRTUAL_ADDRESS_WIDTH - get_base_width(), VIRTUAL_ADDRESS_WIDTH);
