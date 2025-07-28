#pragma once

#include <stddef.h>

typedef struct timer_node_s {
	struct timer_node_s* next;
	struct timer_node_s* prev;
	void *conn;
	size_t expiration;
} timer_node_t;

typedef struct timer_wheel_s {
	timer_node_t** slots;
	int num_slots;
	int slot_interval;
	int current_slot;
} timer_wheel_t;

timer_wheel_t* timer_wheel_create(int num_slots, int slot_interval);
void timer_wheel_destroy(timer_wheel_t* tw);
timer_node_t* timer_node_add(timer_wheel_t* tw, void* conn, int timeout_sec);
void timer_node_remove(timer_node_t* node);
timer_node_t* timer_wheel_tick(timer_wheel_t* tw);

