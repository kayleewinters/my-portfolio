/* COP 3502C PA6
This program is written by: Kaylee Winters */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------- Global Modes ---------- */
typedef enum {
    MODE_ADOPTION = 0, /* Max-heap by adoption_key (higher is better) */
    MODE_TRIAGE = 1    /* Min-heap by triage_key (lower is more urgent)*/
} Mode;

/* ---------- Core Cat Record ---------- */
typedef struct Cat {
    char *name;         /* dynamically allocated */
    char *breed;        /* dynamically allocated */
    int age;
    int friendliness;
    int health;
    int arrival_id;     /* strictly increasing when ADDed */
    int quarantine;     /* 1 => cannot be adopted */
    double key;         /* cached priority value for active mode */
} Cat;

/* ---------- Array-Based Binary Heap of Cat* ---------- */
typedef struct {
    Cat **arr;
    int size;
    int capacity;
    Mode mode;
} CatHeap;

/* ---------- Global Shelter State ---------- */
typedef struct {
    Mode mode;
    char *featured_breed;
    double alpha;
    int next_arrival_id;
    CatHeap heap;
} Shelter;

/* ========== Internal Heap Helpers ========== */

void free_cat(Cat *c) {
    if (c != NULL) {
        if (c->name != NULL) free(c->name);
        if (c->breed != NULL) free(c->breed);
        free(c);
    }
}

int cmp_cats(const Cat *a, const Cat *b, Mode mode) {
    double diff = a->key - b->key;
    if (fabs(diff) > 1e-9) {
        if (mode == MODE_ADOPTION) return (diff > 0); /* Higher key is better */
        else return (diff < 0); /* Lower key is more urgent */
    }
    int name_cmp = strcmp(a->name, b->name);
    if (name_cmp != 0) return (name_cmp < 0);
    return (a->arrival_id < b->arrival_id);
}

void heapify_up(CatHeap *heap, int idx) {
    while (idx > 0) {
        int p = (idx - 1) / 2;
        if (cmp_cats(heap->arr[idx], heap->arr[p], heap->mode)) {
            Cat *tmp = heap->arr[idx];
            heap->arr[idx] = heap->arr[p];
            heap->arr[p] = tmp;
            idx = p;
        } else break;
    }
}

void heapify_down(CatHeap *heap, int idx, int limit) {
    while (1) {
        int l = 2 * idx + 1;
        int r = 2 * idx + 2;
        int best = idx;
        if (l < limit && cmp_cats(heap->arr[l], heap->arr[best], heap->mode)) best = l;
        if (r < limit && cmp_cats(heap->arr[r], heap->arr[best], heap->mode)) best = r;
        if (best != idx) {
            Cat *tmp = heap->arr[idx];
            heap->arr[idx] = heap->arr[best];
            heap->arr[best] = tmp;
            idx = best;
        } else break;
    }
}

/* ========== Required Function Implementations ========== */

int find_cat_index(CatHeap *heap, char *name) {
    for (int i = 0; i < heap->size; i++) {
        if (strcmp(heap->arr[i]->name, name) == 0) return i;
    }
    return -1;
}

double compute_adoption_key(Cat *c, Shelter *S) {
    double base = 1.6 * c->friendliness + 1.1 * c->health - 0.7 * c->age;
    double mult = 1.0;
    if (S->featured_breed != NULL && strcmp(c->breed, S->featured_breed) == 0) {
        mult = S->alpha;
    }
    return (base * mult) + (-1e-6 * c->arrival_id);
}

double compute_triage_key(Cat *c) {
    double age_bonus = (c->age > 12) ? (double)(c->age - 12) : 0.0;
    return (100.0 - c->health) * 2.0 + age_bonus * 1.0 - 0.05 * c->friendliness;
}

void recompute_all_keys_and_build(Shelter *S) {
    for (int i = 0; i < S->heap.size; i++) {
        if (S->mode == MODE_ADOPTION) S->heap.arr[i]->key = compute_adoption_key(S->heap.arr[i], S);
        else S->heap.arr[i]->key = compute_triage_key(S->heap.arr[i]);
    }
    for (int i = (S->heap.size / 2) - 1; i >= 0; i--) {
        heapify_down(&S->heap, i, S->heap.size);
    }
}

