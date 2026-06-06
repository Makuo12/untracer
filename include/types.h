#ifndef TYPES_H_
#define TYPES_H_
using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;
using u16 = unsigned short;

#define unlikely(x) \
    __builtin_expect(!!(x), 0)
#define MEM_BARRIER() \
    __asm__ volatile("" ::: "memory")
#endif