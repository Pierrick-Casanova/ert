#define  _GNU_SOURCE   /* Must define this to get access to pthread_rwlock_t */
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <hash.h>
#include <util.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <block_fs.h>
#include <vector.h>
#include <buffer.h>

#define MOUNT_MAP_MAGIC_INT 8861290

typedef struct file_node_struct file_node_type;
typedef struct sort_node_struct sort_node_type;


typedef enum {
  ALLOC_NODE_ACTION    = 1,     /* A new node is added. */
  DELETE_NODE_ACTION   = 2,     /* A node is deleted. */
  REUSE_NODE_ACTION    = 3      /* We are reusing a node from the free_nodes list. */
} block_fs_action_type;

/**
   Internal index layout:

   |<StatusTag: Int><Key: String><node_size: Int><data_size: Int>|
             
  /|\                                                           /|\ 
   |                                                             | 
   |                                                             |
          
node_offset                                                  data_offset

  The node_offset and data_offset values are not stored on disk, but rather
  implicitly read with ftell() calls.
*/
    
   


struct file_node_struct{
  long int         node_offset;   /* The offset into the data_file of this node. NEVER Changed. */
  long int         data_offset;    
  int              node_size;     /* The size in bytes of this node - must be >= data_size. NEVER Changed. */
  int              data_size;     /* The size of the data stored in this node - in addition the node might need to store header information. */
  file_node_type * next;          /* Implementing doubly linked list behaviour WHEN the node is in */
  file_node_type * prev;          /*     the free_nodes list of the block_fs object. */
};




/* Help structure only used for pretty-printing the block file layout. */
struct sort_node_struct {
  long int          node_offset;
  int               node_size;
  int               data_size;
  char            * name; 
};



struct block_fs_struct {
  char           * path;
  char           * mount_file;
  char           * base_name;
  int              version;
  
  char           * log_file;
  char           * data_file;
  char           * index_file;
  
  FILE           * data_stream;
  FILE           * index_stream;
  FILE           * log_stream;

  long int         data_data_size;  /* The total number of bytes in the data_file. */
  long int         free_size;       /* Size of 'holes' in the data file. */ 
  int              block_size;      /* The size of blocks in bytes. */

  
  pthread_mutex_t  io_lock;         /* Lock held during fread / fwrite to the data file. */
  pthread_rwlock_t rw_lock;         /* Read-write lock during all access to the fs. */
  
  int              num_free_nodes;   
  hash_type      * index;
  file_node_type * free_nodes;
  vector_type    * file_nodes;      /* This vector owns all the file_node instances - the index and free_nodes structures
                                       only contain pointers to the objects stored in this vector. */
  int              write_count;     /* This just counts the number of writes since the file system was mounted. */
  bool             log_transactions;
};


static void block_fs_log_reuse_node( block_fs_type * block_fs ,  const char * filename , const file_node_type * file_node);
static void block_fs_log_alloc_node( block_fs_type * block_fs ,  const char * filename , const file_node_type * file_node);
static void block_fs_log_delete_node( block_fs_type * block_fs ,  const char * filename );
static void block_fs_clear_log( block_fs_type * block_fs );
static void block_fs_apply_log( block_fs_type * block_fs );

static void block_fs_fprintf_free_nodes( block_fs_type * block_fs) {
  file_node_type * current = block_fs->free_nodes;
  int counter = 0;
  while (current != NULL ) {
    printf("Offset:%ld   node_size:%d   data_size:%d \n",current->node_offset , current->node_size , current->data_size);
    if (current->next == current)
      util_abort("%s: linked list broken \n",__func__);
    current = current->next;
    counter++;
  }
  printf("\n");
  if (counter != block_fs->num_free_nodes) 
    util_abort("%s: Counter:%d    num_free_nodes:%d \n",__func__ , counter , block_fs->num_free_nodes);
}


/*****************************************************************/
/* file_node functions */



static void file_node_fprintf(const file_node_type * node, const char * name) {
  printf("%s: [Offset:%ld  Node_size:%d] \n",name , node->node_offset , node->node_size);
}



/**
   Observe that the two input arguments to this function should NEVER change. They represent offset and size
   in the underlying data file, and that is for ever fixed.
*/


