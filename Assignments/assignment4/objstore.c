#include "lib.h"
#include <pthread.h>

// 4 ints are indirect pointers(4*4096KB)
// sizeof(object) = 56 bytes
// #(object per block) = 4096 / 56 = 73
// Max. no. of objects = 10^6
struct object {
    int id;
    int size;
    char key[32];
    int data_block_pointers[4];
};

// Make total size a multiple of 4KB
// Max no. of block for 32 GB
#define BITMAP_SIZE 32*1024*1024/4
// Cache size is 128 MB
#define CACHE_PAGES 128*1024/4

pthread_mutex_t create_object_lock;

struct super_object {
    // Store 0 for unused, otherwise used
    // If > 1, it indicates the page distance from objfs->cache
    // Always wasting 2 cache memory pages
    pthread_mutex_t block_bitmap_mutex;
    ushort block_bitmap[BITMAP_SIZE];

    // 1 => dirty else not
    char dirty_flag[CACHE_PAGES];
    // If 0 => unused, otherwise used
    // All other positive value will indicate which block # it is caching
    struct object obj[1000000];

    pthread_mutex_t cache_bitmap_mutex;
    int cache_bitmap[CACHE_PAGES];
    int last_cache_index;
    // Align this struct to 4K boundry
    char dummy[424];
};

#define DATA_BLOCKS_START sizeof(struct super_object)/BLOCK_SIZE

#define malloc_4k(x) do{                                                \
        (x) = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); \
        if((x) == MAP_FAILED)                                           \
            (x)=NULL;                                                   \
    }while(0);
#define free_4k(x) munmap((x), BLOCK_SIZE)

#define malloc_nk(x, n) do{                                               \
        (x) = mmap(NULL, (n)*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); \
        if((x) == MAP_FAILED)                                           \
            (x)=NULL;                                                   \
    }while(0);
#define free_nk(x, n) munmap((x), (n)*BLOCK_SIZE)

#define malloc_superobject(x) do{                                                \
        (x) = mmap(NULL, sizeof(struct super_object), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); \
        if((x) == MAP_FAILED)                                           \
            (x)=NULL;                                                   \
    }while(0);
#define free_superobject(x) munmap((x), sizeof(struct super_object))

#define indexToID(x) ({int retval; retval = x+2; retval;})
#define idToIndex(x) ({int retval; retval = x-2; retval;})

// Global variable
struct super_object *sobject;
int total_num_disk_blocks;

// Courtesy: http://www.cse.yorku.ca/~oz/hash.html
unsigned int hash(const char *str)
{
    unsigned int hash = 5381;
    int c;

    while ((c = *str++) != '\0')
        hash = ((hash << 5) + hash) + c;

    return hash % 1000000;
}

unsigned int get_free_cache_page(struct objfs_state *);
int get_new_datablock(struct objfs_state *);
void set_block_bitmap(int);

