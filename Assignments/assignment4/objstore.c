#include "lib.h"

// 4 ints are indirect pointers(4*4096KB)
// Similar with cache_pointers
// sizeof(object) = 4 + 4 + 4*4 + 4 + 32 + 4*4 = 76 bytes
// #(object per block) = 4096 / 88
// Max. no. of objects = 10^6
// Reserve starting 32768 blocks (256 for bitmap)

struct object {
    long id;
    long size;
    int cache_pointers[4];
    int dirty;
    char key[32];
    int data_block_pointers[4];
};

struct super_object {
    char bitmap[256 * BLOCK_SIZE];
    struct object obj[1000000];
    char dummy[2559];
};

#define malloc_superobject(x) do{                                                \
        (x) = mmap(NULL, sizeof(struct super_object), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); \
        if((x) == MAP_FAILED)                                           \
            (x)=NULL;                                                   \
    }while(0);

#define free_superobject(x) munmap((x), sizeof(struct super_object))

#define indexToID(x) ({int retval; retval = x+2; retval;})
#define idToIndex(x) ({int retval; retval = x-2; retval;})

struct super_object *sobject;

// Courtesy: http://www.cse.yorku.ca/~oz/hash.html
int hash(const char *str)
{
    int hash = 5381;
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
    struct super_object *sblock = objfs->objstore_data;

    struct object *obj = &sblock->obj[index];
    while (obj->id && strcmp(obj->key, key)) {
        index++;
        index %= 1000000;
        obj = &sblock->obj[index];
    }

    if (obj->id == 0)
        return -1;
    return obj->id;
}

// TODO: Add locks
int bitmap_ispresent(int num_block)
{
    char bitmap = sobject->bitmap[num_block / 8];
    int offset = num_block % 8;
    return (bitmap & (1 << offset));
}

void bitmap_flipbit(int num_block)
{
    char bitmap = sobject->bitmap[num_block / 8];
    int offset = num_block % 8;
    bitmap = (bitmap ^ (1 << offset));
    sobject->bitmap[num_block / 8] = bitmap;
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
    obj->dirty = 0;
    strncpy(obj->key, key, 32);
    for (index=0;index<4;++index) {
        obj->cache_pointers[index] = -1;
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

/*
  Destroys an object with obj.key=key. Object ID is ensured to be >=2.

  Return value: Success --> 0
                Failure --> -1
*/
void free_data_block(int data_block_pointer)
{
    int *ptr = (int *)data_block_pointer;
    for (int i=0;i<BLOCK_SIZE/sizeof(int);++i) {
        if (ptr[i] == -1)
            break;
        bitmap_flipbit(ptr[i]);
    }
}
long destroy_object(const char *key, struct objfs_state *objfs)
{
    int index = idToIndex(find_object_id(key, objfs));
    struct object *obj = &sobject->obj[index];
    if (obj->id == 0)
        return -1;
    for (int i=0;i<4;++i) {
        free_data_block(obj->data_block_pointers[i]);
    }
    memset(obj, 0, sizeof(struct object));
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
    long new_id = create_object(newname, objfs);
    long old_id = find_object_id(key, objfs);

    return -1;
}

/*
  Writes the content of the buffer into the object with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_write(int objid, const char *buf, int size, struct objfs_state *objfs)
{
   return -1;
}

/*
  Reads the content of the object onto the buffer with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_read(int objid, char *buf, int size, struct objfs_state *objfs)
{
   return -1;
}

/*
  Reads the object metadata for obj->id = buf->st_ino
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat
*/
int fillup_size_details(struct stat *buf)
{
   return -1;
}

/*
   Set your private pointeri, anyway you like.
*/
// Block# 0 to 255 store block bitmap
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
        if (read_block(objfs, j * BLOCK_SIZE, (char *)sobject + i))
            return -1;
        i += 4096;
        j++;
    }

    dprintf("%lu\n", sizeof(struct super_object));
    objfs->objstore_data = sobject;
    dprintf("Done objstore init\n");
    return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs)
{
    objfs->objstore_data = NULL;
    free_superobject(sobject);
    dprintf("Done objstore destroy\n");
    return 0;
}
