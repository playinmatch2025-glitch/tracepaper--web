#ifndef tp_table_h
#define tp_table_h

#include "common.h"
#include "value.h"

// ch.20: open-addressing hash table with tombstones, keyed by interned strings.
typedef struct {
  ObjString* key;
  Value value;
} Entry;

typedef struct {
  int count;  // live entries + tombstones
  int capacity;
  Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void tableRemoveWhite(Table* table);  // ch.26: weak references for the string pool
void markTable(Table* table);

#endif
