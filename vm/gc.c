#include "gc.h"
#include "obj.h"
#include "io.h"

#define VM_MEM_MAX ((VM_FRAMES_UNITS * sizeof(vm_stack_frame_t)) + (VM_LOCALS_UNITS * sizeof(vm_obj_t)))

#define VM_GC_CLIP(val) (val & (VM_MEM_END - 1))
#define VM_GC_UNCLIP(val) (val | (VM_MEM_END))
#define VM_GC_ISCLIP(val) (val < (VM_MEM_END))

#define VM_GC_MEM_GROW (1)

size_t vm_stats_memsize = VM_MEM_MAX;

size_t vm_mem_top = 0;
uint8_t vm_mem[VM_MEM_MAX];
#if defined(VM_GC_THREADS)
vm_gc_entry_t vm_gc_objs0[VM_MEM_UNITS];
#endif
vm_gc_entry_t vm_gc_objs1[VM_MEM_UNITS];
vm_gc_entry_t vm_gc_objs2[VM_MEM_UNITS];

void *vm_mem_grow(size_t size)
{
    void *ret = &vm_mem[vm_mem_top];
    vm_mem_top += size;
    return ret;
}

void vm_mem_reset(void)
{
    vm_mem_top = 0;
}

size_t vm_gc_hash(uint64_t h)
{
    return (h * 2) % VM_MEM_UNITS;
}

vm_gc_entry_t vm_gc_find_in(vm_gc_entry_array_t elems, uint64_t ptr, size_t head)
{
    do
    {
        vm_gc_entry_t ent = elems[head];
        if (VM_GC_CLIP(ent.ptr) == ptr)
        {
            return ent;
        }
        if (ent.ptr == 0)
        {
            return (vm_gc_entry_t){
                .ptr = 0,
            };
        }
        head++;
        if (head == VM_MEM_UNITS)
        {
            head = 0;
        }
    } while (true);
}

vm_obj_t vm_gc_get(vm_gc_t *gc, uint64_t ptr)
{
#if defined(VM_GC_THREADS)
    vm_gc_entry_t e0 = vm_gc_find_in(gc->objs0, ptr, vm_gc_hash(ptr));
    if (e0.ptr != 0)
    {
        return e0.obj;
    }
#endif
    vm_gc_entry_t e1 = vm_gc_find_in(gc->objs1, ptr, vm_gc_hash(ptr));
    if (e1.ptr != 0)
    {
        return e1.obj;
    }
    return vm_gc_find_in(gc->objs2, ptr, vm_gc_hash(ptr)).obj;
}

bool vm_gc_mark_entry_yes(vm_gc_entry_array_t elems, uint64_t ptr, size_t head)
{
    do
    {
        vm_gc_entry_array_t ent = &elems[head];
        if (VM_GC_CLIP(ent->ptr) == VM_GC_CLIP(ptr))
        {
            ent->ptr = VM_GC_UNCLIP(ent->ptr);
            return true;
        }
        if (ent->ptr == 0)
        {
            return false;
        }
        head++;
        if (head == VM_MEM_UNITS)
        {
            head = 0;
        }
    } while (true);
}

void vm_gc_mark_val_yes(vm_gc_t *gc, uint64_t ptr)
{
    vm_gc_mark_entry_yes(gc->objs2, ptr, vm_gc_hash(ptr));
}

void vm_gc_mark_ptr_yes(vm_gc_t *gc, uint64_t ptr)
{
    size_t len = vm_gc_sizeof(gc, ptr);
    vm_gc_mark_val_yes(gc, ptr);
    for (size_t cur = 0; cur < len; cur++)
    {
        vm_obj_t obj = vm_gc_get_index(gc, ptr, cur);
        vm_gc_mark_val_yes(gc, ptr + cur + 1);
        if (vm_obj_is_ptr(obj))
        {
            vm_gc_mark_ptr_yes(gc, vm_obj_to_ptr(obj));
        }
    }
}

void vm_gc_move(vm_gc_entry_array_t to, vm_gc_entry_t value)
{
    size_t head = vm_gc_hash(value.ptr);
    while (to[head].ptr != 0)
    {
        head++;
        if (head == VM_MEM_UNITS)
        {
            head = 0;
        }
    }
    to[head] = value;
}

void vm_gc_shift(vm_gc_entry_array_t to, vm_gc_entry_t value)
{
    size_t head = vm_gc_hash(value.ptr);
    while (true)
    {
        if (to[head].ptr == value.ptr)
        {
            break;
        }
        if (vm_obj_is_dead(to[head].obj))
        {
            to[head] = value;
        }
        head++;
        if (head == VM_MEM_UNITS)
        {
            head = 0;
        }
    }
}

size_t vm_gc_count(vm_gc_entry_array_t ents)
{
    size_t ret = 0;
    for (size_t index = 0; index < VM_MEM_UNITS; index++)
    {
        if (ents[index].ptr != 0)
        {
            ret += 1;
        }
    }
    return ret;
}

