#ifndef __COMMON_H_
#define __COMMON_H_

#define PAGE_SIZE 0x1000

#define ALIGN_UP(_val_, _align_) (((_val_) + ((_align_) - 1)) & ~((_align_) - 1))

#define ALIGN_DOWN(_val_, _align_) ((_val_) & ~((_align_) - 1))

#define MIN(_x_, _y_) (((_x_) < (_y_)) ? (_x_) : (_y_))
#define MAX(_x_, _y_) (((_x_) > (_y_)) ? (_x_) : (_y_))

#define OFFSET_OF(_type_, _field_) ((u32)&(((_type_ *)0)->_field_))

#endif
