#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

typedef struct HashNode {
	char *key;
	int idx;
	struct HashNode *next;
} HashNode;

struct HashTable {
	HashNode **buckets;
	int bucket_count;
};

static unsigned long hash_str(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = (unsigned char)*str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

HashTable *ht_create(int buckets)
{
	HashTable *ht = malloc(sizeof(HashTable));
	if (!ht) { perror("malloc"); return NULL; }
	ht->bucket_count = buckets;
	ht->buckets = calloc(buckets, sizeof(HashNode*));
	if (!ht->buckets) { free(ht); perror("calloc"); return NULL; }
	return ht;
}

void ht_free(HashTable *ht)
{
	if (!ht)
		return;

	for (int i = 0; i < ht->bucket_count; ++i) {
		HashNode *n = ht->buckets[i];
		while (n) {
			HashNode *t = n->next;
			free(n->key);
			free(n);
			n = t;
		}
	}

	free(ht->buckets);
	free(ht);
}

void ht_clear(HashTable *ht)
{
	if (!ht)
		return;

	for (int i = 0; i < ht->bucket_count; ++i) {
		HashNode *n = ht->buckets[i];
		while (n) {
			HashNode *t = n->next;
			free(n->key);
			free(n);
			n = t;
		}
		ht->buckets[i] = NULL;
	}
}

void ht_insert(HashTable *ht, const char *key, int idx)
{
	if (!ht || !key)
		return;

	unsigned long h = hash_str(key) % ht->bucket_count;
	HashNode *n = malloc(sizeof(HashNode));
	if (!n)
		return;

	n->key = strdup(key);
	n->idx = idx;
	n->next = ht->buckets[h];
	ht->buckets[h] = n;
}

int ht_find(HashTable *ht, const char *key)
{
	if (!ht || !key)
		return -1;

	unsigned long h = hash_str(key) % ht->bucket_count;
	HashNode *n = ht->buckets[h];

	while (n) {
		if (strcmp(n->key, key) == 0)
			return n->idx;
		n = n->next;
	}

	return -1;
}