#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <linux/ftrace.h>
#include <linux/sort.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist Kernel Module - FDI-UCM");
MODULE_AUTHOR("Miguel Higuera Romero & Alejandro Nicolás Ibarra Loik");

#define BUF_LEN 100

/* Entrada de proc */
static struct proc_dir_entry *proc_entry;

/* Lista enlazada */
struct list_head mylist;
int count = 0;

/* Nodos de la lista */
typedef struct {
  int data;
  struct list_head links;
} list_item_t;

/* Función que limpia la lista entera */
void cleanUpList(void){
	list_item_t *mynodo;
	struct list_head* cur_node=NULL;
  struct list_head* aux=NULL;

  /* Recorremos la lista */
  list_for_each_safe(cur_node, aux, &mylist) {
    mynodo = list_entry(cur_node,list_item_t, links);
    /* Eliminamos nodo de la lista */
    list_del(cur_node);

    /*Liberamos memoria dinámica del nodo */
    vfree(mynodo);
  }
  count = 0;
}

/* Funcion de comparacion */
int cmpElement(const void *a, const void *b){
  int *aa = (int *) a;
  int *bb = (int *) b;
  if(*aa > *bb){
    return 1;
  }else{
    return -1;
  }
}

/* Funcion de cambio */
void swapp(void * a, void * b, int size){
  void *aux = a;
  a = b;
  b = aux;
}

void addElement(int num){
  list_item_t *mynodo;		//nodo a añadir/eliminar
  /* Creamos el nuevo nodo */
  mynodo = (list_item_t*) vmalloc(sizeof(list_item_t));
  
  /* Guardamos el valor leido */
  mynodo->data = num;
  
  /* Añadimos el nodo a la lista */
  list_add_tail(&mynodo->links, &mylist);
  count++;
}

/* Funcion que ordena la lista */
void sortlist(void){
  int *arr;
  int i = 0, j = 0;
  list_item_t *mynodo;
	struct list_head* cur_node=NULL;
  struct list_head* aux=NULL;
  
  arr = (int *) vmalloc(count*sizeof(int));

  /* Recorremos la lista */
  list_for_each_safe(cur_node, aux, &mylist) {
    mynodo = list_entry(cur_node,list_item_t, links);
    
    arr[i] = mynodo->data;
    i++;
  }
  sort(&arr[0], count, sizeof(int), cmpElement, NULL);
  cleanUpList();
  for(j=0; j < i; j++){
    addElement(arr[j]);
  }
  vfree(arr);
}

/* Función write */
static ssize_t modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
  int available_space = BUF_LEN-1;
  int num;					//Numero leído en add/remove
  char cadena[BUF_LEN];		//Cadena escrita en /proc
  char modlist[BUF_LEN];	//Copia de buffer de usuario
  list_item_t *mynodo;		//nodo a añadir/eliminar

  /* The application can write in this entry just once !! */
  if ((*off) > 0)
    return 0;

  if (len > available_space) {
    printk(KERN_INFO "Modlist: not enough space!!\n");
    return -ENOSPC;
  }

  /* Transfer data from user to kernel space */
  if (copy_from_user( &modlist[0], buf, len ))
    return -EFAULT;

  modlist[len] = '\0'; 	/* Add the '\0' */
  *off+=len;            /* Update the file pointer */

  /* Leemos ordenes */
  if(sscanf(&modlist[0],"add %i",&num)) {
    addElement(num);

    /* Informamos */
    printk(KERN_INFO "Modlist: Added element %i to the list\n", num);

  }
  else if(sscanf(&modlist[0],"remove %i",&num)) {

    /* Si la lista no esta vacia */
    if(list_empty(&mylist) == 0) {
      struct list_head* cur_node=NULL;
      struct list_head* aux=NULL;

  	  /* Para cada nodo de la lista comprobamos si contiene num */
      list_for_each_safe(cur_node, aux, &mylist) {
        mynodo = list_entry(cur_node,list_item_t, links);
        if(mynodo->data == num) {
      		/* Eliminamos de la lista */
            list_del(cur_node);
          /* Liberamos memoria dinámica */
          vfree(mynodo);
          count--;
  		  }
      }
      printk(KERN_INFO "Modlist: Element %i deleted from list\n", num);
    }
    else
      printk(KERN_INFO "Modlist: List empty\n");
  }
  else if(sscanf(&modlist[0],"%s", cadena)){
    if(strcmp("cleanup", cadena)==0) {
      /* Si la lista no esta vacia */
      if(list_empty(&mylist) == 0) {
        /* Limpiamos la lista entera */
        cleanUpList();
        /* Informamos */
        printk(KERN_INFO "Modlist: List cleaned up\n");
      }
      else
        printk(KERN_INFO "Modlist: List empty\n");
    }
    else if(strcmp("sort", cadena) == 0){
      /* Si la lista no esta vacia */
      if(list_empty(&mylist) == 0) {
        /* Ordenamos cadena */
        sortlist();
        printk(KERN_INFO "Modlist: List sorted\n");
      }
      else
        printk(KERN_INFO "Modlist: List empty\n");
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

  int nr_bytes;						// Bytes leídos
  list_item_t *item=NULL;			//Nodo a leer
  struct list_head* cur_node=NULL;
  char msg[BUF_LEN];				//Mensaje final
  char msgtmp[BUF_LEN];				//Mensajes temporales
  msg[0] = '\0';

  if ((*off) > 0) /* Tell the application that there is nothing left to read */
      return 0;

  /* Listamos los elementos de la lista */
  list_for_each(cur_node, &mylist) {
    item = list_entry(cur_node,list_item_t, links);

    /* Escribimos en msgtmp el dato, el \n y lo concatenamos al msg final */
    sprintf(msgtmp, "%i", item->data);
    strcat(msgtmp, "\n");
    strcat(msg, msgtmp);
  }
  strcat(msg, "\0");

  /* Cargamos los bytes leídos */
  nr_bytes=strlen(msg);

  /*if (len<nr_bytes)
    return -ENOSPC;*/

  /* Enviamos datos al espacio de ususario */
  if (copy_to_user(buf, msg, nr_bytes))
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



int init_modlist_module( void )
{
	/* Creamos la entrada de proc */
	proc_entry = proc_create( "modlist", 0666, NULL, &proc_entry_fops);
	if (proc_entry == NULL) {
	  printk(KERN_INFO "Modlist: Can't create /proc entry\n");
	  return -ENOMEM;
	} else {
	  printk(KERN_INFO "Modlist: Module loaded\n");
	}

	/* Inicializamos la lista */
	INIT_LIST_HEAD(&mylist);

	return 0;
}


void exit_modlist_module( void )
{
  /* Limpiamos la lista para liberar memoria */
  cleanUpList();

  /* Eliminamos la entrada de proc */
  remove_proc_entry("modlist", NULL);

  /* Informamos */
  printk(KERN_INFO "Modlist: Module unloaded.\n");
}


module_init( init_modlist_module );
module_exit( exit_modlist_module );