# C0-Virtual-Machine
A C0 virtual machine that executes C0 bytecode generated by C0 compiler 

C0 is a language similar to C. It is used exclusively in CMU 15122: Priciples of Imperative Computation course for instructive purposes. 
A compiler will first generate C0 bytecode. Then this virtual machine, based on C, is going to execute the C0 bytecode following a stack machine principle. 


==========================================================

Compiling and running your C0VM implementation (with -DDEBUG)
(Must compile iadd.c0 to iadd.bc0 first)
   % make
   % ./c0vmd tests/iadd.bc0

Compiling and running your C0VM implementation (without -DDEBUG)
(Must compile iadd.c0 to iadd.bc0 first)
   % make
   % ./c0vm tests/iadd.bc0

==========================================================
