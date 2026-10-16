#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "Library_simple.h"

#define REC_HEAP_CAPACITY 64

// Global library instance - central data structure that holds all genres, members, and books
library_t *library = NULL;
// Global SLOTS variable - total number of display slots for the library (set by S command)
int SLOTS = 0;

// Function prototypes
library_t* create_library();
void destroy_library();
genre_t* create_genre(int gid, char* name);
book_t* create_book(int bid, int gid, char* title);
member_t* create_member(int sid, char* name);
loan_t* create_loan(int sid, int bid);

// Genre management
genre_t* find_genre(int gid);
genre_t* insert_genre(genre_t* genre);
void add_genre(int gid, char* name);

// Book management
book_t* find_book(int bid);
void add_book(int bid, int gid, char* title);
void update_book_title_command(int bid, char* new_title);
void remove_book_from_genre_list(book_t* book);
void insert_book_in_genre_list(book_t* book);
void update_book_position(book_t* book);

// Member management
member_t* find_member(int sid);
member_t* insert_member(member_t* member);
void add_member(int sid, char* name);

// Loan management
loan_t* find_loan(member_t* member, int bid);
void add_loan(int sid, int bid);
int remove_loan(int sid, int bid);

// Return and rating
void return_book(int sid, int bid, char* score_str, char* status);

// Display calculation
void calculate_display();
void print_genre(int gid);
void print_member_loans(int sid);
void print_display();
void print_stats();

// Utility functions
char* strdup(const char* s);
void free_string(char* s);
int parse_score(char* score_str, int* score_out);
book_node_t* insert_book_index(book_node_t* root, book_t* book, int* status);
book_node_t* remove_book_index(book_node_t* root, const char* title);
book_node_t* find_book_by_title(book_node_t* root, const char* title);
void free_book_index(book_node_t* root);
rec_heap_t* create_rec_heap(int capacity);
void destroy_rec_heap(rec_heap_t* heap);
void rec_heap_insert_or_update(book_t* book);
void rec_heap_remove(book_t* book);
member_activity_t* create_member_activity(int sid);
void destroy_member_activity_list(member_activity_t* head);
void increment_member_loans(member_t* member);
void increment_member_reviews(member_t* member, int score);
void print_active_members();
void print_top_books(int k);
void update_system_stats_on_book_add();
void update_system_stats_on_member_add();
void update_system_stats_on_loan_add();
void update_system_stats_on_loan_remove();
void update_system_stats_on_review(int score);
void print_system_stats();
void reset_library();
int extract_quoted_argument(const char* line, char* buffer, size_t buffer_size);
void sanitize_argument(char* text);

// Main execution
void execute_command(char* line);
void process_input_file(FILE* file);



// Creates and initializes a new library instance
library_t* create_library() {
    library_t* lib = malloc(sizeof(library_t));
    lib->genres = NULL;
    lib->members = NULL;
    lib->books = NULL;
    lib->book_index = NULL;
    lib->recommendations = create_rec_heap(REC_HEAP_CAPACITY);
    lib->activity_head = NULL;
    lib->total_books = 0;
    lib->total_members = 0;
    lib->active_loans = 0;
    lib->total_loans = 0;
    lib->total_scores_sum = 0;
    lib->total_reviews = 0;
    return lib;
}

// Destroys the entire library and frees all allocated memory
void destroy_library() {
    if (!library) return;
    
    // Free all genres and their books
    genre_t* genre = library->genres;
    while (genre) {
        genre_t* next_genre = genre->next_by_gid;
        
        // Free display array if exists
        if (genre->display) {
            free(genre->display);
        }
        
        // Free books in genre (standard doubly linked list starting at sentinel->next)
        book_t* book = genre->book_sentinel.next_in_genre;
        while (book) {
            book_t* next_book = book->next_in_genre;
            free(book);
            book = next_book;
        }
        
        // genre->name is an array, no need to free
        free(genre);
        genre = next_genre;
    }
    
    // Free all members and their loans
    member_t* member = library->members;
    while (member) {
        member_t* next_member = member->next_all;
        
        // Free loans (singly linked list with sentinel)
        loan_t* loan = member->loans_head.next;
        while (loan) {
            loan_t* next_loan = loan->next;
            free(loan);
            loan = next_loan;
        }
        
        // member->name is an array, no need to free
        free(member);
        member = next_member;
    }
    
    // Free global books list
    // (Books are already freed from genre lists, no need to free again)
    free_book_index(library->book_index);
    destroy_rec_heap(library->recommendations);
    destroy_member_activity_list(library->activity_head);
    
    free(library);
    library = NULL;
}

