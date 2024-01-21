/**
 * task.c
 * Управление потоками
 *
 * Данный файл содержит функции для создания и удаления потоков.
 *
 */

#include <arch.h>
#include <log.h>
#include <mem.h>

static volatile uint64_t next_thread_id = 0;
static task_t *last_task = NULL;
static task_t *kernel_task = NULL;
task_t *current_task = NULL;

extern uint64_t full_init;

void task_switch_asm(task_t *, task_t *);

void task_switch(struct frame *state) {
	UNUSED(state);

	// LOG("Смена потоков\n");
	asm volatile("cli");

	task_t *next = current_task->next;
	task_t *last = current_task;

	current_task = next;

	// LOG("Смена потоков 2\n");
	task_switch_asm(last, next);
	asm volatile("sti");
}

uint64_t task_new_thread(void (*func)(void *), void *arg) {
	asm volatile("cli");

	LOG("Выделение потока\n");
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	uint64_t *stack = mem_alloc(STACK_SIZE);
	task_t *new_task = mem_alloc(sizeof(task_t));

	tool_memset(stack, 0, STACK_SIZE);
	tool_memset(new_task, 0, sizeof(task_t));

	uint64_t *stack_top = (uint64_t *)((uint64_t)stack + STACK_SIZE);

	*(--stack_top) = (uint64_t)arg;
	*(--stack_top) = (uint64_t)func;

	new_task->rsp = (uint64_t)new_task->stack + STACK_SIZE - sizeof(uint64_t) * 2;

	new_task->stack = stack;
	new_task->id = next_thread_id++;

	new_task->last = current_task;
	new_task->next = current_task->next;
	current_task->next->last = new_task;
	current_task->next = new_task;

	LOG("Создан новый поток с ID: %u\n", new_task->id);

	if (full_init) {
		asm volatile("sti"); // Включаем прерывания
	}

	return new_task->id;
}

void dummy(uint64_t n) {
	for (;;) { asm volatile("hlt"); }
}

void task_init( ) {
	asm volatile("cli");
	idt_set_int(32, task_switch);

	uint64_t rsp;
	uint64_t cr3;

	asm volatile("mov %%rsp, %0" : "=r"(rsp));
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	kernel_task = mem_alloc(sizeof(task_t));
	tool_memset(kernel_task, 0, sizeof(task_t));

	kernel_task->id = next_thread_id++;
	kernel_task->rsp = rsp;

	current_task = kernel_task;

	current_task->last = current_task;
	current_task->next = current_task;

	last_task = kernel_task;

	// task_new_thread(dummy, 2 + 2);

	LOG("Потоки инициализированы\n");
}
