#pragma once

struct vm_io_res_t;
typedef struct vm_io_res_t vm_io_res_t;

#include "opcode.h"

struct vm_io_res_t
{
  size_t nops;
  vm_opcode_t *ops;
};

static inline vm_io_res_t vm_io_read(const char *filename)
{
  FILE *file = fopen(filename, "rb");
  if (file == NULL)
  {
    fprintf(stderr, "cannot run vm: file could not be read\n");
    return (vm_io_res_t) {
      .nops = 0,
      .ops = NULL,
    };
  }
  size_t nalloc = 1 << 16;
  vm_opcode_t *ops = vm_malloc(sizeof(vm_opcode_t) * nalloc);
  size_t nops = 0;
  size_t size;
  for (;;)
  {
    vm_opcode_t op = 0;
    size = fread(&op, sizeof(vm_opcode_t), 1, file);
    if (size == 0)
    {
      break;
    }
    if (nops + 1 >= nalloc)
    {
      nalloc *= 4;
      ops = vm_realloc(ops, sizeof(vm_opcode_t) * nalloc);
    }
    ops[nops++] = (size_t)op;
  }
  fclose(file);
  return (vm_io_res_t){
      .nops = nops,
      .ops = ops,
  };
}