// Creates a new genre with circular doubly linked list for books
genre_t* create_genre(int gid, char* name) {
    genre_t* genre = malloc(sizeof(genre_t));
    genre->gid = gid;
    // Copy name safely with bounds checking
    strncpy(genre->name, name, NAME_MAX - 1);
    genre->name[NAME_MAX - 1] = '\0';
    
    // Initialize doubly linked list with sentinel
    genre->book_sentinel.prev_in_genre = NULL;
    genre->book_sentinel.next_in_genre = NULL;
    genre->book_sentinel.gid = gid;
    
    genre->lost_count = 0;
    genre->invalid_count = 0;
    // Initialize display fields
    genre->display_count = 0;
    genre->display = NULL;
    genre->next_by_gid = NULL;
    return genre;
}

// Creates a new book with initial rating statistics
book_t* create_book(int bid, int gid, char* title) {
    book_t* book = malloc(sizeof(book_t));
    book->bid = bid;
    book->gid = gid;
    // Copy title safely with bounds checking
    strncpy(book->title, title, TITLE_MAX - 1);
    book->title[TITLE_MAX - 1] = '\0';
    
    // Initialize rating statistics
    book->sum_scores = 0;
    book->n_reviews = 0;
    book->avg = 0;
    book->lost_flag = 0;
    book->on_loan = 0;
    book->heap_pos = -1;
    
    // Initialize list pointers
    book->prev_in_genre = NULL;
    book->next_in_genre = NULL;
    book->next_global = NULL;
    return book;
}

// Creates a new member with sentinel node for loan management
member_t* create_member(int sid, char* name) {
    member_t* member = malloc(sizeof(member_t));
    member->sid = sid;
    // Copy name safely with bounds checking
    strncpy(member->name, name, NAME_MAX - 1);
    member->name[NAME_MAX - 1] = '\0';
    
    // Initialize sentinel node for loans
    member->loans_head.sid = sid;
    member->loans_head.bid = -1;
    member->loans_head.next = NULL;
    
    // Initialize doubly linked list pointers for global member list
    member->prev_all = NULL;
    member->next_all = NULL;
    member->activity = NULL;
    return member;
}

// Creates a new loan record
loan_t* create_loan(int sid, int bid) {
    loan_t* loan = malloc(sizeof(loan_t));
    loan->sid = sid;
    loan->bid = bid;
    loan->next = NULL;
    return loan;
}

// Finds a genre by its gid using binary search-like traversal
genre_t* find_genre(int gid) {
    genre_t* genre = library->genres;
    while (genre && genre->gid < gid) {
        genre = genre->next_by_gid;
    }
    return (genre && genre->gid == gid) ? genre : NULL;
}

genre_t* insert_genre(genre_t* new_genre) {
    if (!library->genres || library->genres->gid > new_genre->gid) {
        new_genre->next_by_gid = library->genres;
        library->genres = new_genre;
        return new_genre;
    }
    
    genre_t* current = library->genres;
    while (current->next_by_gid && current->next_by_gid->gid < new_genre->gid) {
        current = current->next_by_gid;
    }
    
    new_genre->next_by_gid = current->next_by_gid;
    current->next_by_gid = new_genre;
    return new_genre;
}

void add_genre(int gid, char* name) {
    if (!name || name[0] == '\0') {
        printf("IGNORED\n");
        return;
    }
    if (find_genre(gid)) {
        printf("IGNORED\n");
        return;
    }
    
    genre_t* genre = create_genre(gid, name);
    insert_genre(genre);
    printf("DONE\n");
}

book_t* find_book(int bid) {
    book_t* book = library->books;
    while (book) {
        if (book->bid == bid) return book;
        book = book->next_global;
    }
    return NULL;
}

void add_book(int bid, int gid, char* title) {
    if (!title || title[0] == '\0') {
        printf("IGNORED\n");
        return;
    }
    if (find_book(bid)) {
        printf("IGNORED\n");
        return;
    }
    
    if (find_book_by_title(library->book_index, title)) {
        printf("IGNORED\n");
        return;
    }
    
    genre_t* genre = find_genre(gid);
    if (!genre) {
        printf("IGNORED\n");
        return;
    }
    
    book_t* book = create_book(bid, gid, title);
    if (!book) {
        printf("IGNORED\n");
        return;
    }
    int inserted = 0;
    library->book_index = insert_book_index(library->book_index, book, &inserted);
    if (!inserted) {
        free(book);
        printf("IGNORED\n");
        return;
    }
    
    book->next_global = library->books;
    library->books = book;
    
    insert_book_in_genre_list(book);
    update_system_stats_on_book_add();
    
    printf("DONE\n");
}

void update_book_title_command(int bid, char* new_title) {
    if (!new_title) {
        printf("IGNORED\n");
        return;
    }
    
    book_t* book = find_book(bid);
    if (!book || book->lost_flag) {
        printf("IGNORED\n");
        return;
    }
    
    if (strcmp(book->title, new_title) == 0) {
        printf("DONE\n");
        return;
    }
    
    if (find_book_by_title(library->book_index, new_title)) {
        printf("IGNORED\n");
        return;
    }
    
    char old_title[TITLE_MAX];
    strncpy(old_title, book->title, TITLE_MAX - 1);
    old_title[TITLE_MAX - 1] = '\0';
    
    library->book_index = remove_book_index(library->book_index, old_title);
    strncpy(book->title, new_title, TITLE_MAX - 1);
    book->title[TITLE_MAX - 1] = '\0';
    
    int inserted = 0;
    library->book_index = insert_book_index(library->book_index, book, &inserted);
    if (!inserted) {
        strncpy(book->title, old_title, TITLE_MAX - 1);
        book->title[TITLE_MAX - 1] = '\0';
        int revert = 0;
        library->book_index = insert_book_index(library->book_index, book, &revert);
        printf("IGNORED\n");
        return;
    }
    
    printf("DONE\n");
}

