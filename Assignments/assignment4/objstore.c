#include "lib.h"

// 4 ints are indirect pointers(4*4096KB)
// sizeof(object) = 64 bytes
// #(object per block) = 4096 / 64 = 64
// Max. no. of objects = 10^6
// Starting reserved disk blocks for super object = 21769
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

struct super_object {
    // Store 0 for unused, otherwise used
    // If > 1, it indicates the page distance from objfs->cache
    // Always wasting 2 cache memory pages
    ushort block_bitmap[BITMAP_SIZE];
    char dirty_flag[BITMAP_SIZE];
    char cache_bitmap[CACHE_PAGES];
    struct object obj[1000000];
    char dummy[512];
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

/*
Returns the object ID.  -1 (invalid), 0, 1 - reserved
*/
long find_object_id(const char *key, struct objfs_state *objfs)
{
    int index = hash(key);
    dprintf("Inside find_object_id with index: %d\n", index);
    struct object *obj = &sobject->obj[index];
    while (obj->id && strcmp(obj->key, key)) {
        index++;
        index %= 1000000;
        obj = &sobject->obj[index];
    }
    if (obj->id == 0)
        return -1;
    return obj->id;
}

/*
  Creates a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.

  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/
long create_object(const char *key, struct objfs_state *objfs)
{
    unsigned long index = hash(key);
    struct object *obj = &sobject->obj[index];
    while (obj->id) {
        if (!strcmp(obj->key, key))
            return -1;
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
    sobject->block_bitmap[block_num] = 1;
}
void clear_block_bitmap(int block_num)
{
    sobject->block_bitmap[block_num] = 0;
}

/*
  Destroys an object with obj.key=key. Object ID is ensured to be >=2.

  Return value: Success --> 0
                Failure --> -1
*/
void free_data_block(int data_block_pointer)
{
    int i, block_num;
    int *tmp;
    malloc_4k(tmp);
    for (i=0;i<BLOCK_SIZE/4;++i) {
        block_num = tmp[i];
        if (block_num == -1)
            break;
        clear_block_bitmap(block_num);
        /* tmp[i] = -1; */
    }
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
        free_data_block(obj->data_block_pointers[i]);
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
    old_obj->id = 0;
    return new_id;
}

/*
  Reads the content of the object onto the buffer with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
void read_indirect_block(int block_num, char *buf, int offset, int size, struct objfs_state *objfs)
{
    if (block_num < 0) {
        // Should not happen
        dprintf("Alert: Read indirect block\n");
        return;
    }
    int new_offset;
    int *block;
    malloc_4k(block);
    if (read_block(objfs, block_num, (char *)block) < 0) {
        free_4k(block);
        return;
    }
    int i;
    for (i=0;i<BLOCK_SIZE/4;++i) {
        new_offset = i*4*1024;
        if (size < offset+new_offset)
            break;
        if (block[i] == -1)
            break;
        if (read_block(objfs, block[i], buf+offset+new_offset) < 0)
            break;
    }
    free_4k(block);
}

long objstore_read(int objid, char *buf, int size, struct objfs_state *objfs, off_t offset)
{
    if (objid < 2)
        return -1;
    size = size + offset;
    struct object *obj = &sobject->obj[idToIndex(objid)];
    if (obj->id == 0) {
        dprintf("Read: objid not found.\n");
        return -1;
    }
    size = (size < obj->size)?size:obj->size;
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
    for (i=0;i<4;++i) {
        my_offset = i*4*1024*1024;
        if (my_offset > aux_size)
            break;
        read_indirect_block(obj->data_block_pointers[i], aux_buf, my_offset, aux_size, objfs);
    }
    dprintf("Val after read i: %d\n", i);
    dprintf("Read: %s\n", aux_buf);
    for (i=offset;i<size;++i)
        buf[i-offset] = aux_buf[i];
    return size-offset;
}

int get_new_datablock()
{
    for (int i=DATA_BLOCKS_START;i<total_num_disk_blocks;++i) {
        if (sobject->block_bitmap[i] == 0)
            return i;
    }
    return -1;
}

void initialize_block(int block_num, struct objfs_state *objfs)
{
    int *block;
    malloc_4k(block);
    read_block(objfs, block_num, (char *)block);
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
int write_indirect_block(int block_num, char *buf, int offset, int size, struct objfs_state *objfs)
{
    /* dprintf("Inside write_indirect with block num: %d\n", block_num); */
    if (block_num < 0) {
        block_num = get_new_datablock();
        set_block_bitmap(block_num);
        initialize_block(block_num, objfs);
    }
    /* dprintf("Inside write_indirect with new block num: %d\n", block_num); */
    int new_offset;
    int *block;
    malloc_4k(block);
    if (read_block(objfs, block_num, (char *)block) < 0) {
        free_4k(block);
        return block_num;
    }
    int i;
    /* dprintf("just before for loop\n"); */
    for (i=0;i<BLOCK_SIZE/4;++i) {
        new_offset = i*4*1024;
        if (size < offset+new_offset)
            break;
        if (block[i] == -1) {
            block[i] = get_new_datablock();
            set_block_bitmap(block[i]);
        }
        if (write_block(objfs, block[i], buf+offset+new_offset) < 0)
            break;
    }
    /* dprintf("Val i after write loop: %d\n", i); */
    while (i < BLOCK_SIZE/4) {
        block[i++] = -1;
    }
    write_block(objfs, block_num, (char *)block);
    free_4k(block);
    return block_num;
}

