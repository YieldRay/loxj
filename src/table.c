#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75 // 最大负载因子

// 初始化哈希表
void initTable(Table *table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table *table)
{
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static void adjustCapacity(Table *table, int capacity);
static Entry *findEntry(Entry *entries, int capacity, ObjString *key);

/**
 * 使用 ObjString 作键，hash 值已提前计算存放于 ObjString
 * @return 是否为新键
 */
bool tableSet(Table *table, ObjString *key, Value value)
{
    // 如果元素数量超过负载因子允许的最大值，则调整容量
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
        adjustCapacity(table, GROW_CAPACITY(table->capacity));

    // 查找条目位置
    Entry *entry = findEntry(table->entries, table->capacity, key);

    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value))
        table->count++;

    entry->key = key;
    entry->value = value;

    return isNewKey;
}

bool tableGet(Table *table, ObjString *key, Value *value)
{
    if (table->count == 0)
        return false;

    Entry *entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    *value = entry->value;
    return true;
}

bool tableDelete(Table *table, ObjString *key)
{
    if (table->count == 0)
        return false;

    Entry *entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    // 将桶设为墓碑桶
    entry->key = NULL;             // 墓碑桶和空桶的键都为 NULL
    entry->value = BOOL_VAL(true); // 哨兵值，不为 nil 即可，表示桶为墓碑
    return true;
}

/**
 * Find entry from entries, based on **capacity** and key
 */
static Entry *findEntry(Entry *entries, int capacity, ObjString *key)
{
    uint32_t index = key->hash % capacity; // 哈希值对应索引
    Entry *tombstone = NULL;               // 墓碑桶，允许被覆盖

    for (;;)
    {
        Entry *entry = &entries[index];

        // 墓碑桶可能是其它键线性探测并删除后留下的，因此第一次探测到墓碑桶不能确保指定键不存在
        // 之后再遇到空桶时，则可以确保能够重用墓碑桶
        if (entry->key == NULL)
        {
            if (IS_NIL(entry->value))
            { // 空桶
                return tombstone != NULL ? tombstone : entry;
            }
            else
            { // 墓碑桶
                if (tombstone == NULL)
                    tombstone = entry;
            }
        }
        else if (entry->key == key)
        {
            return entry;
        }

// 线性探测法处理冲突，继续查找下一个位置
#ifdef LOXJ_OPTIMIZE_HASH
        index = (index + 1) & (capacity - 1); // 要计算某个数与任何2的幂的模数，可以简单地将该数与2的幂减1进行位相与
#else
        index = (index + 1) % capacity;
#endif
    }
}

static void adjustCapacity(Table *table, int capacity)
{
    Entry *entries = ALLOCATE(Entry, capacity);

    for (int i = 0; i < capacity; i++)
    { // init new entries
        entries[i].key = NULL;
        entries[i].value = NIL_VAL; // 注意此行，空桶的值为 nil，因此墓碑桶的值可以为其它
    }

    table->count = 0;

    for (int i = 0; i < table->capacity; i++)
    { // copy old entries to new entries
        Entry *entry = &table->entries[i];
        if (entry->key == NULL)
            continue;

        Entry *dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;   // replace with new entries
    table->capacity = capacity; // replace with new capacity
}

void tableAddAll(Table *from, Table *to)
{
    for (int i = 0; i < from->capacity; i++)
    {
        Entry *entry = &from->entries[i];
        if (entry->key != NULL)
            tableSet(to, entry->key, entry->value);
    }
}

ObjString *tableFindString(Table *table, const char *chars, int length, uint32_t hash)
{ // 哈希表当集合用
    if (table->count == 0)
        return NULL;

#ifdef LOXJ_OPTIMIZE_HASH
    uint32_t index = hash & (table->capacity - 1);
#else
    uint32_t index = hash % table->capacity;
#endif

    for (;;)
    {
        Entry *entry = &table->entries[index];
        if (entry->key == NULL)
        {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value))
                return NULL;
        }
        else if (entry->key->length == length &&
                 entry->key->hash == hash &&
                 memcmp(entry->key->chars, chars, length) == 0)
        {
            // We found it.
            return entry->key;
        }

#ifdef LOXJ_OPTIMIZE_HASH
        index = (index + 1) & (table->capacity - 1);
#else
        index = (index + 1) % table->capacity;
#endif
    }
}

void markTable(Table *table)
{
    for (int i = 0; i < table->capacity; i++)
    {
        Entry *entry = &table->entries[i];
        markObject((Obj *)entry->key);
        markValue(entry->value);
    }
}

void tableRemoveWhite(Table *table)
{
    for (int i = 0; i < table->capacity; i++)
    {
        Entry *entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked)
        {
            tableDelete(table, entry->key);
        }
    }
}