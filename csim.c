////////////////////////////////////////////////////////////////////////////////
// Main File:        csim.c
// This File:        csim.c
// Other Files:      n/a
// Semester:         CS 354 Lecture 001      SPRING 2024
// Grade Group:      gg1  (See canvas.wisc.edu/groups for your gg#)
// Instructor:       deppeler
// 
// Author:           lilian huang
// Email:            llhuang@wisc.edu
// CS Login:         lilian
//
/////////////////////////// SEARCH LOG //////////////////////////////// 
// Online sources: do not rely web searches to solve your problems, 
// but if you do search for help on a topic, include Query and URLs here.
// IF YOU FIND CODED SOLUTIONS, IT IS ACADEMIC MISCONDUCT TO USE THEM
//                               (even for "just for reference")
// Date:   Query:                      URL:
// --------------------------------------------------------------------- 
// 
// 
// AI chats: save a transcript.  No need to submit, just have available 
// if asked.
/////////////////////////// COMMIT LOG  ////////////////////////////// 
//  Date and label your work sessions, include date and brief description
//  Date:   	Commit Message: 
//  -------------------------------------------------------------------
// 	3/20/24 - began to implement init_heap()
//  3/26/24 - completed init_heap()
//  3/29/24 - completed free_cache(), completed access_data()
//	4/01/24 - debug access_data() and rewrote verbose mode
//  4/02/24 - completed verbose mode, debugged, compelted csim.c
//
///////////////////////// OTHER SOURCES OF HELP ////////////////////////////// 
// Persons:          Identify persons by name, relationship to you, and email.
//                   Describe in detail the the ideas and help they provided.
// Date:   Name (email):   Helped with: (brief description)
// ---------------------------------------------------------------------------
// 
// 
//
//////////////////////////// 80 columns wide ///////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Copyright 2013,2019-2024
// Posting or sharing this file is prohibited, including any changes/additions.
// Used by permission for Spring 2024
////////////////////////////////////////////////////////////////////////////////

/**
 * csim.c:  
 * A cache simulator that can replay traces (from Valgrind) and output
 * statistics to determine the number of hits, misses, and evictions.
 * The replacement policy is ______________________________.
 *
 * Implementation and assumptions:
 *  1. (L) load or (S) store cause at most one cache miss and a possible eviction.
 *  2. (I) Instruction loads are ignored.
 *  3. (M) Data modify is treated as a load followed by a store to the same
 *  address. Hence, an (M) operation can result in two cache hits, 
 *  or a miss and a hit plus a possible eviction.
 */  

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

/******************************************************************************/
/* DO NOT MODIFY THESE VARIABLE NAMES and TYPES                               */
/* DO UPDATE THEIR VALUES AS NEEDED BY YOUR CSIM                              */

//Globals set by command line args.
int b = 0; //number of (b) bits
int s = 0; //number of (s) bits
int E = 0; //number of lines per set

//Globals derived from command line args.
int B; //block size in bytes: B = 2^b
int S; //number of sets: S = 2^s

//Global counters to track cache statistics in access_data().
int hit_cnt = 0;
int miss_cnt = 0;
int evict_cnt = 0;

//Global to control trace output
int verbosity = 0; //print trace if set
/******************************************************************************/


//Type mem_addr_t: Use when dealing with addresses or address masks.
typedef unsigned long long int mem_addr_t;

//Type cache_line_t: Use when dealing with cache lines.
typedef struct cache_line {                    
    char valid;
    mem_addr_t tag;
    //Add a data member as needed by your implementation
    int counter;
} cache_line_t;

int time_counter = 0;

//Type cache_set_t: Use when dealing with cache sets
//Note: Each set is a pointer to a heap array of one or more cache lines.
typedef cache_line_t* cache_set_t;

//Type cache_t: Use when dealing with the cache.
//Note: A cache is a pointer to a heap array of one or more sets.
typedef cache_set_t* cache_t;

// Create the cache we're simulating. 
//Note: A cache is a pointer to a heap array of one or more cache sets.
cache_t cache;  

