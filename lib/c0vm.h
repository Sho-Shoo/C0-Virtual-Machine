/**************************************************************************/
/*              COPYRIGHT Carnegie Mellon University 2021                 */
/* Do not post this file or any derivative on a public site or repository */
/**************************************************************************/
/* C0VM structs, function signatures, and opcodes
 * 15-122, Principles of Imperative Computation
 * William Lovas, Rob Simmons, and Tom Cortina
 *
 * C1 Features: Ishan Bhargava (Spring 2020)
 */

#ifndef _C0VM_H_
#define _C0VM_H_

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

#include "xalloc.h"
#include "contracts.h"
#include "c0vm_abort.h"

#define BYTECODE_VERSION 11

typedef int8_t byte;
typedef uint8_t ubyte;


/*** .bc0 file format, including function info and native info ***/

struct bc0_file {
  uint32_t magic;
  uint16_t version;

  /* integer constant pool */
  uint16_t int_count;
  int32_t *int_pool;     // \length(int_pool) == int_count

  /* string literal pool */
  /* stores all strings consecutively with NUL terminators */
  uint16_t string_count;
  char *string_pool;     // \length(string_pool) == string_count

  /* function pool */
  uint16_t function_count;
  struct function_info *function_pool; // \length(function_pool) == function_count

  /* native function tables */
  uint16_t native_count;
  struct native_info *native_pool; // \length(native_pool) == native_count
};

struct function_info {
  uint8_t num_args;
  uint8_t num_vars;
  uint16_t code_length;
  ubyte *code;            // \length(code) == code_length
};

struct native_info {
  uint16_t num_args;
  uint16_t function_table_index;
};


/*** Interface for c0vm.c ***/

// type of arbitrary c0 values -- variables, operands, etc.
typedef struct c0_value_header c0_value;

// C0 arrays
typedef struct {
  int count;      // number of elements
  int elt_size;   // size of each element (in bytes)
  void *elems;    // elements themselves
} c0_array;

// A 'tagged pointer', aka a void* pointer which knows its original type
typedef struct {
  void* p;       // the "real pointer". Cannot be NULL
  uint16_t tag;  // types used in casts are mapped to numbers
} c0_tagged_ptr;

// Converting back and forth from 32-bit ints
static inline c0_value int2val(int32_t i);
static inline int32_t val2int(c0_value v);

// Converting back and forth from pointers of any kind
static inline c0_value ptr2val(void *p);
static inline void* val2ptr(c0_value v);

// Creates a c0_value with a given pointer and tag
static inline c0_value tagged_ptr2val(void* p, uint16_t tag);
// Converts a c0_value to a tagged pointer struct (definition above).
// May return NULL if the original pointer was NULL e.g. (void*)NULL
static inline c0_tagged_ptr* val2tagged_ptr(c0_value v);

// Return a function pointer given a pool index and whether it's native or static
static inline void* create_funptr(bool is_native, uint16_t index);
// Returns whether function pointer points to a native or static function
static inline bool is_native_funptr(void *p);
// Returns the pool index of a function pointer
static inline uint16_t funptr2index(void *p);

// Checks if two c0_values are equal.
// Calls c0_value_error if the two values are not the same kind
// (e.g. val_equal() on an int and a pointer)
static inline bool val_equal(c0_value v1, c0_value v2);


/*** Implementation and helper functions ***/

// C0 values
enum c0_val_kind { C0_INTEGER, C0_POINTER };

struct c0_value_header {
  enum c0_val_kind kind;
  union {
    int32_t i;
    // if ptr & (1L << 63), then
    // this is actually a pointer to c0_tagged_ptr.
    // this pointer can be NULL!
    void *p;
  } payload;
};

static inline c0_value int2val(int32_t i) {
  c0_value v;
  v.kind = C0_INTEGER;
  v.payload.i = i;
  return v;
}

static inline int32_t val2int(c0_value v) {
  if (v.kind != C0_INTEGER)
    c0_value_error("Invalid cast from c0_value (a pointer) to an integer");
  return v.payload.i;
}

