#include <asm/processor.h>

/* common cpu data for all cpus */
struct cpuinfo_x86 boot_cpu_data __read_mostly = {
#ifdef CONFIG_X86_32
	.wp_works_ok = -1,
#endif
};
EXPORT_SYMBOL(boot_cpu_data);