void vm_gc_run1(vm_gc_t *gc)
{
    gc->calls++;
    size_t num_dead = 0;
    for (size_t index = 0; index < gc->nlocals; index++)
    {
        vm_obj_t cur = gc->base[index];
        if (vm_obj_is_ptr(cur))
        {
            uint64_t ptr = vm_obj_to_ptr(cur);
            vm_gc_mark_ptr_yes(gc, ptr);
            num_dead = 0;
        }
        else if (vm_obj_is_dead(cur))
        {
            if (num_dead == 256)
            {
                break;
            }
            num_dead += 1;
        }
        else
        {
            num_dead = 0;
        }
    }
    for (size_t index = 0; index < VM_MEM_UNITS; index++)
    {
        vm_gc_entry_array_t ent = &gc->objs2[index];
        if (ent->ptr != 0)
        {
            if (!VM_GC_ISCLIP(ent->ptr))
            {
                vm_gc_shift(gc->objs2, *ent);
            }
            else
            {
                ent->obj = vm_obj_of_dead();
            }
        }
    }
    for (size_t index = 0; index < VM_MEM_UNITS; index++)
    {
        if (vm_obj_is_dead(gc->objs2[index].obj))
        {
            gc->objs2[index].ptr = 0;
        }
        else if (!VM_GC_ISCLIP(gc->objs2[index].ptr))
        {
            gc->objs2[index].ptr = VM_GC_CLIP(gc->objs2[index].ptr);
        }
    }
    for (size_t index = 0; index < VM_MEM_UNITS; index++)
    {
        vm_gc_entry_array_t ent = &gc->objs1[index];
        if (ent->ptr != 0)
        {
            vm_gc_entry_t entv = *ent;
            entv.ptr = VM_GC_CLIP(entv.ptr);
            vm_gc_move(gc->objs2, entv);
            ent->ptr = 0;
        }
    }
#if defined(VM_GC_THREADS)
    vm_gc_entry_array_t old1 = gc->objs1;
    gc->objs1 = gc->objs0;
    gc->objs0 = old1;
#endif
}

#if defined(VM_GC_THREADS)
void *vm_gc_run_thread(void *gc_arg)
{
    vm_gc_t *gc = gc_arg;
    while (true)
    {
        vm_gc_run1(gc);
    }
    return NULL;
}
#endif

void vm_gc_start(vm_gc_t *gc, vm_obj_t *base, size_t nlocals)
{
    gc->base = base;
    gc->nlocals = nlocals;
    gc->last = 2;
#if defined(VM_GC_THREADS)
    gc->objs0 = &vm_gc_objs0[0];
#endif
    gc->objs1 = &vm_gc_objs1[0];
    gc->objs2 = &vm_gc_objs2[0];
    for (long i = 0; i < VM_MEM_UNITS; i++)
    {
#if defined(VM_GC_THREADS)
        gc->objs0[i].ptr = 0;
        gc->objs0[i].obj = vm_obj_of_dead();
#endif
        gc->objs1[i].ptr = 0;
        gc->objs1[i].obj = vm_obj_of_dead();
        gc->objs2[i].ptr = 0;
        gc->objs2[i].obj = vm_obj_of_dead();
    }
    gc->calls = 0;
#if defined(VM_GC_THREADS)
    pthread_create(&gc->thread, NULL, &vm_gc_run_thread, gc);
#endif
}

void vm_gc_stop(vm_gc_t *gc)
{
#if defined(VM_GC_THREADS)
    pthread_cancel(gc->thread);
#endif
    // printf("calls: %zu\n", gc->calls);
}

vm_obj_t vm_gc_new(vm_gc_t *gc, size_t size, vm_obj_t *values)
{
    uint64_t where = gc->last;
    {
#if !defined(VM_GC_THREADS)
        if (gc->last % ((VM_MEM_UNITS / 10) * 5) == 0)
        {
            vm_gc_run1(gc);
        }
#endif
        uint64_t ptr = gc->last;
        // vm_print(gc, vm_obj_of_num(ptr));
        // vm_puts(", ");
        // vm_print(gc, vm_obj_of_num(size));
        // vm_puts("\n");
        vm_gc_entry_t entry = (vm_gc_entry_t){
            .ptr = ptr,
            .obj = vm_obj_of_num(size),
        };
        gc->last += 1;
#if defined(VM_GC_THREADS)
        vm_gc_move(gc->objs0, entry);
#else
        vm_gc_move(gc->objs1, entry);
#endif
    }
    for (size_t i = 0; i < size; i++)
    {
#if !defined(VM_GC_THREADS)
        if (gc->last % ((VM_MEM_UNITS / 10) * 5) == 0)
        {
            vm_gc_run1(gc);
        }
#endif
        uint64_t ptr = gc->last;
        vm_gc_entry_t entry = (vm_gc_entry_t){
            .ptr = ptr,
            .obj = values[i],
        };
        gc->last += 1;
#if defined(VM_GC_THREADS)
        vm_gc_move(gc->objs0, entry);
#else
        vm_gc_move(gc->objs1, entry);
#endif
    }
    return vm_obj_of_ptr(where);
}

// void vm_gc_set_index(vm_gc_t *gc, uint64_t ptr, size_t index, vm_obj_t value)
// {
// #if defined(VM_GC_THREADS)
//     vm_gc_move(gc->objs0, entry);
// #else
//     vm_gc_move(gc->objs1, entry);
// #endif
// }

vm_obj_t vm_gc_get_index(vm_gc_t *gc, uint64_t ptr, size_t index)
{
    return vm_gc_get(gc, ptr + index + 1);
}

size_t vm_gc_sizeof(vm_gc_t *gc, uint64_t ptr)
{
    return (size_t)vm_obj_to_num(vm_gc_get(gc, ptr));
}