#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "unity_fixture.h"
#include "mock_critical_section.h"
#include <ubuffer.h>

#define BUFFER_CAPACITY 15
#define MAX_INT_TYPE_IN_BUFFER (BUFFER_CAPACITY / sizeof(int))
TEST_GROUP(TestUbuffer);

static uBuffer ubuffer;
static char *buffer_memory;
static uBuffer ubuffer_clone;
static char *data_clone[BUFFER_CAPACITY];

void clone_buffer() {
    memcpy((char*) &ubuffer_clone, (char*) &ubuffer, sizeof(uBuffer));
    memcpy((char*) data_clone, buffer_memory, BUFFER_CAPACITY);
}

void assert_buffer_equal_clone() {
    TEST_ASSERT_EQUAL_MEMORY(&ubuffer_clone, &ubuffer, sizeof(uBuffer));
    TEST_ASSERT_EQUAL_MEMORY(data_clone, buffer_memory, BUFFER_CAPACITY);
}

void fill_buffer() {
    int *item;
    for (int i = 0; i < MAX_INT_TYPE_IN_BUFFER; i++) {
        TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
        TEST_ASSERT_EQUAL((int *) buffer_memory + i, item);
        TEST_ASSERT_EQUAL((i + 1) * sizeof(int), ubuffer.size);
        *item = i;
    };

    TEST_ASSERT_EQUAL(UBUFFER_FULL, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(MAX_INT_TYPE_IN_BUFFER * sizeof(int), ubuffer.size);
}

void empty_a_full_buffer(int first_value_in_buffer) {
    int *item;

    //    Pop everything out
    for (int i = 0; i < MAX_INT_TYPE_IN_BUFFER; i++) {
        TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_free_first(&ubuffer, (void **) &item, sizeof(int)));
        TEST_ASSERT_EQUAL(i + first_value_in_buffer, *item);
    }

    //    Try to pop an item when the buffer is empty
    TEST_ASSERT_EQUAL(UBUFFER_EMPTY, ubuffer_free_first(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(0, ubuffer.size);
}

TEST_SETUP(TestUbuffer) {
    buffer_memory = (char*) malloc(BUFFER_CAPACITY);
    ubuffer_init(&ubuffer, buffer_memory, BUFFER_CAPACITY);
}

TEST_TEAR_DOWN(TestUbuffer) {
    free(buffer_memory);
}

TEST(TestUbuffer, Test_Init) {
    TEST_ASSERT_EQUAL(BUFFER_CAPACITY, ubuffer.capacity);
    TEST_ASSERT_EQUAL(buffer_memory, ubuffer.start);
    TEST_ASSERT_EQUAL(0, ubuffer.head);
    TEST_ASSERT_EQUAL(0, ubuffer.size);
    TEST_ASSERT_EQUAL(0, ubuffer.num_empty_bytes_at_end);
    TEST_ASSERT_EQUAL(0, mock_critical_section_state());
}

TEST(TestUbuffer, Test_Allocate) {
    int *item;
    ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int));
    *item = 10;
    TEST_ASSERT_EQUAL(ubuffer.start, item);
    TEST_ASSERT_EQUAL(sizeof(int), ubuffer.size);
    TEST_ASSERT_EQUAL(0, mock_critical_section_state());
    TEST_ASSERT_EQUAL(1, mock_critical_section_calls());
}

TEST(TestUbuffer, Test_Free) {
    int *item_pushed, *item_poped;
    ubuffer_allocate_next(&ubuffer, (void **) &item_pushed, sizeof(int));
    ubuffer_free_first(&ubuffer, (void **) &item_poped, sizeof(int));
    TEST_ASSERT_EQUAL(item_pushed, item_poped);
    TEST_ASSERT_EQUAL(BUFFER_CAPACITY, ubuffer.capacity);
    TEST_ASSERT_EQUAL(buffer_memory, ubuffer.start);
    TEST_ASSERT_EQUAL(0, ubuffer.head);
    TEST_ASSERT_EQUAL(0, ubuffer.size);
    TEST_ASSERT_EQUAL(0, mock_critical_section_state());
}

