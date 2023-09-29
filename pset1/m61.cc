#include "m61.hh"
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <unordered_map>
#include <list>
#include <iostream>
#include <sys/mman.h>


struct cell {
    size_t size;
    size_t pos;
    bool is_free;
};

struct m61_memory_buffer {
    char* buffer;
    size_t pos = 0;
    size_t size = 8 << 20; // 8 MiB
    std::unordered_map<char*, size_t> memory_map;
    std::list<cell> slots;
    m61_memory_buffer();
    ~m61_memory_buffer();
};

static m61_memory_buffer default_buffer;


m61_memory_buffer::m61_memory_buffer() {
    void* buf = mmap(nullptr, size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(buf != MAP_FAILED);
    buffer = static_cast<char*>(buf);
    slots.push_front({size, 0, true});
}

m61_memory_buffer::~m61_memory_buffer() {
    munmap(buffer, size);
}
static m61_statistics stats;

m61_statistics::m61_statistics() {
    memset(this, 0, sizeof(m61_statistics));
    this->heap_min = (uintptr_t) &default_buffer.buffer[0];
}

// Helper function to find free memory
std::list<cell>::iterator find_free_memory(size_t sz) {
    for (auto it = default_buffer.slots.begin(); it != default_buffer.slots.end(); ++it) {
        if (it->is_free && it->size >= sz) {
            return it;
        }
    }
    return default_buffer.slots.end();
}

// Implementation of m61_malloc
void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    auto cell_to_insert = find_free_memory(sz);
    if (cell_to_insert == default_buffer.slots.end()) {
        // No free memory, update stats and return nullptr
        stats.nfail++;
        stats.fail_size += sz;
        return nullptr;
    }

    void* ptr = &default_buffer.buffer[cell_to_insert->pos];
    size_t current_pos = cell_to_insert->pos;
    size_t remaining_size = cell_to_insert->size - sz;

    // Update metadata
    default_buffer.memory_map[static_cast<char*>(ptr)] = sz;
    default_buffer.slots.insert(cell_to_insert, {sz, current_pos, false});

    // If space left in the cell, mark it as free
    if (remaining_size > 0) {
        default_buffer.slots.insert(cell_to_insert, {remaining_size, current_pos + sz, true});
    }

    // Remove the old cell
    default_buffer.slots.erase(cell_to_insert);

    // Update statistics
    stats.heap_min = std::min(stats.heap_min, reinterpret_cast<uintptr_t>(ptr));
    stats.heap_max = std::max(stats.heap_max, reinterpret_cast<uintptr_t>(ptr) + sz - 1);
    stats.ntotal++;
    stats.nactive++;
    stats.active_size += sz;
    stats.total_size += sz;

    return ptr;
}
void m61_free(void* ptr, const char* file, int line) {
    if (!ptr) return;

    auto it = default_buffer.memory_map.find(static_cast<char*>(ptr));
    if (it == default_buffer.memory_map.end()) {
        fprintf(stderr, "MEMORY BUG: invalid free of pointer %p, not in heap\n%s:%d\n", ptr, file, line);
        exit(1);
    }

    size_t sz = it->second;
    stats.active_size -= sz;
    default_buffer.memory_map.erase(it);

    auto freed_slot_it = std::find_if(
        default_buffer.slots.begin(), 
        default_buffer.slots.end(),
        [&](const cell& slot) { 
            return slot.pos == reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(default_buffer.buffer); 
        });

    if (freed_slot_it != default_buffer.slots.end()) {
        freed_slot_it->is_free = true;
    }

    // Coalesce with the next free slot, if any
    auto next_slot = std::next(freed_slot_it);
    if (next_slot != default_buffer.slots.end() && next_slot->is_free) {
        freed_slot_it->size += next_slot->size;
        default_buffer.slots.erase(next_slot);
    }

    // Coalesce with the previous free slot, if any
    if (freed_slot_it != default_buffer.slots.begin()) {
        auto prev_slot = std::prev(freed_slot_it);
        if (prev_slot->is_free && prev_slot->pos + prev_slot->size == freed_slot_it->pos) {
            prev_slot->size += freed_slot_it->size;
            default_buffer.slots.erase(freed_slot_it);
        }
    }

    --stats.nactive;
}



void* m61_calloc(size_t count, size_t sz, const char* file, int line) {
    // Your code here (not needed for first tests).
   
    if (default_buffer.pos + (count * sz) > default_buffer.size || (sz != 0 && count > (size_t)-1 / sz )) {
        // Not enough space left in default buffer for allocation
        stats.nfail++;
        stats.fail_size += sz*count;
        return nullptr;
    }

    void* ptr = m61_malloc(count * sz, file, line);
    if (ptr) {
        memset(ptr, 0, count * sz);
    }
    return ptr;
}


/// m61_get_statistics()
///    Return the current memory statistics.

m61_statistics m61_get_statistics() {
    // Your code here.
    // The handout code sets all statistics to enormous numbers.
    
    return stats;
}


/// m61_print_statistics()
///    Prints the current memory statistics.

void m61_print_statistics() {
    m61_statistics current_stats = m61_get_statistics();
    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           current_stats.nactive, current_stats.ntotal, current_stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           current_stats.active_size, current_stats.total_size, current_stats.fail_size);
}


/// m61_print_leak_report()
///    Prints a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report() {
    // Your code here.
}