#ifdef CACHE         // CACHED implementation
// Read the cached page OR
// Read the block and then cache it
static int read_cached(struct objfs_state *objfs, int block_num, char *user_buf)
{
    int cache_index;
    char *cache_addr;
    if (sobject->block_bitmap[block_num] == 0) {
        return -1;
    }
    if (sobject->block_bitmap[block_num] == 1) {
        cache_index = get_free_cache_page(objfs);
        cache_addr = objfs->cache + (cache_index << 12);
        sobject->cache_bitmap[cache_index] = block_num;
        sobject->block_bitmap[block_num] = cache_index;
        sobject->dirty_flag[cache_index] = 0;
        read_block(objfs, block_num, user_buf);
        memcpy(cache_addr, user_buf, BLOCK_SIZE);
        return 0;
    }
    cache_index = sobject->block_bitmap[block_num];
    cache_addr = objfs->cache + (cache_index << 12);
    memcpy(user_buf, cache_addr, BLOCK_SIZE);
    return 0;
}
// Write to buffer
static int write_cached(struct objfs_state *objfs, int block_num, char *user_buf)
{
    if (sobject->block_bitmap[block_num] == 0) {
        return -1;
    }
    int cache_index;
    if (sobject->block_bitmap[block_num] > 1) {
        cache_index = sobject->block_bitmap[block_num];
    } else {
        cache_index = get_free_cache_page(objfs);
        // Since cache_index > 1
        sobject->block_bitmap[block_num] = cache_index;
        sobject->cache_bitmap[cache_index] = block_num;
    }
    char *cache_addr = objfs->cache + (cache_index << 12);
    memcpy(cache_addr, user_buf, BLOCK_SIZE);
    sobject->dirty_flag[cache_index] = 1;
    return 0;
}
static int cache_sync(struct objfs_state *objfs)
{
    int i;
    int block_num;
    for (i=0;i<CACHE_PAGES;++i) {
        if (sobject->cache_bitmap[i] == 0)
            continue;
        block_num = sobject->cache_bitmap[i];
        if (sobject->dirty_flag[i] == 0) {
            sobject->block_bitmap[block_num] = 1;
            continue;
        }
        write_block(objfs, block_num, objfs->cache + (i << 12));
        sobject->cache_bitmap[i] = 0;
        sobject->dirty_flag[i] = 0;
        sobject->block_bitmap[block_num] = 1;
    }
    return 0;
}
static void flush_cache(int cache_index, struct objfs_state *objfs)
{
    int block_num;
    block_num = sobject->cache_bitmap[cache_index];
    if (sobject->dirty_flag[cache_index] != 0) {
        write_block(objfs, block_num, objfs->cache + (cache_index << 12));
    }

    pthread_mutex_lock(&sobject->cache_bitmap_mutex);
    sobject->cache_bitmap[cache_index] = 0;
    pthread_mutex_unlock(&sobject->cache_bitmap_mutex);

    // was greater than 0 so should not be a problem
    sobject->block_bitmap[block_num] = 1;
}
#else  //uncached implementation
static int read_cached(struct objfs_state *objfs, int block_num, char *user_buf)
{
    return -1;
}
static int write_cached(struct objfs_state *objfs, int block_num, char *user_buf)
{
    return -1;
}
static int cache_sync(struct objfs_state *objfs)
{
    return 0;
}
static void flush_cache(int cache_index, struct objfs_state *objfs)
{
    return;
}
#endif

// Ensure returned index > 1
unsigned int get_free_cache_page(struct objfs_state *objfs)
{
    pthread_mutex_lock(&sobject->cache_bitmap_mutex);
    unsigned int new_cache_index = sobject->last_cache_index + 1;
    new_cache_index %= CACHE_PAGES;
    if (new_cache_index < 2)
        new_cache_index = 2;
    sobject->last_cache_index = new_cache_index;

    if (sobject->cache_bitmap[new_cache_index] != 0)
        flush_cache(new_cache_index, objfs);

    pthread_mutex_unlock(&sobject->cache_bitmap_mutex);
    return new_cache_index;
}

/*
Returns the object ID.  -1 (invalid), 0, 1 - reserved
*/
long find_object_id(const char *key, struct objfs_state *objfs)
{
    int index = hash(key);
    struct object *obj = &sobject->obj[index];
    if (obj->id && !strcmp(obj->key, key))
        return obj->id;
    int previous_index = index;
    index += 1;
    index %= 1000000;
    obj = &sobject->obj[index];
    while (previous_index != index) {
        if (obj->id && !strcmp(obj->key, key))
            return obj->id;
        index++;
        index %= 1000000;
        obj = &sobject->obj[index];
    }
    return -1;
}

/*
  Creates a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.

  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/
long create_object(const char *key, struct objfs_state *objfs)
{
    pthread_mutex_lock(&create_object_lock);
    unsigned long index = hash(key);
    struct object *obj = &sobject->obj[index];

    while (obj->id) {
        if (!strcmp(obj->key, key)) {
            pthread_mutex_unlock(&create_object_lock);
            return -1;
        }
        index++;
        index %= 1000000;
        obj = &sobject->obj[index];
    }

    obj->id = indexToID(index);

    obj->size = 0;
    strncpy(obj->key, key, 32);
    for (index=0;index<4;++index) {
        obj->data_block_pointers[index] = -1;
    }
    pthread_mutex_unlock(&create_object_lock);
    return obj->id;
}

/*
  One of the users of the object has dropped a reference
  Can be useful to implement caching.
  Return value: Success --> 0
                Failure --> -1
*/
long release_object(int objid, struct objfs_state *objfs)
{
    return 0;
}

