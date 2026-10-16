#ifndef LIB_DS_H
#define LIB_DS_H

#include <stddef.h>

#define TITLE_MAX 128
#define NAME_MAX  64

extern int SLOTS;

typedef struct loan {
    int sid;
    int bid;
    struct loan *next;
} loan_t;

struct member_activity;

typedef struct book {
    int  bid;
    int  gid;
    char title[TITLE_MAX];
    int sum_scores;
    int n_reviews;
    int avg;
    int lost_flag;
    int on_loan;
    int heap_pos;
    struct book *prev_in_genre;
    struct book *next_in_genre;
    struct book *next_global;
} book_t;

typedef struct member {
    int  sid;
    char name[NAME_MAX];
    loan_t loans_head;
    struct member *prev_all;
    struct member *next_all;
    struct member_activity *activity;
} member_t;

typedef struct book_node {
    char title[TITLE_MAX];
    book_t *book;
    struct book_node *lc;
    struct book_node *rc;
    int height;
} book_node_t;

typedef struct rec_heap {
    book_t **data;
    int size;
    int capacity;
} rec_heap_t;

typedef struct member_activity {
    int sid;
    int loans_count;
    int reviews_count;
    int score_sum;
    struct member *member_ref;
    struct member_activity *next;
} member_activity_t;

typedef struct genre {
    int  gid;
    char name[NAME_MAX];
    book_t book_sentinel;
    int lost_count;
    int invalid_count;
    int     display_count;
    book_t **display;
    struct genre *next_by_gid;
} genre_t;

typedef struct library {
    genre_t  *genres;
    member_t *members;
    book_t   *books;
    book_node_t *book_index;
    rec_heap_t *recommendations;
    member_activity_t *activity_head;
    int total_books;
    int total_members;
    int active_loans;
    int total_loans;
    long long total_scores_sum;
    int total_reviews;
} library_t;

#endif
