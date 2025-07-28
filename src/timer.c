#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "timer.h"

timer_wheel_t* timer_wheel_create(int num_slots, int slot_interval) {
	timer_wheel_t* tw = malloc(sizeof(timer_wheel_t));
	if (!tw) return NULL;

	tw->slots = calloc(num_slots, sizeof(timer_node_t*));
	if (!tw->slots) {
		free(tw);
		return NULL;
	}

	tw->num_slots = num_slots;
	tw->slot_interval = slot_interval;
	tw->current_slot = 0;

	return tw;
}

void timer_wheel_destroy(timer_wheel_t* tw) {
	if (!tw) return;

	for (int i = 0; i < tw->num_slots; i++) {
		timer_node_t* current = tw->slots[i];
		while (current) {
			timer_node_t* to_free = current;
			current = current->next;
			free(to_free);
		}
	}
	free(tw->slots);
	free(tw);
}

timer_node_t* timer_node_add(timer_wheel_t* tw, void* conn, int timeout_sec) {
	timer_node_t* node = malloc(sizeof(timer_node_t));
	if (!node) return NULL;

	node->conn = conn;
	node->expiration = time(NULL) + timeout_sec;

	int ticks_to_expire = timeout_sec / tw->slot_interval;
	if(ticks_to_expire == 0) ticks_to_expire = 1;

	int target_slot = (tw->current_slot + ticks_to_expire) % tw->num_slots;

	node->next = tw->slots[target_slot];
	node->prev = NULL;
	if (tw->slots[target_slot]) {
		tw->slots[target_slot]->prev = node;
	}
	tw->slots[target_slot] = node;

	return node;
}

void timer_node_remove(timer_node_t* node) {
	if (!node) return;

	if (node->prev) {
		node->prev->next = node->next;
	}
	if (node->next) {
		node->next->prev = node->prev;
	}

	free(node);
}

timer_node_t* timer_wheel_tick(timer_wheel_t* tw) {
	tw->current_slot = (tw->current_slot + 1) % tw->num_slots;
	timer_node_t* expired_list = tw->slots[tw->current_slot];
	tw->slots[tw->current_slot] = NULL;

	timer_node_t* current = expired_list;
	while(current) {
		current->prev = NULL;
		current = current->next;
	}

	return expired_list;
}

