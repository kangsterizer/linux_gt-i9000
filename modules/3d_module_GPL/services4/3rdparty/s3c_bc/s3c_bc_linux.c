/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 * 		Samsung Electronics System LSI. modify
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful but, except 
 * as otherwise stated in writing, without any warranty; without even the 
 * implied warranty of merchantability or fitness for a particular purpose. 
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK 
 *
 ******************************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/ioctl.h>

#if defined(LMA)
#include <linux/pci.h>
#else
#include <linux/dma-mapping.h>
#endif

#include "s3c_bc.h"
#include "s3c_bc_linux.h"
#include "pvrmodule.h"

#define DEVNAME	S3C_BC_DEVICE_NAME
#define	DRVNAME	DEVNAME

#if defined(__i386__) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)) && defined(SUPPORT_LINUX_X86_PAT) && defined(SUPPORT_LINUX_X86_WRITECOMBINE)
#include <asm/cacheflush.h>
#endif

MODULE_SUPPORTED_DEVICE(DEVNAME);

#if defined(LDM_PLATFORM) || defined(LDM_PCI)

static struct class *psPvrClass;
#endif

int S3C_BC_Bridge(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

static int AssignedMajorNumber;

static struct file_operations S3C_BC_fops = {
	ioctl:S3C_BC_Bridge,
};


#define unref__ __attribute__ ((unused))

#if defined(LMA)

#define VENDOR_ID_PVR               0x1010
#define DEVICE_ID_PVR               0x1CF1
#endif



static int __init S3C_BC_ModInit(void)
{
#if defined(LDM_PLATFORM) || defined(LDM_PCI)
    struct device *psDev;
#endif

#if defined(LMA)
	struct pci_dev *psPCIDev;
	int error;
#endif

#if defined(LMA)
	psPCIDev = pci_get_device(VENDOR_ID_PVR, DEVICE_ID_PVR, NULL);
	if (psPCIDev == NULL)
	{
		printk(KERN_ERR DRVNAME ": S3C_BC_ModInit:  pci_get_device failed\n");

		goto ExitError;
	}

	if ((error = pci_enable_device(psPCIDev)) != 0)
	{
		printk(KERN_ERR DRVNAME ": S3C_BC_ModInit: pci_enable_device failed (%d)\n", error);
		goto ExitError;
	}
#endif

	AssignedMajorNumber = register_chrdev(0, DEVNAME, &S3C_BC_fops);

	if (AssignedMajorNumber <= 0)
	{
		printk(KERN_ERR DRVNAME ": S3C_BC_ModInit: unable to get major number\n");

		goto ExitDisable;
	}

#if defined(DEBUG)
	printk(KERN_DEBUG DRVNAME ": S3C_BC_ModInit: major device %d\n", AssignedMajorNumber);
#endif

#if defined(LDM_PLATFORM) || defined(LDM_PCI)
	
	psPvrClass = class_create(THIS_MODULE, DEVNAME);

	if (IS_ERR(psPvrClass))
	{
		printk(KERN_ERR DRVNAME ": S3C_BC_ModInit: unable to create class (%ld)", PTR_ERR(psPvrClass));
		goto ExitUnregister;
	}

	psDev = device_create(psPvrClass, NULL, MKDEV(AssignedMajorNumber, 0),
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,26))
						  NULL,
#endif 
						  DEVNAME);
	if (IS_ERR(psDev))
	{
		printk(KERN_ERR DRVNAME ": S3C_BC_ModInit: unable to create device (%ld)", PTR_ERR(psDev));
		goto ExitDestroyClass;
	}
#endif 

	if(S3C_BC_Register() != S3C_BC_OK)
	{
		printk (KERN_ERR DRVNAME ": S3C_BC_ModInit: can't init device\n");
		goto ExitUnregister;
	}
	
	//printk("s3c_bc: physical base addres = 0x%x\n", S3C_BC_DEVICE_PHYS_ADDR_START);

#if defined(LMA)
	
	pci_disable_device(psPCIDev);
#endif

	return 0;

#if defined(LDM_PLATFORM) || defined(LDM_PCI)
ExitDestroyClass:
	class_destroy(psPvrClass);
#endif
ExitUnregister:
	unregister_chrdev(AssignedMajorNumber, DEVNAME);
ExitDisable:
#if defined(LMA)
	pci_disable_device(psPCIDev);
ExitError:
#endif
	return -EBUSY;
} 


static void __exit S3C_BC_ModCleanup(void)
{
#if defined(LDM_PLATFORM) || defined(LDM_PCI)
	device_destroy(psPvrClass, MKDEV(AssignedMajorNumber, 0));
	class_destroy(psPvrClass);
#endif

	unregister_chrdev(AssignedMajorNumber, DEVNAME);
	
	if(S3C_BC_Unregister() != S3C_BC_OK)
	{
		printk (KERN_ERR DRVNAME ": S3C_BC_ModCleanup: can't deinit device\n");
	}

} 


void *BCAllocKernelMem(unsigned long ulSize)
{
	return kmalloc(ulSize, GFP_KERNEL);
}

void BCFreeKernelMem(void *pvMem)
{
	kfree(pvMem);
}

S3C_BC_ERROR BCOpenPVRServices (IMG_HANDLE *phPVRServices)
{
	
	*phPVRServices = 0;
	return (S3C_BC_OK);
}


S3C_BC_ERROR BCClosePVRServices (IMG_HANDLE unref__ hPVRServices)
{
	
	return (S3C_BC_OK);
}

S3C_BC_ERROR BCGetLibFuncAddr (char *szFunctionName, PFN_BC_GET_PVRJTABLE *ppfnFuncTable)
{
	if(strcmp("PVRGetBufferClassJTable", szFunctionName) != 0)
	{
		return (S3C_BC_ERROR_INVALID_PARAMS);
	}

	
	*ppfnFuncTable = PVRGetBufferClassJTable;

	return (S3C_BC_OK);
}

int S3C_BC_Bridge(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = -EFAULT;
	int command = _IOC_NR(cmd);
	S3C_BC_ioctl_package *psBridge = (S3C_BC_ioctl_package *)arg;

	if(!access_ok(VERIFY_WRITE, psBridge, sizeof(S3C_BC_ioctl_package)))
	{
		return err;
	}

	switch(command)
	{
		case _IOC_NR(S3C_BC_ioctl_get_physical_base_address):
		{
			psBridge->outputparam = S3C_BC_DEVICE_PHYS_ADDR_START;
			break;
		}
		default:
			return err;
	}

	return 0;
}

module_init(S3C_BC_ModInit);
module_exit(S3C_BC_ModCleanup);
