/* FinSH config file */

#ifndef __MSH_CFG_H__
#define __MSH_CFG_H__

/* =============================================================
 * ????:
 * ?? #ifndef ??,??? rtconfig.h ???????
 * ?????? Warning,???????????
 * ============================================================= */

// <<< Use Configuration Wizard in Context Menu >>>

#ifndef RT_USING_FINSH
    #define RT_USING_FINSH
#endif

#ifndef FINSH_USING_MSH
    #define FINSH_USING_MSH
#endif

#ifndef FINSH_USING_MSH_ONLY
    #define FINSH_USING_MSH_ONLY
#endif

// <h>FinSH Configuration
// <o>the priority of finsh thread <1-30>
//  <i>the priority of finsh thread
//  <i>Default: 21
#ifndef FINSH_THREAD_PRIORITY
    #define FINSH_THREAD_PRIORITY       21
#endif

// <o>the stack of finsh thread <1-4096>
//  <i>the stack of finsh thread
//  <i>Default: 4096  (4096Byte)
#ifndef FINSH_THREAD_STACK_SIZE
    #define FINSH_THREAD_STACK_SIZE     1024
#endif

#ifndef FINSH_USING_SYMTAB
    #define FINSH_USING_SYMTAB
#endif

// <c1>Enable command description
//  <i>Enable command description
#ifndef FINSH_USING_DESCRIPTION
    #define FINSH_USING_DESCRIPTION
#endif
//  </c>
// </h>

// <<< end of configuration section >>>
#endif