static inline c0_value ptr2val(void *p) {
  c0_value v;
  v.kind = C0_POINTER;
  v.payload.p = p;
  return v;
}

static inline void *val2ptr(c0_value v) {
  if (v.kind == C0_INTEGER)
    c0_value_error("Invalid cast from c0_value (an integer) to a pointer");

  // Note that v may be a tagged pointer or a function pointer as well here.
  // This could happen in AMSTORE
  return v.payload.p;
}


// Common support for tagged pointers and function pointers
#define PTR_TYPE_SHIFT 62
#define TAGGEDPTR_BITS ((uintptr_t)0x2)
#define FUNPTR_BITS ((uintptr_t)0x1)
#define TAGGEDPTR_MASK ((uintptr_t)(TAGGEDPTR_BITS << PTR_TYPE_SHIFT))
#define FUNPTR_MASK ((uintptr_t)(FUNPTR_BITS << PTR_TYPE_SHIFT))

// Returns the pointer type
static inline uintptr_t ptr_type(void *p) {
  return (uintptr_t)p >> PTR_TYPE_SHIFT;
}

static inline bool is_taggedptr(void *p) {
  return ptr_type(p) == TAGGEDPTR_BITS;
}

static inline bool is_funptr(void *p) {
  return ptr_type(p) == FUNPTR_BITS;
}


// Tagged pointers

// Helper functions for distinguishing between
// regular pointers and void* tagged pointers.
// Not useful for c0vm.c
static inline void *mark_tagged_ptr(c0_tagged_ptr *p) {
  // NULL does not need to be marked as a tagged pointer
  // since it has "all" the tags simultaneously
  REQUIRES(p != NULL);
  REQUIRES(!is_taggedptr(p));

  return (void*)((uintptr_t)p | TAGGEDPTR_MASK);
}

static inline c0_tagged_ptr *unmark_tagged_ptr(void* p) {
  REQUIRES(p != NULL);
  REQUIRES(is_taggedptr(p));

  return (c0_tagged_ptr*)((uintptr_t)p ^ TAGGEDPTR_MASK);
}

static inline c0_value tagged_ptr2val(void *p, uint16_t tag) {
  if (p == NULL) {
    // The NULL pointer must never be tagged
    return ptr2val(NULL);
  }

  c0_tagged_ptr *tagged_ptr = xmalloc(sizeof *tagged_ptr);
  tagged_ptr->p = p;
  tagged_ptr->tag = tag;

  return ptr2val(mark_tagged_ptr(tagged_ptr));
}

static inline c0_tagged_ptr* val2tagged_ptr(c0_value v) {
  if (v.kind != C0_POINTER)
    c0_value_error("val2tagged_ptr: Invalid cast from c0_value (an integer) to a pointer");

  void* p = v.payload.p;

  // NULL pointer is never really tagged, but can "act" like one
  if (p == NULL) return NULL;
  if (!is_taggedptr(p))
    c0_value_error("val2tagged_ptr: pointer is not a tagged pointer");

  // Remove 'tag' bit
  return unmark_tagged_ptr(p);
}


// Function pointers
#define FUNPTR_TYPE_SHIFT 31

// Create a function pointer which can be pushed to the stack using val2ptr
static inline void* create_funptr(bool is_native, uint16_t index) {
  uintptr_t ptr = FUNPTR_MASK;
  ptr |= ((uintptr_t)(is_native) << FUNPTR_TYPE_SHIFT);
  ptr |= index;

  ENSURES(is_funptr((void*)ptr));
  return (void*)ptr;
}

static inline bool is_native_funptr(void *p) {
  if (!is_funptr(p)) {
    c0_value_error("is_native_funptr: pointer is not a function pointer");
  }

  return ((uintptr_t)p >> FUNPTR_TYPE_SHIFT) & 0x1;
}

static inline uint16_t funptr2index(void *p) {
  if (!is_funptr(p)) {
    c0_value_error("funptr2index: pointer is not a function pointer");
  }

  return (uint16_t)((uintptr_t)p & 0xFFFF);
}