void set_block_bitmap(int block_num)
{
    pthread_mutex_lock(&sobject->block_bitmap_mutex);
    sobject->block_bitmap[block_num] = 1;
    pthread_mutex_unlock(&sobject->block_bitmap_mutex);
}
void clear_block_bitmap(int block_num)
{
    pthread_mutex_lock(&sobject->block_bitmap_mutex);
    sobject->block_bitmap[block_num] = 0;
    pthread_mutex_unlock(&sobject->block_bitmap_mutex);
}

/*
  Destroys an object with obj.key=key. Object ID is ensured to be >=2.

  Return value: Success --> 0
                Failure --> -1
*/
void free_data_block(struct objfs_state *objfs, int data_block_pointer)
{
    int i, block_num;
    int *tmp;
    malloc_4k(tmp);
    read_block(objfs, data_block_pointer, (char *)tmp);
    for (i=0;i<BLOCK_SIZE/4;++i) {
        block_num = tmp[i];
        if (block_num == -1)
            break;
        int cache_index = sobject->block_bitmap[block_num];
        if (cache_index > 1) {

            pthread_mutex_lock(&sobject->cache_bitmap_mutex);
            sobject->cache_bitmap[cache_index] = 0;
            sobject->dirty_flag[cache_index] = 0;
            pthread_mutex_unlock(&sobject->cache_bitmap_mutex);

        }
        clear_block_bitmap(block_num);
    }
    clear_block_bitmap(data_block_pointer);
    free_4k(tmp);
}

long destroy_object(const char *key, struct objfs_state *objfs)
{
    int id = find_object_id(key, objfs);
    if (id == -1)
        return -1;
    int index = idToIndex(id);
    struct object *obj = &sobject->obj[index];
    if (obj->id == 0)
        return -1;
    for (int i=0;i<4;++i) {
        if (obj->data_block_pointers[i] == -1)
            break;
        free_data_block(objfs, obj->data_block_pointers[i]);
    }
    obj->id = 0;
    return 0;
}

