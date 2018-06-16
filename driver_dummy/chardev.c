#define FOR_OPTEE 1  //for conditional compilation
#define DEVICE_NAME "dummy_two"
#define R_REGS 3 //registers availables to read in one smc read
#define W_REGS 4 //registers availables to write in one smc write

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>        //struct file_operations
#include <linux/cdev.h>      //makes char devices availables: struct cdev, cdev_add()
#include <linux/semaphore.h> //semaphores to coordinate syncronization https://tuxthink.blogspot.com/2011/05/using-semaphores-in-linux.html
#include <linux/uaccess.h>   //copy_to_user, copy_from_user
#include <linux/slab.h>      //kmalloc, kfree
#include <linux/mm.h>        //vm_area_struct, PAGE_SIZE
#include <linux/ioctl.h>     //_IO, _IOR, _IOW

#if FOR_OPTEE
#include <linux/arm-smccc.h>
#include "chardev_smc.h"        //SMC call definition
#endif


//struct for ioctl queries
struct mem_t{
  unsigned long* data;
  unsigned long size;
};

#define DUMMY_SYNC _IO('q', 1001)
#define DUMMY_WRITE _IOW('q', 1002, struct mem_t*)
#define DUMMY_READ  _IOW('q', 1003, struct mem_t*)


//struct for device
struct Dummy_device{
  unsigned long* data;
  unsigned long size;
  struct semaphore sem;
};


//variables (declared here to not use the small available kernel stack).
#if FOR_OPTEE
struct arm_smccc_res res;       //hold returned values from smc call
#endif
struct Dummy_device dum_dev;
struct cdev *my_char_dev; // struct that will allow us to register the char device.
int major_number, ret; // will hold the device major number. used for returns.
dev_t dev_num; // stuct that stores the device number given by the OS.

int device_open(struct inode *inode, struct file *filp);
ssize_t device_read(struct file* filp, char* userBuffer, size_t bufCount, loff_t* curOffset);
ssize_t device_write(struct file* filp, const char* userBuffer, size_t bufCount, loff_t* curOffset);
int device_close(struct inode *inode, struct file *filp);
int device_mmap(struct file *filp, struct vm_area_struct *vma);
long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int __low_write(struct mem_t *mem);
int __low_read(struct mem_t *mem);

// tell the kernel what functions to call when the user operates in our device file.
struct file_operations fops = {
  .owner           = THIS_MODULE,
  .open            = device_open,
  .release         = device_close,
  .write           = device_write,
  .read            = device_read,
  .mmap            = device_mmap,
  .unlocked_ioctl  = device_ioctl
};



//////////////////////////////////////////////////////////////
////////////////////// DEFINITIONS ///////////////////////////
//////////////////////////////////////////////////////////////
int device_open(struct inode *inode, struct file *filp){
  printk(KERN_WARNING DEVICE_NAME ": [opening]\n");
  ret = down_interruptible(&dum_dev.sem);
  if(ret != 0){
    printk(KERN_ALERT DEVICE_NAME ":     could not lock device during open\n");
    return ret;
  }
#if FOR_OPTEE
  arm_smccc_smc(OPTEE_SMC_OPEN_DUMMY, 0, 0, 0, 0, 0, 0, 0, &res);
  if(res.a0 != OPTEE_SMC_RETURN_OK){
    printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_RETURN: failure\n");
    return -1;
  }
  printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_RETURN: ok\n");
  if(res.a1 != OPTEE_SMC_DUMMY_SUCCESS){
    printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_OPEN_DUMMY: failure\n");
    return -1;
  }
#endif
  dum_dev.size=0;
  dum_dev.data = (unsigned long*) get_zeroed_page(GFP_KERNEL);
  if(!dum_dev.data){
    printk(KERN_ALERT DEVICE_NAME ":     could not allocate page\n");
    return -1;
  }
  dum_dev.size=PAGE_SIZE;
  printk(KERN_INFO DEVICE_NAME ":     kernel %ld bytes allocated for the device\n", dum_dev.size);
  printk(KERN_INFO DEVICE_NAME ":     kernel opened\n");
  return 0;
}


