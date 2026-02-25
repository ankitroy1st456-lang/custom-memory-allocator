#include <iostream>
using namespace std;

const size_t POOL_SIZE = 1024 * 1024; // 1 MB per pool
const size_t ALIGNMENT = 16;
const int NUM_CLASSES = 4;

struct BlockHeader
{
    size_t size;
    bool free;
    BlockHeader *next;
    BlockHeader *prev;
};

struct PoolHeader
{
    unsigned char *base;
    BlockHeader *firstBlock;
    PoolHeader *next;
};

PoolHeader *poolList = nullptr;
BlockHeader *freeLists[NUM_CLASSES] = {nullptr};

// --- Alignment helper ---
size_t alignSize(size_t size)
{
    return size % ALIGNMENT > 0 ? size + (ALIGNMENT - size % ALIGNMENT) : size;
}

// --- Class assignment ---
int getClass(size_t size)
{
    size_t alignedSize = alignSize(size);
    if (alignedSize <= 64)
        return 0;
    else if (alignedSize <= 256)
        return 1;
    else if (alignedSize <= 1024)
        return 2;
    else
        return 3;
}

// --- Insert block into free list ---
void insertBlock(BlockHeader *block)
{
    int cls = getClass(block->size);
    block->next = freeLists[cls];
    if (freeLists[cls])
        freeLists[cls]->prev = block;
    block->prev = nullptr;
    freeLists[cls] = block;
}

// --- Pool initialization ---
void initPool(size_t size = POOL_SIZE)
{
    PoolHeader *pool = new PoolHeader;
    pool->base = new unsigned char[size];

    pool->firstBlock = reinterpret_cast<BlockHeader *>(pool->base);
    pool->firstBlock->size = size - sizeof(BlockHeader);
    pool->firstBlock->free = true;
    pool->firstBlock->next = nullptr;
    pool->firstBlock->prev = nullptr;

    pool->next = poolList;
    poolList = pool;

    insertBlock(pool->firstBlock);
}

// --- Split and allocate ---
void *splitAndAllocate(BlockHeader *block, size_t size)
{
    size_t alignedSize = alignSize(size);
    size_t totalSize = alignedSize + sizeof(BlockHeader);

    if (block->size >= totalSize + sizeof(BlockHeader) + ALIGNMENT)
    {
        // split
        BlockHeader *newBlock = reinterpret_cast<BlockHeader *>(
            reinterpret_cast<unsigned char *>(block) + totalSize);
        newBlock->size = block->size - totalSize;
        newBlock->free = true;
        newBlock->next = nullptr;
        newBlock->prev = nullptr;
        insertBlock(newBlock);

        block->size = alignedSize;
    }

    block->free = false;
    return reinterpret_cast<unsigned char *>(block) + sizeof(BlockHeader);
}

// --- Find block in pool ---
BlockHeader *findBlockInPool(PoolHeader *pool, size_t size)
{
    int cls = getClass(size);
    for (int c = cls; c < NUM_CLASSES; c++)
    {
        BlockHeader *current = freeLists[c];
        while (current)
        {
            if (current->free && current->size >= size)
            {
                // remove from free list
                if (current->prev)
                    current->prev->next = current->next;
                if (current->next)
                    current->next->prev = current->prev;
                if (freeLists[c] == current)
                    freeLists[c] = current->next;
                return current;
            }
            current = current->next;
        }
    }
    return nullptr;
}

// --- Allocation with expansion ---
void *myMalloc(size_t size)
{
    size_t alignedSize = alignSize(size);

    PoolHeader *pool = poolList;
    while (pool)
    {
        BlockHeader *block = findBlockInPool(pool, alignedSize);
        if (block)
            return splitAndAllocate(block, alignedSize);
        pool = pool->next;
    }

    // expand if none found
    initPool();
    return myMalloc(size);
}

// --- Free with bidirectional coalescing ---
void myFree(void *ptr)
{
    if (!ptr)
        return;

    BlockHeader *block = reinterpret_cast<BlockHeader *>(
        reinterpret_cast<unsigned char *>(ptr) - sizeof(BlockHeader));
    block->free = true;

    // forward coalescing
    BlockHeader *next = reinterpret_cast<BlockHeader *>(
        reinterpret_cast<unsigned char *>(block) + sizeof(BlockHeader) + block->size);
    if ((unsigned char *)next < poolList->base + POOL_SIZE && next->free)
    {
        block->size += sizeof(BlockHeader) + next->size;
        block->next = next->next;
        if (next->next)
            next->next->prev = block;
    }

    // backward coalescing
    if (block->prev && block->prev->free)
    {
        BlockHeader *prev = block->prev;
        prev->size += sizeof(BlockHeader) + block->size;
        prev->next = block->next;
        if (block->next)
            block->next->prev = prev;
        block = prev;
    }

    insertBlock(block);
}

// --- Debug display across pools ---
void displayAllPools()
{
    cout << "\n=== Allocator State Across Pools ===\n";
    PoolHeader *pool = poolList;
    int poolIndex = 0;

    while (pool)
    {
        cout << "Pool " << poolIndex << ":\n";
        BlockHeader *current = reinterpret_cast<BlockHeader *>(pool->base);
        int blockIndex = 0;
        while ((unsigned char *)current < pool->base + POOL_SIZE)
        {
            int cls = getClass(current->size);
            cout << "  Block " << blockIndex
                 << " | size: " << current->size
                 << " | free: " << (current->free ? "yes" : "no")
                 << " | class: " << cls
                 << endl;

            current = reinterpret_cast<BlockHeader *>(
                reinterpret_cast<unsigned char *>(current) + sizeof(BlockHeader) + current->size);
            blockIndex++;
        }
        pool = pool->next;
        poolIndex++;
    }
    cout << "====================================\n";
}

// --- Demo ---
int main()
{
    initPool();
    void *a = myMalloc(900000); 
    void *b = myMalloc(200000); 
    void *p1 = myMalloc(40);
    void *p2 = myMalloc(200);
    void *p3 = myMalloc(800);

    displayAllPools();

    myFree(p1);
    myFree(p2);
    myFree(p3);

    displayAllPools();

    return 0;
}