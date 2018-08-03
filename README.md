# Memory-System-In-C-
Memory system practice in C++
This repository contains some of the files from a memory system assignment
assigned to us by our professor. We were required to edit the initialize,
malloc, and free methods that would be the core of the new memory system, as well as create
any other methods that we think would help. 

The way it works is by initializing a large space in memory as a free block.
Then whenever the user needs to use space, the program will designate enough space 
for that information and create a used block then insert it within a free block large enough
to accomodate it. The Free blocks are sorted chronologically, where two are three adjacent 
Free blocks will combine to form one larger one. and the Used blocks are sorted
by the most recently allocated used block placed at the front of the linked list.
Headers are used to keep important information such as how large the block is, what type 
of block, as well as whether the block above it is free or not. 