int __low_write(struct mem_t *mem){
  int sul = sizeof(unsigned long);
  char args[sul*W_REGS];
  int i,j,k,index = 0;
  //unsigned longs (ulongs) are the mean of transportation:
  //how many ulongs do we need to move the data?
  unsigned long ulongs = mem->size%sul == 0 ? mem->size/sul : mem->size/sul+1;

  //how many calls do we need for that number of ulongs
  unsigned long calls  = ulongs%W_REGS==0 ? ulongs/W_REGS : ulongs/W_REGS+1;
  

  //reset cursor
  arm_smccc_smc(OPTEE_SMC_RESET_DUMMY, 1, 0, 0, 0, 0, 0, 0, &res);
  if(res.a0 != OPTEE_SMC_RETURN_OK){
    printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_RETURN: failure\n");
    return -1;
  }
  printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_RETURN: ok\n");
  if(res.a1 != OPTEE_SMC_DUMMY_SUCCESS){
    printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_RESET_DUMMY: failure\n");
    return -1;
  }

  printk(KERN_INFO DEVICE_NAME ":     wr calls : %ld\n", calls);
  printk(KERN_INFO DEVICE_NAME ":     call size: %d\n",W_REGS);
  printk(KERN_INFO DEVICE_NAME ":     chars    : %ld\n",mem->size);
  printk(KERN_INFO DEVICE_NAME ":     ulongs   : %ld\n",ulongs);

  
  for(i=0; i<calls; ++i){    //for each call
    for(j=0; j<W_REGS; ++j){ //for each long
      for(k=0; k<sul; ++k){  //for each char
	index = j*sul+k;
	if(i*W_REGS*sul+index<mem->size){
	  args[index] = (char) ((char*)(mem->data)) [i*W_REGS*sul+index];
	}else{
	  args[index] = 0;
	}
	//printk(KERN_INFO DEVICE_NAME ":     args[%2d] = mem->data[%2d] = %c\n", index, i*W_REGS+index, args[index]);
      }
    }

#if FOR_OPTEE
    printk(KERN_INFO DEVICE_NAME ":     arm_smccc_smc(OPTEE_SMC_WRITE_DUMMY,\n");

    arm_smccc_smc(OPTEE_SMC_WRITE_DUMMY, ((unsigned long*)args)[0], ((unsigned long*)args)[1], ((unsigned long*)args)[2], ((unsigned long*)args)[3], 0, 0, 0, &res);
    if(res.a0 != OPTEE_SMC_RETURN_OK){
        printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_WRITE_DUMMY: SMC failure\n");
        return -1;
    }else{
	printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_WRITE_DUMMY OK\n");
    }

    if(res.a1 != OPTEE_SMC_DUMMY_SUCCESS){
        printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_WRITE_DUMMY: failure - writing thread\n");
        return -1;
    } 
#else
    printk(KERN_INFO DEVICE_NAME ":     smc(SMC_WRITE,\n");
    for(j=0; j<W_REGS; ++j){
      for(k=0; k<sul; ++k){
	if(args[j*8+k]!=0){
	  printk(KERN_INFO DEVICE_NAME ":       %c\n",args[j*8+k]);
	}else{
	  printk(KERN_INFO DEVICE_NAME ":       Ø\n");
	}
      }
      printk(KERN_INFO DEVICE_NAME ":        \n");
    }
    printk(KERN_INFO DEVICE_NAME ":       &res\n");
    printk(KERN_INFO DEVICE_NAME ":     );\n");
#endif
  }

#if FOR_OPTEE
  //last pure zero write
  arm_smccc_smc(OPTEE_SMC_WRITE_DUMMY, 0, 0, 0, 0, 0, 0, 0, &res);
    if(res.a0 != OPTEE_SMC_RETURN_OK){
        printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_WRITE_DUMMY: (zeroes) SMC failure\n");
        return -1;
   }
#endif
   return 0;
}

