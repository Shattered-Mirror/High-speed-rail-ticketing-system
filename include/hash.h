#ifndef HASH_H
#define HASH_H


typedef struct HashTable HashTable;

HashTable *ht_create(int buckets);
void ht_free(HashTable *ht);
void ht_clear(HashTable *ht);
void ht_insert(HashTable *ht, const char *key, int idx);
int ht_find(HashTable *ht, const char *key);

#endif 