static file_node_type * file_node_alloc( long int offset , int node_size) {
  file_node_type * file_node = util_malloc( sizeof * file_node , __func__);
  
  file_node->node_offset = offset;    /* These should NEVER change. */
  file_node->node_size   = node_size;
  file_node->data_size   = 0;
  file_node->data_offset = 0;
  
  file_node->next = NULL;
  file_node->prev = NULL;
  
  return file_node;
}



static void file_node_free( file_node_type * file_node ) {
  free( file_node );
}


static void file_node_free__( void * file_node ) {
  file_node_free( (file_node_type *) file_node );
}



static void file_node_index_fwrite( const file_node_type * file_node , FILE * stream ) {
  util_fwrite_long( file_node->node_offset , stream);
  util_fwrite_int( file_node->node_size    , stream );
  util_fwrite_int( file_node->data_size    , stream );
}



static void file_node_index_fread( file_node_type * file_node , FILE * stream ) {
  file_node->node_offset = util_fread_long( stream );
  file_node->node_size   = util_fread_int( stream );
  file_node->data_size   = util_fread_int( stream );
}

/**
   This will instantiate a file_node instance from an index file; in particular the data_offset will be locked
   to the node_offset.
   
*/
static file_node_type * file_node_index_fread_alloc( FILE * stream ) {
  file_node_type * node = file_node_alloc( 0,0);
  file_node_index_fread( node , stream );
  node->data_offset = node->node_offset;
  return node;
}




/* file_node functions - end. */
/*****************************************************************/

static inline void block_fs_aquire_wlock( block_fs_type * block_fs ) {
  pthread_rwlock_wrlock( &block_fs->rw_lock );
}


static inline void block_fs_release_rwlock( block_fs_type * block_fs ) {
  pthread_rwlock_unlock( &block_fs->rw_lock );
}


static inline void block_fs_aquire_rlock( block_fs_type * block_fs ) {
  pthread_rwlock_rdlock( &block_fs->rw_lock );
}



/**
   
*/   
static void block_fs_fwrite_index( block_fs_type * block_fs ) {
  fseek( block_fs->index_stream , 0 , SEEK_SET );
  util_fwrite_long( block_fs->data_data_size , block_fs->index_stream );
  
  /* Writing the nodes from the index. */
  util_fwrite_int( hash_get_size( block_fs->index ) , block_fs->index_stream );
  {
    hash_iter_type * iter        = hash_iter_alloc( block_fs->index );
    while ( !hash_iter_is_complete( iter )) {
      const char * key            = hash_iter_get_next_key( iter );
      const file_node_type * node = hash_get( block_fs->index , key );
      util_fwrite_string( key , block_fs->index_stream );
      file_node_index_fwrite( node , block_fs->index_stream );
    }
    hash_iter_free( iter );
  }
  
  /* Inserting the free nodes - the holes. */
  util_fwrite_int( block_fs->num_free_nodes , block_fs->index_stream );
  {
    file_node_type * current = block_fs->free_nodes;
    while (current != NULL) {
      file_node_index_fwrite( current , block_fs->index_stream );
      current = current->next;
    }
  }
  fsync( fileno( block_fs->index_stream ) );
  ftell( block_fs->index_stream );
  /**
     Delete the log. 
  */
  if (block_fs->log_transactions)
    block_fs_clear_log( block_fs );
}




static void block_fs_insert_index_node( block_fs_type * block_fs , const char * filename , const file_node_type * file_node) {
  block_fs_log_alloc_node( block_fs , filename , file_node);
  hash_insert_ref( block_fs->index , filename , file_node);
}



/**
   Inserts a file_node instance in the linked list of free nodes. The
   list is sorted in order of increasing node size.
*/

