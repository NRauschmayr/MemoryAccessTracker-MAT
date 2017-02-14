# MemoryAccessTracker-MAT-
The aim of this PIN tool is to track memory accesses and to compute the offsets between the accessed addresses and the start of the corresponding memory blocks. Therefore the tool records all allocations (malloc, calloc, realloc, mmap, memalign, brk, sbrk, free, munmap) and insert them in a Splay tree. The Splay tree guarantees a faster lookup of memory ranges. Since a binary might easily generate billions of accesses, the tool provides the following mechanisms to reduce the overhead:
   - Signal Handler in order to switch on/off the instrumentation for memory accesses
   - Instrumenting only routines of interests (currently main-function)
	
How to install:
   - Download and install PIN
   - set PIN_ROOT to the Pin directory
   - call make.
   
How to execute the Pin Tool:
    pin -t obj-intel64/mt.so -- $YourBinary
	

