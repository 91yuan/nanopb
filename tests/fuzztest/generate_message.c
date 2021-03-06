/* Generates a random, valid protobuf message. Useful to seed
 * external fuzzers such as afl-fuzz.
 */

#include <pb_encode.h>
#include <pb_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "alltypes_static.pb.h"

static uint64_t random_seed;

/* Uses xorshift64 here instead of rand() for both speed and
 * reproducibility across platforms. */
static uint32_t rand_word()
{
    random_seed ^= random_seed >> 12;
    random_seed ^= random_seed << 25;
    random_seed ^= random_seed >> 27;
    return random_seed * 2685821657736338717ULL;
}

/* Fills a buffer with random data. */
static void rand_fill(uint8_t *buf, size_t count)
{
    while (count--)
    {
        *buf++ = rand_word() & 0xff;
    }
}

/* Check that size/count fields do not exceed their max size.
 * Otherwise we would have to loop pretty long in generate_message().
 * Note that there may still be a few encoding errors from submessages.
 */
static void limit_sizes(alltypes_static_AllTypes *msg)
{
    pb_field_iter_t iter;
    pb_field_iter_begin(&iter, alltypes_static_AllTypes_fields, msg);
    while (pb_field_iter_next(&iter))
    {
        if (PB_LTYPE(iter.type) == PB_LTYPE_BYTES)
        {
            ((pb_bytes_array_t*)iter.pData)->size %= iter.data_size - PB_BYTES_ARRAY_T_ALLOCSIZE(0);
        }
        
        if (PB_HTYPE(iter.type) == PB_HTYPE_REPEATED)
        {
            *((pb_size_t*)iter.pSize) %= iter.array_size;
        }
        
        if (PB_HTYPE(iter.type) == PB_HTYPE_ONEOF)
        {
            /* Set the oneof to this message type with 50% chance. */
            if (rand_word() & 1)
            {
                *((pb_size_t*)iter.pSize) = iter.tag;
            }
        }
    }
}

static void generate_message()
{
    alltypes_static_AllTypes msg;
    uint8_t buf[4096];
    pb_ostream_t stream = {0};
    
    do {
        if (stream.errmsg)
            fprintf(stderr, "Encoder error: %s\n", stream.errmsg);
        
        stream = pb_ostream_from_buffer(buf, sizeof(buf));
        rand_fill((void*)&msg, sizeof(msg));
        limit_sizes(&msg);
    } while (!pb_encode(&stream, alltypes_static_AllTypes_fields, &msg));
    
    fwrite(buf, 1, stream.bytes_written, stdout);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: generate_message <seed>\n");
        return 1;
    }

    random_seed = atol(argv[1]);

    fprintf(stderr, "Random seed: %u\n", (unsigned)random_seed);
    
    generate_message();
    
    return 0;
}

