An efficient, low-fragmentation malloc implementation:

  Our solution uses a segregated explicit free list split into 10 different "bucket sizes" - powers of 2 from <64 
  to >16384. The free lists use LIFO coalescing to insert new elements upon free. The segregation of the free list 
  provided approximately double the throughput than a single explicit list.
 
  Some additional important performance optimizations involve: 
 
  1) realloc'ing larger blocks than requested, 
  2) pre-allocating additional free blocks in a small size class,
  3) rounding up block sizes to even powers of 2 if they're "close enough" (85% of the way there, e.g. >0.85*2^N)
 
  The justification for #1 is that often when one reallocs a block, they are likely to do so again. Instead of waiting, let's 
  pre-emptively give them more than they bargained for, rendering future realloc calls as doing nothing but returning the original
  pointer (because they will have space). Common examples of this in practice include extending vectorized array objects, such as 
  the C++ Vector<> class which will somewhat more than double the original list size when it gets full.
  
  Pre-allocating additional small blocks is a predictive feature based on the following logic: if one creates one small block of
  memory, they will likely want more of that same small size. So it pays in performance to set up your free list so you can service
  these requests very quickly in the future. 
 
  Rounding up block sizes near powers of 2 to full powers of 2 helps deal with the unfortunate case of users slightly underallocating
  their data structures and subsequently reallocating to a size not significantly larger. We found 85% to be a good threshold which
  balanced these cases while limiting internal fragmentation. While the numeric values to implement these optimizations were 
  heuristically chosen, ideally a smarter dynamic memory package could learn a history of allocations and tune as such. 
 
 
 
 