// c0_value equality
static inline bool val_equal(c0_value v1, c0_value v2) {
  // Usually indicates programming bug in c0vm.c
  if (v1.kind != v2.kind) {
    c0_value_error("val_equal: invalid comparison of an int and a pointer");
  }

  if (v1.kind == C0_INTEGER) return val2int(v1) == val2int(v2);

  void* p1 = v1.payload.p;
  void* p2 = v2.payload.p;

  // Necessary because we can compare
  // NULL and void*, which are different 'kinds' of pointers
  if ((p1 == NULL) != (p2 == NULL)) return false;
  if ((p1 == NULL) && (p2 == NULL)) return true;
  // If both are NULL, then they are both not tagged pointers.
  // Note NULL is always represented as NULL, and never is
  // 'marked' as a function pointer or tagged pointer

  if (is_taggedptr(p1) && is_taggedptr(p2)) {
    // Assuming a well-typed program, it's not possible for
    // one pointer to be tagged and the other to not be.
    // But VM implementation errors could cause this so it's
    // good to check for this condition anyway
    c0_tagged_ptr* t1 = unmark_tagged_ptr(p1);
    c0_tagged_ptr* t2 = unmark_tagged_ptr(p2);

    ASSERT(!is_taggedptr(t1));
    ASSERT(!is_taggedptr(t2));
    ASSERT(t1->p != NULL && t2->p != NULL);

    return t1->p == t2->p;
  }

  if (!is_taggedptr(p1) && !is_taggedptr(p2)) {
    // Function pointers and regular pointers can be compared directly.
    // However, it is not legal to compare e
    if (ptr_type(p1) != ptr_type(p2)) {
      c0_value_error(
        "val_equal: invalid comparison between "
        "a function pointer and a regular pointer");
    }
    return p1 == p2;
  }

  c0_value_error(
    "val_equal: invalid comparison of "
    "a tagged pointer and an untagged pointer");
  __builtin_unreachable();
}


/*** instruction opcodes ***/

enum instructions {
/* arithmetic operations */
  IADD = 0x60,
  IAND = 0x7E,
  IDIV = 0x6C,
  IMUL = 0x68,
  IOR  = 0x80,
  IREM = 0x70,
  ISHL = 0x78,
  ISHR = 0x7A,
  ISUB = 0x64,
  IXOR = 0x82,

/* stack operations */
  DUP = 0x59,
  POP = 0x57,
  SWAP = 0x5F,

/* memory allocation */
  NEWARRAY = 0xBC,
  ARRAYLENGTH = 0xBE,
  NEW = 0xBB,

/* memory access */
  AADDF = 0x62,
  AADDS = 0x63,
  IMLOAD = 0x2E,
  AMLOAD = 0x2F,
  IMSTORE = 0x4E,
  AMSTORE = 0x4F,
  CMLOAD = 0x34,
  CMSTORE = 0x55,

/* local variables */
  VLOAD = 0x15,
  VSTORE = 0x36,

/* constants */
  ACONST_NULL = 0x01,
  BIPUSH = 0x10,
  ILDC = 0x13,
  ALDC = 0x14,

/* control flow */
  NOP = 0x00,
  IF_CMPEQ = 0x9F,
  IF_CMPNE = 0xA0,
  IF_ICMPLT = 0xA1,
  IF_ICMPGE = 0xA2,
  IF_ICMPGT = 0xA3,
  IF_ICMPLE = 0xA4,
  GOTO = 0xA7,
  ATHROW = 0xBF,
  ASSERT = 0xCF,

/* function calls and returns */
  INVOKESTATIC = 0xB8,
  INVOKENATIVE = 0xB7,
  RETURN = 0xB0,

/* C1 */
  ADDROF_STATIC = 0x16,
  ADDROF_NATIVE = 0x17,
  INVOKEDYNAMIC = 0xB6,

  CHECKTAG = 0xC0,
  HASTAG = 0xC1,
  ADDTAG = 0xC2
};

/*** interface functions (used in c0vm-main.c) ***/

struct bc0_file *read_program(char *filename);
void free_program(struct bc0_file *program);

int execute(struct bc0_file *bc0);


#endif /* _C0VM_H_ */