member_t* find_member(int sid) {
    member_t* member = library->members;
    while (member && member->sid < sid) {
        member = member->next_all;
    }
    return (member && member->sid == sid) ? member : NULL;
}

member_t* insert_member(member_t* new_member) {
    if (!library->members || library->members->sid > new_member->sid) {
        new_member->next_all = library->members;
        if (library->members) library->members->prev_all = new_member;
        library->members = new_member;
        return new_member;
    }
    
    member_t* current = library->members;
    while (current->next_all && current->next_all->sid < new_member->sid) {
        current = current->next_all;
    }
    
    new_member->next_all = current->next_all;
    new_member->prev_all = current;
    if (current->next_all) current->next_all->prev_all = new_member;
    current->next_all = new_member;
    return new_member;
}

void add_member(int sid, char* name) {
    if (!name || name[0] == '\0') {
        printf("IGNORED\n");
        return;
    }
    if (find_member(sid)) {
        printf("IGNORED\n");
        return;
    }
    
    member_t* member = create_member(sid, name);
    member_activity_t* activity = create_member_activity(sid);
    if (!member || !activity) {
        if (activity) free(activity);
        if (member) free(member);
        printf("IGNORED\n");
        return;
    }
    
    insert_member(member);
    activity->member_ref = member;
    activity->next = library->activity_head;
    library->activity_head = activity;
    member->activity = activity;
    update_system_stats_on_member_add();
    
    printf("DONE\n");
}

loan_t* find_loan(member_t* member, int bid) {
    loan_t* loan = member->loans_head.next;
    while (loan) {
        if (loan->bid == bid) return loan;
        loan = loan->next;
    }
    return NULL;
}

void add_loan(int sid, int bid) {
    member_t* member = find_member(sid);
    if (!member) {
        printf("IGNORED\n");
        return;
    }
    
    if (find_loan(member, bid)) {
        printf("IGNORED\n");
        return;
    }
    
    book_t* book = find_book(bid);
    if (!book) {
        printf("IGNORED\n");
        return;
    }
    
    if (book->on_loan || book->lost_flag) {
        printf("IGNORED\n");
        return;
    }
    
    loan_t* loan = create_loan(sid, bid);
    loan->next = member->loans_head.next;
    member->loans_head.next = loan;
    book->on_loan = 1;
    increment_member_loans(member);
    update_system_stats_on_loan_add();
    printf("DONE\n");
}

int remove_loan(int sid, int bid) {
    member_t* member = find_member(sid);
    if (!member) return 0;
    
    loan_t* prev = &member->loans_head;
    loan_t* loan = member->loans_head.next;
    
    while (loan && loan->bid != bid) {
        prev = loan;
        loan = loan->next;
    }
    
    if (loan) {
        prev->next = loan->next;
        free(loan);
        return 1;
    }
    return 0;
}

void remove_book_from_genre_list(book_t* book) {
    genre_t* genre = find_genre(book->gid);
    if (!genre) return;
    
    if (book->prev_in_genre) {
        book->prev_in_genre->next_in_genre = book->next_in_genre;
    } else {
        genre->book_sentinel.next_in_genre = book->next_in_genre;
    }

    if (book->next_in_genre) {
        book->next_in_genre->prev_in_genre = book->prev_in_genre;
    }

    book->prev_in_genre = NULL;
    book->next_in_genre = NULL;
}

// Inserts a book into its genre's sorted circular doubly linked list
void insert_book_in_genre_list(book_t* book) {
    genre_t* genre = find_genre(book->gid);
    if (!genre) return;
    
    // Find position in sorted list (descending by avg, then by bid for tie-breaking)
    book_t* current = genre->book_sentinel.next_in_genre;
    book_t* prev = NULL;
    while (current && 
           (current->avg > book->avg || 
            (current->avg == book->avg && current->bid < book->bid))) {
        prev = current;
        current = current->next_in_genre;
    }
    
    book->prev_in_genre = prev;
    book->next_in_genre = current;
    
    if (prev) {
        prev->next_in_genre = book;
    } else {
        genre->book_sentinel.next_in_genre = book;
    }
    
    if (current) {
        current->prev_in_genre = book;
    }
}

// Repositions a book in its genre's sorted list after rating change
void update_book_position(book_t* book) {
    remove_book_from_genre_list(book);
    insert_book_in_genre_list(book);
}