static void block_fs_insert_free_node( block_fs_type * block_fs , file_node_type * new ) {
  /* Special case: starting with a empty list. */
  printf("=================================================================\n");
  printf("Inserting node with offset:%ld \n",new->node_offset);
  block_fs_fprintf_free_nodes( block_fs);
  new->data_size = 0;
  if (block_fs->free_nodes == NULL) {
    new->next = NULL;
    new->prev = NULL;
    block_fs->free_nodes = new;
  } else {
    file_node_type * current = block_fs->free_nodes;
    file_node_type * prev    = NULL;
    
    while ( current != NULL && (current->node_size < new->node_size)) {
      prev = current;
      current = current->next;
    }
    
    if (current == NULL) {
      /* 
         The new node should be added at the end of the list - i.e. it
         will not have a next node.
      */
      new->next = NULL;
      new->prev = prev;
      prev->next = new;
    } else {
      /*
        The new node should be placed BEFORE the current node.
      */
      if (prev == NULL) {
        /* The new node should become the new list head. */
        block_fs->free_nodes = new;
        new->prev = NULL;
      } else {
        prev->next = new;
        new->prev  = prev;
      }
      current->prev = new;
      new->next  = current;
    }
    if (new != NULL)     if (new->next == new) util_abort("%s: broken LIST1 \n",__func__);
    if (prev != NULL)    if (prev->next == prev) util_abort("%s: broken LIST2 \n",__func__);
    if (current != NULL) if (current->next == current) util_abort("%s: Broken LIST 3\n",__func__);
    
  }
  block_fs->num_free_nodes++;
  block_fs->free_size += new->node_size;
  block_fs_fprintf_free_nodes( block_fs);
  printf("=================================================================\n");
}


/**
   Installing the new node AND updating file tail. 
*/

static void block_fs_install_node(block_fs_type * block_fs , file_node_type * node) {
  block_fs->data_data_size = util_int_max( block_fs->data_data_size , node->node_offset + node->node_size);  /* Updating the total size of the file - i.e the next available offset. */
  vector_append_owned_ref( block_fs->file_nodes , node , file_node_free__ );
}


static void block_fs_set_filenames( block_fs_type * block_fs ) {
  util_safe_free( block_fs->log_file );
  util_safe_free( block_fs->data_file );
  util_safe_free( block_fs->index_file );

  {
    char * log_ext   = util_alloc_sprintf("log_%d" , block_fs->version );
    char * index_ext = util_alloc_sprintf("index_%d" , block_fs->version );
    char * data_ext  = util_alloc_sprintf("data_%d" , block_fs->version );

    block_fs->log_file   = util_alloc_filename( block_fs->path , block_fs->base_name , log_ext);
    block_fs->data_file  = util_alloc_filename( block_fs->path , block_fs->base_name , data_ext);
    block_fs->index_file = util_alloc_filename( block_fs->path , block_fs->base_name , index_ext);
    
    free( log_ext );
    free( index_ext );
    free( data_ext );
  }
}


static block_fs_type * block_fs_alloc_empty( const char * mount_file , int block_size) {
  block_fs_type * block_fs   = util_malloc( sizeof * block_fs , __func__);
  block_fs->mount_file       = util_alloc_string_copy( mount_file );
  block_fs->index            = hash_alloc();
  block_fs->file_nodes       = vector_alloc_new();
  block_fs->free_nodes       = NULL;
  block_fs->num_free_nodes   = 0;
  block_fs->write_count      = 0;
  block_fs->data_data_size   = 0;
  block_fs->free_size        = 0; 
  block_fs->block_size       = block_size;
  block_fs->log_transactions = false;

  pthread_mutex_init( &block_fs->io_lock  , NULL);
  pthread_rwlock_init( &block_fs->rw_lock , NULL);
  {
    FILE * stream     = util_fopen( mount_file , "r");
    int id            = util_fread_int( stream );
    block_fs->version = util_fread_int( stream );
    fclose( stream );

    if (id != MOUNT_MAP_MAGIC_INT) 
      util_abort("%s: The file:%s does not seem to a valid block_fs mount map \n",__func__ , mount_file);
  }
  util_alloc_file_components( mount_file , &block_fs->path , &block_fs->base_name, NULL );
  block_fs->log_file   = NULL;
  block_fs->data_file  = NULL;
  block_fs->index_file = NULL;
  block_fs_set_filenames( block_fs );
  return block_fs;
}



static void block_fs_fwrite_mount_info__( const char * mount_file , int version ) {
  FILE * stream = util_fopen( mount_file , "w");
  util_fwrite_int( MOUNT_MAP_MAGIC_INT , stream );
  util_fwrite_int( version , stream );
  fclose( stream );
}



/**
   This function opens the index file. If the index does not already
   exist an empty index table is written to it.
*/

