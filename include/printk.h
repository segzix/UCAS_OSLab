#ifndef INCLUDE_PRINTK_H_
#define INCLUDE_PRINTK_H_

/* kernel print */
int printk(const char *fmt, ...);

/* vt100 print */
int printv(const char *fmt, ...);

/* (QEMU-only) save print content to logfile */
int printl(const char *fmt, ...);

/* save print content to destbuf */
int sprintk(char* dstbuf, const char *fmt, ...);

#endif
