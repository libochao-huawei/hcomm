#ifndef _STUB_IO_H_
#define _STUB_IO_H_

extern UT_MAP_DECLARE(readl, proc);
extern UT_CNT_RANGE_DECLARE(readl, proc);

extern UT_RETURN_STUB_DECLARE(readl);
int stub_io_common_error(void);

extern UT_ENTRY_STUB_DECLARE(platform_get_resource);
extern UT_RETURN_STUB_DECLARE(platform_get_resource);

#endif