/*
  Renames a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.
  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/
long rename_object(const char *key, const char *newname, struct objfs_state *objfs)
{
    if(strlen(newname) > 32)
        return -1;
    int old_id = find_object_id(key, objfs);
    if (old_id == -1)
        return -1;
    int new_id = create_object(newname, objfs);
    if (new_id == -1)
        return -1;
    struct object *new_obj = &sobject->obj[idToIndex(new_id)];
    struct object *old_obj = &sobject->obj[idToIndex(old_id)];

    new_obj->size = old_obj->size;
    for (int i=0;i<4;++i) {
        new_obj->data_block_pointers[i] = old_obj->data_block_pointers[i];
    }
    pthread_mutex_lock(&create_object_lock);
    old_obj->id = 0;
    pthread_mutex_unlock(&create_object_lock);
    return new_id;
}


/*
  Reads the content of the object onto the buffer with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
void read_indirect_block(int block_num, int block_offset, char *buf, int offset, int size, struct objfs_state *objfs)
{
    if (block_num < 0) {
        return;
    }
    int new_offset;
    int *block;
    malloc_4k(block);
    if (read_cached(objfs, block_num, (char *)block) == -1) {
        read_block(objfs, block_num, (char *)block);
    }
    int i=1;
    for (i=block_offset;i<BLOCK_SIZE/4;++i) {
        new_offset = (i-block_offset)*4*1024;
        if (size <= offset+new_offset)
            break;
        if (block[i] == -1)
            break;
        if (read_cached(objfs, block[i], buf+offset+new_offset) != -1) {
            continue;
        }
        if (read_block(objfs, block[i], buf+offset+new_offset) < 0)
            break;
    }
    free_4k(block);
}

long objstore_read(int objid, char *buf, int size, struct objfs_state *objfs, off_t offset)
{
    if (objid < 2)
        return -1;

    struct object *obj = &sobject->obj[idToIndex(objid)];
    if (obj->id == 0) {
        return -1;
    }
    if (size > (obj->size - offset))
        size = obj->size - offset;

    int block_offset = offset/BLOCK_SIZE;
    int data_block_pointer_index = block_offset/1024;
    block_offset %= 1024;

    int my_offset;
    char *aux_buf;
    int aux_size;
    if (size % BLOCK_SIZE) {
        aux_size = size/BLOCK_SIZE + 1;
    } else {
        aux_size = size/BLOCK_SIZE;
    }
    malloc_nk(aux_buf, aux_size);
    int i;
    for (i=data_block_pointer_index;i<4;++i) {
        my_offset = (i-data_block_pointer_index)*4*1024*1024;
        if (my_offset >= size)
            break;
        read_indirect_block(obj->data_block_pointers[i], block_offset, aux_buf, my_offset, aux_size*BLOCK_SIZE, objfs);
        block_offset = 0; // reset block offset for remaining read
    }
    for (i=0;i<size;++i)
        buf[i] = aux_buf[i+(offset%BLOCK_SIZE)];
    return size;
}

int get_new_datablock(struct objfs_state *objfs)
{
    pthread_mutex_lock(&sobject->block_bitmap_mutex);
    for (int i=DATA_BLOCKS_START;i<total_num_disk_blocks;++i) {
        if (sobject->block_bitmap[i] == 0) {
            sobject->block_bitmap[i] = 1; // reserve it
            pthread_mutex_unlock(&sobject->block_bitmap_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&sobject->block_bitmap_mutex);
    return -1;
}

void initialize_block(int block_num, struct objfs_state *objfs)
{
    int *block;
    malloc_4k(block);
    for (int i=0;i<BLOCK_SIZE/4;++i)
        block[i] = -1;
    write_block(objfs, block_num, (char *)block);
    free_4k(block);
}
/*
  Writes the content of the buffer into the object with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
int write_indirect_block(int block_num, int block_offset, char *buf, int offset, int size, struct objfs_state *objfs)
{
    if (block_num < 0) {
        block_num = get_new_datablock(objfs);
        set_block_bitmap(block_num);
        initialize_block(block_num, objfs);
    }
    int new_offset;
    int *block;
    char *tmp;
    malloc_4k(block);
    malloc_4k(tmp);
    if (read_cached(objfs, block_num, (char *)block) == -1) {
        read_block(objfs, block_num, (char *)block);
    }
    int i;
    for (i=block_offset;i<BLOCK_SIZE/4;++i) {
        new_offset = (i-block_offset)*4*1024;
        if (size <= offset+new_offset)
            break;
        if (block[i] == -1) {
            block[i] = get_new_datablock(objfs);
            set_block_bitmap(block[i]);
        }
        for (int j=0;j<BLOCK_SIZE;++j)
            tmp[j] = buf[offset+new_offset+j];
        if (write_cached(objfs, block[i], buf+offset+new_offset) != -1)
            continue;
        if (write_block(objfs, block[i], tmp) < 0)
            break;
    }
    while (i < BLOCK_SIZE/4) {
        block[i++] = -1;
    }
    if (write_cached(objfs, block_num, (char *)block) == -1) {
        write_block(objfs, block_num, (char *)block);
    }
    if (read_cached(objfs, block_num, (char *)block) == -1) {
        read_block(objfs, block_num, (char *)block);
    }
    free_4k(tmp);
    free_4k(block);
    return block_num;
}

int read_indirect_helper(struct objfs_state *objfs, int block_num, int block_offset, char *buf)
{
    char *block;
    malloc_4k(block);
    if (read_cached(objfs, block_num, block) == -1)
        read_block(objfs, block_num, block);
    int new_block_num = block[block_offset];
    if (new_block_num < 0) {
        return 0;
    }
    if (read_cached(objfs, new_block_num, buf) == -1)
        read_block(objfs, new_block_num, buf);
    free_4k(block);
    return 0;
}
long objstore_write(int objid, const char *buf, int size, struct objfs_state *objfs, off_t offset)
{
    if (objid < 2)
        return -1;
    struct object *obj = &sobject->obj[idToIndex(objid)];
    if (obj->id == 0)
        return -1;
    if (size > 16*1024*1024-offset)
        size = 16*1024*1024-offset;

    int block_offset = offset/BLOCK_SIZE; // Get starting block number
    int data_block_pointer_index = block_offset / 1024;
    block_offset %= 1024;

    char *aux_buf;
    int aux_size;
    if (size % BLOCK_SIZE) {
        aux_size = size/BLOCK_SIZE + 1;
    } else {
        aux_size = size/BLOCK_SIZE;
    }
    malloc_nk(aux_buf, aux_size);
    int i, my_offset;

    if ((offset%BLOCK_SIZE) > 0)
        read_indirect_helper(objfs, obj->data_block_pointers[data_block_pointer_index], block_offset, aux_buf);

    for (i=0;i<size;++i) {
        aux_buf[i+(offset%BLOCK_SIZE)] = buf[i];
    }
    for (i=data_block_pointer_index;i<4;++i) {
        my_offset = (i-data_block_pointer_index)*4*1024*1024;
        if (my_offset >= size)
            break;
        obj->data_block_pointers[i] = write_indirect_block(obj->data_block_pointers[i], block_offset, aux_buf, my_offset, aux_size*BLOCK_SIZE, objfs);
        block_offset = 0; // Reset block offset for next write
    }
    while (i < 4) {
        obj->data_block_pointers[i++] = -1;
    }

    free_nk(aux_buf, aux_size);
    obj->size = offset + size;
    return size;
}
/*
  Reads the object metadata for obj->id = buf->st_ino
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat
*/
int cal_number_blocks(int block_num, struct objfs_state *objfs)
{
    if (block_num < 0) {
        return 0;
    }
    int res = 0;
    int *block;
    malloc_4k(block);
    if (read_cached(objfs, block_num, (char *)block) == -1)
        read_block(objfs, block_num, (char *)block);
    for (int i=0;i<BLOCK_SIZE/4;++i) {
        if (block[i] < 0)
            break;
        res++;
    }
    free_4k(block);
    return res;
}