static void block_fs_open_index( block_fs_type * block_fs) {
  if (util_file_exists( block_fs->index_file )) {
    block_fs->index_stream = util_fopen(block_fs->index_file , "r+");  
    fseek( block_fs->index_stream , 0 , SEEK_SET);
  } else {
    block_fs->index_stream = util_fopen(block_fs->index_file , "w+");  /* This is THE fopen() of the index file - will stay open for the lifetime of the block_fs instance. */
    block_fs_fwrite_index( block_fs );                                 /* Ensure that the existing (empty) index is valid. */
    fseek( block_fs->index_stream , 0 , SEEK_SET);
  }
}



static void block_fs_open_data( block_fs_type * block_fs ) {
  if (util_file_exists( block_fs->data_file ))
    block_fs->data_stream = util_fopen( block_fs->data_file , "r+");
  else
    block_fs->data_stream = util_fopen( block_fs->data_file , "w+");
}


static void block_fs_open_log( block_fs_type * block_fs ) {
  if (util_file_exists( block_fs->log_file )) {
    block_fs->log_stream = util_fopen( block_fs->log_file , "r+");
    fseek( block_fs->log_stream , 0  , SEEK_END);
  } else
    block_fs->log_stream = util_fopen( block_fs->log_file , "w+");
}



block_fs_type * block_fs_mount( const char * mount_file , int block_size) {
  printf("Trying to mount:%s \n",mount_file);
  if (!util_file_exists(mount_file)) 
    /* This is a brand new filesystem - create the mount map first. */
    block_fs_fwrite_mount_info__( mount_file , 0 );
  {
    block_fs_type * block_fs = block_fs_alloc_empty( mount_file , block_size );
    block_fs_open_index( block_fs );
    block_fs->data_data_size = util_fread_long( block_fs->index_stream );
    {
      int index_size = util_fread_int( block_fs->index_stream );
      for (int i = 0; i < index_size; i++) {
        char * filename            = util_fread_alloc_string( block_fs->index_stream );
        file_node_type * file_node = file_node_index_fread_alloc( block_fs->index_stream );
        
        block_fs_install_node( block_fs , file_node );
        block_fs_insert_index_node(block_fs , filename , file_node);
        free( filename );
      }
    }

    /* Loading information about free nodes - i.e. holes in the datafile. */
    {
      int free_size = util_fread_int( block_fs->index_stream );
      for (int i = 0; i < free_size; i++) {
        file_node_type * file_node = file_node_index_fread_alloc( block_fs->index_stream );
        
        block_fs_install_node( block_fs , file_node );
        block_fs_insert_free_node( block_fs , file_node );
      }
    }
    {
      bool unclean_umount = util_file_exists( block_fs->log_file );
      
      if (unclean_umount) 
        block_fs_apply_log( block_fs );
      
      block_fs->log_transactions = true;
      block_fs_open_log( block_fs );
      
      if (unclean_umount) 
        block_fs_fwrite_index( block_fs );
    }
    
    block_fs_open_data( block_fs );
    return block_fs;
  }
}


static void block_fs_unlink_free_node( block_fs_type * block_fs , file_node_type * node) {
  file_node_type * prev = node->prev;
  file_node_type * next = node->next;
  
  if (prev == NULL)
      /* Special case: popping off the head of the list. */
    block_fs->free_nodes = next;
  else
    prev->next = next;
  
  if (next != NULL)
    next->prev = prev;

  block_fs->num_free_nodes--;
  block_fs->free_size -= node->node_size;
}


/**
   This function first checks the free nodes if any of them can be
   used, otherwise a new node is created.
*/

