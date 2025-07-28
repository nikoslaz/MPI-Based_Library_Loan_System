#include "List.h"
#include <stdio.h>
#include <stdlib.h>

List* list_create(void) {
    List* list = (List*)malloc(sizeof(List));
    if (list == NULL) {
        fprintf(stderr, "List creation failed\n");
        exit(EXIT_FAILURE);
    }
    list->data = NULL;
    list->size = 0;
    list->capacity = 0;
    return list;
}

void list_init(List* list, int capacity) {
    list->data = (int*)malloc(capacity * sizeof(int));
    if (list->data == NULL) {
        fprintf(stderr, "List init failed\n");
        exit(EXIT_FAILURE);
    }
    list->size = 0;
    list->capacity = capacity;
}   

int list_contains(List* list, int val) {
    for (int i = 0; i < list->size; i++) {
        if (list->data[i] == val) {
            return 1;
        }
    }
    return 0;
}

void list_add(List* list, int value) {
    if (list->size >= list->capacity) {
        list->capacity = (list->capacity == 0) ? 1024 : list->capacity * 2;
        list->data = (int*)realloc(list->data, list->capacity * sizeof(int));
        if (list->data == NULL) {
            fprintf(stderr, "List realloc failed\n");
            exit(EXIT_FAILURE);
        }
    }
    list->data[list->size++] = value;
}


void list_remove(List* list, int val) {
    for (int i = 0; i < list->size; i++) {
        if (list->data[i] == val) {
            for (int j = i; j < list->size - 1; j++) {
                list->data[j] = list->data[j+1];
            }
            list->size--;
            break;
        }
    }
}

int list_is_empty(List* list) {
    return list->size == 0;
}

void list_free(List* list) {
    if (list == NULL) {
        return;
    }
    if (list->data) {
        free(list->data);
    }
    list->data = NULL;
    list->size = 0;
    list->capacity = 0;
}