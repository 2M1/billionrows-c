
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#define FILE_PATH "challange/measurements.txt"
#define N_THREADS 10    // arbitrary, will be one of the parameters for performance tuning later on.
#define MAP_SIZE 11000 // this is 1000 larger than the actual max amount of cities, so that i don't have to handle wrap-around/overflow for now.
#define MAX_KEY_LENGTH 100 // given by the challange.
                           //
#define max(a, b)  a > b ? a : b
#define min(a,b) a > b ? b : a

#define RETURN_OK 0
#define RETURN_ERR 1

// ============== hashmap ======================

struct city_values {
    size_t count;   // NOTE: never create instances with a count of 0, because averages are calculated with sum/count on the fly.
    int64_t sum;
    int8_t min;
    int8_t max;
};

struct hashmap {
    size_t _longest_offset;
    char keys[MAP_SIZE][MAX_KEY_LENGTH];
    struct city_values contents[MAP_SIZE];
};

size_t hash(char name[MAX_KEY_LENGTH]) {

    size_t hash = 102983;
    int c;
    while ((c = *name++)) {
        hash ^= ((hash << 5) + hash) + c;
    }
    return hash % MAP_SIZE;

}

struct hashmap *map_create() {
    struct hashmap *map = malloc(sizeof(*map));
    if (!map) {
        return NULL;
    }
    
    map->_longest_offset = 1; // allow at least one recheck, because otherwise comparisons whether the offset was met will fail when the initial hash returned the correct position
    for (size_t i = 0; i < MAX_KEY_LENGTH; ++i) {
        map->keys[i][0] = '\0'; // mark as empty
    }

    return map;
}

__attribute__((always_inline))
size_t inline _map_key_location(struct hashmap *map, char key[MAX_KEY_LENGTH]) {
    // NOTE: does only work if the key is in the map, will loop indefinetly while overflowing memory otherwise.
    size_t key_hash = hash(key);
    size_t offset = 0;
    while (strncmp(map->keys[key_hash], key, MAX_KEY_LENGTH) != 0 && offset++ < map->_longest_offset) key_hash++;
    if (offset >= map->_longest_offset) return -1;
    return key_hash;
}

bool map_put(struct hashmap *map, char key[MAX_KEY_LENGTH], struct city_values values) {
    size_t key_hash = hash(key);
    size_t offset = 0;
    while (map->keys[key_hash][0] != '\0' && strncmp(key, map->keys[key_hash], MAX_KEY_LENGTH) != 0) {
        key_hash++;  // NOTE: might run out of bounds (should not happen due to excess available space)
        offset++;
    }

    strncpy(map->keys[key_hash], key, MAX_KEY_LENGTH);
    map->contents[key_hash] = values;
    map->_longest_offset = max(map->_longest_offset, offset);

    return true;
}

/// upates a existing key or inserts it otherwise.
/// retruns: true if a key was updated, false if an insertion was performed.
bool map_update(struct hashmap *map, char key[MAX_KEY_LENGTH], int8_t value){
    size_t city_index = _map_key_location(map, key);
    if (city_index == -1) {
        struct city_values values = { .count = 1, .max = value, .min = value, .sum = value };
        map_put(map, key, values);        
        return false;
    }
    map->contents[city_index].sum += value;
    map->contents[city_index].max = max(map->contents[city_index].max, value);
    map->contents[city_index].min = min(map->contents[city_index].min, value);
    map->contents[city_index].count++;

    return true;
}

struct city_values map_get(struct hashmap *map, char key[MAX_KEY_LENGTH]) {
    size_t index = _map_key_location(map, key);
    if (index == -1) {
        struct city_values values = { .count = -1 }; // avoid accidental division by 0 problems :)
        return values;
    }
    return map->contents[index];
}

/// prints a single city in the format required by the challange: [name]=[average temp]/[min temp]/[max temp]
void print_city(struct hashmap *map, size_t index) {
    struct city_values values = map->contents[index];
    printf("%.100s=%zul/%d/%d",  map->keys[index], values.sum / values.count, values.min, values.max);
}


// =============== preparing file:

static size_t file_size;
static int fd;



int get_file_size() {
    fd = open(FILE_PATH, O_RDONLY);  
    if (fd == -1) {
        perror("unable to open file: ");
        return RETURN_ERR;
    }

    struct stat statbuf = {};

    if (fstat(fd, &statbuf) == -1) {
        perror("unable to get file stats: ");
        return RETURN_ERR;
    }

    file_size = statbuf.st_size;

    return RETURN_OK;
}



void read_file_in(int fd, struct hashmap *map, size_t len) {
    char *file_mem = mmap(NULL, len, PROT_READ,  MAP_POPULATE | MAP_PRIVATE, fd, 0);
    if (file_mem == MAP_FAILED) {
        perror("mmap failed: ");
        return;
    }


    char curr_name[MAX_KEY_LENGTH + 1];
    char curr_temp[5];  // -99.9 is max.
    int8_t temp;
    bool sign = false;
    size_t curr_name_len = 0;

    char *curr_pos = &file_mem[0];


    while (curr_pos < file_mem + len) {
        curr_name_len = 0;
        // reading in name:
        for (; *curr_pos != ';'; curr_pos++) {
           curr_name[curr_name_len++] = *curr_pos; 
       }
        curr_name[curr_name_len] = '\0';
        curr_pos++; // skip ; 
        // reading in temp:
        temp = 0;
        if (*curr_pos == '-') {
           sign = true; 
        } else {
           sign = false; 
        }
        for (; *curr_pos != '\n'; ++curr_pos) {
           if (*curr_pos == '.')  { continue; } 
            
           temp *= 10;
           temp += (int8_t)(*curr_pos - '0');
        }

        map_update(map, curr_name, temp ) ;

        curr_pos++; // skip \n
    }
}



int main(int argc, char *argv[]) {

    if (get_file_size() != RETURN_OK) {
        return EXIT_FAILURE;
    }

    struct hashmap *map = map_create();
    struct city_values values = { .count = 1, .max = 100, .sum = 100, .min = 100 };

    char tohash1[100] = "hello";
    char tohash2[MAX_KEY_LENGTH] ="hell" ;

//    map_put(map, tohash1, values);
//    map_put(map, tohash2, values);   
//    map_update(map, tohash2, 120);
//    printf("retreived value: ");
//    print_city(map, _map_key_location(map, tohash2));
//    printf("\n");
    printf("file size: %zul\n", file_size);
    read_file_in(fd, map,  file_size/1000); 
    printf("chunk size: %zul\n", file_size / N_THREADS);
    printf("hashes: %zul, %zul\n", hash(tohash1), hash(tohash2));
    return EXIT_SUCCESS;
}