int __low_read(struct mem_t *mem){
  int sul = sizeof(unsigned long);
  char buffer[sul*R_REGS]; //buffer for one call
  int i,j,k,index = 0;
  //unsigned longs (ulongs) are the mean of transportation,
  //how many ulongs can we possiblly need in the worst case?
  unsigned long max_ulongs = PAGE_SIZE/sul;
                                    
  //how many calls could we possible need we need for that number of ulongs
  unsigned long max_calls  = max_ulongs/R_REGS;
  
  mem->size=0;
  printk(KERN_WARNING DEVICE_NAME ":         [low_read]\n");
  
  //reset cursor
  arm_smccc_smc(OPTEE_SMC_RESET_DUMMY, 0,0,0,0,0,0,0,&res);
  //SILLY CORRECTNESS CHECK
  if(res.a0 != OPTEE_SMC_RETURN_OK){
    printk(KERN_INFO DEVICE_NAME ":         arm_smccc_smc(write,0,0,...) failed\n");
    return -1;
  }
  printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_RETURN: ok\n");
  if(res.a1 != OPTEE_SMC_DUMMY_SUCCESS){
    printk(KERN_INFO DEVICE_NAME ":     OPTEE_SMC_RESET_DUMMY: failure\n");
    return -1;
  }

  
  for(i=0; i<max_calls; ++i){    //for each call
    arm_smccc_smc(OPTEE_SMC_READ_DUMMY,0,0,0,0,0,0,0,&res);
    if(res.a0 != OPTEE_SMC_RETURN_OK){
      printk(KERN_INFO DEVICE_NAME ":         arm_smccc_smc(read,...) failed\n");
      return -1;
    }
    
    //copy the results into buffer array (everything)
    for(k=0; k<sul; ++k){
      buffer[k]        = ((char*) &(res.a1))[k];
      buffer[sul+k]    = ((char*) &(res.a2))[k];
      buffer[2*sul+k]  = ((char*) &(res.a3))[k];
    }

    // pass one char at the time to mem->data and increment mem->size,
    // if that char is 0, then exit all the loops.
    for(k=0; k<sul*R_REGS; ++k){
      ((char*)(mem->data))[i*sul*R_REGS+k] = buffer[k];
      mem->size += 1;
      if(!buffer[k]){
	printk(KERN_WARNING DEVICE_NAME ":         low_read\n");
	return 0;
      }
    }
  }
  return 0;
}

ssize_t device_read(struct file* filp, char* userBuffer, size_t bufCount, loff_t* curOffset){
  struct mem_t mem;
  
  printk(KERN_WARNING DEVICE_NAME ": [reading]\n");
  if(bufCount > dum_dev.size){
    bufCount = dum_dev.size;
    printk(KERN_INFO DEVICE_NAME ":     data read truncated to %ld\n",dum_dev.size);
  }
  mem.size = 0;
  mem.data = kmalloc(PAGE_SIZE,GFP_KERNEL);
  if(!mem.data){
    printk(KERN_INFO DEVICE_NAME ":     error allocating memory\n");
    return -EACCES;
  }
  
  printk(KERN_INFO DEVICE_NAME ":     reading available chars from device\n");

  __low_read(&mem);

  // move data from kernel space (device) to user space (process).
  // copy_to_user(to,from,size)
  if(copy_to_user(userBuffer, mem.data, mem.size)){
    printk(KERN_INFO DEVICE_NAME ":     error in copy_to_user\n");
    return -EACCES;
  }

  if(mem.data){
    kfree(mem.data);
    printk(KERN_INFO DEVICE_NAME ":     I am a responsable developer, mem unallocated :{)\n");
  }
  printk(KERN_INFO DEVICE_NAME ":     read\n");
  return ret;
}

