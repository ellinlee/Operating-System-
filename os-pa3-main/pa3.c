/**********************************************************************
 * Copyright (c) 2020-2024
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "list_head.h"
#include "vm.h"

/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;

/**
 * TLB of the system.
 */
extern struct tlb_entry tlb[];

/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];

/**
 * lookup_tlb(@vpn, @rw, @pfn)
 *
 * DESCRIPTION
 *   Translate @vpn of the current process through TLB. DO NOT make your own
 *   data structure for TLB, but should use the defined @tlb data structure
 *   to translate. If the requested VPN exists in the TLB and it has the same
 *   rw flag, return true with @pfn is set to its PFN. Otherwise, return false.
 *   The framework calls this function when needed, so do not call
 *   this function manually.
 *
 * RETURN
 *   Return true if the translation is cached in the TLB.
 *   Return false otherwise
 */
bool lookup_tlb(unsigned int vpn, unsigned int rw, unsigned int *pfn)
{
    for (int i = 0; i < NR_PAGEFRAMES; i++)
    {
        if (tlb[i].valid && tlb[i].vpn == vpn && tlb[i].rw == rw)
        {
            *pfn = tlb[i].pfn;
            return true;
        }
    }
    return false;
}

void insert_tlb(unsigned int vpn, unsigned int rw, unsigned int pfn)
{
    for (int i = 0; i < NR_PAGEFRAMES; i++)
    {
        if (tlb[i].valid && tlb[i].vpn == vpn)
        {
            tlb[i].pfn = pfn;
            tlb[i].rw = rw;
            return;
        }
    }

    for (int i = 0; i < NR_PAGEFRAMES; i++)
    {
        if (!tlb[i].valid)
        {
            tlb[i].valid = true;
            tlb[i].vpn = vpn;
            tlb[i].rw = rw;
            tlb[i].pfn = pfn;
            return;
        }
    }
}

unsigned int alloc_page(unsigned int vpn, unsigned int rw)
{
    for (unsigned int i = 0; i < NR_PAGEFRAMES; i++)
    {
        if (mapcounts[i] == 0)
        {
            mapcounts[i] = 1;

            struct pagetable_entry *pte = &current->pagetable.entries[vpn];
            pte->valid = true;
            pte->pfn = i;
            pte->rw = rw;

            insert_tlb(vpn, rw, i);

            return i;
        }
    }
    return -1;
}

void free_page(unsigned int vpn)
{
    struct pagetable_entry *pte = &current->pagetable.entries[vpn];
    if (!pte->valid)
        return;

    unsigned int pfn = pte->pfn;
    if (--mapcounts[pfn] == 0)
    {
        pte->valid = false;
        pte->pfn = 0;
        pte->rw = 0;
    }

    for (int i = 0; i < NR_PAGEFRAMES; i++)
    {
        if (tlb[i].valid && tlb[i].vpn == vpn)
        {
            tlb[i].valid = false;
        }
    }
}

bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
    struct pagetable_entry *pte = &current->pagetable.entries[vpn];

    if (!pte->valid)
    {
        return alloc_page(vpn, rw) != -1;
    }
    else if (rw == ACCESS_WRITE && !pte->rw)
    {
        unsigned int pfn = alloc_page(vpn, ACCESS_WRITE);
        if (pfn == -1)
            return false;

        pte->rw = ACCESS_WRITE;
        return true;
    }

    return false;
}

void switch_process(unsigned int pid)
{
    struct process *next = NULL;
    struct process *temp;

    list_for_each_entry(temp, &processes, list)
    {
        if (temp->pid == pid)
        {
            next = temp;
            break;
        }
    }

    if (!next)
    {
        next = malloc(sizeof(struct process));
        next->pid = pid;
        memcpy(&next->pagetable, &current->pagetable, sizeof(struct pagetable));

        list_add_tail(&next->list, &processes);
    }

    list_del(&next->list);
    list_add_tail(&current->list, &processes);

    current = next;
    ptbr = &current->pagetable;
}
