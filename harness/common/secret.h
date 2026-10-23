/*
 * secret.h - marcatura dei dati segreti per ctgrind (era POISON.H in defines.h).
 *
 * Con -DCTGRIND (make TOOL=ctgrind) poison() rende la memoria "non
 * inizializzata" per memcheck: ogni branch o indirizzo che ne dipende produce
 * un errore. Senza CTGRIND le macro non fanno nulla, quindi la libreria del
 * decoder e' la stessa per tutti gli strumenti e valgrind serve solo qui.
 */
 #pragma once

 #if defined(CTGRIND)
 #include <valgrind/memcheck.h>
 #define poison(addr, len)      VALGRIND_MAKE_MEM_UNDEFINED(addr, len)
 #define unpoison(addr, len)    VALGRIND_MAKE_MEM_DEFINED(addr, len)
 #define is_poisoned(addr, len) VALGRIND_CHECK_MEM_IS_DEFINED(addr, len)
 #else
 #define poison(addr, len)      ((void)(addr), (void)(len))
 #define unpoison(addr, len)    ((void)(addr), (void)(len))
 #define is_poisoned(addr, len) 0
 #endif
 
 