#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <linux/ftrace.h>
#include <linux/spinlock.h>
#include <linux/sort.h>

/* Datos del módulo */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SMP-Safe Kernel Module - FDI-UCM");
MODULE_AUTHOR("Miguel Higuera Romero & Alejandro Nicolás Ibarra Loik");

/* Constantes */
#define BUF_LEN 100

/* Entrada de proc */
static struct proc_dir_entry *proc_entry;

/* Lista enlazada */
struct list_head mylist;
int listCount;  //Contador de nodos de la lista

typedef struct {
  int data;
  struct list_head links;
} list_item_t;

/* Inicializo el spin-lock */
DEFINE_SPINLOCK(sp);


/* Función que limpia la lista entera */
void cleanUpList(void){
	list_item_t *mynodo;
	struct list_head* cur_node=NULL;
    struct list_head* aux=NULL;

    spin_lock(&sp);

    /* Recorremos la lista */
    list_for_each_safe(cur_node, aux, &mylist) {
        mynodo = list_entry(cur_node,list_item_t, links);

        /* Eliminamos nodo de la lista */
        list_del(cur_node);

        /*Liberamos memoria dinámica del nodo */
        vfree(mynodo);
    }
    listCount = 0;

    spin_unlock(&sp);

}

/* Funcion de comparacion */
int cmpElement(const void *a, const void *b){
    /* Recogemos y tipamos los nodos */
    list_item_t *aa = *((list_item_t **) a);
    list_item_t *bb = *((list_item_t **) b);

    /* Devolvemos <0 si a > b o >=0 si a <= b */
    return aa->data - bb->data;
}

/* Funcion de cambio. Recibe las direcciones de dos nodos e intercambia
 * sus contenidos
 */
void swapp(void * a, void * b, int size){
    /* Recogemos y tipamos los nodos */
    list_item_t *aa = *((list_item_t **) a);
    list_item_t *bb = *((list_item_t **) b);

    /* Intercambiamos el dato de las posiciones */
    int aux = aa->data;
    aa->data = bb->data;
    bb->data = aux;
}

/* Función que añade un elemento a la lista */
void addElement(int num){
    list_item_t *mynodo;
    
    /* Reservamos memoria dinámica para el nuevo nodo */
    mynodo = (list_item_t*) vmalloc(sizeof(list_item_t));

    spin_lock(&sp);
    /* Guardamos el valor leido */
    mynodo->data = num;

    /* Añadimos el nodo a la lista */
    list_add_tail(&mynodo->links, &mylist);
    /** Incrementamos el contador de nodos */
    listCount++;
    spin_unlock(&sp);

    
}

/* Funcion que ordena la lista */
void sortlist(void){
    spin_lock(&sp);    

    list_item_t *arr[listCount];    // Array de punteros que guarda las
                                    //direcciones de los nodos de la lista
    int i = 0;
    list_item_t *mynodo;            // Puntero hacia el nodo actual
    struct list_head* cur_node=NULL;

    

    /* Recorremos la lista */
    list_for_each(cur_node, &mylist) {
        mynodo = list_entry(cur_node,list_item_t, links);

        /* Guardamos la dirección del nodo en el array */
        arr[i] = mynodo;
        i++;
    }

    /* Ordenamos la lista */
    sort(&arr[0], listCount, sizeof(list_item_t *), cmpElement, swapp);

    spin_unlock(&sp);

}

void removeElement(int num){
    struct list_head* cur_node=NULL;
    struct list_head* aux=NULL;
    list_item_t *mynodo;


    spin_lock(&sp);

    /* Para cada nodo de la lista comprobamos si contiene num */
    list_for_each_safe(cur_node, aux, &mylist) {
        mynodo = list_entry(cur_node,list_item_t, links);
        if(mynodo->data == num) {
            /* Eliminamos de la lista */
            list_del(cur_node);
            /* Liberamos memoria dinámica */
            vfree(mynodo);
            listCount--;
        }
    }

    spin_unlock(&sp);

}

