/**************************************************************************/
/*              COPYRIGHT Carnegie Mellon University 2021                 */
/* Do not post this file or any derivative on a public site or repository */
/**************************************************************************/
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "lib/xalloc.h"
#include "lib/stack.h"
#include "lib/contracts.h"
#include "lib/c0v_stack.h"
#include "lib/c0vm.h"
#include "lib/c0vm_c0ffi.h"
#include "lib/c0vm_abort.h"

/* call stack frames */
typedef struct frame_info frame;
struct frame_info {
  c0v_stack_t S;   /* Operand stack of C0 values */
  ubyte *P;        /* Function body */
  size_t pc;       /* Program counter */
  c0_value *V;     /* The local variables */
};

int execute(struct bc0_file *bc0) {
  REQUIRES(bc0 != NULL);

  /* Variables */
  c0v_stack_t S = c0v_stack_new(); /* Operand stack of C0 values */

  // Initialize for P, the byte code for the function 
  ubyte *P = bc0->function_pool->code; // Allocate for P 

  size_t pc = 0;     /* Current location within the current byte array P */

  size_t num_vars = (size_t) bc0->function_pool->num_vars; 
  c0_value *V = xcalloc(num_vars, sizeof(c0_value));   /* Local variables (you won't need this till Task 2) */

  //(void) V;         // silences compilation errors about V being currently unused

  /* The call stack, a generic stack that should contain pointers to frames */
  /* You won't need this until you implement functions. */
  gstack_t callStack = stack_new();
  (void) callStack; // silences compilation errors about callStack being currently unused

  while (true) {

#ifdef DEBUG
    /* You can add extra debugging information here */
    fprintf(stderr, "Opcode %x -- Stack size: %zu -- PC: %zu\n",
            P[pc], c0v_stack_size(S), pc);
#endif

    switch (P[pc]) {

    /* Additional stack operation: */

    c0_value v1; 
    c0_value v2;

    case POP: {
      assert(!c0v_stack_empty(S));
      pc++;
      c0v_pop(S);
      break;
    }

    case DUP: {
      assert(!c0v_stack_empty(S));
      pc++;
      c0_value v = c0v_pop(S);
      c0v_push(S,v);
      c0v_push(S,v);
      break;
    }

    case SWAP:
      pc++; 
      assert(!c0v_stack_empty(S)); // Safety 
      v2 = c0v_pop(S);
      assert(!c0v_stack_empty(S)); // Safety 
      v1 = c0v_pop(S);
      c0v_push(S, v2);
      c0v_push(S, v1); 
      break;


    /* Returning from a function.
     * This currently has a memory leak! You will need to make a slight
     * change for the initial tasks to avoid leaking memory.  You will
     * need to revise it further when you write INVOKESTATIC. */

    case RETURN: {
      c0_value retval = c0v_pop(S);
      // Another way to print only in DEBUG mode
      // IF_DEBUG(fprintf(stderr, "Returning %d from execute()\n", val2int(retval)));

      // Free everything before returning from the execute function! 
      free(V); // Free the local vars 
      c0v_stack_free(S); // Free the operand stack 

      if(stack_empty(callStack)) {
        stack_free(callStack, &free);
        return val2int(retval); 
      }
      else { // otherwise, pick up the caller function 
        frame* caller = (frame*) pop(callStack); 
        S = caller->S; 
        P = caller->P; 
        V = caller->V; 
        pc = caller->pc; 
        free(caller);
        c0v_push(S, retval); // push returned value into the stack
      }

      break; 
      
    }


    /* Arithmetic and Logical operations */

    // Variables that will be used later repetitively 
    int x; 
    int y; 
    int res; 
    void* ptr; 

    case IADD: // Addition 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 
      // printf("Calculating %d + %d\n", x, y);

      res = (int)((unsigned int)x + (unsigned int)y); // Deal with overflow
      // printf("Result is: %d\n", res);
      c0v_push(S, int2val(res));
      break; 

    case ISUB: // Subtraction 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      res = (int)((unsigned int)x - (unsigned int)y); // Deal with overflow
      c0v_push(S, int2val(res));
      break; 

    case IMUL: // Multiplication 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      res = (int)((unsigned int)x * (unsigned int)y); // Deal with overflow
      c0v_push(S, int2val(res));
      break; 

    case IDIV: // Division 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      if(y == 0 || (x == -2147483648 && y == -1)){
        c0_arith_error("Division by 0 error");  
      } 

      res = x/y; 
      c0v_push(S, int2val(res));
      break; 

    case IREM: // Modulus 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      if(y == 0 || (x == -2147483648 && y == -1)){
        c0_arith_error("Division by 0 error");  
      } 

      res = x%y; 
      c0v_push(S, int2val(res));
      break; 

    case IAND: // The & bit operation 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      res = x&y; 
      c0v_push(S, int2val(res));
      break; 

    case IOR: // The | bit operation 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      res = x|y; 
      c0v_push(S, int2val(res));
      break; 

    case IXOR: // The ^ bit operation 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      res = x^y; 
      c0v_push(S, int2val(res));
      break; 

    case ISHR: // The >> bit operation 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      if(!(0 <= y && y <= 31)) c0_arith_error("Shift by invalid numbe of bits");

      res = x>>y; 
      c0v_push(S, int2val(res));
      break; 

    case ISHL: // The << bit operation 
      pc++; 
      assert(!c0v_stack_empty(S));  // Safety 
      y = val2int(c0v_pop(S)); 
      assert(!c0v_stack_empty(S));  // Safety 
      x = val2int(c0v_pop(S)); 

      if(!(0 <= y && y <= 31)) c0_arith_error("Shift by invalid numbe of bits");

      res = x<<y; 
      c0v_push(S, int2val(res));
      break; 


    /* Pushing constants */

    case BIPUSH:
      pc++; 
      int pushed = (int)(byte)P[pc]; 
      // printf("Pushed int is %d\n", pushed);
      c0_value val = int2val(pushed); // Record the next value to be pushed
      pc++; 
      c0v_push(S, val); 
      break; 

    case ILDC:
      pc++; 
      uint32_t c1 = (uint32_t)P[pc]; 
      pc++; 
      uint32_t c2 = (uint32_t)P[pc]; 
      pc++; 

      res = (int) bc0->int_pool[(c1<<8)|c2]; // By definition 
      c0v_push(S, int2val(res)); 

      break; 

    case ALDC:
      pc++; 
      c1 = (uint32_t)P[pc]; 
      pc++; 
      c2 = (uint32_t)P[pc]; 
      pc++; 

      ptr = (void*) &bc0->string_pool[(c1<<8)|c2];
      c0v_push(S, ptr2val(ptr)); 

      break; 

    case ACONST_NULL:
      pc++; 
      ptr = (void*) NULL; 
      c0v_push(S, ptr2val(ptr));
      break; 


    /* Operations on local variables */

    ubyte ind; 

    case VLOAD:
      pc++; 
      ind = P[pc]; // index in V
      pc++; 
      c0v_push(S, V[ind]);
      break; 

    case VSTORE:
      pc++; 
      ind= P[pc]; 
      pc++; 
      // constant = val2int(c0v_pop(S)); 
      // V[ind] = int2val(constant); 
      V[ind] = c0v_pop(S); 
      break; 


    /* Assertions and errors */

    char *a; 

    case ATHROW:
      pc++; 
      a = val2ptr(c0v_pop(S));
      c0_user_error((char*) a); 
      break; 

    case ASSERT:
      pc++; 
      a = val2ptr(c0v_pop(S));
      x = val2int(c0v_pop(S)); 

      if(x == 0) c0_assertion_failure((char*) a); 

      break; 


    /* Control flow operations */

    case NOP:
      pc++; 
      break; 

    // 0x9F if_cmpeq <o1,o2>  S, v1, v2 -> S       (pc = pc+(o1<<8|o2) if v1 == v2)    

    ubyte o1; 
    ubyte o2;  
    int offset1; 
    int offset2; 

    case IF_CMPEQ:
      pc++; 
      v1 = c0v_pop(S); 
      v2 = c0v_pop(S); 

      o1 = P[pc]; 
      pc++; 
      o2 = P[pc]; 
      pc++; 

      offset1 = (int16_t) (byte) o1; 
      offset2 = (int16_t) (byte) o2; 

      // Has to cancel the pc change from line 346 and 343 
      if(val_equal(v1, v2)) pc = pc+(offset1<<8|offset2) - 3; 

      break; 

    // 0xA0 if_cmpne <o1,o2>  S, v1, v2 -> S       (pc = pc+(o1<<8|o2) if v1 != v2)  
    case IF_CMPNE:
      pc++; 
      v1 = c0v_pop(S); 
      v2 = c0v_pop(S); 

      o1 = P[pc]; 
      pc++; 
      o2 = P[pc]; 
      pc++; 

      offset1 = (int16_t) (byte) o1; 
      offset2 = (int16_t) (byte) o2; 

      if(!val_equal(v1, v2)) pc = pc+(offset1<<8|offset2) - 3; 

      break; 

    // 0xA1 if_icmplt <o1,o2> S, x:w32, y:w32 -> S (pc = pc+(o1<<8|o2) if x < y) 
    case IF_ICMPLT:
      pc++; 
      y = val2int(c0v_pop(S)); 
      x = val2int(c0v_pop(S)); 

      o1 = P[pc]; 
      pc++; 
      o2 = P[pc]; 
      pc++; 

      offset1 = (int16_t) (byte) o1; 
      offset2 = (int16_t) (byte) o2; 

      if(x < y) pc = pc+(offset1<<8|offset2) - 3; 

      break; 

    // 0xA2 if_icmpge <o1,o2> S, x:w32, y:w32 -> S (pc = pc+(o1<<8|o2) if x >= y)
    case IF_ICMPGE:
      pc++; 
      y = val2int(c0v_pop(S)); 
      x = val2int(c0v_pop(S)); 

      o1 = P[pc]; 
      pc++; 
      o2 = P[pc]; 
      pc++; 

      offset1 = (int16_t) (byte) o1; 
      offset2 = (int16_t) (byte) o2; 

      if(x >= y) pc = pc+(offset1<<8|offset2) - 3; 

      break; 

    // 0xA3 if_icmpgt <o1,o2> S, x:w32, y:w32 -> S (pc = pc+(o1<<8|o2) if x > y)  
    case IF_ICMPGT:
      pc++; 
      y = val2int(c0v_pop(S)); 
      x = val2int(c0v_pop(S)); 

      o1 = P[pc]; 
      pc++; 
      o2 = P[pc]; 
      pc++; 

      offset1 = (int16_t) (byte) o1; 
      offset2 = (int16_t) (byte) o2; 

      if(x > y) pc = pc+(offset1<<8|offset2) - 3; 

      break; 

    // 0xA4 if_icmple <o1,o2> S, x:w32, y:w32 -> S (pc = pc+(o1<<8|o2) if x <= y)  
    case IF_ICMPLE: 
      pc++; 
      y = val2int(c0v_pop(S)); 
      x = val2int(c0v_pop(S)); 

      o1 = P[pc]; 
      pc++; 
      o2 = P[pc]; 
      pc++; 

      offset1 = (int16_t) (byte) o1; 
      offset2 = (int16_t) (byte) o2; 

      if(x <= y) pc = pc+(offset1<<8|offset2) - 3; 
      break; 

    case GOTO:
      pc++; 
      o1 = P[pc]; int16_t newo1 = (int16_t) o1; 
      pc++; 
      o2 = P[pc]; int16_t newo2 = (int16_t) o2; 
      pc++; 

      int16_t offset = (int16_t) (newo1<<8|newo2); 

      pc = pc + offset - 3;

      break;


    /* Function call operations: */

    c0_value* temp; 

    case INVOKESTATIC:
      pc++; 
      c1 = P[pc]; 
      pc++; 
      c2 = P[pc]; 
      pc++; 

      // information of the function being called upon 
      struct function_info* fi = &bc0->function_pool[c1<<8|c2]; 
      // Construct a temporary variable array for future use 
      temp = xcalloc(sizeof(c0_value), fi->num_vars);
      for(size_t i = 0; i < fi->num_args; i++)
      {
        temp[fi->num_args-i-1] = c0v_pop(S);
      }

      // /* call stack frames */
      // typedef struct frame_info frame;
      // struct frame_info {
      //   c0v_stack_t S;   /* Operand stack of C0 values */
      //   ubyte *P;        /* Function body */
      //   size_t pc;       /* Program counter */
      //   c0_value *V;     /* The local variables */
      // };

      // Initialize a new caller frame to be pushed into caller stack 
      frame* callerframe = xmalloc(sizeof(frame)); 
      callerframe->S = S; 
      callerframe->P = P; 
      callerframe->pc = pc; 
      callerframe->V = V; 
      push(callStack, (stack_elem) callerframe); 

      // Initialize the new function call 
      S = c0v_stack_new(); 
      P = fi->code; 
      pc = 0; 
      V = temp; 

      break; 


    case INVOKENATIVE:
      pc++; 
      c1 = P[pc]; 
      pc++; 
      c2 = P[pc]; 
      pc++; 

      // information of the native function being called upon 
      struct native_info* ni = &bc0->native_pool[c1<<8|c2]; 
      // Construct a temporary variable array for future use 
      temp = xcalloc(sizeof(c0_value), ni->num_args);
      for(size_t i = 0; i < ni->num_args; i++)
      {
        ASSERT(ni->num_args-i-1 < ni->num_args);
        temp[ni->num_args-i-1] = c0v_pop(S);
      }

      // Keep the native function 
      native_fn* fn = native_function_table[ni->function_table_index]; 
      // Get the result of the native function 
      c0_value result = (*fn)(temp); 
      c0v_push(S, result); // Push the result back to the c0 value stack

      break; 


    /* Memory allocation and access operations: */

    size_t size; 

    case NEW:
      pc++; 
      size = (size_t) P[pc]; // get the byte size to be allocated; 
      pc++; 

      ptr = xcalloc(1, size); 
      c0v_push(S, ptr2val(ptr));

      break; 

    case IMLOAD:
      pc++; 

      ptr = val2ptr(c0v_pop(S)); // Get the integer from opprand stack 
      if(ptr == NULL) c0_memory_error("Memory error"); 

      x = *(int*) ptr; 
      c0v_push(S, int2val(x));

      break; 

    case IMSTORE:
      pc++; 

      x = val2int(c0v_pop(S)); 
      ptr = val2ptr(c0v_pop(S)); 
      if(ptr == NULL) c0_memory_error("Memory error");

      *(int*)ptr = x; // Modify memory 

      break; 


    case AMLOAD: {
      pc++; 

      void** A = val2ptr(c0v_pop(S)); 
      if(A == NULL) c0_memory_error("Memory error");;
      void* B = *A; 
      c0v_push(S, ptr2val(B));

      break; 
    
    }

    case AMSTORE: {
      pc++; 

      void* B = val2ptr(c0v_pop(S)); 
      void** A = val2ptr(c0v_pop(S)); 
      if(A == NULL) c0_memory_error("Memory error");
      *A = B; 

      break; 

    }

    case CMLOAD: {
      pc++; 

      void* A = val2ptr(c0v_pop(S));
      if(A == NULL) c0_memory_error("Memory error"); 
      x = (int)(*(byte*)A); // Has to explicit cast 
      c0v_push(S, int2val(x));

      break; 

    }

    case CMSTORE: {
      pc++; 

      x = val2int(c0v_pop(S));
      void* A = val2ptr(c0v_pop(S));
      if(A == NULL) c0_memory_error("Memory error"); 

      *(byte*)A = x & 0x7f; 

      break; 
    }

    case AADDF:{
      pc++; 
      ubyte f = P[pc]; // Field offset value, unsigned one byte
      pc++; 

      void* A = val2ptr(c0v_pop(S)); 
      char* new_ptr = (char*)A + f; 

      c0v_push(S, ptr2val((void*)new_ptr));

      break; 

    }


    /* Array operations: */

    case NEWARRAY: {
      pc++; 
      int s = (int)(byte)P[pc]; // Number of bytes of each element in array
      pc++; 

      int n = val2int(c0v_pop(S)); // Number of elements in the array 

      if(n == 0) c0v_push(S, ptr2val((void*) NULL)); // NULL pointer if length 0
      else if(n < 0) c0_memory_error("Invalid array length"); 
      else {
        // Allocate the array in C heap 
        char* arr = xcalloc((size_t)s, (size_t)n); 

        // Allocate and initialize a c0_array struct
        c0_array* c0arr = xmalloc(sizeof(c0_array));
        c0arr->count = n; 
        c0arr->elt_size = s; 
        c0arr->elems = (void*) arr; 

        // Push pointer to c0 array struct back to stack
        c0v_push(S, ptr2val((void*) c0arr)); 

      }

      break; 

    }

    case ARRAYLENGTH: {
      pc++; 

      void* A = val2ptr(c0v_pop(S));
      c0_array* c0arr = (c0_array*) A; // Obtain pointer to c0 array struct 

      int n; 
      if(c0arr == NULL) n = 0; // NULL pointer represents array of length 0
      else              n = c0arr->count; // Length/size of the array 
      
      c0v_push(S, int2val(n));

      break; 

    }


    case AADDS: {
      pc++; 

      int i = val2int(c0v_pop(S)); 
      c0_array* A = (c0_array*) val2ptr(c0v_pop(S));

      // Check validity of array struct pointer 
      if(A == NULL) c0_memory_error("Accessing array of length 0");
      if(!(0 <= i && i < A->count)) c0_memory_error("Index out of bound"); 

      int s = A->elt_size; // size of each element 
      char* pointer = ((char*)A->elems) + s*i; 
      c0v_push(S, ptr2val((void*)pointer)); 

      break; 

    }


    /* BONUS -- C1 operations */

    case CHECKTAG:

    case HASTAG:

    case ADDTAG:

    case ADDROF_STATIC:

    case ADDROF_NATIVE:

    case INVOKEDYNAMIC:

    default:
      fprintf(stderr, "invalid opcode: 0x%02x\n", P[pc]);
      abort();
    }
  }

  /* cannot get here from infinite loop */
  assert(false);
}
