# MemoryAccessTracker-MAT-
The aim of this PIN tool is to track memory accesses and to compute the offsets between the accessed addresses and the start of the corresponding memory blocks and to derive a memory graph from it. 
	
How to install:
   - Download and install PIN
   - set PIN_ROOT to the Pin directory
   - call make.
   
How to execute the Pin Tool:
```
pin -t obj-intel64/mat.so -- $YourBinary 
```
## Overview

The PIN Tool has the following functionalities
<p>
<img src="https://user-images.githubusercontent.com/12165606/34743024-925c7304-f588-11e7-8ec3-86f09906bfa5.png", width="400" height="400" />
 </p>
 The tool records all allocations (malloc, calloc, realloc, mmap, memalign, brk, sbrk, free, munmap) and inserts them in a Splay tree. The Splay tree guarantees a faster lookup of memory ranges. Additionally a threshold can be set to only record larger allocations. Since a binary might easily generate billions of accesses, the tool provides the following mechanisms to reduce the overhead:
 
   * Signal Handler in order to switch on/off the instrumentation for memory accesses
   * Instrumenting only routines of interests (currently main-function)
   
## Memory Graph
A memory graph is created from the memory trace. This happens currently in a postprocessing step, but work is ongoing to output such a graph directly from the PIN Tool. The memory trace contains information about which sourcecodeline has accessed which allocations at which time. In order to derive access patterns from that, relative memory accesses are used instead of raw addresses. Relative means that the distance from accessing an allocation to start of allocation is computed. For instance, sequential accesses to an array A:
<p>
<img src="https://user-images.githubusercontent.com/12165606/34743931-d3d2f594-f58b-11e7-9dce-ff2a24530674.png", width="300" height="100" />
 </p>
Relative accesses to the buffer A can be described as (A 0) (A 8) (A 16) (A 24) (A 32) where the second element in
the tuple defines the accessed address. Alternatively the access can be defined as (A 8), where the second element defines the stride. The stride is the difference between consecutive accesses e.g. accessing element 4 at address 24 is followed by accessing element 5 at address 32 and therefore the stride is 32 − 24 = 8

A memory graph is created where each node represents a certain bufferID, stride and type (read or write access):
 ```
if ( bufferID , stride , type ) not in nodes :
   createNewNode ()
else :
   counter = getNode ( bufferID , stride , type )
   counter = counter + 1
```
An example graph: 
<p>
<img src="https://user-images.githubusercontent.com/12165606/34743029-94b8a898-f588-11e7-968a-8736f44598fc.png", width="800" height="600" />
 </p>
 The nodes are read the following way: **18 40 356352 runARawLoops.cxx-524 - 44201** means that Buffer with ID 18 was accessed with stride 40. The size of the buffer is 356352 Bytes. The access was made in file runARawLoops.cxx at line 524. This memory access was made 44201 times and it was always followed by an access with the notation **18 -32 356352 runARawLoops.cxx-524** Gray nodes indicate write access, white nodes indicate read accesses. Currently noces carry some more information such as the object size and sourcecodeline, since this makes the graph more readable. However, for later processings this information is skipped.
 
 
Memory graphs allow to tremendously reduce the size of memory traces. Comparing memory traces from different workloads becomes then a graph similarity problem and can be solved with N-Gram Mining and Levenhstein distance. 

This work was done during my time as Visiting Scientist at LLNL and the work would not have been possible without the great support I received there.