/* Función write */
static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    int available_space = BUF_LEN-1;
    int num;					 //Numero leído en add/remove
    char procstr[BUF_LEN];		 //Cadena escrita en /proc
    char kbuff[BUF_LEN];	     //Copia de buffer de usuario
    list_item_t *mynodo;		 //nodo a añadir/eliminar

    /* The application can write in this entry just once !! */
    if ((*off) > 0)
        return 0;

    if (len > available_space) {
        printk(KERN_INFO "Modlist: not enough space!!\n");
        return -ENOSPC;
    }

    /* Transfer data from user to kernel space */
    if (copy_from_user( &kbuff[0], buf, len ))
        return -EFAULT;

    kbuff[len] = '\0'; 	/* Add the '\0' */
    *off+=len;            /* Update the file pointer */

    /* Leemos ordenes */
    if(sscanf(&kbuff[0],"add %i",&num)) {
        addElement(num);

        /* Informamos */
        printk(KERN_INFO "Modlist: Added element %i to the list\n", num);

    }else if(sscanf(&kbuff[0],"remove %i",&num)) {
        /* Si la lista no esta vacia */
        if(list_empty(&mylist) == 0) {
            removeElement(num);

            /* Informamos */
            printk(KERN_INFO "Modlist: Element %i deleted from list\n", num);
        }else
            printk(KERN_INFO "Modlist: Can't remove element %i. The list empty\n", num);
    }else if(sscanf(&kbuff[0],"%s", procstr)){
        if(strcmp("cleanup", procstr)==0) {
            /* Si la lista no esta vacia */
            if(list_empty(&mylist) == 0) {
                /* Limpiamos la lista entera */
                cleanUpList();
                /* Informamos */
                printk(KERN_INFO "Modlist: List cleaned up\n");
            }else
                printk(KERN_INFO "Modlist: Can't cleanup list. The list empty\n");
        }else if(strcmp("sort", procstr) == 0){
            /* Si la lista no esta vacia */
            if(list_empty(&mylist) == 0) {
                /* Ordenamos cadena */
                sortlist();
                printk(KERN_INFO "Modlist: List sorted\n");
            }else
                printk(KERN_INFO "Modlist: Can't sort list. The list empty\n");
        }
    }
    else {
        // retornar un codigo de error correspondiente (instruccion incorrecta)
        return -EINVAL;
    }
    return len;
}

/* Función read */
static ssize_t modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

    int nr_bytes;					    // Bytes leídos
    list_item_t *item=NULL;			    //Nodo a leer
    struct list_head* cur_node=NULL;
    char kbuff[BUF_LEN];			    //buffer final
    char strtmp[BUF_LEN];				//cadenas temporales
    kbuff[0] = '\0';
	int cerrojo = 1;

    if ((*off) > 0) /* Tell the application that there is nothing left to read */
        return 0;

    /* Listamos los elementos de la lista */
    spin_lock(&sp);
    list_for_each(cur_node, &mylist) {
        item = list_entry(cur_node,list_item_t, links);
        /* Escribimos en msgtmp el dato, el \n y lo concatenamos al kbuff final */
        sprintf(strtmp, "%i\n", item->data);
    	if(strlen(kbuff) < BUF_LEN - strlen(strtmp) && cerrojo){
      	      strcat(kbuff, strtmp);
    	}else{
    		cerrojo = 0;
    	}
    }
    spin_unlock(&sp);
    strcat(kbuff, "\0");

    /* Cargamos los bytes leídos */
    nr_bytes=strlen(kbuff);

    /* Enviamos datos al espacio de ususario */
    if (copy_to_user(buf, kbuff, nr_bytes))
        return -EINVAL;

    (*off)+=len;  /* Update the file pointer */

    /* Informamos */
    printk(KERN_INFO "Modlist: Elements listed\n");

    return nr_bytes;
}

static const struct file_operations proc_entry_fops = {
    .read = modlist_read,
    .write = modlist_write,
};

/* Función init module */
int init_modlist_module( void )
{
    /* Creamos la entrada de proc */
    proc_entry = proc_create( "smp-safe", 0666, NULL, &proc_entry_fops);
    if (proc_entry == NULL) {
        printk(KERN_INFO "SMP-Safe: Can't create /proc entry\n");
        return -ENOMEM;
    } else {
        printk(KERN_INFO "SMP-Safe: Module loaded\n");
    }

    /* Inicializamos la lista */
    INIT_LIST_HEAD(&mylist);

    /* Inicializamos el contador de nodos */
    listCount = 0;

    return 0;
}

/* Función exit module */
void exit_modlist_module( void )
{
    /* Limpiamos la lista para liberar memoria si no está vacía */
    if(list_empty(&mylist) == 0)
        cleanUpList();

    /* Eliminamos la entrada de proc */
    remove_proc_entry("smp-safe", NULL);

    /* Informamos */
    printk(KERN_INFO "SMP-Safe: Module unloaded.\n");
}


module_init( init_modlist_module );
module_exit( exit_modlist_module );
