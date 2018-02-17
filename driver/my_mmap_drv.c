/* 
 * Memory mapping: provides user programs with direct access to device memory
 * Mapped area must be multiple of PAGE_SIZE, and starting address aligned to
 * PAGE_SIZE
 *
 * syscall: mmap(caddr_t addr, size_t len, int ptro, int flags, int fd, off_t off)
 * file operation: int (*mmap)(struct file *f, struct vm_area_struct *vma)
 *
 * Driver needs to: build page tables for address range, and replace vma->vm_ops 
 * Building page tables:
 *	- all at once: remap_page_range
 *	- one page at a time: nopage method. Finds correct page for address, and 
 *    increments its reference cout. Must be implemented if driver supports
 *    mremap syscall
 *
 * fields in struct vm_area_struct:
 *	- unsigned long vm_start, vm_end 		: virtual address range covered by VMA
 *	- struct file *vm_file					: file associated with VMA
 *	- struct vm_operations_struct *vm_ops	: functions that kernel
 *                   							will invoke to operate in VMA
 *	- void *vm_private_data					: used by driver to store its own 
 * 												information
 *
 * VMA operations:
 *	- void (*open)(struct vm_area_struct *vma):
 * 						invoked when a new reference
 *                   	to the VMA is made, except when the VMA is first created,
 *                   	when mmap is called
 *	- struct page *(*nopage)(struct vm_area_struct *area,
 *					unsigned long address, int write_access): 
 *						invoked by page fault handler when process tries to access 
 *						valid page in VMA, but not currently in memory
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

#define DEVNAME "my_mmap"

const char *msg = "My mmap options implement, this is file: ";
#define PRINTFUNC()	pr_info("== %s ==\n", __func__);

struct mmap_info{
	void *data;
	int ref;	// mmapped times
};

static void my_vma_open(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;

	info->ref++;
	PRINTFUNC();
}

static void my_vma_close(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;

	info->ref--;
	PRINTFUNC();
}

static int my_vma_nopage(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct mmap_info *info;
	struct page *page = NULL;
	int err = 0;

	do{
		if(!vma || (unsigned long)vmf->virtual_address > vma->vm_end){
			pr_err("== %s: invalid address! ==\n", __func__);
			err = VM_FAULT_SIGBUS;
			break;
		}

		info = (struct mmap_info *)vma->vm_private_data;
		if(!info->data){
			pr_err("== %s: No data pool pointer ==\n", __func__);
			err = VM_FAULT_SIGBUS;
			break;
		}

		PRINTFUNC();
		/* Get the page */
		page = virt_to_page(info->data);
		get_page(page);
		vmf->page = page;
	}while(0);	

	return err;	
}

struct vm_operations_struct my_vm_ops = {
	.open	= my_vma_open,
	.close	= my_vma_close,
	.fault	= my_vma_nopage,
};

static int my_open(struct inode *ip, struct file *filp)
{
	struct mmap_info *info;

	PRINTFUNC();
	info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
	info->data = (void *)get_zeroed_page(GFP_KERNEL);
	
	memcpy((char *)info->data, msg, strlen(msg));
	memcpy((char *)info->data + strlen(msg),
				filp->f_dentry->d_name.name,
				strlen(filp->f_dentry->d_name.name));
	filp->private_data = info;
	
	return 0;
}

static int my_release(struct inode * ip, struct file *filp)
{
	struct mmap_info *info = filp->private_data;
	
	PRINTFUNC();
	free_page((unsigned long)info->data);
	kfree(info);
	filp->private_data = NULL;	
	
	return 0;
}

static int my_mmap(struct file *filp, struct vm_area_struct *vma)   
{  
	vma->vm_ops = &my_vm_ops;
	vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP); 
	/* assigned the pointer of structure */
	vma->vm_private_data = filp->private_data;
	my_vma_open(vma);	

	return  0;  
}  

static struct file_operations my_fops = {
	.owner		= THIS_MODULE,
	.open		= my_open,
	.release	= my_release,
	.mmap		= my_mmap,	
};

static struct miscdevice my_miscdev = {
	.minor	= 99,
	.name	= DEVNAME,
	.fops	= &my_fops,
};

static int __init my_mmap_example_init(void)
{
	int ret;
	
	ret = misc_register(&my_miscdev);
	if(ret){
		pr_err("== %s: misc register failed ==\n", __func__);
		return ret;
	}

	PRINTFUNC();

	return 0;
}

static void __exit my_mmap_example_exit(void)
{
	misc_deregister(&my_miscdev);
	PRINTFUNC();
}

module_init(my_mmap_example_init);
module_exit(my_mmap_example_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Phil Chang");  