// Handles book return with rating and status processing
void return_book(int sid, int bid, char* score_str, char* status) {
    member_t* member = find_member(sid);
    if (!member) {
        printf("IGNORED\n");
        return;
    }
    
    if (!find_loan(member, bid)) {
        printf("IGNORED\n");
        return;
    }
    
    book_t* book = find_book(bid);
    if (!book) {
        printf("IGNORED\n");
        return;
    }
    genre_t* genre = find_genre(book->gid);
    
    if (strcmp(status, "lost") == 0) {
        if (strcmp(score_str, "NA") != 0) {
            printf("IGNORED\n");
            return;
        }
        
        if (remove_loan(sid, bid)) {
            update_system_stats_on_loan_remove();
        }
        rec_heap_remove(book);
        library->book_index = remove_book_index(library->book_index, book->title);
        book->lost_flag = 1;
        book->on_loan = 0;
        if (genre) genre->lost_count++;
        printf("DONE\n");
        return;
    }
    
    if (strcmp(status, "ok") != 0) {
        printf("IGNORED\n");
        return;
    }
    
    if (strcmp(score_str, "NA") == 0) {
        if (remove_loan(sid, bid)) {
            update_system_stats_on_loan_remove();
        }
        book->on_loan = 0;
        printf("DONE\n");
        return;
    }
    
    int score = 0;
    if (!parse_score(score_str, &score) || score < 0) {
        if (genre) genre->invalid_count++;
        printf("IGNORED\n");
        return;
    }
    
    if (score > 10) {
        if (genre) genre->invalid_count++;
        printf("IGNORED\n");
        return;
    }
    
    if (remove_loan(sid, bid)) {
        update_system_stats_on_loan_remove();
    }
    book->sum_scores += score;
    book->n_reviews++;
    book->avg = book->sum_scores / book->n_reviews;
    book->on_loan = 0;
    
    increment_member_reviews(member, score);
    update_system_stats_on_review(score);
    update_book_position(book);
    rec_heap_insert_or_update(book);
    printf("DONE\n");
}

// Calculates display slots distribution using quota and largest remainder method
void calculate_display() {
    genre_t* genre = library->genres;
    while (genre) {
        if (genre->display) {
            free(genre->display);
            genre->display = NULL;
        }
        genre->display_count = 0;
        genre = genre->next_by_gid;
    }
    
    if (SLOTS == 0) {
        printf("DONE\n");
        return;
    }
    
    // Calculate points for each genre
    int total_valid = 0;
    genre = library->genres;
    while (genre) {
        int points = 0;
        book_t* book = genre->book_sentinel.next_in_genre;
        while (book) {
            if (book->n_reviews > 0 && book->lost_flag == 0) {
                points += book->sum_scores;
            }
            book = book->next_in_genre;
        }
        genre->display_count = points;
        total_valid += points;
        genre = genre->next_by_gid;
    }
    
    if (total_valid == 0) {
        printf("DONE\n");
        return;
    }
    
    // Calculate quota for proportional distribution
    int quota = total_valid / SLOTS;
    if (quota == 0) {
        printf("DONE\n");
        return;
    }
    
    // Calculate initial seats and remainders
    genre_t** genre_array = malloc(sizeof(genre_t*) * 1000);
    int* remainders = malloc(sizeof(int) * 1000);
    int genre_count = 0;
    
    genre = library->genres;
    while (genre) {
        genre_array[genre_count] = genre;
        int initial_seats = genre->display_count / quota;
        remainders[genre_count] = genre->display_count - (initial_seats * quota);
        genre->display_count = initial_seats;
        genre_count++;
        genre = genre->next_by_gid;
    }
    
    // Distribute remaining seats based on largest remainders
    int remaining_slots = SLOTS;
    for (int i = 0; i < genre_count; i++) {
        remaining_slots -= genre_array[i]->display_count;
    }
    
    // Sort by remainder (descending) and gid (ascending) for tie-breaking
    for (int i = 0; i < genre_count - 1; i++) {
        for (int j = i + 1; j < genre_count; j++) {
            if (remainders[j] > remainders[i] || 
                (remainders[j] == remainders[i] && genre_array[j]->gid < genre_array[i]->gid)) {
                genre_t* temp_genre = genre_array[i];
                genre_array[i] = genre_array[j];
                genre_array[j] = temp_genre;
                int temp_rem = remainders[i];
                remainders[i] = remainders[j];
                remainders[j] = temp_rem;
            }
        }
    }
    
    // Distribute remaining slots to top genres
    for (int i = 0; i < remaining_slots && i < genre_count; i++) {
        genre_array[i]->display_count++;
    }
    
    // Select books for display
    genre = library->genres;
    while (genre) {
        if (genre->display_count > 0) {
            genre->display = malloc(sizeof(book_t*) * genre->display_count);
            int count = 0;
            book_t* book = genre->book_sentinel.next_in_genre;
            while (book && count < genre->display_count) {
                if (book->lost_flag == 0) {
                    genre->display[count] = book;
                    count++;
                }
                book = book->next_in_genre;
            }
            genre->display_count = count;
        }
        genre = genre->next_by_gid;
    }
    
    free(genre_array);
    free(remainders);
    printf("DONE\n");
}