static file_node_type * block_fs_get_new_node( block_fs_type * block_fs , const char * filename , size_t min_size) {
  
  file_node_type * current = block_fs->free_nodes;
  file_node_type * prev    = NULL;
  printf("1:*****************************************************************\n");
  printf("Ser etter en node med size >= %ld \n",min_size);
  block_fs_fprintf_free_nodes( block_fs);
  
  while (current != NULL && (current->node_size < min_size)) {
    prev = current;
    current = current->next;
  }
  if (current != NULL) {
    /* 
       Current points to a file_node which can be used. Before we return current we must:
       
       1. Remove current from the free_nodes list.
       2. Add current to the index hash.
       
    */
    block_fs_unlink_free_node( block_fs , current );
    block_fs_log_reuse_node( block_fs , filename , current);
    printf("Leverer en resirkulert node Offset:%ld \n", current->node_offset);
    
    current->next = NULL;
    current->prev = NULL;
    block_fs_fprintf_free_nodes( block_fs);
    printf("2:*****************************************************************\n") ; 
    return current;
  } else {
    /* No usable nodes in the free nodes list - must allocate a brand new one. */

    long int offset;
    int node_size;
    file_node_type * new_node;
    
    {
      div_t d   = div( min_size , block_fs->block_size );
      node_size = d.quot * block_fs->block_size;
      if (d.rem)
        node_size += block_fs->block_size;
    }
    /* Must lock the total size here ... */
    offset = block_fs->data_data_size;
    new_node = file_node_alloc(offset , node_size);
    block_fs_install_node( block_fs , new_node );  /* <- This will update the total file size. */
    
    printf("New:%s Offset:%ld  Node_size:%d   File4_size:%d \n",filename , new_node->node_offset , new_node->node_size , new_node->data_size);
    printf("2:*****************************************************************\n") ; 
    return new_node;
  }
}


bool block_fs_has_file( const block_fs_type * block_fs , const char * filename) {
  return hash_has_key( block_fs->index , filename );
}




static void block_fs_unlink_file__( block_fs_type * block_fs , const char * filename ) {
  file_node_type * node = hash_pop( block_fs->index , filename );
  block_fs_log_delete_node( block_fs , filename );
  printf("Skal slette node med offset: %ld \n",node->node_offset);
  block_fs_insert_free_node( block_fs , node );
}


void block_fs_unlink_file( block_fs_type * block_fs , const char * filename) {
  block_fs_aquire_wlock( block_fs );
  block_fs_unlink_file__( block_fs , filename );
  block_fs_release_rwlock( block_fs );
}

/**
   The single lowest-level write function:
   
     2. fsync() the datafile.
     3. seek to correct position.
     4. Write the data with util_fwrite()
     5. fsync() again.

     7. increase the write_count
     8. set the data_size field of the node.

   Observe that when 'designing' this file-system the priority has
   been on read-spead, one consequence of this is that all write
   operations are sandwiched between two fsync() calls; that
   guarantees that the read access (which should be the fast path) can
   be without any calls to fsync().

   Not necessary to lock - since all writes are protected by the
   'global' rwlock anyway.
*/


static void block_fs_fwrite__(block_fs_type * block_fs , file_node_type * node , const void * ptr , int data_size) {
  fsync( fileno(block_fs->data_stream) );
  fseek( block_fs->data_stream , node->node_offset , SEEK_SET );
  util_fwrite( ptr , 1 , data_size , block_fs->data_stream , __func__);
  fsync( fileno(block_fs->data_stream) );
  
  block_fs->write_count++;
  node->data_size = data_size; 
  
  fseek( block_fs->data_stream , block_fs->data_data_size , SEEK_SET );
  ftell( block_fs->data_stream );
}



void block_fs_fwrite_file(block_fs_type * block_fs , const char * filename , const void * ptr , size_t data_size) {
  printf("Want to write:%s \n",filename);
  block_fs_aquire_wlock( block_fs );
  {
    file_node_type * file_node;
    
    if (block_fs_has_file( block_fs , filename )) {
      file_node = hash_get( block_fs->index , filename );
      if (file_node->node_size < data_size) {
        /* The current node is too small for the new content:
           
          1. Remove the existing node, from the index and insert it into the free_nodes list.
          2. Get a new node.
        
        */
        block_fs_unlink_file__( block_fs , filename );
        file_node = block_fs_get_new_node( block_fs , filename , data_size );
      }
    } else 
      file_node = block_fs_get_new_node( block_fs , filename , data_size );
    
    
    /* The actual writing ... */
    block_fs_fwrite__( block_fs , file_node , ptr , data_size);
    block_fs_insert_index_node(block_fs , filename , file_node);

  }
  block_fs_release_rwlock( block_fs );
}



void block_fs_fwrite_buffer(block_fs_type * block_fs , const char * filename , const buffer_type * buffer) {
  block_fs_fwrite_file( block_fs , filename , buffer_get_data( buffer ) , buffer_get_size( buffer ));
}


