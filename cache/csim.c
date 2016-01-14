/*aa
 * Name: Xuan Li
 * ID: xuanli1
 * Date: 02/26/2014
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "cachelab.h"

typedef struct
{
	int valid;
	long long tag; 
} Line;

int parseOption(int argc, char* argv[], int* verbal, int* s, int* E, int* b,char** t)
{
/*
 *parseOption - retrieve command variables
 */
	int option;

	while((option = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
		switch(option)
		{
			case 'h':
				printf("help information");
				break;
			case 'v':
				*verbal = 1;
				break;
			case 's':
				*s = atoi(optarg);
				break;
			case 'E':
				*E = atoi(optarg);
				break;
			case 'b':
				*b = atoi(optarg);
				break;
			case 't':
				*t = optarg;
				break;
			default:
				printf("wrong variable");
				return -1;
		}
	}
	return 0;
}

int initCache(int S, int E, Line*** p_cache, int*** p_queue)
{
/*
 * initCache - create cache space 
 */
	int i, j;
	*p_cache = (Line**)malloc(sizeof(Line*) * S);

	if(*p_cache == NULL)
	{
		return -1;
	}

	(*p_cache)[0] = (Line*)malloc(sizeof(Line) * S * E);

	if((*p_cache)[0] == NULL)
	{
		return -1;
	}

	for(i = 1; i < S; i++)
	{
		(*p_cache)[i] = (*p_cache)[i-1] + E;
	}

	memset((*p_cache)[0], 0, S * E);

	/* initialize update for LRU*/
	*p_queue = (int**)malloc(sizeof(int*) * S);

	if(*p_queue == NULL)
	{
		return -1;
	}

	(*p_queue)[0] = (int*)malloc(sizeof(int) * S * E);

	if((*p_queue)[0] == NULL)
	{
		return -1;
	}

	for(i = 1; i < S; i++)
	{
		(*p_queue)[i] = (*p_queue)[i-1] + E;
	}

	for(i = 0; i < S; i++)
	{
		for(j = 0; j < E; j++)
		{
			(*p_queue)[i][j] = j;
		}
	}
	return 0;
}

int updateCache(int E, int*** p_queue, long long set, int pos)
{
/*
 * updateCache - update for LRU
 */
	int i = 0;

	for(i = 0; i < E; i++)
	{
		if((*p_queue)[set][i] > pos)
		{
			(*p_queue)[set][i]--;
		}

	}
	return 0;
}

int scanCache(int s, int E, Line*** p_cache, int*** p_queue, long long set, long long tag)
{
/*
 * scanCache - scan cache data
 */
	int i = 0;

        for(i = 0; i < E; i++)
	{
		if((*p_cache)[set][i].tag == tag && (*p_cache)[set][i].valid == 1)
		{
			updateCache(E, p_queue, set, (*p_queue)[set][i]);
			(*p_queue)[set][i] = (E - 1);
			return 1;
		}
	}
	return 0;
}

int loadCache(int E, Line*** p_cache, int*** p_queue, long long set,long long tag)
{
/*
 * loadCache - adjust and load data
 */
	int i = 0;
	int eviction = 0;

	for(i = 0; i < E; i++)
	{
		if((*p_queue)[set][i] == 0)
		{
			eviction = (*p_cache)[set][i].valid;
			(*p_cache)[set][i].valid = 1;
			(*p_cache)[set][i].tag = tag;
			updateCache(E, p_queue, set, (*p_queue)[set][i]);
			(*p_queue)[set][i] = (E - 1);
			return eviction;
		}
	}
	return -1;
}

int simuCache(int verbal, int s, int E, int b, char* trace, Line*** pCache,int*** pQueue, int* cHit, int* cMiss, int* cEviction)
{
/*
 * simuCache - simulate cache function
 */
	FILE* pFile = NULL;
	char ins = 0;
	long long addr = 0;
	int size = 0;
	long long set = 0, tag = 0;
	int hit, eviction = 0;

	pFile = fopen(trace,"r");

	if(pFile == NULL)
	{
		return -1;
	}

	while(fscanf(pFile, " %c %llx,%d", &ins, &addr, &size) != -1)
	{
		if(ins == 'I')
		{
			continue;

		}

		/* addr = [tag][set][block] */
		set = (addr >> b) & ~(-1 << s);
		tag = addr >> (s + b);

		/* scan cache to get data */
		if((hit = scanCache(s, E, pCache, pQueue, set, tag)) == -1)
		{
			return -1;

		}

		/* load data if missed */
		if(hit == 0)
		{
			if((eviction = loadCache(E, pCache, pQueue, set, tag)) == -1)
			{
				return -1;
			}
		}

		/* to sum */
		if(hit == 1)
		{
			(*cHit)++;
		}

		else
		{
			(*cMiss)++;

			if(eviction == 1)
          		{
				(*cEviction)++;

			}
		}

		if(ins == 'M')
		{
			(*cHit)++;
		}

		/* printed if -v is specified*/
		if(verbal == 1)
		{
			printf("%c %llx,%d %s %s %s\n", ins, addr, size,
					(hit ? "hit" : "miss"),
					(hit && eviction ? "eviction" : ""),
					(ins == 'M' ? "hit" : ""));
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{
	int word;
	int s = 0, E = 0, b = 0;
	char* trace = NULL;
	Line** cache = NULL;
	int** queue = NULL;
	int hit = 0, miss = 0, eviction = 0;

	if(parseOption(argc, argv, &word, &s, &E, &b, &trace) == -1)
	{
		return -1;
	}

	if(initCache(1 << s, E, &cache, &queue) == -1)
	{
		return -1;
	}

	if(simuCache(word, s, E, b, trace, &cache, &queue, &hit, &miss, &eviction) == -1)
	{
		return -1;
	}

	printSummary(hit, miss, eviction);
        return 0;
}

