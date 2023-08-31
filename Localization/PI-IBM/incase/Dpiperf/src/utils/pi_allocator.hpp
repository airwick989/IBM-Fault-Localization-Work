//
// Linux Perf Util - C++ Allocator for Perf Utils
//

#include <stdlib.h>
#include <new>
#include <limits>

template <class T>
class pi_allocator {
public:

   // Member Types
   typedef T value_type;
   typedef T* pointer;
   typedef T& reference;
   typedef const T* const_pointer;
   typedef const T& const_reference;
   typedef std::size_t size_type;
   typedef std::ptrdiff_t difference_type;
   template <class U> struct rebind {
      typedef pi_allocator<U> other;
   };

   // Constructors
   pi_allocator() throw() {}
   pi_allocator(const pi_allocator&) throw() {}
   template <class U> pi_allocator(const pi_allocator<U>&) throw() {}

   // Destructor
   ~pi_allocator() throw() {}

   pointer       address(reference x) const { return &x; }
   const_pointer address(const_reference  x) const { return &x; }

   pointer allocate (size_type n, void *hint = 0) {
      if (0 == n)
         return NULL;
      pointer ret = (pointer)malloc(n * sizeof(value_type));
      if (NULL == ret)
         throw std::bad_alloc();
      return ret;
   }

   void deallocate (pointer p, size_type n) {
      free(p);
   }

   size_type max_size() const throw() {
      return std::numeric_limits<size_type>::max() / sizeof(value_type);
   }

   void construct ( pointer p, const_reference val) {
      new ((void *)p) value_type(val);
   }

   void destroy (pointer p) {
      p->~value_type();
   }

};