void print_genre(int gid) {
    genre_t* genre = find_genre(gid);
    if (!genre) {
        printf("IGNORED\n");
        return;
    }
    
    book_t* book = genre->book_sentinel.next_in_genre;
    while (book) {
        printf("%d, %d\n", book->bid, book->avg);
        book = book->next_in_genre;
    }
}

void print_member_loans(int sid) {
    member_t* member = find_member(sid);
    if (!member) {
        printf("IGNORED\n");
        return;
    }
    
    printf("Loans:\n");
    loan_t* loan = member->loans_head.next;
    while (loan) {
        printf("%d\n", loan->bid);
        loan = loan->next;
    }
}

void print_display() {
    printf("Display:\n");
    
    genre_t* genre = library->genres;
    int has_display = 0;
    
    while (genre) {
        if (genre->display_count > 0 && genre->display) {
            has_display = 1;
            printf("%d:\n", genre->gid);
            for (int i = 0; i < genre->display_count; i++) {
                printf("%d, %d\n", genre->display[i]->bid, genre->display[i]->avg);
            }
        }
        genre = genre->next_by_gid;
    }
    
    if (!has_display) {
        printf("(empty)\n");
    }
}

void print_stats() {
    printf("SLOTS=%d\n", SLOTS);
    
    genre_t* genre = library->genres;
    while (genre) {
        int points = 0;
        book_t* book = genre->book_sentinel.next_in_genre;
        while (book) {
            if (book->n_reviews > 0 && book->lost_flag == 0) {
                points += book->sum_scores;
            }
            book = book->next_in_genre;
        }
        printf("%d: points=%d\n", genre->gid, points);
        genre = genre->next_by_gid;
    }
}

member_activity_t* create_member_activity(int sid) {
    member_activity_t* activity = malloc(sizeof(member_activity_t));
    if (!activity) return NULL;
    activity->sid = sid;
    activity->loans_count = 0;
    activity->reviews_count = 0;
    activity->score_sum = 0;
    activity->member_ref = NULL;
    activity->next = NULL;
    return activity;
}

void destroy_member_activity_list(member_activity_t* head) {
    while (head) {
        member_activity_t* next = head->next;
        free(head);
        head = next;
    }
}

void increment_member_loans(member_t* member) {
    if (!member || !member->activity) return;
    member->activity->loans_count++;
}

void increment_member_reviews(member_t* member, int score) {
    if (!member || !member->activity) return;
    member->activity->reviews_count++;
    member->activity->score_sum += score;
}

void print_active_members() {
    printf("Active Members:\n");
    if (!library) {
        printf("(empty)\n");
        return;
    }
    member_activity_t* node = library->activity_head;
    if (!node) {
        printf("(empty)\n");
        return;
    }
    
    // Find max(loans_count + reviews_count)
    int max_activity = 0;
    member_activity_t* current = node;
    while (current) {
        int activity = current->loans_count + current->reviews_count;
        if (activity > max_activity) {
            max_activity = activity;
        }
        current = current->next;
    }
    
    if (max_activity == 0) {
        printf("(empty)\n");
        return;
    }
    
    // Print only members with max activity
    while (node) {
        int activity = node->loans_count + node->reviews_count;
        if (activity == max_activity) {
            member_t* member = node->member_ref ? node->member_ref : find_member(node->sid);
            int avg_review = 0;
            if (node->reviews_count > 0) {
                avg_review = node->score_sum / node->reviews_count;
            }
            if (member) {
                printf("sid: %d, name: %s, loans: %d, reviews: %d\n", 
                       node->sid, member->name, node->loans_count, avg_review);
            } else {
                printf("sid: %d, loans: %d, reviews: %d\n", 
                       node->sid, node->loans_count, avg_review);
            }
        }
        node = node->next;
    }
}

static int node_height(book_node_t* node) {
    return node ? node->height : 0;
}

static int max_int(int a, int b) {
    return (a > b) ? a : b;
}

static book_node_t* rotate_right(book_node_t* y) {
    book_node_t* x = y->lc;
    book_node_t* T2 = x->rc;
    
    x->rc = y;
    y->lc = T2;
    
    y->height = max_int(node_height(y->lc), node_height(y->rc)) + 1;
    x->height = max_int(node_height(x->lc), node_height(x->rc)) + 1;
    
    return x;
}

static book_node_t* rotate_left(book_node_t* x) {
    book_node_t* y = x->rc;
    book_node_t* T2 = y->lc;
    
    y->lc = x;
    x->rc = T2;
    
    x->height = max_int(node_height(x->lc), node_height(x->rc)) + 1;
    y->height = max_int(node_height(y->lc), node_height(y->rc)) + 1;
    
    return y;
}

static int get_balance(book_node_t* node) {
    if (!node) return 0;
    return node_height(node->lc) - node_height(node->rc);
}

