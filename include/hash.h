#ifndef HASH_H
#define HASH_H

#include <type.h>

static inline uint32_t hash(uint32_t key, uint32_t hash_key)
{
    return key%hash_key;
}

#endif /** HASH_H */