ssize_t device_write(struct file* filp, const char* userBuffer, size_t bufCount, loff_t* curOffset){
  struct mem_t mem;
  printk(KERN_WARNING DEVICE_NAME ": [writing]\n");
  mem.size = bufCount;
  if(mem.size > dum_dev.size){
    mem.size = dum_dev.size;
    printk(KERN_INFO DEVICE_NAME ":     data to write truncated to %ld\n",dum_dev.size);
  }
  mem.data = kmalloc(mem.size,GFP_KERNEL);
  if(!mem.data){
    printk(KERN_INFO DEVICE_NAME ":     error allocating memory\n");
    return -1;
  }
  memset(mem.data,0,mem.size);
  // copy_from_user(to,from,size)
  if(copy_from_user(mem.data,userBuffer,mem.size)){
    printk(KERN_INFO DEVICE_NAME ":     error in copy_from_user()\n");
    return -EACCES;
  }

  if(__low_write(&mem)){
    printk(KERN_INFO DEVICE_NAME ":     error in __low_write()\n");
    return -1;
  }
  if(mem.data){
    kfree(mem.data);
    printk(KERN_INFO DEVICE_NAME ":     I am a responsable developer, mem unallocated :{)\n");
  }
  printk(KERN_INFO DEVICE_NAME ":     %ld chars written into device\n",mem.size);
  printk(KERN_INFO DEVICE_NAME ":     written\n");
  return 0;
}


//mmap method, maps allocated memory into userspace
int device_mmap(struct file *filp, struct vm_area_struct *vma){
  printk(KERN_WARNING DEVICE_NAME ": [mmaping]\n");
#if FOR_OPTEE
  printk(KERN_INFO DEVICE_NAME ":     Not implemented... yet\n");
#else
  /**
   * remap_pfn_range - remap kernel memory to userspace
   * int remap_pfn_range(struct vm_area_struct* vma, // user vma to map to 
                         unsigned long addr,         // target user address to start at
   *                     unsigned long pfn,          // physical address of kernel memory
                         unsigned long size,         // size of map area
                         pgprot_t prot);             // page protection flags for this mapping
   *  Note: this is only safe if the mm semaphore is held when called.
   */
  if(remap_pfn_range(vma,
		     vma->vm_start,
		     __pa(dum_dev.data)>>PAGE_SHIFT, //physical address of data. shifted to get physical page number
		     PAGE_SIZE,
		     vma->vm_page_prot))
    return -EAGAIN;
  vma->vm_flags |= VM_LOCKED | (VM_DONTEXPAND | VM_DONTDUMP);

#endif
  printk(KERN_INFO DEVICE_NAME ":     mmaped\n");
  return 0;
}

long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
  struct mem_t mem, umem;
  unsigned long* udata; //user space pointer to data
  printk(KERN_WARNING DEVICE_NAME ": [ioctl-ing]\n");
  switch(cmd){


  case DUMMY_SYNC:
    printk(KERN_INFO DEVICE_NAME ":     command DUMMY_SYNC\n");
    printk(KERN_INFO DEVICE_NAME ":     Not implemented... yet\n");
    break;


  case DUMMY_WRITE:
    printk(KERN_INFO DEVICE_NAME ":     command DUMMY_WRITE\n");
    if(copy_from_user(&mem,(struct mem_t*)arg, sizeof(struct mem_t))){
      printk(KERN_INFO DEVICE_NAME ":     error in copy_from_user(struct)\n");
      return -EACCES;
    }

    if(mem.size > dum_dev.size){
      mem.size = dum_dev.size;
      printk(KERN_INFO DEVICE_NAME ":     data to write truncated to %ld\n",dum_dev.size);
    }

    udata = mem.data;
    mem.data = kmalloc(mem.size,GFP_KERNEL);
    if(!mem.data){
      printk(KERN_INFO DEVICE_NAME ":     error allocating memory\n");
      return -1;
    }
    memset(mem.data,0,mem.size);
    
    if(copy_from_user(mem.data,udata,mem.size)){
      printk(KERN_INFO DEVICE_NAME ":     error in copy_from_user(data)\n");
      return -EACCES;
    }

    if(__low_write(&mem)){
      printk(KERN_INFO DEVICE_NAME ":     error in __low_write()\n");
      return -1;
    }

    if(mem.data){
    kfree(mem.data);
    printk(KERN_INFO DEVICE_NAME ":     I am a responsable developer, mem unallocated :{)\n");
  }
    break;

  case DUMMY_READ:
    printk(KERN_INFO DEVICE_NAME ":     command DUMMY_READ\n");
    
    if(copy_from_user(&umem,(struct mem_t*)arg, sizeof(struct mem_t))){
      printk(KERN_INFO DEVICE_NAME ":     error in copy_from_user(struct)\n");
      return -EACCES;
    }

    // mem is used by __low_read
    mem.size = 0;
    mem.data = kmalloc(PAGE_SIZE,GFP_KERNEL);
    if(!mem.data){
      printk(KERN_INFO DEVICE_NAME ":     error allocating memory\n");
      return -1;
    }
    memset(mem.data,0,PAGE_SIZE);

    if(__low_read(&mem)){
      printk(KERN_INFO DEVICE_NAME ":     error in __low_read()\n");
      return -1;
    }

    // filling up umem to be sent
    umem.size = mem.size;
    
    // copying structure (and its .size value)
    if(copy_to_user((struct mem_t*)arg, &umem, sizeof(struct mem_t))){
      printk(KERN_INFO DEVICE_NAME ":     error in copy_to_user(struct)\n");
      return -EACCES;
    }

    // copying data to user's structure
    if(copy_to_user(umem.data, mem.data, mem.size)){
      printk(KERN_INFO DEVICE_NAME ":     error in copy_to_user(data)\n");
      return -EACCES;
    }

    if(mem.data){
      kfree(mem.data);
      printk(KERN_INFO DEVICE_NAME ":     I am a responsable developer, mem unallocated :{)\n");
    }
    break;


  default:
    printk(KERN_INFO DEVICE_NAME ":     command %d not valid\n",cmd);
  }
  printk(KERN_INFO DEVICE_NAME ":     ioctled\n");
  return 0;
}

