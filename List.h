#ifndef LIST_H
#define LIST_H

typedef struct List {
    int* data;
    int size;
    int capacity;
} List;

List* list_create(void);
void list_init(List* list, int capacity);
void list_add(List* list, int value);
void list_remove(List* list, int val);
void list_free(List* list);
int list_contains(List* list, int val);
int list_is_empty(List* list);

#endif // LIST_H