int fillup_size_details(struct stat *buf, struct objfs_state *objfs)
{
    int id = buf->st_ino;
    if (id < 2)
        return -1;
    int index = idToIndex(id);
    struct object *obj = &sobject->obj[index];
    if (obj->id == 0)
        return -1;
    buf->st_size = obj->size;
    int num_blocks = 0;
    for (int i=0;i<4;++i) {
        if (obj->data_block_pointers[i] < 0)
            break;
        num_blocks += cal_number_blocks(obj->data_block_pointers[i], objfs);
    }
    buf->st_blocks = num_blocks;
    return 0;
}

/*
   Set your private pointeri, anyway you like.
*/
int objstore_init(struct objfs_state *objfs)
{
    // sobject is global variable
    malloc_superobject(sobject);
    if(!sobject){
        dprintf("%s: malloc\n", __func__);
        return -1;
    }

    int i = 0;
    int j = 0;
    while (i < sizeof(struct super_object)) {
        if (read_block(objfs, j, (char *)sobject + i) < 0) {
            return -1;
        }
        i += BLOCK_SIZE;
        j++;
    }

    pthread_mutex_init(&sobject->block_bitmap_mutex, NULL);
    pthread_mutex_init(&sobject->cache_bitmap_mutex, NULL);
    pthread_mutex_init(&create_object_lock, NULL);

    sobject->last_cache_index = 1;
    for (i=0;i<CACHE_PAGES;++i) {
        sobject->cache_bitmap[i] = 0;
        sobject->dirty_flag[i] = 0;
    }

    objfs->objstore_data = sobject;
    struct stat sbuf;
    if(fstat(objfs->blkdev, &sbuf) < 0){
        perror("fstat");
        exit(-1);
    }

    // Global variable
    total_num_disk_blocks = sbuf.st_size/BLOCK_SIZE;

    dprintf("Done objstore init\n");
    return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs)
{
    pthread_mutex_destroy(&sobject->block_bitmap_mutex);
    pthread_mutex_destroy(&sobject->cache_bitmap_mutex);
    pthread_mutex_destroy(&create_object_lock);
    // Sync the data block before unmounting
    cache_sync(objfs);
    objfs->objstore_data = NULL;
    for (int i=0;i<sizeof(struct super_object);i+=BLOCK_SIZE) {
        write_block(objfs, i/BLOCK_SIZE, (char *)(sobject)+i);
    }
    free_superobject(sobject);
    dprintf("Done objstore destroy\n");
    return 0;
}