/* init_cache:
 * Allocates the data structure for a cache with S sets and E lines per set.
 * Initializes all valid bits and tags with 0s.
 */                    
void init_cache() { 
    // set B to 2^b and S to 2^s
    B = pow(2, b);
    S = pow(2, s);

    // allocate memory for the cache: array of pointers to cache sets
    cache = malloc(sizeof(cache_set_t) * S);

    // checks if malloc succeeded
    if (cache == NULL) {
        fprintf(stderr, "Failed to allocate memory for cache\n");
        exit(1);
    }

    // pointer to help iterate through cache sets
    cache_set_t* set_ptr = cache;
    // for each set in the cache
    for (int i = 0; i < S; i++, set_ptr++){
        // allocate memory for each set: array of cache lines
        *set_ptr = malloc(sizeof(cache_line_t) * E);

        // check that malloc succeeded
        if (*set_ptr == NULL) {
            fprintf(stderr, "Failed to allocate memory for cache set\n");
            exit(1);
        }

        // pointer to help iterate through cache lines within a set
        cache_line_t* line_ptr = *set_ptr;
        // initialize each line in the set
        for(int j = 0; j < E; j++, line_ptr++){
            line_ptr->valid = 0;
            line_ptr->tag = 0;
            line_ptr->counter = 0;
        }
    }

}


/* free_cache:
 * Frees all heap allocated memory used by the cache.
 */                    
void free_cache() {  

    // iterate through each set in cache
    for(int i = 0; i < S; i++){
        free(*(cache + i)); // free each set (array of type cache_line_t)
    }

    // free the cache (array of type cache_set_t)
    free(cache);
}


/* access_data:
 * Simulates data access at given "addr" memory address in the cache.
 *
 * If already in cache, increment hit_cnt DONE
 * If not in cache, cache it (set tag), increment miss_cnt DONE
 * If a line is evicted, increment evict_cnt DONE
 */                    
void access_data(mem_addr_t addr) {

    mem_addr_t req_set = (addr >> b) & ((1 << s) - 1);
    mem_addr_t tag = addr >> (s + b);

    // calculate the set's base address using address arithmetic
    cache_line_t* set_base = *(cache + req_set);
    cache_line_t* current_line = set_base;

    cache_line_t* lru_line = NULL; // pointer to track least recently used line
    int oldest_time = INT_MAX; // time of the least recently used line
    cache_line_t* empty_line = NULL; // pointer to track the first empty line

    // iterate through the set's lines using pointer arithmetic
    for (int i = 0; i < E; i++) {
        if (current_line->valid && current_line->tag == tag) { // check if the line's v-bit is set and tag matches the request (hit)
            // cache hit
            hit_cnt++;
            current_line->counter = time_counter++;
            return; // early exit on hit
        }

        if (!current_line->valid && !empty_line) {
            // first empty line found
            empty_line = current_line;
        }

        if (current_line->valid && current_line->counter < oldest_time) {
            // line is lru than the current lru line
            oldest_time = current_line->counter; // update the lru time
            lru_line = current_line; // update the lru line
        }

        current_line++; // move to the next line using pointer arithmetic
    }

    // cache miss handling
    miss_cnt++;
    // decide whether to use an empty line or the lru line
    if (empty_line) {
        lru_line = empty_line; // use the empty line as the lru line
    }

    // if no empty line was found, we will use the lru line
    if (lru_line) {
        if (lru_line->valid) {
            // line is being evicted
            evict_cnt++;
        }
        // update the cache line with the new data
        lru_line->valid = 1;
        lru_line->tag = tag;
        lru_line->counter = time_counter++;
    }
}


/* replay_trace:
 * Replays the given trace file against the cache.
 *
 * Reads the input trace file line by line.
 * Extracts the type of each memory access : L/S/M
 * TRANSLATE each "L" as a load i.e. 1 memory access
 * TRANSLATE each "S" as a store i.e. 1 memory access
 * TRANSLATE each "M" as a load followed by a store i.e. 2 memory accesses 
 */                    
