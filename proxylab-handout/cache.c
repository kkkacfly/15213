/*
 * cache for proxy.c
 *
 * In this cache I implement a linked list based cache to cache 
 * web content using LRU policy. In each cache block there is a 
 * request header, block size, data and pointer to the previous 
 * and next block. The hit cache would be moved to head whenever
 * cache hit happens. P-V commends are implemented to make it 
 * thread-safe.
 *
 * Name: Xuan Li
 * ID: xuanli1
 * Date: 04/25/2015
 */

#include "csapp.h"
#include "cache.h"

/* create and initial a new cache */
CACHE *cache_init() {
    CACHE *cache = Malloc (sizeof(CACHE));
    CACHE_B *extra_header = (CACHE_B *)malloc(sizeof(CACHE_B));
    cache->head = extra_header;
    cache->head->next = NULL;
    cache->cache_size = 0;
    cache->block_cnt = 0;
    sem_init(&cache->mutex, 0, 1);
    return cache;
}

/* create a new block given required id, data and size */
CACHE_B *create_block(char *id, char *data, unsigned int size) {
    CACHE_B *temp = (CACHE_B *)malloc(sizeof(CACHE_B));
    temp->id = (char *)malloc(strlen(id) + 1);
    strcpy(temp->id, id);
    temp->data = (char *)malloc(size);
    memcpy(temp->data, data, size);
    temp->size = size;
    temp->prev = NULL;
    temp->next = NULL;
    return temp;
}

/* help function for updating the hit block to the head of the list */
void insert_cache_after_head(CACHE *cache, CACHE_B *block) {
    if (cache->head->next == NULL) {
        cache->head->next = block;
        block->prev = cache->head;
        block->next = NULL;
    }
    else {
        block->next = cache->head->next;
        cache->head->next->prev = block;
        cache->head->next = block;
        block->prev = cache->head;
    }
    cache->cache_size += block->size;
    cache->block_cnt++;
}

/* used to leave the hit cache empty */
void clear_cache(CACHE *cache, CACHE_B *block) {
    if (block->next == NULL) {
        CACHE_B *temp = block->prev;
        temp->next = NULL;
    }
    else {
        CACHE_B *temp = block->prev;
        temp->next = block->next;
        block->next->prev = temp;
    }
    cache->cache_size -= block->size;
    cache->block_cnt--;
}

/* maintain the size of cache within the required MAX_CACHE_SIZE */
void cache_control(CACHE *cache, int exp_size) {
    P(&cache->mutex);
    while (cache->cache_size > exp_size) {
        CACHE_B *end = cache->head;
        while (end->next != NULL) {
            end = end->next;
        }
        CACHE_B *end_prev = end->prev;
        end_prev->next = NULL;
        cache->cache_size -= end->size;
        cache->block_cnt --;
        Free(end);
    }
    V(&cache->mutex);
    return;
}

/* help function to update the cache */
void cache_move_to_head(CACHE *cache, CACHE_B *block) {
    P(&cache->mutex);
    clear_cache(cache, block);
    insert_cache_after_head(cache, block);
    V(&cache->mutex);
}

/* update the linked list when given a new uri */
void cache_update(CACHE *cache, char *uri, char *data, unsigned size) {
    P(&cache->mutex);
    if (size > MAX_OBJECT_SIZE) {
	 return;
    }
    if (size + cache->cache_size > MAX_CACHE_SIZE) {
        cache_control(cache, MAX_CACHE_SIZE - size);
    }
    CACHE_B *new_block = create_block(uri, data, size);
    insert_cache_after_head(cache, new_block);
    V(&cache->mutex);
    return;
}
 
/* to check if a given uri has been cached or not */
int cache_check(CACHE *cache, char *uri) {
    CACHE_B *ptr = cache->head->next;
    while (ptr) {
        if (!strcmp(uri, ptr->id)) {
            return 1;
        }
        ptr = ptr->next;
    }
    return 0;
}

/* fetch the recently used block when given uri */
CACHE_B *cache_get(CACHE *cache, char *uri) {
    CACHE_B *ptr = cache->head->next;
    while (ptr) {
        if (!strcmp(uri, ptr->id)) {
            cache_move_to_head(cache, ptr);
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

