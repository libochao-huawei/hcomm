#include <linux/debugfs.h>
#include <linux/types.h>

typedef unsigned short		umode_t;
struct dentry root_dir;
struct dentry *debugfs_create_file(const char *name, umode_t mode,
				   struct dentry *parent, void *data,
				   const struct file_operations *fops)
{
	if (!parent || !fops)
		return NULL;

	if (parent->d_flags == 0xff)
		return NULL;

	parent->d_fsdata = fops;

	return parent;
}
EXPORT_SYMBOL_GPL(debugfs_create_file);


struct dentry *debugfs_create_dir(const char *name, struct dentry *parent)
{
	if (strncmp(name, "abcdefg", 7) == 0)
		return NULL;

	if (strncmp(name, "test on", 7) == 0)
		root_dir.d_flags = 0xff;
	else if (strncmp(name, "test off", 8) == 0)
		root_dir.d_flags = 0;

	return &root_dir;
}
EXPORT_SYMBOL_GPL(debugfs_create_dir);

void debugfs_remove_recursive(struct dentry *dentry)
{
	return ;
}
EXPORT_SYMBOL_GPL(debugfs_remove_recursive);

int simple_open(struct inode *inode, struct file *file)
{
	return 0;
}
EXPORT_SYMBOL(simple_open);

int kstrtouint(const char *s, unsigned int base, unsigned int *res)
{
	char *ptr;

	*res  = strtol(s, &ptr, base);
	if (*res == 0)
		return 1;

	return 0;
}
EXPORT_SYMBOL(kstrtouint);

int kstrtou16(const char *s, unsigned int base, unsigned short *res)
{
	unsigned long val;
	char *ptr;

	val = strtol(s, &ptr, base);
	if (val >= 0x10000)
		return -22;

	*res = (unsigned short)val;

	return 0;
}
EXPORT_SYMBOL(kstrtou16);
