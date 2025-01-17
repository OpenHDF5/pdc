# PDC Documentations
  + [PDC user APIs](#pdc-user-apis)
    - [PDC general APIs](#pdc-general-apis)
    - [PDC container APIs](#pdc-container-apis)
    - [PDC object APIs](#pdc-object-apis)
    - [PDC region APIs](#pdc-region-apis)
    - [PDC property APIs](#pdc-property-apis)
    - [PDC query APIs](#pdc-query-apis)
  + [PDC data types](#PDC-type-categories)
    - [Basic types](#basic-types)
    - [Histogram structure](#histogram-structure)
    - [Container info](#container-info)
    - [Container life time](#container-life-time)
    - [Object property](#object-property)
    - [Object info](#object-info)
    - [Object structure](#object-structure)
    - [Region info](#region-info)
    - [Access type](#access-type)
    - [Transfer request status](#transfer-request-status)
    - [Query operators](#query-operators)
    - [Query structures](#query-structures)
    - [Selection structure](#selection-structure)
  + [Developers notes](#developer-notes)
# PDC user APIs
  ## PDC general APIs
  + pdcid_t PDCinit(const char *pdc_name)
    - Input: 
      + pdc_name is the reference for PDC class. Recommended use "pdc"
    - Output: 
      + PDC class ID used for future reference.
    - All PDC client applications must call PDCinit before using it. This function will setup connections from clients to servers. A valid PDC server must be running.
    - For developers: currently implemented in pdc.c.
  + perr_t PDCclose(pdcid_t pdcid)
    - Input: 
      + PDC class ID returned from PDCinit.
    - Ouput: 
      + SUCCEED if no error, otherwise FAIL.
    - This is a proper way to end a client-server connection for PDC. A PDCinit must correspond to one PDCclose.
    - For developers: currently implemented in pdc.c.
  + perr_t PDC_Client_close_all_server()
    - Ouput: 
      + SUCCEED if no error, otherwise FAIL.
    - Close all PDC servers that running.
    - For developers: see PDC_client_connect.c
  ## PDC container APIs
  + pdcid_t PDCcont_create(const char *cont_name, pdcid_t cont_prop_id)
    - Input: 
      + cont_name: the name of container. e.g "c1", "c2"
      + cont_prop_id: property ID for inheriting a PDC property for container.
    - Output: pdc_id for future referencing of this container, returned from PDC servers.
    - Create a PDC container for future use. 
    - For developers: currently implemented in pdc_cont.c. This function will send a name to server and receive an container id. This function will allocate necessary memories and initialize properties for a container.
  + pdcid_t PDCcont_create_col(const char *cont_name, pdcid_t cont_prop_id)
    - Input: 
      + cont_name: the name to be assigned to a container. e.g "c1", "c2"
      + cont_prop_id: property ID for inheriting a PDC property for container.
    - Output: pdc_id for future referencing.
    - Exactly the same as PDCcont_create, except all processes must call this function collectively. Create a PDC container for future use collectively.
    - For developers: currently implemented in pdc_cont.c.
  + pdcid_t PDCcont_open(const char *cont_name, pdcid_t pdc)
    - Input: 
      + cont_name: the name of container used for PDCcont_create.
      + pdc: PDC class ID returned from PDCinit.
    - Output:
      + error code. FAIL OR SUCCEED
    - Open a container. Must make sure a container named cont_name is properly created (registered by PDCcont_create at remote servers).
    - For developers: currently implemented in pdc_cont.c. This function will make sure the metadata for a container is returned from servers. For collective operations, rank 0 is going to broadcast this metadata ID to the rest of processes. A struct _pdc_cont_info is created locally for future reference.
  + perr_t PDCcont_close(pdcid_t id)
    - Input: 
      + container ID, returned from PDCcont_create.
    - Output: 
      + error code, SUCCEED or FAIL.
    - Correspond to PDCcont_open. Must be called only once when a container is no longer used in the future.
    - For developers: currently implemented in pdc_cont.c. The reference counter of a container is decremented. When the counter reaches zero, the memory of the container can be freed later.
  + struct pdc_cont_info *PDCcont_get_info(const char *cont_name)
     - Input: 
       + name of the container
     - Output: 
       + Pointer to a new structure that contains the container information [See container info](#container-info)
     - Get container information
     - For developers: See pdc_cont.c. Use name to search for pdc_id first by linked list lookup. Make a copy of the metadata to the newly malloced structure.
  + perr_t PDCcont_persist(pdcid_t cont_id)
    - Input:
      + cont_id: container ID, returned from PDCcont_create.
    - Output: 
      + error code, SUCCEED or FAIL.
    - Make a PDC container persist.
    - For developers, see pdc_cont.c. Set the container life field PDC_PERSIST.
  + perr_t PDCprop_set_cont_lifetime(pdcid_t cont_prop, pdc_lifetime_t cont_lifetime)
    - Input:
      + cont_prop: Container property pdc_id
      + cont_lifetime: See [container life time](#container-life-time)
    - Output: 
      + error code, SUCCEED or FAIL.
    - Set container life time for a property.
    - For developers, see pdc_cont.c.
  + pdcid_t PDCcont_get_id(const char *cont_name, pdcid_t pdc_id)
    - Input:
      + cont_name: Name of the container
      + pdc_id: PDC class ID, returned by PDCinit
    - Output: 
      + container ID
    - Get container ID by name. This function is similar to open.
    - For developers, see pdc_client_connect.c. It will query the servers for container information and create a container structure locally.
  + perr_t PDCcont_del(pdcid_t cont_id)
    - Input:
      + cont_id: container ID, returned from PDCcont_create.
    - Output: 
      + error code, SUCCEED or FAIL.
    - Deleta a container
    - For developers: see pdc_client_connect.c. Need to send RPCs to servers for metadata update.
  + perr_t PDCcont_put_tag(pdcid_t cont_id, char *tag_name, void *tag_value, psize_t value_size)
    - Input:
      + cont_id: Container ID, returned from PDCcont_create.
      + tag_name: Name of the tag
      + tag_value: Value to be written under the tag
      + value_size: Number of bytes for the tag_value (tag_size may be more informative)
    - Output: 
      + error code, SUCCEED or FAIL.
    - Record a tag_value under the name tag_name for the container referenced by cont_id.
    - For developers: see pdc_client_connect.c. Need to send RPCs to servers for metadata update.
  + perr_t PDCcont_get_tag(pdcid_t cont_id, char *tag_name, void **tag_value, psize_t *value_size)
    - Input:
      + cont_id: Container ID, returned from PDCcont_create.
      + tag_name: Name of the tag
      + value_size: Number of bytes for the tag_value (tag_size may be more informative)
    - Output:
      + tag_value: Pointer to the value to be read under the tag
      + error code, SUCCEED or FAIL.
    - Retrieve a tag value to the memory space pointed by the tag_value under the name tag_name for the container referenced by cont_id.
    - For developers: see pdc_client_connect.c. Need to send RPCs to servers for metadata retrival.
  + perr_t PDCcont_del_tag(pdcid_t cont_id, char *tag_name)
    - Input:
      + cont_id: Container ID, returned from PDCcont_create.
      + tag_name: Name of the tag
    - Output: 
      + error code, SUCCEED or FAIL.
    - Delete a tag for a container by name
    - For developers: see pdc_client_connect.c. Need to send RPCs to servers for metadata update.
  + perr_t PDCcont_put_objids(pdcid_t cont_id, int nobj, pdcid_t *obj_ids)
    - Input:
      + cont_id: Container ID, returned from PDCcont_create.
      + nobj: Number of objects to be written
      + obj_ids: Pointers to the object IDs
    - Output: 
      + error code, SUCCEED or FAIL.
    - Put an array of objects to a container.
    - For developers: see pdc_client_connect.c. Need to send RPCs to servers for metadata update.
  + perr_t PDCcont_get_objids(pdcid_t cont_id ATTRIBUTE(unused), int *nobj ATTRIBUTE(unused), pdcid_t **obj_ids ATTRIBUTE(unused) )
     TODO:
  + perr_t PDCcont_del_objids(pdcid_t cont_id, int nobj, pdcid_t *obj_ids)
    - Input:
      + cont_id: Container ID, returned from PDCcont_create.
      + nobj: Number of objects to be deleted
      + obj_ids: Pointers to the object IDs
    - Output: 
      + error code, SUCCEED or FAIL.
    - Delete an array of objects to a container.
    - For developers: see pdc_client_connect.c. Need to send RPCs to servers for metadata update.
  ## PDC object APIs
  + pdcid_t PDCobj_create(pdcid_t cont_id, const char *obj_name, pdcid_t obj_prop_id)
    - Input:
      + cont_id: Container ID, returned from PDCcont_create.
      + obj_name: Name of objects to be created
      + obj_prop_id: Property ID to be inherited from.
    - Output: 
      + Local object ID
    - Create a PDC object.
    - For developers: see pdc_obj.c. This process need to send the name of the object to be created to the servers. Then it will receive an object ID. The object structure will inherit attributes from its container and  input object properties.
  + PDCobj_create_mpi(pdcid_t cont_id, const char *obj_name, pdcid_t obj_prop_id, int rank_id, MPI_Comm comm)
    - Input:
      + cont_id: Container ID, returned from PDCcont_create.
      + obj_name: Name of objects to be created
      + rank_id: Which rank ID the object is placed to
      + comm: MPI communicator for the rank_id
    - Output: 
      + Local object ID
    - Create a PDC object at the rank_id in the communicator comm. This function is a colllective operation.
    - For developers: see pdc_mpi.c. If rank_id equals local process rank, then a local object is created. Otherwise we create a global object. The object metadata ID is broadcasted to all processes if a global object is created using MPI_Bcast.
  + pdcid_t PDCobj_open(const char *obj_name, pdcid_t pdc)
    - Input:
      + obj_name: Name of objects to be created
      + pdc: PDC class ID, returned from PDCInit
    - Output: 
      + Local object ID
    - Open a PDC ID created previously by name.
    - For developers: see pdc_obj.c. Need to communicate with servers for metadata of the object.
  + perr_t PDCobj_close(pdcid_t obj_id)
    - Input:
      + obj_id: Local object ID to be closed.
    - Output:
      + error code, SUCCEED or FAIL.
    - Close an object. Must do this after open an object.
    - For developers: see pdc_obj.c. Dereference an object by reducing its reference counter.
  + struct pdc_obj_info *PDCobj_get_info(pdcid_t obj)
    - Input:
      + obj_name: Local object ID
    - Output:
      + object information see [object information](#object-info)
    - Get a pointer to a structure that describes the object metadata.
    - For developers: see pdc_obj.c. Pull out local object metadata by ID.
  + pdcid_t PDCobj_put_data(const char *obj_name, void *data, uint64_t size, pdcid_t cont_id)
    - Input:
      + obj_name: Name of object
      + data: Pointer to data memory
      + size: Size of data
      + cont_id: Container ID of this object
    - Output:
      + Local object ID created locally with the input name
    - Write data to an object.
    - For developers: see pdc_client_connect.c. Nedd to send RPCs to servers for this request. (TODO: change return value to perr_t)
  + perr_t PDCobj_get_data(pdcid_t obj_id, void *data, uint64_t size)
    - Input:
      + obj_id: Local object ID
      + size: Size of data
    - Output:
      + data: Pointer to data to be filled
      + error code, SUCCEED or FAIL.
    - Read data from an object.
    - For developers: see pdc_client_connect.c. Use PDC_obj_get_info to retrieve name. Then forward name to servers to fulfill requests.
  + perr_t PDCobj_del_data(pdcid_t obj_id)
    - Input:
      + obj_id: Local object ID
    - Output:
      + error code, SUCCEED or FAIL.
    - Delete data from an object.
    - For developers: see pdc_client_connect.c. Use PDC_obj_get_info to retrieve name. Then forward name to servers to fulfill requests.
  + perr_t PDCobj_put_tag(pdcid_t obj_id, char *tag_name, void *tag_value, psize_t value_size)
    - Input:
      + obj_id: Local object ID
      + tag_name: Name of the tag to be entered
      + tag_value: Value of the tag
      + value_size: Number of bytes for the tag_value
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the tag value for a tag
    - For developers: see pdc_client_connect.c. Need to use PDC_add_kvtag to submit RPCs to the servers for metadata update.
  + perr_t PDCobj_get_tag(pdcid_t obj_id, char *tag_name, void **tag_value, psize_t *value_size)
    - Input:
      + obj_id: Local object ID
      + tag_name: Name of the tag to be entered
    - Output:
      + tag_value: Value of the tag
      + value_size: Number of bytes for the tag_value
      + error code, SUCCEED or FAIL.
    - Get the tag value for a tag
    - For developers: see pdc_client_connect.c. Need to use PDC_get_kvtag to submit RPCs to the servers for metadata update.
  + perr_t PDCobj_del_tag(pdcid_t obj_id, char *tag_name)
    - Input:
      + obj_id: Local object ID
      + tag_name: Name of the tag to be entered
    - Output:
      + error code, SUCCEED or FAIL.
    - Delete a tag.
    - For developers: see pdc_client_connect.c. Need to use PDCtag_delete to submit RPCs to the servers for metadata update.
  ## PDC region APIs
  + pdcid_t PDCregion_create(psize_t ndims, uint64_t *offset, uint64_t *size)
    - Input:
      + ndims: Number of dimensions
      + offset: Array of offsets
      + size: Array of offset length
    - Output:
      + Region ID
    - Create a region with ndims offset/length pairs
    - For developers: see pdc_region.c. Need to use PDC_get_kvtag to submit RPCs to the servers for metadata update.
  + perr_t PDCregion_close(pdcid_t region_id)
    - Input:
      + region_id: PDC ID returned from PDCregion_create
    - Output:
      + None
    - Close a PDC region
    - For developers: see pdc_region.c. Free offset and size arrays.
  + perr_t PDCbuf_obj_map(void *buf, pdc_var_type_t local_type, pdcid_t local_reg, pdcid_t remote_obj, pdcid_t remote_reg)
    - Input:
      + buf: Memory buffer
      + local_type: one of PDC basic types, see [PDC basic types](#basic-types)
      + local_reg: Local region ID
      + remote_obj: Remote object ID
      + remote_reg: Remote region ID
    - Output:
      + Region ID
    - Create a region with ndims offset/length pairs. At this stage of PDC development, the buffer has to be filled if you are performing PDC_WRITE with lock and release functions.
    - For developers: see pdc_region.c. Need to use PDC_get_kvtag to submit RPCs to the servers for metadata update.
  + perr_t PDCbuf_obj_unmap(pdcid_t remote_obj_id, pdcid_t remote_reg_id)
    - Input:
      + remote_obj_id: remote object ID
      + remote_reg_id: remote region ID
    - Output:
      + error code, SUCCEED or FAIL.
    - Unmap a region to the user buffer. PDCbuf_obj_map must be called previously.
    - For developers: see pdc_region.c.
  + perr_t PDCreg_obtain_lock(pdcid_t obj_id, pdcid_t reg_id, pdc_access_t access_type, pdc_lock_mode_t lock_mode)
    - Input:
      + obj_id: local object ID
      + reg_id: remote region ID
      + access_type: [PDC access type](#access-type)
      + lock_mode:  PDC_BLOCK or PDC_NOBLOCK
    - Output:
      + error code, SUCCEED or FAIL.
    - Obtain the lock to access a region in an object.
    - For developers: see pdc_region.c.
  + perr_t PDCreg_release_lock(pdcid_t obj_id, pdcid_t reg_id, pdc_access_t access_type)
    - Input:
      + obj_id: local object ID
      + reg_id: remote region ID
      + access_type: [PDC access type](#access-type)
    - Output:
      + error code, SUCCESS or FAIL.
    - Release the lock to access a region in an object. PDC_READ data is available after this lock release.
    - For developers: see pdc_region.c.
  + pdcid_t PDCregion_transfer_create(void *buf, pdc_access_t access_type, pdcid_t obj_id, pdcid_t local_reg, pdcid_t remote_reg)
    - Input:
      + buf: The data buffer to be transferred.
      + access_type: Data type to be transferred.
      + obj_id: Local object id the region attached to.
      + local_reg: Region ID describing the shape of buf.
      + remote_reg: Region ID describing the shape of file domain stored at server.
    - Output:
      + Region ransfer request ID generated.
    - Wrap necessary componenets for a region transfer request into a PDC ID to be referred later.
    - For developers: see pdc_region.c. This function only contains local memory operations.
  + perr_t PDCregion_transfer_close(pdcid_t transfer_request_id)
    - Input:
      + transfer_request_id: Region transfer request ID referred to
    - Output:
      + SUCCEED or FAIL
    - Clearn up function corresponds to PDCregion_transfer_create. transfer_request_id is no longer valid.
    - For developers: see pdc_region.c. This function only contains local memory operations.
  + perr_t PDCregion_transfer_start(pdcid_t transfer_request_id)
    - Input:
      + transfer_request_id: Region transfer request ID referred to
    - Output:
      + Region ID
    - Start a region transfer from local region to remote region for an object on buf. By the end of this function, neither data transfer nor I/O are guaranteed be finished.
    - For developers: see pdc_region.c. Bulk transfer and RPC are set up. The server side will immediately return upon receiving argument payload, ignoring completion of data transfers.
  + perr_t PDCregion_transfer_wait(pdcid_t transfer_request_id)
    - Input:
      + transfer_request_id: Region transfer request ID referred to
    - Output:
      + Region ID
    - Block until the region transfer process is finished for the input region transfer request. By the end of this function, data buffer passed by the buf argument in function PDCregion_transfer_create can be reused. In addition, data consistency at server side is guaranteed for future region transfer request operations.
    - For developers: see pdc_region.c. One RPC is used. There will be an infinite loop checking for the completion of essential operations at the server side. Once all operations are done, the server will return the RPC to the client.
  + perr_t PDCregion_transfer_status(pdcid_t transfer_request_id, pdc_transfer_status_t *completed);
    - Input:
      + transfer_request_id: Region transfer request ID referred to
    - Output:
      + completed: [Transfer request status](#transfer-request-status)
      + SUCCEED or FAIL
    - Check for the completion of a region transfer request. PDC_TRANSFER_STATUS_COMPLETE is equivalent to the result of PDCregion_transfer_wait. PDC_TRANSFER_STATUS_PENDING refers to the case that the region transfer request is not completed. PDC_TRANSFER_STATUS_NOT_FOUND refers to the case either the request is invalid or the request completion has been checked by either this function or PDCregion_transfer_wait previously.
    - For developers: see pdc_region.c. One RPC is used. The server immediately returns the status of the region transfer request.
## PDC property APIs
  + pdcid_t PDCprop_create(pdc_prop_type_t type, pdcid_t pdcid)
    - Input:
      + type: one of the followings
      ```
      typedef enum {
          PDC_CONT_CREATE = 0,
          PDC_OBJ_CREATE
      } pdc_prop_type_t;
      ```
      - pdcid: PDC class ID, returned by PDCInit.
    - Output:
      + PDC property ID
    - Initialize a property structure.
    - For developers: see pdc_prop.c.
  + perr_t PDCprop_close(pdcid_t id)
    - Input:
      + id: PDC property ID
    - Output:
      + error code, SUCCEED or FAIL.
    - Close a PDC property after openning.
    - For developers: see pdc_prop.c. Decrease reference counter for this property.
  + perr_t PDCprop_set_obj_user_id(pdcid_t obj_prop, uint32_t user_id)
    - Input:
      + obj_prop: PDC property ID (has to be an object)
      + user_id: PDC user ID
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the user ID of an object.
    - For developers: see pdc_obj.c. Update the user_id field under [object property](#object-property). See developer's note for more details about this structure.
  + perr_t PDCprop_set_obj_data_loc(pdcid_t obj_prop, char *loc) 
    - Input:
      + obj_prop: PDC property ID (has to be an object)
      + loc: location
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the location of an object.
    - For developers: see pdc_obj.c. Update the data_loc field under [object property](#object-property). See developer's note for more details about this structure.
  + perr_t PDCprop_set_obj_app_name(pdcid_t obj_prop, char *app_name)
    - Input:
      + obj_prop: PDC property ID (has to be an object)
      + app_name: application name
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the application name of an object.
    - For developers: see pdc_obj.c. Update the app_name field under [object property](#object-property). See developer's note for more details about this structure.
  + perr_t PDCprop_set_obj_time_step(pdcid_t obj_prop, uint32_t time_step)
    - Input:
      + obj_prop: PDC property ID (has to be an object)
      + time_step: time step
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the time step of an object.
    - For developers: see pdc_obj.c. Update the time_step field under [object property](#object-property). See developer's note for more details about this structure.
  + perr_t PDCprop_set_obj_tags(pdcid_t obj_prop, char *tags)
    - Input:
      + obj_prop: PDC property ID (has to be an object)
      + tags: tags
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the tags of an object.
    - For developers: see pdc_obj.c. Update the tags field under [object property](#object-property). See developer's note for more details about this structure. 
  + perr_t PDCprop_set_obj_dims(pdcid_t obj_prop, PDC_int_t ndim, uint64_t *dims)
    - Input:
      + obj_prop: PDC property ID (has to be an object)
      + ndim: number of dimensions
      + dims: array of dimensions
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the dimensions of an object.
    - For developers: see pdc_obj.c. Update the obj_prop_pub->ndim and obj_prop_pub->dims fields under [object property public](#object-property-public). See developer's note for more details about this structure.
  + perr_t PDCprop_set_obj_type(pdcid_t obj_prop, pdc_var_type_t type)
    - Input:
      + obj_prop: PDC property ID (has to be an object)
      + type: one of PDC basic types, see [PDC basic types](#basic-types)
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the type of an object.
    - For developers: see pdc_obj.c. Update the obj_prop_pub->type field under [object property public](#object-property-public). See developer's note for more details about this structure.
  + perr_t PDCprop_set_obj_buf(pdcid_t obj_prop, void *buf)
    - Input:
      + obj_prop: PDC property ID (has to be an object)
      + buf: user memory buffer
    - Output:
      + error code, SUCCEED or FAIL.
    - Set the user memory buffer of an object.
    - For developers: see pdc_obj.c. Update the buf field under [object property public](#object-property-public). See developer's note for more details about this structure.
  + pdcid_t PDCprop_obj_dup(pdcid_t prop_id)
    - Input:
      + prop_id: PDC property ID (has to be an object)
    - Output:
      + a new property ID copied.
    - Duplicate an object property
    - For developers: see pdc_prop.c. Duplicate the property structure. The ID will be registered with the PDC class. Similar to create and set all the fields.
## PDC query APIs
  + pdc_query_t *PDCquery_create(pdcid_t obj_id, pdc_query_op_t op, pdc_var_type_t type, void *value)
    - Input:
      + obj_id: local PDC object ID
      + op: one of the followings, see [PDC query operators](#query-operators)
      + type: one of PDC basic types, see [PDC basic types](#basic-types)
      + value: constraint value.
    - Output:
      + a new query structure, see [PDC query structure](#query-structure)
    - Create a PDC query.
    - For developers, see pdc_query.c. The constraint field of the new query structure is filled with the input arguments. Need to search for the metadata ID using object ID.
  + void PDCquery_free(pdc_query_t *query)
    - Input:
      + query: PDC query from PDCquery_create
    - Free a query structure.
    - For developers, see pdc_client_server_common.c.
  + void PDCquery_free_all(pdc_query_t *root)
    - Input:
      + root: root of queries to be freed
    - Output:
      + error code, SUCCEED or FAIL.
    - Free all queries from a root.
    - For developers, see pdc_client_server_common.c. Recursively free left and right branches.
  + pdc_query_t *PDCquery_and(pdc_query_t *q1, pdc_query_t *q2)
    - Input:
      + q1: First query
      + q2: Second query
    - Ouput:
      + A new query after and operator.
    - Perform the and operator on the two PDC queries.
    - For developers, see pdc_query.c
  + pdc_query_t *PDCquery_or(pdc_query_t *q1, pdc_query_t *q2)
    - Input:    
      + q1: First query
      + q2: Second query
    - Ouput:
      + A new query after or operator.
    - Perform the or operator on the two PDC queries.
    - For developers, see pdc_query.c
  + perr_t PDCquery_sel_region(pdc_query_t *query, struct pdc_region_info *obj_region)
    - Input:    
      + query: Query to select the region
      + obj_region: An object region
    - Ouput:
      + error code, SUCCEED or FAIL.
    - Select a region for a PDC query.
    - For developers, see pdc_query.c. Set the region pointer of the query structure to the obj_region pointer.
  + perr_t PDCquery_get_selection(pdc_query_t *query, pdc_selection_t *sel)
    - Input:    
      + query: Query to get the selection
    - Ouput:
      + sel: PDC selection defined as the following. This selection describes the query shape, see [PDC selection structure](#selection-structure)
      + error code, SUCCEED or FAIL.
    - Get the selection information of a PDC query.
    - For developers, see pdc_query.c and PDC_send_data_query in pdc_client_connect.c. Copy the selection structure received from servers to the sel pointer.
  + perr_t PDCquery_get_nhits(pdc_query_t *query, uint64_t *n)
    - Input:    
      + query: Query to calculate the number of hits
    - Ouput:
      + n: number of hits
      + error code, SUCCEED or FAIL.
    - Get the number of hits for a PDC query
    - For developers, see pdc_query.c and PDC_send_data_query in pdc_client_connect.c. Copy the selection structure received from servers to the sel pointer.
  + perr_t PDCquery_get_data(pdcid_t obj_id, pdc_selection_t *sel, void *obj_data)
    - Input:
      + obj_id: The object for query
      + sel: Selection of the query, query_id is inside it.
    - Output:
      + obj_data: Pointer to the data memory filled with query data.
    - Retrieve data from a PDC query for an object.
    - For developers, see pdc_query.c and PDC_Client_get_sel_data in pdc_client_connect.c.
  + perr_t PDCquery_get_histogram(pdcid_t obj_id)
    - Input:
      + obj_id: The object for query
    - Output:
      + error code, SUCCEED or FAIL.
    - Retrieve histogram from a query for a PDC object.
    - For developers, see pdc_query.c. This is a local operation that does not really do anything.
  + void PDCselection_free(pdc_selection_t *sel)
    - Input:
      + sel: Pointer to the selection to be freed.
    - Output:
      + None
    - Free a selection structure.
    - For developers, see pdc_client_connect.c. Free the coordinates.
  + void PDCquery_print(pdc_query_t *query)
    - Input:
      + query: the query to be printed
    - Output:
      + None
    - Print the details of a PDC query structure.
    - For developers, see pdc_client_server_common.c.
  + void PDCselection_print(pdc_selection_t *sel)
    - Input:
      + sel: the PDC selection to be printed
    - Output:
      + None
    - Print the details of a PDC selection structure.
    - For developers, see pdc_client_server_common.c.
  ## PDC hist APIs
  + pdc_histogram_t *PDC_gen_hist(pdc_var_type_t dtype, uint64_t n, void *data)
    - Input:
      + dtype: One of the PDC basic types see [PDC basic types](#basic-types)
      + n: number of values with the basic types.
      + data: pointer to the data buffer.
    - Output:
      + a new [PDC histogram structure](#histogram-structure)
    - Generate a PDC histogram from data. This can be used to optimize performance.
    - For developers, see pdc_hist_pkg.c
  + pdc_histogram_t *PDC_dup_hist(pdc_histogram_t *hist)
    - Input:
      + hist: [PDC histogram structure](#histogram-structure)
    - Output:
      + a copied [PDC histogram structure](#histogram-structure)
    - Copy a histogram from an existing one
    - For developers, see pdc_hist_pkg.c
  + pdc_histogram_t *PDC_merge_hist(int n, pdc_histogram_t **hists)
    - Input:
      + hists: an array of [PDC histogram structure](#histogram-structure) to be merged
    - Output
      + A merged [PDC histogram structure](#histogram-structure)
    - Merge multiple PDC histograms into one
    - For developers, see pdc_hist_pkg.c
  + void PDC_free_hist(pdc_histogram_t *hist)
    - Input:
      + hist: the [PDC histogram structure](#histogram-structure) to be freed.
    - Output:
      + None
    - Delete a histogram       
    - For developers, see pdc_hist_pkg.c, free structure's internal arrays.
  + void PDC_print_hist(pdc_histogram_t *hist)
    - Input:
      + hist: the [PDC histogram structure](#histogram-structure) to be printed.
    - Output:
      + None:
    - Print a PDC histogram's information. The counter for every bin is displayed.
    - For developers, see pdc_hist_pkg.c.
# PDC Data types
  ## Basic types
  ```
  typedef enum {
    PDC_UNKNOWN      = -1, /* error                                      */
    PDC_INT          = 0,  /* integer types                              */
    PDC_FLOAT        = 1,  /* floating-point types                       */
    PDC_DOUBLE       = 2,  /* double types                               */
    PDC_CHAR         = 3,  /* character types                            */
    PDC_COMPOUND     = 4,  /* compound types                             */
    PDC_ENUM         = 5,  /* enumeration types                          */
    PDC_ARRAY        = 6,  /* Array types                                */
    PDC_UINT         = 7,  /* unsigned integer types                     */
    PDC_INT64        = 8,  /* 64-bit integer types                       */
    PDC_UINT64       = 9,  /* 64-bit unsigned integer types              */
    PDC_INT16        = 10, 
    PDC_INT8         = 11,
    NCLASSES         = 12  /* this must be last                          */
  } pdc_var_type_t;
  ```
  ## Histogram structure
  ```
  typedef struct pdc_histogram_t {
     pdc_var_type_t dtype;
     int            nbin;
     double         incr;
     double        *range;
     uint64_t      *bin;
  } pdc_histogram_t;
  ```
  ## Container info
  ```
     struct pdc_cont_info {
      /*Inherited from property*/
      char                   *name;
      /*Registered using PDC_id_register */
      pdcid_t                 local_id;
      /* Need to register at server using function PDC_Client_create_cont_id */
      uint64_t                meta_id;
  };
  ```
  ## Container life time
  ```
  typedef enum {
    PDC_PERSIST,
    PDC_TRANSIENT
  } pdc_lifetime_t;
  ```
  ## Object property public
  ```
  struct pdc_obj_prop *obj_prop_pub {
      /* This ID is the one returned from PDC_id_register . This is a property ID*/
      pdcid_t           obj_prop_id;
      /* object dimensions */
      size_t            ndim;
      uint64_t         *dims;
      pdc_var_type_t    type;
  };
  ```
  ## Object property
  ```
  struct _pdc_obj_prop {
      /* Suffix _pub probably means public attributes to be accessed. */
      struct pdc_obj_prop *obj_prop_pub {
          /* This ID is the one returned from PDC_id_register . This is a property ID*/
          pdcid_t           obj_prop_id;
          /* object dimensions */
          size_t            ndim;
          uint64_t         *dims;
          pdc_var_type_t    type;
      };
      /* This ID is returned from PDC_find_id with an input of ID returned from PDC init. 
       * This is true for both object and container. 
       * I think it is referencing the global PDC engine through its ID (or name). */
      struct _pdc_class   *pdc{
          char        *name;
          pdcid_t     local_id;
      };
      /* The following are created with NULL values in the PDC_obj_create function. */
      uint32_t             user_id;
      char                *app_name;
      uint32_t             time_step;
      char                *data_loc;
      char                *tags;
      void                *buf;
      pdc_kvtag_t         *kvtag;

      /* The following have been added to support of PDC analysis and transforms.
         Will add meanings to them later, they are not critical. */
      size_t            type_extent;
      uint64_t          locus;
      uint32_t          data_state;
      struct _pdc_transform_state transform_prop{
          _pdc_major_type_t storage_order;
          pdc_var_type_t    dtype;
          size_t            ndim;
          uint64_t          dims[4];
          int               meta_index; /* transform to this state */
      };
  };
  ```
  ## Object info
  ```
  struct pdc_obj_info  {
      /* Directly coped from user argument at object creation. */
      char                   *name;
      /* 0 for location = PDC_OBJ_LOAL. 
       * When PDC_OBJ_GLOBAL = 1, use PDC_Client_send_name_recv_id to retrieve ID. */
      pdcid_t                 meta_id;
      /* Registered using PDC_id_register */
      pdcid_t                 local_id;
      /* Set to 0 at creation time. *
      int                     server_id;
      /* Object property. Directly copy from user argument at object creation. */
      struct pdc_obj_prop    *obj_pt;
  };
  ```
  ## Object structure
  ```
  struct _pdc_obj_info {
      /* Public properties */
      struct pdc_obj_info    *obj_info_pub {
          /* Directly copied from user argument at object creation. */
          char                   *name;
          /* 0 for location = PDC_OBJ_LOAL. 
          * When PDC_OBJ_GLOBAL = 1, use PDC_Client_send_name_recv_id to retrieve ID. */
          pdcid_t                 meta_id;
          /* Registered using PDC_id_register */
          pdcid_t                 local_id;
          /* Set to 0 at creation time. *
          int                     server_id;
          /* Object property. Directly copy from user argument at object creation. */
          struct pdc_obj_prop    *obj_pt;
      };
      /* Argument passed to obj create*/
      _pdc_obj_location_t     location enum {
          /* Either local or global */
          PDC_OBJ_GLOBAL,
          PDC_OBJ_LOCAL
      }
      /* May be used or not used depending on which creation function called. */
      void                   *metadata;
      /* The container pointer this object sits in. Copied*/
      struct _pdc_cont_info  *cont;
      /* Pointer to object property. Copied*/
      struct _pdc_obj_prop   *obj_pt;
      /* Linked list for region, initialized with NULL at create time.*/
      struct region_map_list *region_list_head {
          pdcid_t                orig_reg_id;
          pdcid_t                des_obj_id;
          pdcid_t                des_reg_id;
          /* Double linked list usage*/
          struct region_map_list *prev;
          struct region_map_list *next;
      };
  };
  ```
  ## Region info
  ```
  struct pdc_region_info {
    pdcid_t               local_id;
    struct _pdc_obj_info *obj;
    size_t                ndim;
    uint64_t             *offset;
    uint64_t             *size;
    bool                  mapping;
    int                   registered_op;
    void                 *buf;
  };
  ```
  ## Access type
  ```
  typedef enum { PDC_NA=0, PDC_READ=1, PDC_WRITE=2 }
  ```
  ## Transfer request status
  ```
  typedef enum {
    PDC_TRANSFER_STATUS_COMPLETE  = 0,
    PDC_TRANSFER_STATUS_PENDING   = 1,
    PDC_TRANSFER_STATUS_NOT_FOUND = 2 
  }
  ```
  ## Query operators
  ```
  typedef enum { 
      PDC_OP_NONE = 0, 
      PDC_GT      = 1, 
      PDC_LT      = 2, 
      PDC_GTE     = 3, 
      PDC_LTE     = 4, 
      PDC_EQ      = 5
  } pdc_query_op_t;
  ```
  ## Query structures
  ```
  typedef struct pdc_query_t {
      pdc_query_constraint_t *constraint{
        pdcid_t            obj_id;
        pdc_query_op_t     op;
        pdc_var_type_t     type;
        double             value;   // Use it as a generic 64bit value
        pdc_histogram_t    *hist;

        int                is_range;
        pdc_query_op_t     op2;
        double             value2;

        void               *storage_region_list_head;
        pdcid_t            origin_server;
        int                n_sent;
        int                n_recv;
    }
      struct pdc_query_t     *left;
      struct pdc_query_t     *right;
      pdc_query_combine_op_t  combine_op;
      struct pdc_region_info *region;             // used only on client
      void                   *region_constraint;  // used only on server
      pdc_selection_t        *sel;
  } pdc_query_t;
  ```
  ## Selection structure
  ```
  typedef struct pdcquery_selection_t {
      pdcid_t  query_id;
      size_t   ndim;
      uint64_t nhits;
      uint64_t *coords;
      uint64_t coords_alloc;
  } pdc_selection_t;
  ```
# Developers notes
  + This note is for developers. It helps developers to understand the code structure of PDC code as fast as possible.
  + PDC internal data structure
    - Linkedlist
      * Linkedlist is an important data structure for managing PDC IDs.
      * Overall. An PDC instance after PDC_Init() has a global variable pdc_id_list_g. See pdc_interface.h
      ```
      struct PDC_id_type {
          PDC_free_t                  free_func;         /* Free function for object's of this type    */
          PDC_type_t                  type_id;           /* Class ID for the type                      */
          unsigned                    init_count;        /* # of times this type has been initialized  */
          unsigned                    id_count;          /* Current number of IDs held                 */
          pdcid_t                     nextid;            /* ID to use for the next atom                */
          PDC_LIST_HEAD(_pdc_id_info)  ids;               /* Head of list of IDs                        */
      };

      struct pdc_id_list {
          struct PDC_id_type *PDC_id_type_list_g[PDC_MAX_NUM_TYPES];
      };
      struct pdc_id_list *pdc_id_list_g;
      ```
      * pdc_id_list_g is an array that stores the head of linked list for each types.
      * The _pdc_id_info is defined as the followng in pdc_id_pkg.h.
      ```
      struct _pdc_id_info {
          pdcid_t             id;             /* ID for this info                 */
          hg_atomic_int32_t   count;          /* ref. count for this atom         */
          void                *obj_ptr;       /* pointer associated with the atom */
          PDC_LIST_ENTRY(_pdc_id_info) entry;
      };
      ```
      * obj_ptr is the pointer to the item the ID refers to.
      * See pdc_linkedlist.h for implementations of search, insert, remove etc. operations
    - ID
      * ID is important for managing different data structures in PDC.
      * e.g Creating objects or containers will return IDs for them
    - pdcid_t PDC_id_register(PDC_type_t type, void *object)
      * This function maintains a linked list. Entries of the linked list is going to be the pointers to the objects. Every time we create an object ID for object using some magics. Then the linked list entry is going to be put to the beginning of the linked list.
      * type: One of the followings
      ```
        typedef enum {
            PDC_BADID            = -1, /* invalid Type                                */
            PDC_CLASS            = 1,  /* type ID for PDC                             */
            PDC_CONT_PROP        = 2,  /* type ID for container property              */
            PDC_OBJ_PROP         = 3,  /* type ID for object property                 */
            PDC_CONT             = 4,  /* type ID for container                       */
            PDC_OBJ              = 5,  /* type ID for object                          */
            PDC_REGION           = 6,  /* type ID for region                          */
            PDC_TRANSFER_REQUEST = 7,  /* type ID for region transfer                          */
            PDC_NTYPES           = 8   /* number of library types, MUST BE LAST!      */
        } PDC_type_t;
      ```
      * Object: Pointer to the class instance created ( bad naming, not necessarily a PDC object).
    - struct _pdc_id_info *PDC_find_id(pdcid_t idid);
      * Use ID to get struct _pdc_id_info. For most of the times, we want to locate the object pointer inside the structure. This is linear search in the linked list.
      * idid: ID you want to search.

  + PDC core classes.
    - Property
      * Property in PDC serves as hint and metadata storage purposes.
      * Different types of object has different classes (struct) of properties.
      * See pdc_prop.c, pdc_prop.h and pdc_prop_pkg.h for details.
    - Container
      * Container property
      ```
      struct _pdc_cont_prop {
          /* This class ID is returned from PDC_find_id with an input of ID returned from PDC init. This is true for both object and container. 
           *I think it is referencing the global PDC engine through its ID (or name). */
         struct _pdc_class *pdc{
             /* PDC class instance name*/
             char        *name;
             /* PDC class instance ID. For most of the times, we only have 1 PDC class instance. This is like a global variable everywhere.*/
             pdcid_t     local_id;
          };
          /* This ID is the one returned from PDC_id_register . This is a property ID type. 
           * Some kind of hashing algorithm is used to generate it at property create time*/
          pdcid_t           cont_prop_id;
          /* Not very important */          pdc_lifetime_t    cont_life;
      };
      ```
      * Container structure (pdc_cont_pkg.h and pdc_cont.h)
      ```
      struct _pdc_cont_info {
          struct pdc_cont_info    *cont_info_pub {
              /*Inherited from property*/
              char                   *name;
              /*Registered using PDC_id_register */
              pdcid_t                 local_id;
              /* Need to register at server using function PDC_Client_create_cont_id */
              uint64_t                meta_id;
          };
          /* Pointer to container property.
           * This struct is copied at create time.*/
          struct _pdc_cont_prop   *cont_pt;
      };
      ```
    - Object
      * Object property
      See [object property](#object-property)
      * Object structure (pdc_obj_pkg.h and pdc_obj.h)
      See [Object structure](#object-structure)
