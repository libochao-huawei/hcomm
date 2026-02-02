/**
 * unit test config header
 *
 * this file will overwrite the kconfig.h 
 */

#undef CONFIG_NR_CPUS
#define CONFIG_NR_CPUS 1

/* debug list  */
#define CONFIG_DEBUG_LIST 1

#define CONFIG_OF 1
#define CONFIG_OF_ADDRESS 1

/* enable Kernel virtual address for page_address stub */
#define WANT_PAGE_VIRTUAL   1

#define CONFIG_KASAN   1
#define CONFIG_KASAN_SHADOW_OFFSET 0

#define CONFIG_PCI 1
#define CONFIG_PCI_IOV 1
#define CONFIG_GENERIC_MSI_IRQ 1
#define CONFIG_DCB 1

#define CONFIG_HNS3_DCB 1
#define CONFIG_HCLGE_DCB 1

//Hi1980
//#define CONFIG_DEBUG_LIST 1
#undef CONFIG_DEBUG_LIST