book_node_t* insert_book_index(book_node_t* root, book_t* book, int* status) {
    if (!book) return root;
    if (!root) {
        book_node_t* node = malloc(sizeof(book_node_t));
        if (!node) {
            if (status) *status = 0;
            return root;
        }
        strncpy(node->title, book->title, TITLE_MAX - 1);
        node->title[TITLE_MAX - 1] = '\0';
        node->book = book;
        node->lc = node->rc = NULL;
        node->height = 1;
        if (status) *status = 1;
        return node;
    }
    
    int cmp = strcmp(book->title, root->title);
    if (cmp == 0) {
        if (status) *status = 0;
        return root;
    } else if (cmp < 0) {
        root->lc = insert_book_index(root->lc, book, status);
    } else {
        root->rc = insert_book_index(root->rc, book, status);
    }
    
    root->height = 1 + max_int(node_height(root->lc), node_height(root->rc));
    int balance = get_balance(root);
    
    if (balance > 1 && strcmp(book->title, root->lc->title) < 0) {
        return rotate_right(root);
    }
    if (balance < -1 && strcmp(book->title, root->rc->title) > 0) {
        return rotate_left(root);
    }
    if (balance > 1 && strcmp(book->title, root->lc->title) > 0) {
        root->lc = rotate_left(root->lc);
        return rotate_right(root);
    }
    if (balance < -1 && strcmp(book->title, root->rc->title) < 0) {
        root->rc = rotate_right(root->rc);
        return rotate_left(root);
    }
    
    return root;
}

book_node_t* find_book_by_title(book_node_t* root, const char* title) {
    if (!root || !title) return NULL;
    int cmp = strcmp(title, root->title);
    if (cmp == 0) return root;
    if (cmp < 0) return find_book_by_title(root->lc, title);
    return find_book_by_title(root->rc, title);
}

static book_node_t* min_value_node(book_node_t* node) {
    book_node_t* current = node;
    while (current && current->lc) {
        current = current->lc;
    }
    return current;
}

book_node_t* remove_book_index(book_node_t* root, const char* title) {
    if (!root || !title) return root;
    
    int cmp = strcmp(title, root->title);
    if (cmp < 0) {
        root->lc = remove_book_index(root->lc, title);
    } else if (cmp > 0) {
        root->rc = remove_book_index(root->rc, title);
    } else {
        if (!root->lc || !root->rc) {
            book_node_t* temp = root->lc ? root->lc : root->rc;
            if (!temp) {
                temp = root;
                root = NULL;
            } else {
                *root = *temp;
            }
            free(temp);
        } else {
            book_node_t* temp = min_value_node(root->rc);
            strncpy(root->title, temp->title, TITLE_MAX - 1);
            root->title[TITLE_MAX - 1] = '\0';
            root->book = temp->book;
            root->rc = remove_book_index(root->rc, temp->title);
        }
    }
    
    if (!root) return NULL;
    
    root->height = 1 + max_int(node_height(root->lc), node_height(root->rc));
    int balance = get_balance(root);
    
    if (balance > 1 && get_balance(root->lc) >= 0) {
        return rotate_right(root);
    }
    if (balance > 1 && get_balance(root->lc) < 0) {
        root->lc = rotate_left(root->lc);
        return rotate_right(root);
    }
    if (balance < -1 && get_balance(root->rc) <= 0) {
        return rotate_left(root);
    }
    if (balance < -1 && get_balance(root->rc) > 0) {
        root->rc = rotate_right(root->rc);
        return rotate_left(root);
    }
    
    return root;
}

void free_book_index(book_node_t* root) {
    if (!root) return;
    free_book_index(root->lc);
    free_book_index(root->rc);
    free(root);
}

void update_system_stats_on_book_add() {
    if (!library) return;
    library->total_books++;
}

void update_system_stats_on_member_add() {
    if (!library) return;
    library->total_members++;
}

void update_system_stats_on_loan_add() {
    if (!library) return;
    library->active_loans++;
    library->total_loans++;
}

void update_system_stats_on_loan_remove() {
    if (!library) return;
    if (library->active_loans > 0) {
        library->active_loans--;
    }
}

void update_system_stats_on_review(int score) {
    if (!library) return;
    library->total_reviews++;
    library->total_scores_sum += score;
}

void print_system_stats() {
    printf("Stats:\n");
    int books = library ? library->total_books : 0;
    int members = library ? library->total_members : 0;
    int loans = library ? library->total_loans : 0;
    int reviews = library ? library->total_reviews : 0;
    double avg = 0.0;
    if (library && library->total_reviews > 0) {
        avg = (double)library->total_scores_sum / (double)library->total_reviews;
    }
    printf("books=%d members=%d loans=%d reviews=%d avg=%.2f\n", books, members, loans, reviews, avg);
}

void reset_library() {
    destroy_library();
    library = create_library();
    SLOTS = 0;
}

