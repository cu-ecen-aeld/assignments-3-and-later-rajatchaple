/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#include <stdlib.h>

#ifdef SYSLOG
#define LOG_DBG(...) syslog(LOG_DEBUG, __VA_ARGS__)
#define LOG_ERROR(...) syslog(LOG_ERR, __VA_ARGS__)
#elif defined(NO_DEBUG)
#define LOG_DBG(...)
#define LOG_ERROR(...)
#else
#define LOG_DBG(...) printf(__VA_ARGS__)
#define LOG_ERROR(...) printf(__VA_ARGS__)
#endif
#endif

#include "aesd-circular-buffer.h"



/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer. 
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
			size_t char_offset, size_t *entry_offset_byte_rtn )
{
    struct aesd_buffer_entry* entryptr = NULL;
    
    uint8_t index;
    long cumulative_size = -1;
    long prev_cumulative_size = 0;

    uint8_t entry_counter;


    LOG_DBG("out %d in %d", buffer->out_offs, buffer->in_offs);
    for(index=buffer->out_offs, entryptr=&((buffer)->entry[index]), entry_counter = 0; \
			entry_counter < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; \
			entry_counter++, index = (index + 1)%AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED , entryptr=&((buffer)->entry[index]))
    {
        //LOG_DBG("%s, size %ld, ", entryptr->buffptr, entryptr->size);

        cumulative_size += (entryptr->size);
        LOG_DBG("%d) %s", index, entryptr->buffptr);

        if((char_offset <= cumulative_size))
            {
                // LOG_DBG("%s, size %ld, ", entryptr->buffptr, entryptr->size);
                // LOG_DBG("(char_offset:%ld : cumulative_size%ld, prev_cumulative_size : %ld\n", (char_offset), cumulative_size, prev_cumulative_size);
                
                *entry_offset_byte_rtn = char_offset - prev_cumulative_size;
                return entryptr;
            }
        prev_cumulative_size = cumulative_size + 1;
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    struct aesd_buffer_entry* aesd_buff_entry = (struct aesd_buffer_entry*) malloc(sizeof(struct aesd_buffer_entry));
    
    memcpy(aesd_buff_entry, add_entry, sizeof(*add_entry));

    if(buffer == NULL)
    {
        return;
    }
        
    buffer->entry[buffer->in_offs].buffptr = aesd_buff_entry->buffptr;
    buffer->entry[buffer->in_offs].size = aesd_buff_entry->size;
    if(buffer->full == true)
    {
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    buffer->full = (buffer->in_offs == buffer->out_offs);
    LOG_DBG("out %d in %d full %d", buffer->out_offs, buffer->in_offs, buffer->full);
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}