/**
   Need extra locking here - because the global rwlock allows many concurrent readers.
*/
static void block_fs_fread__(block_fs_type * block_fs , long int offset , void * ptr , size_t read_bytes) {
  pthread_mutex_lock( &block_fs->io_lock );
  printf("Seek:%ld \n",offset);
  fseek( block_fs->data_stream , offset , SEEK_SET);
  util_fread( ptr , 1 , read_bytes , block_fs->data_stream , __func__);
  pthread_mutex_unlock( &block_fs->io_lock );
}


/**
   Reads the full content of 'filename' into the buffer. 
*/

void block_fs_fread_realloc_buffer( block_fs_type * block_fs , const char * filename , buffer_type * buffer) {
  block_fs_aquire_rlock( block_fs );
  {
    file_node_type * node = hash_get( block_fs->index , filename);
    int data_size         = node->data_size;
    
    buffer_clear( buffer );   /* Setting: content_size = 0; pos = 0;  */
    {
      /* 
         Going low-level - essentially a second implementation of
         block_fs_fread__():
      */
      pthread_mutex_lock( &block_fs->io_lock );
      fseek( block_fs->data_stream , node->node_offset , SEEK_SET);
      buffer_stream_fread( buffer , data_size , block_fs->data_stream );
      pthread_mutex_unlock( &block_fs->io_lock );
      
    }
    buffer_rewind( buffer );  /* Setting: pos = 0; */
  }
  block_fs_release_rwlock( block_fs );
}





/**
   This function will:

    1. seek to beginning of filename (util_abort() if filename does not exist).
    2. seek forward offset into filename (NB negative offset -> util_abort()).
    3. read read_bytes bytes into the ptr.
*/

void block_fs_fread(block_fs_type * block_fs , const char * filename , long int offset , void * ptr , size_t read_bytes) {
  block_fs_aquire_rlock( block_fs );
  {
    file_node_type * node = hash_get( block_fs->index , filename);
    if (offset < 0 || (read_bytes + offset) > node->data_size)
      util_abort("%s: invalid offset:%ld \n",__func__ , offset);
    
    block_fs_fread__( block_fs , node->node_offset + offset , ptr , read_bytes);
  }
  block_fs_release_rwlock( block_fs );
}


/*
  This function will read all the data stored in 'filename' - it is
  the responsability of the calling scope that ptr is sufficiently
  large to hold it. You can use block_fs_get_filesize() first to
  check.
*/


void block_fs_fread_file( block_fs_type * block_fs , const char * filename , void * ptr) {
  block_fs_aquire_rlock( block_fs );
  {
    file_node_type * node = hash_get( block_fs->index , filename);
    block_fs_fread__( block_fs , node->node_offset , ptr , node->data_size);
  }
  block_fs_release_rwlock( block_fs );
}


int block_fs_get_filesize( const block_fs_type * block_fs , const char * filename) {
  file_node_type * node = hash_get( block_fs->index , filename);
  return node->data_size;
}




/**
   If one of the files data_file or index_file already exists the
   function will fail hard.
*/


void block_fs_sync( block_fs_type * block_fs ) {
  block_fs_fwrite_index( block_fs );
}



/**
   Close/synchronize the open file descriptors and free all memory
   related to the block_fs instance.
*/

void block_fs_close( block_fs_type * block_fs ) {
  block_fs_sync( block_fs );
  fclose( block_fs->index_stream );
  fclose( block_fs->data_stream );
  fclose( block_fs->log_stream );
  
  unlink( block_fs->log_file );

  free( block_fs->base_name );
  free( block_fs->log_file );
  free( block_fs->index_file );
  free( block_fs->data_file );
  free( block_fs->path );
  free( block_fs->mount_file );
  
  hash_free( block_fs->index );
  vector_free( block_fs->file_nodes );
  printf("Closing with fragmentation: %g \n",block_fs->free_size * 1.0 / block_fs->data_data_size);
  free( block_fs );
}



/*****************************************************************/
/* Functions related to pretty-printing an image of the data file. */
/*****************************************************************/

static sort_node_type * sort_node_alloc(const char * name , long int offset , int node_size , int data_size) {
  sort_node_type * node = util_malloc( sizeof * node , __func__);
  node->name            = util_alloc_string_copy( name );
  node->node_offset          = offset;
  node->node_size       = node_size;
  node->data_size       = data_size;
  return node;
}

static void sort_node_free( sort_node_type * node ) {
  free( node->name );
  free( node );
}