int device_close(struct inode *inode, struct file *filp){
  printk(KERN_WARNING DEVICE_NAME ": [closing]\n");
  if(dum_dev.size>0){
    free_page((unsigned long)dum_dev.data);
    printk(KERN_INFO DEVICE_NAME ":     %ld bytes released from the device\n", dum_dev.size);
    dum_dev.size=0;
  }
  up(&dum_dev.sem);
  printk(KERN_INFO DEVICE_NAME ":     Good bye\n");
  return 0;
}







static int driver_entry(void){
  printk(KERN_WARNING "\n" DEVICE_NAME ": [[registring]]\n");
  // register the device with the system:
  // (step 1) dynamic allocation of device major number
  // alloc_chardev_region(dev_t*, uint fminor, uint count, char* name)
  ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
  if(ret < 0){
    printk(KERN_ALERT DEVICE_NAME ":     failed to allocate major number\n");
    return ret;   //propagate error
  }
  major_number = MAJOR(dev_num); //extract major number assigned by OS.
  printk(KERN_INFO DEVICE_NAME ":     use \"mknod /dev/" \
	 DEVICE_NAME " c %d 0\" to create device file\n",major_number);

  // (step 2) create the cdev structure
  my_char_dev = cdev_alloc();  // initialize char dev.
  my_char_dev->ops = &fops;    // file operations offered by the driver.
  my_char_dev->owner = THIS_MODULE;

  // (step 3) add the character device to the system
  // int cdev_add(struct cdev* dev, dev_t num, unsigned int count)
  ret = cdev_add(my_char_dev, dev_num, 1);
  if(ret < 0){
    printk(KERN_ALERT DEVICE_NAME ":     failed to add cdev to kernel\n");
    return ret;   //propagate error
  }

  //(step 4) initialize semaphore
  sema_init(&dum_dev.sem,1);   // initial value 1, it can access only one thread at the time.
  printk(KERN_INFO DEVICE_NAME ":     registred\n");
  return 0;
}

static void driver_exit(void){
  printk(KERN_WARNING DEVICE_NAME ": [[removing]]\n");
  // remove char device from system
  cdev_del(my_char_dev);

  // deallocate device major number
  unregister_chrdev_region(dev_num,1);
  printk(KERN_INFO DEVICE_NAME ":     device removed from kernel\n");
}


// macros to define entry and exit functions of the driver
module_init(driver_entry)
module_exit(driver_exit)
MODULE_LICENSE("GPL");