int extract_quoted_argument(const char* line, char* buffer, size_t buffer_size) {
    if (!line || !buffer || buffer_size == 0) return 0;
    const char* first = strchr(line, '"');
    if (!first) return 0;
    const char* second = strchr(first + 1, '"');
    if (!second) return 0;
    size_t len = (size_t)(second - first - 1);
    if (len >= buffer_size) {
        len = buffer_size - 1;
    }
    memcpy(buffer, first + 1, len);
    buffer[len] = '\0';
    return 1;
}

void sanitize_argument(char* text) {
    if (!text) return;
    char* start = text;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    char* end = text + strlen(text);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';
    if (start != text) {
        memmove(text, start, (size_t)(end - start) + 1);
    }
    size_t len = strlen(text);
    if (len >= 2 && text[0] == '"' && text[len - 1] == '"') {
        memmove(text, text + 1, len - 2);
        text[len - 2] = '\0';
    }
}

static int compare_book_priority(book_t* a, book_t* b) {
    if (a->avg != b->avg) {
        return (a->avg > b->avg) ? 1 : -1;
    }
    if (a->bid != b->bid) {
        return (a->bid < b->bid) ? 1 : -1;
    }
    return 0;
}

static void heap_swap(rec_heap_t* heap, int i, int j) {
    book_t* tmp = heap->data[i];
    heap->data[i] = heap->data[j];
    heap->data[j] = tmp;
    heap->data[i]->heap_pos = i;
    heap->data[j]->heap_pos = j;
}

static void heapify_up(rec_heap_t* heap, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (compare_book_priority(heap->data[index], heap->data[parent]) > 0) {
            heap_swap(heap, index, parent);
            index = parent;
        } else {
            break;
        }
    }
}

static void heapify_down(rec_heap_t* heap, int index) {
    while (1) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int largest = index;
        
        if (left < heap->size && compare_book_priority(heap->data[left], heap->data[largest]) > 0) {
            largest = left;
        }
        if (right < heap->size && compare_book_priority(heap->data[right], heap->data[largest]) > 0) {
            largest = right;
        }
        if (largest != index) {
            heap_swap(heap, index, largest);
            index = largest;
        } else {
            break;
        }
    }
}