void cmd_add(Shelter *S, char *name, char *breed, int age, int friendl, int health) {
    if (find_cat_index(&S->heap, name) != -1) {
        printf("Name %s already exists.\n", name);
        return;
    }
    Cat *c = (Cat *)malloc(sizeof(Cat));
    c->name = (char *)malloc(strlen(name) + 1);
    strcpy(c->name, name);
    c->breed = (char *)malloc(strlen(breed) + 1);
    strcpy(c->breed, breed);
    c->age = age; c->friendliness = friendl; c->health = health;
    c->arrival_id = S->next_arrival_id++;
    c->quarantine = 0;
    c->key = (S->mode == MODE_ADOPTION) ? compute_adoption_key(c, S) : compute_triage_key(c);

    if (S->heap.size >= S->heap.capacity) {
        S->heap.capacity = (S->heap.capacity == 0) ? 16 : S->heap.capacity * 2;
        S->heap.arr = (Cat **)realloc(S->heap.arr, S->heap.capacity * sizeof(Cat *));
    }
    S->heap.arr[S->heap.size] = c;
    S->heap.size++;
    printf("Added %s.\n", name);
}

void cmd_update(Shelter *S, char *name, char *field, int new_val) {
    int idx = find_cat_index(&S->heap, name);
    if (idx == -1) { printf("Cat %s not found.\n", name); return; }
    Cat *c = S->heap.arr[idx];
    if (strcmp(field, "QUARANTINE") == 0) {
        c->quarantine = new_val;
        printf("Updated %s: QUARANTINE=%d.\n", name, new_val);
        return;
    }
    int valid = 1;
    if (strcmp(field, "AGE") == 0) c->age = new_val;
    else if (strcmp(field, "FRIEND") == 0) c->friendliness = new_val;
    else if (strcmp(field, "HEALTH") == 0) c->health = new_val;
    else { printf("Unknown field %s.\n", field); valid = 0; }

    if (valid) {
        c->key = (S->mode == MODE_ADOPTION) ? compute_adoption_key(c, S) : compute_triage_key(c);
        int p = (idx - 1) / 2;
        if (idx > 0 && cmp_cats(S->heap.arr[idx], S->heap.arr[p], S->heap.mode)) heapify_up(&S->heap, idx);
        else heapify_down(&S->heap, idx, S->heap.size);
        printf("Updated %s: %s=%d. Priority adjusted.\n", name, field, new_val);
    }
}

void cmd_remove(Shelter *S, char *name) {
    int idx = find_cat_index(&S->heap, name);
    if (idx == -1) { printf("Cat %s not found.\n", name); return; }
    Cat *c = S->heap.arr[idx];
    S->heap.arr[idx] = S->heap.arr[S->heap.size - 1];
    S->heap.size--;
    if (idx < S->heap.size) {
        int p = (idx - 1) / 2;
        if (idx > 0 && cmp_cats(S->heap.arr[idx], S->heap.arr[p], S->heap.mode)) heapify_up(&S->heap, idx);
        else heapify_down(&S->heap, idx, S->heap.size);
    }
    printf("Removed %s.\n", c->name);
    free_cat(c);
}

void cmd_peek(Shelter *S) {
    if (S->heap.size == 0) { printf("No cats available.\n"); return; }
    Cat *c = S->heap.arr[0];
    const char *m = (S->mode == MODE_ADOPTION) ? "ADOPTION" : "TRIAGE";
    printf("Top[%s]: [%s] (key=%.2f, name=%s, breed=%s, age=%d, friend=%d, health=%d)\n", m, m, c->key, c->name, c->breed, c->age, c->friendliness, c->health);
}

void cmd_serve(Shelter *S) {
    if (S->heap.size == 0) { printf("No cats available.\n"); return; }
    if (S->mode == MODE_ADOPTION) {
        Cat **skipped = (Cat **)malloc(S->heap.size * sizeof(Cat *));
        int skip_count = 0; Cat *target = NULL;
        while (S->heap.size > 0) {
            Cat *top = S->heap.arr[0];
            S->heap.arr[0] = S->heap.arr[S->heap.size - 1]; S->heap.size--;
            heapify_down(&S->heap, 0, S->heap.size);
            if (top->quarantine == 0) { target = top; break; }
            skipped[skip_count++] = top;
        }
        for (int i = 0; i < skip_count; i++) {
            S->heap.arr[S->heap.size] = skipped[i];
            heapify_up(&S->heap, S->heap.size); S->heap.size++;
        }
        free(skipped);
        if (target == NULL) printf("No adoptable cats available.\n");
        else {
            printf("Serve now: %s (key=%.2f, name=%s, breed=%s, age=%d, friend=%d, health=%d)\n", target->name, target->key, target->name, target->breed, target->age, target->friendliness, target->health);
            free_cat(target);
        }
    } else {
        Cat *target = S->heap.arr[0];
        S->heap.arr[0] = S->heap.arr[S->heap.size - 1]; S->heap.size--;
        heapify_down(&S->heap, 0, S->heap.size);
        printf("Serve now: %s (key=%.2f, name=%s, breed=%s, age=%d, friend=%d, health=%d)\n", target->name, target->key, target->name, target->breed, target->age, target->friendliness, target->health);
        free_cat(target);
    }
}