void replay_trace(char* trace_fn) {           
    char buf[1000];  
    mem_addr_t addr = 0;
    unsigned int len = 0;
    FILE* trace_fp = fopen(trace_fn, "r"); 

    if (!trace_fp) { 
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);   
    }

    while (fgets(buf, 1000, trace_fp) != NULL) {
        if (buf[1] == 'S' || buf[1] == 'L' || buf[1] == 'M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);

        // record the state before the access
        int old_hit_cnt = hit_cnt;
        int old_miss_cnt = miss_cnt;
        int old_evict_cnt = evict_cnt;

        // perform the access for load or single operation
        access_data(addr);

        // if it's an 'M', perform the access again for store
        if (buf[1] == 'M') {
            // record the state after load and before store
            int store_hit_cnt = hit_cnt;
            int store_miss_cnt = miss_cnt;
            int store_evict_cnt = evict_cnt;

            // access the data for store
            access_data(addr);

            // if verbosity is enabled, print the correct sequence for 'M' operation
            if (verbosity) {
                printf("%c %llx,%u ", buf[1], addr, len);
                // load part of 'M'
                if (old_hit_cnt < store_hit_cnt) {
                    printf("hit ");
                } else {
                    printf("miss ");
                }
                if (old_evict_cnt < store_evict_cnt) {
                    printf("eviction ");
                }
                
                // store part of 'M', dynamically determined
                if (store_hit_cnt < hit_cnt) {
                    printf("hit "); // if hit count increased, it's a hit
                } else if (store_miss_cnt < miss_cnt) {
                    printf("miss "); // if miss count increased, it's a miss
                } else if (store_evict_cnt < evict_cnt) {
                    printf("eviction "); // if eviction count increased, there was an eviction
                }
                printf("\n");
            }
        } else {
            // for 'L' and 'S' operations, just process once
            if (verbosity) {
                printf("%c %llx,%u ", buf[1], addr, len);
                // print outcomes based on the change in counters
                if (old_hit_cnt < hit_cnt) {
                    printf("hit ");
                }
                if (old_miss_cnt < miss_cnt) {
                    printf("miss ");
                }
                if (old_evict_cnt < evict_cnt) {
                    printf("eviction ");
                }
                printf("\n");
            }
        }
    }
}
    fclose(trace_fp);
}  


/*
 * print_usage:
 * Print information on how to use csim to standard output.
 */                    
void print_usage(char* argv[]) {                 
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Verbose flag.\n");
    printf("  -s <num>   Number of s bits for set index.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of b bits for word and byte offsets.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}  


/*
 * print_summary:
 * Prints a summary of the cache simulation statistics to a file.
 */                    
void print_summary(int hits, int misses, int evictions) {                
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}  


/*
 * main:
 * parses command line args, 
 * makes the cache, 
 * replays the memory accesses, 
 * frees the cache and 
 * prints the summary statistics.  
 */                    
int main(int argc, char* argv[]) {                      
    char* trace_file = NULL;
    char c;

    // Parse the command line arguments: -h, -v, -s, -E, -b, -t 
    while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (c) {
            case 'b':
                b = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'h':
                print_usage(argv);
                exit(0);
            case 's':
                s = atoi(optarg);
                break;
            case 't':
                trace_file = optarg;
                break;
            case 'v':
                verbosity = 1;
                break;
            default:
                print_usage(argv);
                exit(1);
        }
    }

    //Make sure that all required command line args were specified.
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        print_usage(argv);
        exit(1);
    }

    //Initialize cache.
    init_cache();

    //Replay the memory access trace.
    replay_trace(trace_file);

    //Free memory allocated for cache.
    free_cache();

    //Print the statistics to a file.
    //DO NOT REMOVE: This function must be called for test_csim to work.
    print_summary(hit_cnt, miss_cnt, evict_cnt);
    return 0;   
}  

// 202401                                     