rec_heap_t* create_rec_heap(int capacity) {
    rec_heap_t* heap = malloc(sizeof(rec_heap_t));
    if (!heap) return NULL;
    heap->data = malloc(sizeof(book_t*) * capacity);
    if (!heap->data) {
        free(heap);
        return NULL;
    }
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

void destroy_rec_heap(rec_heap_t* heap) {
    if (!heap) return;
    free(heap->data);
    free(heap);
}

void rec_heap_remove(book_t* book) {
    if (!library || !library->recommendations || !book) return;
    int pos = book->heap_pos;
    rec_heap_t* heap = library->recommendations;
    if (pos < 0 || pos >= heap->size) return;
    
    book->heap_pos = -1;
    if (heap->size == 0) return;
    
    heap->size--;
    if (pos == heap->size) {
        return;
    }
    
    heap->data[pos] = heap->data[heap->size];
    heap->data[pos]->heap_pos = pos;
    heapify_up(heap, pos);
    heapify_down(heap, pos);
}

static int find_worst_index(rec_heap_t* heap) {
    if (heap->size == 0) return -1;
    int worst = 0;
    for (int i = 1; i < heap->size; i++) {
        if (compare_book_priority(heap->data[i], heap->data[worst]) < 0) {
            worst = i;
        }
    }
    return worst;
}

void rec_heap_insert_or_update(book_t* book) {
    if (!library || !library->recommendations || !book) return;
    rec_heap_t* heap = library->recommendations;
    
    if (book->lost_flag || book->n_reviews == 0) {
        rec_heap_remove(book);
        return;
    }
    
    if (book->heap_pos >= 0 && book->heap_pos < heap->size) {
        heapify_up(heap, book->heap_pos);
        heapify_down(heap, book->heap_pos);
        return;
    }
    
    if (heap->size < heap->capacity) {
        heap->data[heap->size] = book;
        book->heap_pos = heap->size;
        heap->size++;
        heapify_up(heap, book->heap_pos);
        return;
    }
    
    int worst = find_worst_index(heap);
    if (worst >= 0 && compare_book_priority(book, heap->data[worst]) > 0) {
        heap->data[worst]->heap_pos = -1;
        heap->data[worst] = book;
        book->heap_pos = worst;
        heapify_up(heap, worst);
        heapify_down(heap, worst);
    } else {
        book->heap_pos = -1;
    }
}

static int cmp_books_desc(const void* a, const void* b) {
    book_t* book_a = *(book_t**)a;
    book_t* book_b = *(book_t**)b;
    int cmp = compare_book_priority(book_a, book_b);
    return (cmp > 0) ? -1 : (cmp < 0) ? 1 : 0;
}

void print_top_books(int k) {
    printf("Top Books:\n");
    if (!library || !library->recommendations || library->recommendations->size == 0) {
        printf("(empty)\n");
        return;
    }
    
    rec_heap_t* heap = library->recommendations;
    if (k > heap->size) k = heap->size;
    if (k <= 0) {
        printf("(empty)\n");
        return;
    }
    
    book_t** temp = malloc(sizeof(book_t*) * heap->size);
    if (!temp) {
        printf("(empty)\n");
        return;
    }
    
    for (int i = 0; i < heap->size; i++) {
        temp[i] = heap->data[i];
    }
    qsort(temp, heap->size, sizeof(book_t*), cmp_books_desc);
    
    for (int i = 0; i < k; i++) {
        printf("%d \"%s\" avg=%d\n", temp[i]->bid, temp[i]->title, temp[i]->avg);
    }
    free(temp);
}

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* dup = malloc(len);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

void free_string(char* s) {
    if (s) free(s);
}

int parse_score(char* score_str, int* score_out) {
    if (!score_str || !score_out || *score_str == '\0') {
        return 0;
    }
    
    int value = 0;
    for (char* p = score_str; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10 + (*p - '0');
    }
    
    *score_out = value;
    return 1;
}

// Parses and executes commands from input file
void execute_command(char* line) {
    if (!line || strlen(line) == 0) return;
    
    char command[10];
    sscanf(line, "%s", command);
    
    if (strcmp(command, "S") == 0) {
        int slots;
        sscanf(line, "S %d", &slots);
        SLOTS = slots;
        printf("DONE\n");
    }
    else if (strcmp(command, "G") == 0) {
        int gid;
        char name[256];
        sscanf(line, "G %d %[^\n]", &gid, name);
        sanitize_argument(name);
        add_genre(gid, name);
    }
    else if (strcmp(command, "BK") == 0) {
        int bid, gid;
        char title[256];
        sscanf(line, "BK %d %d %[^\n]", &bid, &gid, title);
        sanitize_argument(title);
        add_book(bid, gid, title);
    }
    else if (strcmp(command, "M") == 0) {
        int sid;
        char name[256];
        sscanf(line, "M %d %[^\n]", &sid, name);
        sanitize_argument(name);
        add_member(sid, name);
    }
    else if (strcmp(command, "L") == 0) {
        int sid, bid;
        sscanf(line, "L %d %d", &sid, &bid);
        add_loan(sid, bid);
    }
    else if (strcmp(command, "R") == 0) {
        int sid, bid;
        char score_str[10], status[10];
        sscanf(line, "R %d %d %s %s", &sid, &bid, score_str, status);
        return_book(sid, bid, score_str, status);
    }
    else if (strcmp(command, "D") == 0) {
        calculate_display();
    }
    else if (strcmp(command, "PG") == 0) {
        int gid;
        sscanf(line, "PG %d", &gid);
        print_genre(gid);
    }
    else if (strcmp(command, "PM") == 0) {
        int sid;
        sscanf(line, "PM %d", &sid);
        print_member_loans(sid);
    }
    else if (strcmp(command, "PD") == 0) {
        print_display();
    }
    else if (strcmp(command, "PS") == 0) {
        print_stats();
    }
    else if (strcmp(command, "F") == 0) {
        char title[TITLE_MAX];
        if (!extract_quoted_argument(line, title, sizeof(title))) {
            const char* rest = line + 1;
            while (*rest == ' ' || *rest == '\t') rest++;
            strncpy(title, rest, TITLE_MAX - 1);
            title[TITLE_MAX - 1] = '\0';
            sanitize_argument(title);
        }
        if (title[0] == '\0') {
            printf("NOT FOUND\n");
        } else {
            book_node_t* node = find_book_by_title(library->book_index, title);
            if (node && node->book && node->book->lost_flag == 0) {
                printf("Book %d \"%s\" avg=%d\n", node->book->bid, node->book->title, node->book->avg);
            } else {
                printf("NOT FOUND\n");
            }
        }
    }
    else if (strcmp(command, "TOP") == 0) {
        int k = 0;
        sscanf(line, "TOP %d", &k);
        print_top_books(k);
    }
    else if (strcmp(command, "AM") == 0) {
        print_active_members();
    }
    else if (strcmp(command, "U") == 0) {
        int bid;
        if (sscanf(line, "U %d", &bid) != 1) {
            printf("IGNORED\n");
            return;
        }
        char new_title[TITLE_MAX];
        if (!extract_quoted_argument(line, new_title, sizeof(new_title))) {
            printf("IGNORED\n");
            return;
        }
        sanitize_argument(new_title);
        update_book_title_command(bid, new_title);
    }
    else if (strcmp(command, "X") == 0) {
        print_system_stats();
    }
    else if (strcmp(command, "BF") == 0) {
        reset_library();
        printf("DONE\n");
    }
}

void process_input_file(FILE* file) {
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        execute_command(line);
    }
}

// Main program entry point
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        return 1;
    }
    
    FILE* file = fopen(argv[1], "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        return 1;
    }
    
    library = create_library();
    
    process_input_file(file);
    
    fclose(file);
    destroy_library();
    
    return 0;
}