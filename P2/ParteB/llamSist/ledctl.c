#include <linux/syscalls.h> /* For SYSCALL_DEFINEi() */
#include <linux/kernel.h>
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>


SYSCALL_DEFINE1(ledctl,unsigned int,leds)
{
    struct tty_driver* kbd_driver= NULL;
    unsigned int mask = (unsigned int) leds;

    printk(KERN_INFO "Ledctl: system call received.\n");
    
    // Check mask
    if(mask >= 0 && mask <= 0x7){
        
        /* Translate mask */
        switch(mask){
            case 2:
                mask = 0x4;
            break;
            case 3:
                mask = 0x5;
            break;
            case 4:
                mask = 0x2;
            break;
            case 5:
                mask = 0x3;
            break;
            default:
            break;
        }
        
        // Get the driver
        kbd_driver = vc_cons[fg_console].d->port.tty->driver;

        // Set leds state
        if((kbd_driver->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask) == ENOIOCTLCMD){
            printk(KERN_INFO "Ledctl: I/O command error.\n");
            return -ENOIOCTLCMD;
        }

        printk(KERN_INFO "Ledctl: Leds changed.\n");
    }else{
        printk(KERN_INFO "Ledctl: Invalid argument\n");
        return -EINVAL;
    }

    return 0;
}
