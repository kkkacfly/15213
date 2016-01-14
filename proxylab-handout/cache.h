/* 
 * this file defines some cache method and variable 
 */

#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* cache structure contains cache information */
typedef struct CACHE {
    struct CACHE_B *head;
    unsigned cache_size;
    unsigned block_cnt;
    sem_t mutex;
} CACHE;

/* Defination of cache block */
typedef struct CACHE_B {
    unsigned int size;
    char *id;
    char *data;
    struct CACHE_B *next;
    struct CACHE_B *prev;
} CACHE_B;

/* methods related to cache and used in proxy.c */
CACHE *cache_init();
CACHE_B *cache_get(CACHE *cache, char *uri);
int cache_check(CACHE *cache, char *uri);
void cache_update(CACHE *cache, char *uri, char *data, unsigned size);