TEST(TestUbuffer, Test_Full) {
    int *item;

    fill_buffer();
    clone_buffer();

    TEST_ASSERT_EQUAL(UBUFFER_FULL, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    assert_buffer_equal_clone();
}

TEST(TestUbuffer, Test_Empty) {
    int *item;

    clone_buffer();

    TEST_ASSERT_EQUAL(UBUFFER_EMPTY, ubuffer_free_first(&ubuffer, (void **) &item, sizeof(int)));
    assert_buffer_equal_clone();
}

TEST(TestUbuffer, Test_Circular_Tail) {
    int *item;

//    Fill the buffer to the maximum amount of integers possible
    fill_buffer();

//    Take one item out
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_free_first(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(0, *item);

//    Add one item (first cyclic function)
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(BUFFER_CAPACITY % sizeof(int), ubuffer.num_empty_bytes_at_end);
    TEST_ASSERT_EQUAL(BUFFER_CAPACITY, ubuffer.size);
    *item = MAX_INT_TYPE_IN_BUFFER;
    TEST_ASSERT_EQUAL(buffer_memory, item);

//    Try to add one more item when buffer is full
    TEST_ASSERT_EQUAL(UBUFFER_FULL, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));

    empty_a_full_buffer(1);

    //make sure we can add more after emptyinh
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_peek_first(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(0, mock_critical_section_state());
}

TEST(TestUbuffer, Test_Nrf_Scenario) {
    int *item;

//    Fill the buffer to the maximum amount of integers possible
    fill_buffer();

//    Take 2 item out
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_free_first(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(0, *item);

    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_free_first(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(1, *item);

//    Add one item (first cyclic function)
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(BUFFER_CAPACITY % sizeof(int), ubuffer.num_empty_bytes_at_end);
    TEST_ASSERT_EQUAL(11, ubuffer.size);
    *item = MAX_INT_TYPE_IN_BUFFER;
    TEST_ASSERT_EQUAL(buffer_memory, item);

    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(BUFFER_CAPACITY % sizeof(int), ubuffer.num_empty_bytes_at_end);
    TEST_ASSERT_EQUAL(BUFFER_CAPACITY, ubuffer.size);
    *item = MAX_INT_TYPE_IN_BUFFER + 1;
    TEST_ASSERT_EQUAL(buffer_memory + sizeof(int), item);

//    Try to add one more item when buffer is full
    TEST_ASSERT_EQUAL(UBUFFER_FULL, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));

    for (int i = 0; i < MAX_INT_TYPE_IN_BUFFER; i++) {
        TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_free_first(&ubuffer, (void **) &item, sizeof(int)));
        TEST_ASSERT_EQUAL(i + 2, *item);
    }

    //make sure we can add more after emptyinh
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_peek_first(&ubuffer, (void **) &item, sizeof(int)));
}

TEST(TestUbuffer, Test_Circular_Head) {
    int *item;

//  Bring the head to the end (minus num_empty_bytes_at_end) of the buffer
    fill_buffer();
    empty_a_full_buffer(0);

    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(buffer_memory, item);
    TEST_ASSERT_EQUAL(sizeof(int), ubuffer.size);
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_peek_first(&ubuffer, (void **) &item, sizeof(int)));
}

TEST(TestUbuffer, Test_Free_Size_0) {
    int *item;

    fill_buffer();
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_free_first(&ubuffer, (void**) &item, 0));
    TEST_ASSERT_EQUAL(buffer_memory, item);
}

TEST(TestUbuffer, Test_Free_Size_0_When_Empty) {
    int *item;

    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_free_first(&ubuffer, (void**) &item, 0));
    TEST_ASSERT_EQUAL(buffer_memory, item);
}

TEST(TestUbuffer, Test_Free_Item_Too_Large) {
    int *item;

    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_allocate_next(&ubuffer, (void **) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(UBUFFER_EMPTY, ubuffer_free_first(&ubuffer, (void**) &item, sizeof(int) + 1));
}

TEST(TestUbuffer, Test_Free_Item_Too_Large_With_Empty_Bytes_At_End) {
    int *item;

    ubuffer.num_empty_bytes_at_end = 2;
    ubuffer.head = ubuffer.head + ubuffer.capacity - 3;
    ubuffer.size = 4;

    TEST_ASSERT_EQUAL(UBUFFER_EMPTY, ubuffer_free_first(&ubuffer, (void**) &item, 2));
}

TEST(TestUbuffer, Test_Peek) {
    int * item;

    fill_buffer();
    clone_buffer();
    TEST_ASSERT_EQUAL(UBUFFER_SUCCESS, ubuffer_peek_first(&ubuffer, (void**) &item, sizeof(int)));
    TEST_ASSERT_EQUAL(buffer_memory, item);
    assert_buffer_equal_clone();
}