static void sort_node_free__( void * node) {
  sort_node_free( (sort_node_type *) node );
}

static int sort_node_cmp( const void * arg1 , const void * arg2 ) {
  const sort_node_type * node1 = (sort_node_type *) arg1;
  const sort_node_type * node2 = (sort_node_type *) arg2;

  if (node1->node_offset > node2->node_offset)
    return 1;
  else
    return -1;
}

static void sort_node_fprintf(const sort_node_type * node, FILE * stream) {
  fprintf(stream , "%-20s  %10ld  %8d  %8d \n",node->name , node->node_offset , node->node_size , node->data_size);
}



void block_fs_fprintf( const block_fs_type * block_fs , FILE * stream) {
  vector_type    * sort_vector = vector_alloc_new();
  
  /* Inserting the nodes from the index. */
  {
    hash_iter_type * iter        = hash_iter_alloc( block_fs->index );
    while ( !hash_iter_is_complete( iter )) {
      const char * key            = hash_iter_get_next_key( iter );
      const file_node_type * node = hash_get( block_fs->index , key );
      sort_node_type * sort_node  = sort_node_alloc( key , node->node_offset , node->node_size , node->data_size);
      vector_append_owned_ref( sort_vector , sort_node , sort_node_free__ );
    }
    hash_iter_free( iter );
  }
  
  /* Inserting the free nodes - the holes. */
  {
    file_node_type * current = block_fs->free_nodes;
    while (current != NULL) {
      sort_node_type * sort_node  = sort_node_alloc( "--FREE--", current->node_offset , current->node_size , 0);
      vector_append_owned_ref( sort_vector , sort_node , sort_node_free__ );
      current = current->next;
    }
  }
  vector_sort(sort_vector , sort_node_cmp);
  fprintf(stream , "=======================================================\n");
  fprintf(stream , "%-20s  %10s   %8s  %8s\n","Filename" , "Offset", "Nodesize","Filesize");
  fprintf(stream , "-------------------------------------------------------\n");
  {
    int i;
    for (i=0; i < vector_get_size( sort_vector ); i++) 
      sort_node_fprintf( vector_iget_const( sort_vector , i) , stream);
  }
  fprintf(stream , "-------------------------------------------------------\n");
  
  vector_free( sort_vector );
}


/*****************************************************************/
/* Operations related to the log_file                            */
/*****************************************************************/

static void block_fs_fsync_log( block_fs_type * block_fs ) {
  fsync( fileno( block_fs->log_stream) );
  ftell( block_fs->log_stream );
}

static void block_fs_log_alloc_node( block_fs_type * block_fs ,  const char * filename , const file_node_type * file_node) {
  if ( block_fs->log_transactions ) {
    printf("Logging:%s \n",filename);
    util_fwrite_int( ALLOC_NODE_ACTION , block_fs->log_stream );
    util_fwrite_string( filename , block_fs->log_stream );
    file_node_index_fwrite( file_node , block_fs->log_stream );
    
    block_fs_fsync_log( block_fs );
  }
}


static void block_fs_log_reuse_node( block_fs_type * block_fs ,  const char * filename , const file_node_type * file_node) {
  if ( block_fs->log_transactions ) {
    util_fwrite_int( REUSE_NODE_ACTION , block_fs->log_stream );
    util_fwrite_string( filename , block_fs->log_stream );
    util_fwrite_long( file_node->node_offset , block_fs->log_stream );
    util_fwrite_int( file_node->data_size , block_fs->log_stream );
    
    block_fs_fsync_log( block_fs );
  }
}


static void block_fs_log_delete_node( block_fs_type * block_fs ,  const char * filename ) {
  if ( block_fs->log_transactions ) {
    util_fwrite_int( DELETE_NODE_ACTION , block_fs->log_stream );
    util_fwrite_string( filename , block_fs->log_stream );

    block_fs_fsync_log( block_fs );
  }
}




