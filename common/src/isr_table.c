/*
 * Copyright (c) 2024 Meta Platforms.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/debug/symtab.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sw_isr_table.h>

static void dump_isr_table_entry(int idx, struct _isr_table_entry *entry)
{

	if ((entry->isr == z_irq_spurious) || (entry->isr == NULL)) {
		return;
	}
#ifdef CONFIG_SYMTAB
	const char *name = symtab_find_symbol_name((uintptr_t)entry->isr, NULL);

	printk("%4d: %s(%p)\n", idx, name, entry->arg);
#else
#if CONFIG_ISR_TABLE_COUNT
        printk("%4d: %p(%p)\t%d\n", idx, entry->isr, entry->arg, entry->count);
#else
        printk("%4d: %p(%p)\n", idx, entry->isr, entry->arg);
#endif /* CONFIG_ISR_TABLE_COUNT */
#endif /* CONFIG_SYMTAB */
}

void dump_isr_table(void)
{
	printk("_sw_isr_table[%d]\n", IRQ_TABLE_SIZE);

	for (int idx = 0; idx < IRQ_TABLE_SIZE; idx++) {
		dump_isr_table_entry(idx, &_sw_isr_table[idx]);
	}
}