long objstore_write(int objid, const char *buf, int size, struct objfs_state *objfs, off_t offset)
{
    if (objid < 2)
        return -1;
    struct object *obj = &sobject->obj[idToIndex(objid)];
    if (obj->id == 0)
        return -1;
    size = size + offset;
    if (size > 16*1024*1024)
        size = 16*1024*1024;
    char *aux_buf;
    int aux_size;
    if (size % BLOCK_SIZE) {
        aux_size = size/BLOCK_SIZE + 1;
    } else {
        aux_size = size/BLOCK_SIZE;
    }
    malloc_nk(aux_buf, aux_size);
    int i, my_offset;
    // Small hack to escape from rewriting this function after signature update
    objstore_read(objid, aux_buf, offset, objfs, 0);
    for (i=offset;i<size;++i)
        aux_buf[i] = buf[i];

    dprintf("Write: %s\n", aux_buf);
    for (i=0;i<4;++i) {
        my_offset = i*4*1024*1024;
        if (my_offset > size)
            break;
        obj->data_block_pointers[i] = write_indirect_block(obj->data_block_pointers[i], aux_buf, my_offset, aux_size, objfs);
    }
    while (i < 4) {
        obj->data_block_pointers[i++] = -1;
    }

    free_nk(aux_buf, aux_size);
    obj->size = size;
    return size;
}


/*
  Reads the object metadata for obj->id = buf->st_ino
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat
*/
int cal_number_blocks(int block_num, struct objfs_state *objfs)
{
    int res = 0;
    int *block;
    malloc_4k(block);
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
    if (id < 0)
        return -1;
    int index = idToIndex(id);
    struct object *obj = &sobject->obj[index];
    if (obj->id == -1)
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
        if (read_block(objfs, j, (char *)sobject + i))
            return -1;
        i += 4096;
        j++;
    }

    objfs->objstore_data = sobject;
    struct stat sbuf;
    if(fstat(objfs->blkdev, &sbuf) < 0){
        perror("fstat");
        exit(-1);
    }

    // Global variable
    total_num_disk_blocks = objfs->disksize;

    dprintf("Super Block size: %lu MB\n", sizeof(struct super_object)/1024/1024);
    dprintf("Done objstore init\n");
    return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs)
{
    objfs->objstore_data = NULL;
    for (int i=0;i<sizeof(struct super_object);i+=BLOCK_SIZE) {
        write_block(objfs, i/BLOCK_SIZE, (char *)(sobject)+i);
    }
    free_superobject(sobject);
    dprintf("Done objstore destroy\n");
    return 0;
}