void cmd_mode(Shelter *S, char *mode_str) {
    if (strcmp(mode_str, "ADOPTION") == 0) S->mode = MODE_ADOPTION;
    else if (strcmp(mode_str, "TRIAGE") == 0) S->mode = MODE_TRIAGE;
    else { printf("Unknown mode %s\n", mode_str); return; }
    S->heap.mode = S->mode;
    printf("Mode set to %s. Rebuilding priorities...\n", mode_str);
    recompute_all_keys_and_build(S);
}

void cmd_featured(Shelter *S, char *breed, double alpha) {
    if (strcmp(breed, "NONE") == 0) {
        if (S->featured_breed != NULL) free(S->featured_breed);
        S->featured_breed = NULL; S->alpha = 1.0;
        printf("Featured breed cleared. Rebuilding priorities...\n");
    } else {
        if (S->featured_breed != NULL) free(S->featured_breed);
        S->featured_breed = (char *)malloc(strlen(breed) + 1);
        strcpy(S->featured_breed, breed); S->alpha = alpha;
        printf("Featured breed set to %s with alpha=%.2f. Rebuilding priorities...\n", breed, alpha);
    }
    recompute_all_keys_and_build(S);
}

void cmd_print(Shelter *S, int k) {
    if (S->heap.size == 0) { printf("No cats available.\n"); return; }
    int limit = (k < S->heap.size) ? k : S->heap.size;
    Cat **copy = (Cat **)malloc(S->heap.size * sizeof(Cat *));
    for (int i = 0; i < S->heap.size; i++) copy[i] = S->heap.arr[i];
    CatHeap temp; temp.arr = copy; temp.size = S->heap.size; temp.mode = S->heap.mode;
    const char *m = (S->mode == MODE_ADOPTION) ? "ADOPTION" : "TRIAGE";
    for (int i = 1; i <= limit; i++) {
        Cat *top = temp.arr[0];
        printf("[%d] %s (key=%.2f, %s)\n", i, top->name, top->key, m);
        temp.arr[0] = temp.arr[temp.size - 1]; temp.size--;
        heapify_down(&temp, 0, temp.size);
    }
    free(copy);
}

int main(void) {
    Shelter S; S.mode = MODE_ADOPTION; S.featured_breed = NULL; S.alpha = 1.0; S.next_arrival_id = 1;
    S.heap.arr = NULL; S.heap.size = 0; S.heap.capacity = 0; S.heap.mode = MODE_ADOPTION;
    int q; if (scanf("%d", &q) != 1) return 0;
    while (q--) {
        char cmd[64]; if (scanf("%s", cmd) != 1) break;
        if (strcmp(cmd, "ADD") == 0) {
            char n[64], b[64]; int ag, f, h; scanf("%s %s %d %d %d", n, b, &ag, &f, &h);
            cmd_add(&S, n, b, ag, f, h);
        } else if (strcmp(cmd, "UPDATE") == 0) {
            char n[64], fld[64]; int v; scanf("%s %s %d", n, fld, &v);
            cmd_update(&S, n, fld, v);
        } else if (strcmp(cmd, "REMOVE") == 0) {
            char n[64]; scanf("%s", n); cmd_remove(&S, n);
        } else if (strcmp(cmd, "PEEK") == 0) cmd_peek(&S);
        else if (strcmp(cmd, "SERVE") == 0) cmd_serve(&S);
        else if (strcmp(cmd, "MODE") == 0) {
            char m[64]; scanf("%s", m); cmd_mode(&S, m);
        } else if (strcmp(cmd, "FEATURED") == 0) {
            char b[64]; double a; scanf("%s %lf", b, &a); cmd_featured(&S, b, a);
        } else if (strcmp(cmd, "PRINT") == 0) {
            int k; scanf("%d", &k); cmd_print(&S, k);
        } else if (strcmp(cmd, "QUIT") == 0) break;
    }
    for (int i = 0; i < S.heap.size; i++) free_cat(S.heap.arr[i]);
    if (S.heap.arr != NULL) free(S.heap.arr);
    if (S.featured_breed != NULL) free(S.featured_breed);
    return 0;
}