static void block_fs_fprintf_log_stream( FILE * stream ) {
  file_node_type * node = file_node_alloc(0,0);
  bool            atEOF = false;
  char * filename       = NULL;
  
  while (!atEOF) {
    block_fs_action_type action;
    long int pos = ftell( stream );
    long int offset;
    if ( fread( &action , sizeof action , 1 , stream) == 1) {
      switch (action) {
      case(ALLOC_NODE_ACTION):
        filename = util_fread_realloc_string( filename , stream );
        file_node_index_fread( node , stream);
        printf("%ld: Added new node:%s \n",pos , filename);
        break;
      case(DELETE_NODE_ACTION):
        filename = util_fread_realloc_string( filename , stream );
        printf("%ld: Deleted node:%s \n",pos , filename);
        break;
      case(REUSE_NODE_ACTION):
        filename = util_fread_realloc_string( filename , stream );
        offset = util_fread_long( stream );
        printf("Reusing the offset at:%ld for node:%s \n",offset , filename);
        break;
      default:
        util_abort("%s: action:%d not recognzied \n",__func__,action);
      }
    } else
      atEOF = true;
  }
  util_safe_free(filename);
  file_node_free( node );
}


static void block_fs_clear_log( block_fs_type * block_fs ) {
  fseek( block_fs->log_stream , 0 , SEEK_SET );
  ftruncate( fileno( block_fs->log_stream ) , 0 );
}


static void block_fs_apply_log( block_fs_type * block_fs ) {
  FILE * stream   = util_fopen( block_fs->log_file , "r");
  bool  atEOF     = false;
  char * filename = NULL;
  
  while (!atEOF) {
    block_fs_action_type action;
    file_node_type * file_node;
    
    if (fread( &action , sizeof action , 1 , stream) == 1) {

      switch (action) {
      case(REUSE_NODE_ACTION):
        filename = util_fread_realloc_string( filename , stream );
        {
          long int node_offset = util_fread_long( stream );
          int      data_size   = util_fread_int( stream ); 
          /* 
             OK - now we must find this node in the free_nodes list, i.e. it has been by a previous step in
             the log playback. In that case we take it from the free_nodes list, and just discard the file_node
             instance we have read/allocated from file, and use the instance from the free list.
          */
          file_node_type * current = block_fs->free_nodes;
          printf("ACTION: REUSE Offset:%ld \n ",node_offset);
          printf("Leter etter offset:%ld \n",node_offset);
          while (current != NULL && (current->node_offset != node_offset)) {
            printf("Sammenligner: %ld %ld \n",current->node_offset , node_offset);
            current = current->next;
          }

          if (current == NULL) 
            util_abort("%s: failed to play back transaction log - you are fucked ... \n",__func__);
          else {
            printf("OK - fant den \n");
            block_fs_unlink_free_node( block_fs , current );
            
            ///*
            //  OK - we found the very node we were looking for in the free nodes list.
            //*/
            //if (prev == NULL) 
            //  /* We are popping off the list head. */
            //  block_fs->free_nodes = current->next;
            //else
            //  prev->next = current->next;
            //
            //if (current->next != NULL) 
            //  current->next->prev = prev;
            //
            //block_fs->num_free_nodes--;
            //block_fs->free_size -= current->node_size;
            
            current->data_size = data_size;
            block_fs_insert_index_node(block_fs , filename , current);
          }
        }
        break;
      case(ALLOC_NODE_ACTION):
        filename = util_fread_realloc_string( filename , stream );
        file_node = file_node_index_fread_alloc(  stream );
        printf("Action ALLOC:%s   %ld\n",filename , file_node->node_offset);
        block_fs_install_node( block_fs , file_node );
        block_fs_insert_index_node(block_fs , filename , file_node);
        break;
      case(DELETE_NODE_ACTION):
        filename = util_fread_realloc_string( filename , stream );
        printf("Action delete:%s \n",filename);
        block_fs_unlink_file__( block_fs , filename );
        break;
      default:
        util_abort("%s: action:%d not recognzied \n",__func__,action);
      }
    } else
      atEOF = true;
  }
  util_safe_free(filename);
  fclose(stream);
}



void block_fs_fprintf_log( block_fs_type * block_fs ) {
  long int init_pos = ftell( block_fs->log_stream );
  fseek( block_fs->log_stream , 0 , SEEK_SET );
  block_fs_fprintf_log_stream( block_fs->log_stream );
  fseek( block_fs->log_stream , init_pos , SEEK_SET );
}



void block_fs_fprintf_logfile( const char * filename) {
  FILE * stream = util_fopen( filename , "r");
  block_fs_fprintf_log_stream( stream );
  fclose( stream );
}