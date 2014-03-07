/* Generic AMPI C tape. Don't touch this. The code is always mirrored with the AMPI repo.
 * Changing code here will result in merge conflicts.
 *
 * See header for more information. 
 */
/*#define DEBUG*/
#define NO_COMM_WORLD
#include <ampi_tape.h> 
#ifdef __cplusplus
#include <ampi_interface.hpp> 
#endif
#ifdef AMPI_COUNT_COMMS
int ampi_comm_count=0;
#endif

AMPI_ht_el *AMPI_ht=NULL;
ampi_tape_entry *ampi_tape;
/*int ampi_tape_counter = 0;*/
long int ampi_vac=0;
long int ampi_vac_stored=0;
long int ampi_chunks=1;

int AMPI_Reset_Tape() {
    ampi_vac=0;
    /* output is deprecated */
    /*printf("AMPI tape has been reset.\n");*/
    return 0;
}

int AMPI_Store_State() {
    ampi_vac_stored = ampi_vac;

    return 0;
}

int AMPI_Restore_State() {
    ampi_vac = ampi_vac_stored;

    return 0;
}

int AMPI_Init(int* argc, char*** argv) {
    long int size=AMPI_CHUNK_SIZE;
    ampi_tape = (ampi_tape_entry*) malloc(size*sizeof(ampi_tape_entry));
    return AMPI_Init_f(argc, argv);
}

int AMPI_Finalize() {
    printf("AMPI chunk size: %d\n", AMPI_CHUNK_SIZE);
    printf("AMPI chunks allocated: %lu\n", ampi_chunks);
    printf("AMPI memory allocated: %lu\n", ampi_chunks*AMPI_CHUNK_SIZE*sizeof(ampi_tape_entry));
#ifdef AMPI_COUNT_COMMS
    printf("AMPI comunications executed: %d\n", ampi_comm_count);
#endif
    return AMPI_Init_b(NULL, NULL);

}

int AMPI_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    if(datatype!=AMPI_DOUBLE) {
      return MPI_Send(buf, count, datatype, dest, tag, comm);
    }
    int i=0;
    ampi_check_tape_size(count+1);
    double * tmp = (double*) malloc(sizeof(double)*count);
    ampi_tape[ampi_vac+count].arg=(int*) malloc(sizeof(int)*2);
    long int new_vac = ampi_vac+count;

    ampi_create_tape_entry(&new_vac);
#pragma omp parallel for
    for(i=0;i<count;i=i+1) {
	ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
	ampi_get_val(buf,&i,&tmp[i]);
	ampi_get_idx(buf, &i, &ampi_tape[ampi_vac+i].idx);
    }

    /*create actual MPI entry*/

    ampi_tape[ampi_vac+count].oc = SEND;
    ampi_tape[ampi_vac+count].arg[0] = count;
    ampi_tape[ampi_vac+count].arg[1] = dest;
    ampi_tape[ampi_vac+count].comm = comm;
    ampi_tape[ampi_vac+count].tag = tag;

    int temp = AMPI_Send_f(tmp, count, datatype, dest, tag, comm);

    ampi_vac+=count+1;
    free(tmp);
    return temp;
}

int AMPI_Recv(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Status *status) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    if(datatype!=AMPI_DOUBLE) {
      return MPI_Recv(buf, count, datatype, dest, tag, comm, status);
    }

    int i=0;
    double * tmp = (double*) malloc(sizeof(double)*count);
    ampi_check_tape_size(count+1);
    ampi_tape[ampi_vac].arg=(int*) malloc(sizeof(int)*2);
    /*INT64 tmp_int64 = 0;*/
    long int new_vac = ampi_vac;

    ampi_create_dummies(buf, &count);
    ampi_create_tape_entry(&new_vac);

    int temp = AMPI_Recv_f(tmp, count, datatype, dest, tag, comm, status);

    ampi_tape[ampi_vac].oc = RECV;
    ampi_tape[ampi_vac].arg[0] = count;
    ampi_tape[ampi_vac].arg[1] = dest;
    ampi_tape[ampi_vac].comm = comm;
    if(status==MPI_STATUS_IGNORE) {
      ampi_tape[ampi_vac].tag = tag;
    }
    else
      ampi_tape[ampi_vac].tag = status->MPI_TAG;

#pragma omp parallel for
    for(i=0;i<count;i=i+1) {
	ampi_tape[ampi_vac+i+1].oc = MPI_DUMMY;
	
	/*ampi_get_idx(buf, &i, &tmp_int64);*/
	ampi_get_idx(buf, &i, &ampi_tape[ampi_vac+i+1].idx);
	ampi_set_val(buf, &i, &tmp[i]);
	/*ampi_tape[ampi_vac+i+1].idx = tmp_int64;*/
    } 
    free(tmp);
    ampi_vac+=count+1;
    return temp;
}

int AMPI_Isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *mpi_request) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    if(datatype!=AMPI_DOUBLE) {
      return MPI_Isend(buf, count, datatype, dest, tag, comm, mpi_request);
    }
    int i=0;
    /*new AMPI request*/
    AMPI_Request *request=(AMPI_Request*) malloc(sizeof(AMPI_Request));
    /*new hash element*/
    AMPI_ht_el *ht_el=(AMPI_ht_el*) malloc(sizeof(AMPI_ht_el));
    ht_el->key=mpi_request;
    request->buf = buf;
    request->aw=0;
    request->request=mpi_request;
    double * tmp = (double*) malloc(sizeof(double)*count);
    ampi_check_tape_size(count+1);
    ampi_tape[ampi_vac+count].arg=(int*) malloc(sizeof(int)*2);
    long int new_vac = ampi_vac+count;

    ampi_create_tape_entry(&new_vac);

    /*create dummy of each element*/

#pragma omp parallel for
    for(i=0 ; i<count ; i=i+1) {
	ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
	ampi_get_val(buf,&i,&tmp[i]);
	ampi_get_idx(buf, &i, &ampi_tape[ampi_vac+i].idx);
    }
    ampi_tape[ampi_vac+count].oc = ISEND;
    ampi_tape[ampi_vac+count].arg[0] = count;
    ampi_tape[ampi_vac+count].arg[1] = dest;
    ampi_tape[ampi_vac+count].comm = comm;
    ampi_tape[ampi_vac+count].tag = tag;

    /*if(request->aw) { */
	/*if antiwait, memory was already allocated*/
	/*tape[tape_entry::vac+count].request = new AMPI_dco_Request;*/
	/*tape[request->va].request = tape[tape_entry::vac+count].request;*/
	/*tape[request->va].arg[0] = tape_entry::vac+count;*/
    /*}*/
    /*else {*/
	ampi_tape[ampi_vac+count].request = (AMPI_Request*) malloc(sizeof(AMPI_Request));
	/*tape[tape_entry::vac+count].request = new AMPI_dco_Request;*/
	/*}*/
    /*point current request index to this tape entry*/
    request->va = ampi_vac+count; 
    request->v = tmp;
    int temp = AMPI_Isend_f(tmp, count, datatype, dest, tag, comm, request);
    ampi_vac+=count+1;
    ht_el->request=*request;
    HASH_ADD_PTR(AMPI_ht,key,ht_el);
    free(request);
    return temp;
}

int AMPI_Irecv(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *mpi_request) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    if(datatype!=AMPI_DOUBLE) {
      return MPI_Irecv(buf, count, datatype, dest, tag, comm, mpi_request);
    }
    int i=0;
    AMPI_Request *request=(AMPI_Request*) malloc(sizeof(AMPI_Request));
    /*INT64 tmp_int64 = 0;*/
    AMPI_ht_el *ht_el=(AMPI_ht_el*) malloc(sizeof(AMPI_ht_el));
    ht_el->key=mpi_request;
    request->buf = buf;
    request->aw=0;
    request->request=mpi_request;
    double * tmp = (double*) malloc(sizeof(double)*count);
    ampi_check_tape_size(count+1);
    ampi_tape[ampi_vac].arg=(int*) malloc(sizeof(int)*2);
    long int new_vac = ampi_vac;

    ampi_create_dummies(buf, &count);
    ampi_create_tape_entry(&new_vac);

    int temp = AMPI_Irecv_f(tmp, count, datatype, dest, tag, comm, request);

    ampi_tape[ampi_vac].arg[0] = count;
    ampi_tape[ampi_vac].arg[1] = dest;
    ampi_tape[ampi_vac].oc = IRECV;
    ampi_tape[ampi_vac].comm = comm;
    ampi_tape[ampi_vac].tag = tag;

#pragma omp parallel for
    for(i=0 ; i<count ; i=i+1) {
	/*ampi_get_idx(buf, &i, &tmp_int64);*/
	ampi_get_idx(buf, &i, &ampi_tape[ampi_vac+i+1].idx);
	/*ampi_tape[ampi_vac+i+1].idx = tmp_int64;*/
	ampi_tape[ampi_vac+i+1].oc = MPI_DUMMY;


    } 
    /*if(request->aw) { */
	/*if antiwait, memory was already allocated*/
	/*tape[tape_entry::vac].request = new AMPI_dco_Request;*/
	/*tape[request->va].request = tape[tape_entry::vac].request;*/
	/*tape[request->va].arg[0] = tape_entry::vac;*/
    /*}*/
    /*else {*/
	ampi_tape[ampi_vac].request = (AMPI_Request*) malloc(sizeof(AMPI_Request));
	/*tape[tape_entry::vac].request =  new AMPI_dco_Request;*/
	/*}*/
    /*request->r.v = tmp;*/
    /*point current request index to this tape entry*/
    request->va = ampi_vac; 
    ampi_vac+=count+1;
    ht_el->request=*request;
    HASH_ADD_PTR(AMPI_ht,key,ht_el);
    free(request);
    return temp;
}

int AMPI_Wait(MPI_Request *mpi_request, MPI_Status *status) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    int i=0;
    int ret=0;
    AMPI_ht_el *ht_req=NULL;
    AMPI_Request *request=(AMPI_Request*) malloc(sizeof(AMPI_Request));
    void *addr=mpi_request;
    HASH_FIND_PTR(AMPI_ht,&addr,ht_req);
    if(ht_req) {
	*request=ht_req->request;
	double * tmp = (double*) request->v;
	ampi_tape[ampi_vac].arg=(int*) malloc(sizeof(int));
	ampi_check_tape_size(1);
	long int new_vac = ampi_vac;

	ampi_create_tape_entry(&new_vac);
	/*get the corresponding isend or irecv tape entry*/

	ampi_tape[ampi_vac].arg[0] = request->va;    
	ampi_tape[ampi_vac].oc = WAIT;
	ret=AMPI_Wait_f(request, status);
	/*finally copy the request to the tape*/
	if(status==MPI_STATUS_IGNORE)
	  request->tag=ampi_tape[request->va].tag;
	else
	  request->tag=status->MPI_TAG;
	*ampi_tape[request->va].request = *request;			
	ampi_tape[ampi_vac].request = ampi_tape[request->va].request;
	if(request->oc == AMPI_IR) {
	    for(i=0 ; i<ampi_tape[request->va].arg[0] ; i=i+1) {
		ampi_set_val(request->buf, &i, &tmp[i]);
	    }	 

	}
	/*ampi_tape[ampi_vac].request->a = &ampi_tape[request->va].d;*/
	ampi_tape[ampi_vac].request->size = ampi_tape[request->va].arg[0];
	ampi_vac++;
	free(tmp);
	HASH_DEL(AMPI_ht,ht_req);
	return ret;
	/*return MPI_Wait(mpi_request,status);*/
    }
    else {
	return MPI_Wait(mpi_request,status);
    }
}

int AMPI_Waitall(int count, MPI_Request *mpi_request, MPI_Status *status) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    int i=0;
    for(i=0;i<count;i=i+1) {
	AMPI_Wait(&mpi_request[i],&status[i]);
    }
    return 0;
}
int AMPI_Waitany(int count, MPI_Request array_of_requests[], int *index, MPI_Status *status) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    int i=0;
    for(i=0;i<count;i=i+1) {
	if(array_of_requests[i]!=MPI_REQUEST_NULL) {
	    *index=i;
	    return AMPI_Wait(&array_of_requests[i],status);
	}
    }
    /* if all requests are NULL we call MPI_Waitany to get the correct return
     * values. No tracing is needed
     */
    return MPI_Waitany(count,array_of_requests,index,status);
}
int AMPI_Bcast(void *buf, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    if(datatype!=AMPI_DOUBLE) {
      return MPI_Bcast(buf, count, datatype, root, comm);
    }
    int rank=0;
    int i=0;
    double * tmp = (double*) malloc(sizeof(double)*count);
    for(i=0;i<count;i++) tmp[i]=0;
    MPI_Comm_rank(comm,&rank);
    ampi_check_tape_size(count+1);

    if(rank==root) {
      for(i = 0 ; i < count ; i=i+1) {
	ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
	ampi_get_val(buf,&i,&tmp[i]);
	ampi_get_idx(buf, &i, &ampi_tape[ampi_vac+i].idx);
      }
    }

    /*create actual MPI entry*/
    long int new_vac = ampi_vac+count;
    if(rank!=root) {
      ampi_create_dummies(buf, &count);
    }
    ampi_create_tape_entry(&new_vac);
    ampi_tape[ampi_vac+count].arg=(int*) malloc(sizeof(int)*2);
    ampi_tape[ampi_vac+count].oc = BCAST;
    ampi_tape[ampi_vac+count].arg[0] = count;
    ampi_tape[ampi_vac+count].arg[1] = root;
    ampi_tape[ampi_vac+count].comm = comm;

    int temp = AMPI_Bcast_f(tmp, count, datatype, root, comm);

    if(rank!=root) {
      for(i=0;i<count;i=i+1) {
	ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
	ampi_get_idx(buf, &i, &ampi_tape[ampi_vac+i].idx);
	ampi_set_val(buf, &i, &tmp[i]);
      } 
    }
    ampi_vac+=count+1;
    free(tmp);
    return temp;
}

int AMPI_Reduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    if(datatype!=AMPI_DOUBLE) {
      MPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
    }
    ampi_check_tape_size(2*count+1);
    int i=0;
    double * tmp_send = (double*) malloc(sizeof(double)*count);
    double * tmp_recv = (double*) malloc(sizeof(double)*count);
    ampi_tape[ampi_vac+count].arg=(int*) malloc(sizeof(int)*3);

    long int new_vac = ampi_vac+count;

    ampi_create_dummies(recvbuf, &count);
    ampi_create_tape_entry(&new_vac);

    /*sendbuf dummies*/

#pragma omp parallel for
    for(i=0 ; i<count ; i=i+1) {
	ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
	ampi_get_val(sendbuf,&i,&tmp_send[i]);
	ampi_get_val(recvbuf,&i,&tmp_recv[i]);
	ampi_get_idx(sendbuf, &i, &ampi_tape[ampi_vac+i].idx);
    }

    /*actual reduce entry*/

    ampi_tape[ampi_vac+count].oc = REDUCE;
    ampi_tape[ampi_vac+count].arg[0] = count;
    ampi_tape[ampi_vac+count].arg[1] = root;
    if(op == MPI_SUM){
	ampi_tape[ampi_vac+count].arg[2] = AMPI_REDUCE_ADD;
    }
    if(op == MPI_PROD){
	ampi_tape[ampi_vac+count].arg[2] = AMPI_REDUCE_MUL;
    }
    if(op == MPI_MIN){
	ampi_tape[ampi_vac+count].arg[2] = AMPI_REDUCE_MIN;
    }
    if(op == MPI_MAX){
	ampi_tape[ampi_vac+count].arg[2] = AMPI_REDUCE_MAX;
    }
    ampi_tape[ampi_vac+count].comm = comm;

    AMPI_Reduce_f(tmp_send, tmp_recv, count, datatype, op, root, comm);

    /*recvbuf entry*/

#pragma omp parallel for
    for(i=0 ; i<count ; i=i+1) {
	ampi_tape[ampi_vac+count+1+i].oc = MPI_DUMMY;
	ampi_get_idx(recvbuf, &i, &ampi_tape[ampi_vac+count+1+i].idx);
	ampi_set_val(recvbuf, &i, &tmp_recv[i]);
    }


    ampi_vac+=2*count+1;
    free(tmp_send);
    free(tmp_recv);
    return 0;
}

int AMPI_Allreduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    if(datatype!=AMPI_DOUBLE) {
      MPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
    }
    ampi_check_tape_size(2*count+1);
    int tapeActive = ampi_is_tape_active();
    int i=0;
    double * tmp_send = (double*) malloc(sizeof(double)*count);
    double * tmp_recv = (double*) malloc(sizeof(double)*count);

    long int new_vac = 0;
    if(tapeActive) {
        ampi_tape[ampi_vac+count].arg=(int*) malloc(sizeof(int)*3);
        new_vac = ampi_vac+count;

        ampi_create_dummies(recvbuf, &count);
        ampi_create_tape_entry(&new_vac);
    }

    /*sendbuf dummies*/

#pragma omp parallel for
    for(i=0 ; i<count ; i=i+1) {
        ampi_get_val(sendbuf,&i,&tmp_send[i]);

        if(tapeActive) {
            ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
            ampi_get_idx(sendbuf, &i, &ampi_tape[ampi_vac+i].idx);
        }
    }

    if(tapeActive) {
        /*actual reduce entry*/

        ampi_tape[ampi_vac+count].oc = ALLREDUCE;
        ampi_tape[ampi_vac+count].arg[0] = count;
        ampi_tape[ampi_vac+count].comm = comm;
        /*ampi_tape[ampi_vac+count].arg[1] = root;*/
        if(op == MPI_SUM){
        ampi_tape[ampi_vac+count].arg[2] = AMPI_REDUCE_ADD;
        }
        if(op == MPI_PROD){
        ampi_tape[ampi_vac+count].arg[2] = AMPI_REDUCE_MUL;
        }
        if(op == MPI_MIN){
        ampi_tape[ampi_vac+count].arg[2] = AMPI_REDUCE_MIN;
        }
        if(op == MPI_MAX){
        ampi_tape[ampi_vac+count].arg[2] = AMPI_REDUCE_MAX;
        }
    }

    AMPI_Allreduce_f(tmp_send, tmp_recv, count, datatype, op, comm);

    /*recvbuf entry*/

#pragma omp parallel for
	for(i=0 ; i<count ; i=i+1) {
	    if(tapeActive) {
	        ampi_tape[ampi_vac+count+1+i].oc = MPI_DUMMY;
	        ampi_get_idx(recvbuf, &i, &ampi_tape[ampi_vac+count+1+i].idx);
	    }
	    ampi_set_val(recvbuf, &i, &tmp_recv[i]);
	}

	if(tapeActive) {
	    ampi_vac+=2*count+1;
	}
    free(tmp_send);
    free(tmp_recv);
    return 0;
}

int AMPI_Scatter(void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
  if(sendtype !=AMPI_DOUBLE || recvtype != AMPI_DOUBLE) {
    return MPI_Scatter(sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm);
  }
  int i=0;
  int size=0;
  MPI_Comm_size(comm,&size);
  ampi_check_tape_size(recvcnt+sendcnt*size+1);
  double * tmp_send = (double*) malloc(sizeof(double)*sendcnt*size);
  double * tmp_recv = (double*) malloc(sizeof(double)*recvcnt);
  ampi_tape[ampi_vac+sendcnt*size].arg=(int*) malloc(sizeof(int)*3);
  ampi_tape[ampi_vac+sendcnt*size].comm = comm;
  ampi_create_dummies(recvbuf, &recvcnt);

  /*sendbuf dummies*/

#pragma omp parallel for
  for(i=0 ; i<sendcnt*size ; i=i+1) {
    ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
    ampi_get_val(sendbuf,&i,&tmp_send[i]);
    ampi_get_idx(sendbuf, &i, &ampi_tape[ampi_vac+i].idx);
  }
  ampi_tape[ampi_vac+sendcnt*size].oc = SCATTER;
  ampi_tape[ampi_vac+sendcnt*size].arg[2] = sendcnt;
  ampi_tape[ampi_vac+sendcnt*size].arg[1] = recvcnt;
  ampi_tape[ampi_vac+sendcnt*size].arg[0] = root;
  ampi_tape[ampi_vac+sendcnt*size].comm = comm;
  long int new_vac=ampi_vac+sendcnt*size;
  ampi_create_tape_entry(&new_vac);

  AMPI_Scatter_f(tmp_send, sendcnt, sendtype, tmp_recv, recvcnt, recvtype, root, comm);

  /*recvbuf entry*/

#pragma omp parallel for
  for(i=0 ; i<recvcnt ; i=i+1) {
    ampi_tape[ampi_vac+sendcnt*size+1+i].oc = MPI_DUMMY;
    ampi_get_idx(recvbuf, &i, &ampi_tape[ampi_vac+sendcnt*size+1+i].idx);
    ampi_set_val(recvbuf, &i, &tmp_recv[i]);
  }

  ampi_vac+=recvcnt+size*sendcnt+1;
  free(tmp_send);
  free(tmp_recv);
  return 0;

}

int AMPI_Scatterv(void *sendbuf, int *sendcnts, int *displs, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
  if(sendtype !=AMPI_DOUBLE || recvtype != AMPI_DOUBLE) {
    return MPI_Scatterv(sendbuf, sendcnts, displs, sendtype, recvbuf, recvcnt, recvtype, root, comm);
  }
    int i=0;
    int size=0;
    int rank=0;
    int max_size=0;
    MPI_Comm_size(comm,&size);
    MPI_Comm_size(comm,&rank);
    /* Allocate maximum size of sendbuf */
    for(i=0;i<size;i=i+1) 
	if(sendcnts[i]+displs[i]>max_size) max_size=sendcnts[i]+displs[i];
    double * tmp_send = (double*) malloc(sizeof(double)*max_size);
    double * tmp_recv = (double*) malloc(sizeof(double)*recvcnt);
    ampi_tape[ampi_vac+max_size].arg=(int*) malloc(sizeof(int)*(3+2*size));
    ampi_tape[ampi_vac+max_size].comm = comm;

    /*sendbuf dummies*/

#pragma omp parallel for
    for(i=0 ; i<max_size ; i=i+1) {
	ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
	ampi_get_val(sendbuf,&i,&tmp_send[i]);
	ampi_get_idx(sendbuf, &i, &ampi_tape[ampi_vac+i].idx);
    }
    for(i=0 ; i<size*2 ; i=i+2) {
	ampi_tape[ampi_vac+max_size].arg[i+3] = sendcnts[i/2];
	ampi_tape[ampi_vac+max_size].arg[i+4] = displs[i/2];
    }
    ampi_tape[ampi_vac+max_size].oc = SCATTERV;
    ampi_tape[ampi_vac+max_size].arg[2] = max_size;
    ampi_tape[ampi_vac+max_size].arg[1] = recvcnt;
    ampi_tape[ampi_vac+max_size].arg[0] = root;
    ampi_tape[ampi_vac+max_size].comm = comm;

    /*AMPI_Scatterv_f(sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm);*/

    /*recvbuf entry*/

#pragma omp parallel for
    for(i=0 ; i<recvcnt ; i=i+1) {
	ampi_tape[ampi_vac+max_size+1+i].oc = MPI_DUMMY;
	ampi_get_idx(recvbuf, &i, &ampi_tape[ampi_vac+max_size+1+i].idx);
	ampi_set_val(recvbuf, &i, &tmp_recv[i]);
    }

    ampi_vac+=recvcnt+max_size+1;
    free(tmp_send);
    free(tmp_recv);
    return 0;

}

int AMPI_Gather(void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
  if(sendtype !=AMPI_DOUBLE || recvtype != AMPI_DOUBLE) {
    return MPI_Gather(sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm);
  }
  int i=0;
  int size=0;
  int rank=0;
  MPI_Comm_size(comm,&size);
  MPI_Comm_rank(comm,&rank);
  double * tmp_send = (double*)malloc(sizeof(double)*sendcnt);
  double * tmp_recv = 0;
  if(rank == root) {
	  tmp_recv = (double*)malloc(sizeof(double)*recvcnt*size);
  }

  /* check for size */
  int recvSize=recvcnt*size;
  int bufferSize = sendcnt+1;
  if(rank == root) {
	  bufferSize = recvcnt*size+sendcnt+1;
  }
  ampi_check_tape_size(bufferSize);

  ampi_tape[ampi_vac+sendcnt].arg=(int*)malloc(sizeof(int)*3);
  ampi_tape[ampi_vac+sendcnt].comm = comm;


  if(rank == root) {
	  ampi_create_dummies(recvbuf, &recvSize);
  }

  /*sendbuf dummies*/

#pragma omp parallel for
  for(i=0 ; i<sendcnt ; i=i+1) {
    ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
    ampi_get_val(sendbuf,&i,&tmp_send[i]);
    ampi_get_idx(sendbuf, &i, &ampi_tape[ampi_vac+i].idx);
  }
  ampi_tape[ampi_vac+sendcnt].oc = GATHER;
  ampi_tape[ampi_vac+sendcnt].arg[2] = sendcnt;
  ampi_tape[ampi_vac+sendcnt].arg[1] = recvcnt;
  ampi_tape[ampi_vac+sendcnt].arg[0] = root;
  ampi_tape[ampi_vac+sendcnt].comm = comm;
  long int new_vac=ampi_vac+sendcnt;
  ampi_create_tape_entry(&new_vac);

  AMPI_Gather_f(tmp_send, sendcnt, sendtype, tmp_recv, recvcnt, recvtype, root, comm);

  /*recvbuf entry*/

  if(rank == root) {
#pragma omp parallel for
	  for(i=0 ; i<recvcnt*size ; i=i+1) {
		ampi_tape[new_vac+1+i].oc = MPI_DUMMY;
		ampi_get_idx(recvbuf, &i, &ampi_tape[new_vac+1+i].idx);
		ampi_set_val(recvbuf, &i, &tmp_recv[i]);
	  }
  }

  ampi_vac+=bufferSize;
  free(tmp_send);
  if( rank == root ) {
    free(tmp_recv);
  }
  return 0;

}

int AMPI_Gatherv(void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
  if(sendtype !=AMPI_DOUBLE || recvtype != AMPI_DOUBLE) {
    return MPI_Gatherv(sendbuf, sendcnt, sendtype, recvbuf, recvcnts, displs, recvtype, root, comm);
  }
  /*ampi_check_tape_size(sendcnt+1);*/
    int i=0;
    int size=0;
    int rank=0;
    int max_size=0;
    MPI_Comm_size(comm,&size);
    MPI_Comm_size(comm,&rank);
    /* Determine and allocate maximum size of recvbuf */
    for(i=0;i<size;i=i+1) 
	if(recvcnts[i]+displs[i]>max_size) max_size=recvcnts[i]+displs[i];
    double * tmp_send = (double*) malloc(sizeof(double)*sendcnt);
    double * tmp_recv = (double*) malloc(sizeof(double)*max_size);
    ampi_tape[ampi_vac+sendcnt].arg=(int*) malloc(sizeof(int)*3);
    ampi_tape[ampi_vac+sendcnt].comm = comm;

    /*sendbuf dummies*/

#pragma omp parallel for
    for(i=0 ; i<sendcnt ; i=i+1) {
	ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
	ampi_get_val(sendbuf,&i,&tmp_send[i]);
	ampi_get_idx(sendbuf, &i, &ampi_tape[ampi_vac+i].idx);
    }
    ampi_tape[ampi_vac+sendcnt].oc = GATHERV;
    ampi_tape[ampi_vac+sendcnt].arg[2] = sendcnt;
    ampi_tape[ampi_vac+sendcnt].arg[1] = max_size;
    ampi_tape[ampi_vac+sendcnt].arg[0] = root;
    ampi_tape[ampi_vac+sendcnt].comm = comm;

    /*AMPI_Gather(sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm);*/

    /*recvbuf entry*/

#pragma omp parallel for
    for(i=0 ; i<max_size ; i=i+1) {
	ampi_tape[ampi_vac+sendcnt+1+i].oc = MPI_DUMMY;
	ampi_get_idx(recvbuf, &i, &ampi_tape[ampi_vac+sendcnt+1+i].idx);
	ampi_set_val(recvbuf, &i, &tmp_recv[i]);
    }

    ampi_vac+=max_size*size+sendcnt+1;
    free(tmp_send);
    free(tmp_recv);
    return 0;

}

int AMPI_Recv_init(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *mpi_request) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
  if(datatype!=AMPI_DOUBLE) {
    return MPI_Recv_init(buf, count, datatype, source, tag, comm, mpi_request);
  }
    ampi_check_tape_size(1);
    AMPI_Request *request=(AMPI_Request*) malloc(sizeof(AMPI_Request));
    AMPI_ht_el *ht_el=(AMPI_ht_el*) malloc(sizeof(AMPI_ht_el));
    ampi_tape[ampi_vac].arg=(int*) malloc(sizeof(int)*2);
    ht_el->key=mpi_request;
    request->buf = buf;
    request->aw=0;
    request->va=ampi_vac;
    ht_el->request=*request;
    ampi_create_tape_entry(&ampi_vac);
    ampi_tape[ampi_vac].oc = RECV_INIT;
    ampi_tape[ampi_vac].arg[0] = count;
    ampi_tape[ampi_vac].arg[1] = source;
    ampi_tape[ampi_vac].comm = comm;
    ampi_tape[ampi_vac].tag = tag;
    HASH_ADD_PTR(AMPI_ht,key,ht_el);
    ampi_vac=ampi_vac+1;
    return 0;
}
int AMPI_Send_init(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *mpi_request) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
  if(datatype!=AMPI_DOUBLE) {
    return MPI_Send_init(buf, count, datatype, dest, tag, comm, mpi_request);
  }
    ampi_check_tape_size(1);
    AMPI_Request *request=(AMPI_Request*) malloc(sizeof(AMPI_Request));
    AMPI_ht_el *ht_el=(AMPI_ht_el*) malloc(sizeof(AMPI_ht_el));
    ampi_tape[ampi_vac].arg=(int*) malloc(sizeof(int)*2);
    ht_el->key=mpi_request;
    request->buf = buf;
    request->aw=0;
    request->va=ampi_vac;
    ht_el->request=*request;
    ampi_create_tape_entry(&ampi_vac);
    ampi_tape[ampi_vac].oc = SEND_INIT;
    ampi_tape[ampi_vac].arg[0] = count;
    ampi_tape[ampi_vac].arg[1] = dest;
    ampi_tape[ampi_vac].comm = comm;
    ampi_tape[ampi_vac].tag = tag;
    HASH_ADD_PTR(AMPI_ht,key,ht_el);
    ampi_vac=ampi_vac+1;
    return 0;
}
int AMPI_Sendrecv_replace(void *buf, int count, MPI_Datatype datatype, int dest, int sendtag, int source, int recvtag, MPI_Comm comm, MPI_Status *status) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
  if(datatype!=AMPI_DOUBLE) {
    return MPI_Sendrecv_replace(buf, count, datatype, dest, sendtag, source, recvtag, comm, status);
  }
    ampi_check_tape_size(2*count+2);
    int i=0;
    double * tmp = (double*) malloc(sizeof(double)*count);
    ampi_tape[ampi_vac+count].arg=(int*) malloc(sizeof(int)*2);
    ampi_tape[ampi_vac+count+1].arg=(int*) malloc(sizeof(int)*2);
    /*This may be important for the openmp loop*/
    /*INT64 tmp_int64 = 0;*/
#pragma omp parallel for
    for(i=0;i<count;i=i+1) {
	ampi_tape[ampi_vac+i].oc = MPI_DUMMY;
	ampi_get_val(buf,&i,&tmp[i]);
	ampi_get_idx(buf, &i, &ampi_tape[ampi_vac+i].idx);
    }

    /*create actual MPI entry*/

    ampi_tape[ampi_vac+count].oc = SENDRECVREPLACE;
    ampi_tape[ampi_vac+count].arg[0] = count;
    ampi_tape[ampi_vac+count].arg[1] = dest;
    ampi_tape[ampi_vac+count].comm = comm;
    ampi_tape[ampi_vac+count].tag = sendtag;

    long int new_vac = ampi_vac+count+1;

    ampi_create_dummies(buf, &count);
    ampi_create_tape_entry(&new_vac);

    int temp=AMPI_Sendrecv_replace_f(tmp, count, datatype, dest, sendtag, source, recvtag, comm, status);

    ampi_tape[ampi_vac+count+1].oc = SENDRECVREPLACE;
    ampi_tape[ampi_vac+count+1].arg[0] = count;
    ampi_tape[ampi_vac+count+1].arg[1] = source;
    ampi_tape[ampi_vac+count+1].comm = comm;
    if(status!=MPI_STATUS_IGNORE)
      ampi_tape[ampi_vac+count+1].tag = status->MPI_TAG;
    else
      ampi_tape[ampi_vac+count+1].tag = recvtag;

#pragma omp parallel for
    for(i=0;i<count;i=i+1) {
	ampi_tape[ampi_vac+count+2+i].oc = MPI_DUMMY;
	
	/*ampi_get_idx(buf, &i, &tmp_int64);*/
	ampi_get_idx(buf, &i, &ampi_tape[ampi_vac+count+2+i].idx);
	ampi_set_val(buf, &i, &tmp[i]);
	/*ampi_tape[ampi_vac+count+2+i].idx = tmp_int64;*/
    } 
    free(tmp);
    ampi_vac+=2*count+2;
    return temp;
}

int AMPI_Start(MPI_Request *mpi_request) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    AMPI_ht_el *ht_req=NULL;
    int ret=0;
    long int va=0;
    AMPI_Request request;
    request.aw=0;
    /*AMPI_Request *request=malloc(sizeof(AMPI_Request));*/
    void *addr=mpi_request;
    HASH_FIND_PTR(AMPI_ht,&addr,ht_req);
    if(ht_req) {
        request=ht_req->request;
	va=request.va;
	if(ampi_tape[va].oc==SEND_INIT) {
	    ret=AMPI_Isend(request.buf,ampi_tape[va].arg[0],MPI_DOUBLE,ampi_tape[va].arg[1],ampi_tape[va].tag,ampi_tape[va].comm,mpi_request);
	    if(addr!=mpi_request) printf("changed\n");
            HASH_FIND_PTR(AMPI_ht,&addr,ht_req);
	    return ret;
	}
	else if(ampi_tape[va].oc==RECV_INIT) {
	    ret=AMPI_Irecv(request.buf,ampi_tape[va].arg[0],MPI_DOUBLE,ampi_tape[va].arg[1],ampi_tape[va].tag,ampi_tape[va].comm,mpi_request);
	    if(addr!=mpi_request) printf("changed\n");
            HASH_FIND_PTR(AMPI_ht,&addr,ht_req);
	    return ret;
	}
	else {
	    printf("Active start No opcode: %d\n",ampi_tape[va].oc); 
	}
    }
    else {
	printf("Passive Start\n");
	return MPI_Start(mpi_request);
    }
    return -1;
}

int AMPI_Startall(int count, MPI_Request array_of_requests[]) {
#ifdef AMPI_COUNT_COMMS
    ampi_comm_count=ampi_comm_count+1;
#endif
    int i=0;
    for(i=0;i<count;i=i+1) AMPI_Start(&array_of_requests[i]);
    return 0;
}

void ampi_interpret_tape(long int idx){ 
    int j=0;
    double *tmp_d;
    double *tmp_d_recv;
    double *tmp_d_send;
    long int i=ampi_vac;
    ampi_tape_entry *tmp_entry;
    MPI_Comm comm;
    MPI_Op op = MPI_SUM;
    comm = MPI_COMM_WORLD;
    MPI_Status status;
    /*ampi_vac=ampi_vac-1;*/
    /*while(ampi_tape[ampi_vac].oc == MPI_DUMMY)*/
    /*ampi_vac=ampi_vac-1;*/
    /*i=ampi_vac;*/
    i=idx;
#ifdef DEBUG
    printf("AMPI_TAPE Interpreter OC: %d\n", ampi_tape[i].oc);
    printf("--------------------------------START\n");
#endif
    switch(ampi_tape[i].oc){ 
	case SEND : {
			tmp_d = (double*) malloc(sizeof(double)*ampi_tape[i].arg[0]);
#ifdef NO_COMM_WORLD
			comm=ampi_tape[i].comm;
#endif
			AMPI_Send_b(tmp_d, ampi_tape[i].arg[0], MPI_DOUBLE, ampi_tape[i].arg[1], ampi_tape[i].tag, comm);
			for(j=0;j<ampi_tape[i].arg[0];j=j+1) {
			    tmp_entry=&ampi_tape[i-ampi_tape[i].arg[0]+j];
			    ampi_set_adj(&tmp_entry->idx, &tmp_d[j]);
#ifdef DEBUG
			    printf("SEND: %d %e\n", tmp_entry->idx, tmp_d[j]);
#endif
			}
			free(tmp_d);
			break;
		    }
	case RECV : {
			tmp_d = (double*) malloc(sizeof(double)*ampi_tape[i].arg[0]);
			for(j=0;j<ampi_tape[i].arg[0];j=j+1) {
			    tmp_entry=&ampi_tape[i+j+1];
			    ampi_get_adj(&tmp_entry->idx, &tmp_d[j]);
#ifdef DEBUG
			    printf("RECV: %d %e\n", tmp_entry->idx, tmp_d[j]);
#endif
			} 
#ifdef NO_COMM_WORLD
			comm=ampi_tape[i].comm;
#endif
			AMPI_Recv_b(tmp_d, ampi_tape[i].arg[0], MPI_DOUBLE, ampi_tape[i].arg[1], ampi_tape[i].tag, comm,&status);
			free(tmp_d);
			break;
		    }
	case ISEND : {
			 /*tmp_d = malloc(sizeof(double)*ampi_tape[i].arg[0]);*/
			 /*if(!tape[i].request->r.aw) {*/
			 tmp_d = (double*) ampi_tape[i].request->a;
#ifdef NO_COMM_WORLD
			comm=ampi_tape[i].comm;
#endif
			 AMPI_Isend_b(tmp_d, ampi_tape[i].arg[0], MPI_DOUBLE, ampi_tape[i].arg[1], ampi_tape[i].tag, comm,ampi_tape[i].request);
			 for(j = 0 ; j < ampi_tape[i].arg[0] ; j++) {
			     tmp_entry=&ampi_tape[i-ampi_tape[i].arg[0]+j];
			     ampi_set_adj(&tmp_entry->idx, &tmp_d[j]);
		     }
		     /*free(tmp_d);*/
			 free(ampi_tape[i].request->request);
			 break;
		     }
	case IRECV : {
			 /*tmp_d = malloc(sizeof(double)*ampi_tape[i].arg[0]);*/
			 tmp_d = (double*) ampi_tape[i].request->a;
#ifdef NO_COMM_WORLD
			comm=ampi_tape[i].comm;
#endif
			 /*if(tape[i].request->r.aw) {*/

			 /*}*/
			 /*else {*/
			     AMPI_Irecv_b(tmp_d, ampi_tape[i].arg[0], MPI_DOUBLE, ampi_tape[i].arg[1], ampi_tape[i].tag, comm,ampi_tape[i].request);
			     /*}*/
			 free(tmp_d);
			 free(ampi_tape[i].request->request);
			 break;
		     }
	case WAIT : {
			tmp_d = (double*) malloc(sizeof(double)*ampi_tape[ampi_tape[i].arg[0]].arg[0]);
			if(ampi_tape[i].request->oc == AMPI_IR) {
			    for(j = 0 ; j < ampi_tape[ampi_tape[i].arg[0]].arg[0] ; j++) {
				tmp_entry=&ampi_tape[ampi_tape[i].arg[0]+j+1];
				ampi_get_adj(&tmp_entry->idx, &tmp_d[j]);
			    }
#ifdef DEBUG
			    printf("AMPI_Wait_interpret: ");
			    printf("%d ", ampi_tape[ampi_tape[i].arg[0]].arg[0]);
			    for(j = 0 ; j < ampi_tape[ampi_tape[i].arg[0]].arg[0] ; j++) {
				printf("%e ", tmp_d[j]);
			    }
			    printf("\n");
#endif
			}
			ampi_tape[i].request->a = tmp_d;
			ampi_tape[i].request->request=(MPI_Request*) malloc(sizeof(MPI_Request));
#ifndef NO_COMM_WORLD 
			ampi_tape[i].request->comm=MPI_COMM_WORLD;
#endif
			AMPI_Wait_b(ampi_tape[i].request,&status);
			ampi_tape[ampi_tape[i].arg[0]].request = ampi_tape[i].request;
			break;
		    }
	case BCAST : {
		       int rank=0;
		       int root=ampi_tape[i].arg[1];
#ifdef NO_COMM_WORLD
			comm=ampi_tape[i].comm;
#endif
		       MPI_Comm_rank(comm,&rank);
		       
			tmp_d = (double*) malloc(sizeof(double)*ampi_tape[i].arg[0]);
			if(rank!=root) {
			  for(j=0;j<ampi_tape[i].arg[0];j=j+1) {
			    tmp_entry=&ampi_tape[i-ampi_tape[i].arg[0]+j];
			    ampi_get_adj(&tmp_entry->idx, &tmp_d[j]);
			  } 
			}
			else {
			  for(j=0;j<ampi_tape[i].arg[0];j=j+1) tmp_d[j]=0;
			}
			 AMPI_Bcast_b(tmp_d, ampi_tape[i].arg[0], MPI_DOUBLE, root, comm);
			if(rank==root) {
			   for(j=0 ; j<ampi_tape[i].arg[0] ; j++) {
			     ampi_set_adj(&ampi_tape[i-ampi_tape[i].arg[0]+j].idx,&tmp_d[j]);
			   }
			}
			 free(tmp_d);
			 break;
		     }
	case REDUCE : {
			  if(ampi_tape[i].arg[2] == AMPI_REDUCE_ADD)
			      op = MPI_SUM;
			  if(ampi_tape[i].arg[2] == AMPI_REDUCE_MUL)
			      op = MPI_PROD;
			  if(ampi_tape[i].arg[2] == AMPI_REDUCE_MIN)
			      op = MPI_MIN;
			  if(ampi_tape[i].arg[2] == AMPI_REDUCE_MAX)
			      op = MPI_MAX;
			  tmp_d_send = (double*) malloc(sizeof(double)*ampi_tape[i].arg[0]);
			  tmp_d_recv = (double*) malloc(sizeof(double)*ampi_tape[i].arg[0]);
			  for(j=0;j<ampi_tape[i].arg[0];j=j+1)
				ampi_get_adj(&ampi_tape[i+1+j].idx, &tmp_d_recv[j]);
			  for(j=0;j<ampi_tape[i].arg[0];j=j+1) tmp_d_send[j]=0;

#ifdef NO_COMM_WORLD
			comm=ampi_tape[i].comm;
#endif
			  AMPI_Reduce_b(tmp_d_send, tmp_d_recv, ampi_tape[i].arg[0], MPI_DOUBLE, op, ampi_tape[i].arg[1], comm);
			  for(j=0 ; j<ampi_tape[i].arg[0] ; j++) {
			      ampi_set_adj(&ampi_tape[i-ampi_tape[i].arg[0]+j].idx,&tmp_d_send[j]);
			  }
			  free(tmp_d_send);
			  free(tmp_d_recv);
			  break;
		      }
	case ALLREDUCE : {
			  if(ampi_tape[i].arg[2] == AMPI_REDUCE_ADD)
			      op = MPI_SUM;
			  if(ampi_tape[i].arg[2] == AMPI_REDUCE_MUL)
			      op = MPI_PROD;
			  if(ampi_tape[i].arg[2] == AMPI_REDUCE_MIN)
			      op = MPI_MIN;
			  if(ampi_tape[i].arg[2] == AMPI_REDUCE_MAX)
			      op = MPI_MAX;
			  tmp_d_send = (double*) malloc(sizeof(double)*ampi_tape[i].arg[0]);
			  tmp_d_recv = (double*) malloc(sizeof(double)*ampi_tape[i].arg[0]);
			  for(j=0;j<ampi_tape[i].arg[0];j=j+1)
			      ampi_get_adj(&ampi_tape[i+1+j].idx, &tmp_d_recv[j]);
#ifdef NO_COMM_WORLD
			comm=ampi_tape[i].comm;
#endif

#ifdef DEBUG
			  printf("AMPI_Allreduce tmp_d_recv: ");
			  for(j=0 ; j<ampi_tape[i].arg[0] ; j++) {
			      /*if(tmp_d_recv[j]!=tmp_d_recv[j]) tmp_d_recv[j]=0;*/
			      printf("%e ",tmp_d_recv[j]);
			  }
			  printf("\n");
#endif
			  AMPI_Allreduce_b(tmp_d_send, tmp_d_recv, ampi_tape[i].arg[0], MPI_DOUBLE, op, comm);
#ifdef DEBUG
			  printf("AMPI_Allreduce tmp_d_send: ");
			  for(j=0 ; j<ampi_tape[i].arg[0] ; j++) {
			      printf("%e ",tmp_d_send[j]);
			  }
			  printf("\n");
#endif
			  for(j=0 ; j<ampi_tape[i].arg[0] ; j++) {
			      ampi_set_adj(&ampi_tape[i-ampi_tape[i].arg[0]+j].idx,&tmp_d_send[j]);
			  }
			  free(tmp_d_send);
			  free(tmp_d_recv);
			  break;
		      }
	case SCATTER : {
#ifdef NO_COMM_WORLD
			 comm=ampi_tape[i].comm;
#endif
			 int size=0;
			 int root=ampi_tape[i].arg[0];
			 int crecv=ampi_tape[i].arg[1];
			 int csend=ampi_tape[i].arg[2];
			 MPI_Comm_size(comm,&size);
			 double *sendbuf=(double*) malloc(sizeof(double)*csend*size);
			 double *recvbuf=(double*) malloc(sizeof(double)*crecv);
			 for(j=0;j<crecv;j++) {
			   ampi_get_adj(&ampi_tape[i+1+j].idx,&recvbuf[j]);
			 }
			 AMPI_Scatter_b(sendbuf,csend,MPI_DOUBLE,recvbuf,crecv,MPI_DOUBLE,root,comm);
			 for(j=0;j<csend*size;j++) {
			   ampi_set_adj(&ampi_tape[i-csend*size+j].idx,&sendbuf[j]);
			 }
			 free(sendbuf);
			 free(recvbuf);
			 break;
		       }
	case SCATTERV : {
			   break;
		       }
	case GATHER : {
#ifdef NO_COMM_WORLD
			 comm=ampi_tape[i].comm;
#endif
			 int size=0;
			 int rank=0;
			 int root=ampi_tape[i].arg[0];
			 int crecv=ampi_tape[i].arg[1];
			 int csend=ampi_tape[i].arg[2];
			 MPI_Comm_size(comm,&size);
			 MPI_Comm_rank(comm,&rank);
			 double *sendbuf=(double*)malloc(sizeof(double)*csend);
			 double *recvbuf=NULL;
			 if(rank == root) {
				 recvbuf = (double*)malloc(sizeof(double)*crecv*size);
				 for(j=0;j<crecv*size;j++) {
				   ampi_get_adj(&ampi_tape[i+1+j].idx,&recvbuf[j]);
				 }
			 }

			 AMPI_Gather_b(sendbuf,csend,MPI_DOUBLE,recvbuf,crecv,MPI_DOUBLE,root,comm);
			 for(j=0;j<csend;j++) {
			   ampi_set_adj(&ampi_tape[i-csend+j].idx,&sendbuf[j]);
			 }
			 free(sendbuf);
			 if(rank == root) {
				 free(recvbuf);
			 }
			   break;
		       }
	case GATHERV : {
			   break;
		       }
	case SENDRECVREPLACE : {
			tmp_d = (double*) malloc(sizeof(double)*ampi_tape[i].arg[0]);
			/*take adjoints out of the tape in the send buffer*/
#pragma omp parallel for private(tmp_entry)
			for(j=0;j<ampi_tape[i].arg[0];j=j+1) {
			    tmp_entry=&ampi_tape[i+j+1];
			    ampi_get_adj(&tmp_entry->idx, &tmp_d[j]);
#ifdef DEBUG
			    printf("SENDRECV_B: %d %e\n", tmp_entry->idx, tmp_d[j]);
#endif
			} 
#ifdef NO_COMM_WORLD
			comm=ampi_tape[i].comm;
#endif
			AMPI_Sendrecv_replace_b(tmp_d, ampi_tape[i].arg[0], MPI_DOUBLE, ampi_tape[i].arg[1], ampi_tape[i].tag, ampi_tape[i-1].arg[1], ampi_tape[i-1].tag, comm, &status);

	                /*two entries for sendrecvreplace, decrease tape index*/
	                i=i-1;
#pragma omp parallel for private(tmp_entry)
			for(j=0;j<ampi_tape[i].arg[0];j=j+1) {
			    tmp_entry=&ampi_tape[i-ampi_tape[i].arg[0]+j];
			    ampi_set_adj(&tmp_entry->idx, &tmp_d[j]);
#ifdef DEBUG
			    printf("SEND: %d %e\n", tmp_entry->idx, tmp_d[j]);
#endif
			}
			free(tmp_d);
			break;
			 }
	default: {
		     printf("Warning: Missing opcode in the AMPI tape interpreter for %d at tape index %lu.\n", ampi_tape[i].oc, i);
		     break;
		      }

    }
}
void ampi_reset_entry(long int idx){ 
  ampi_vac=idx;
}
void ampi_print_tape() {
    /* TODO error in printout */
    /*int i=0;*/
    /*for(i=ampi_vac;i>0;i--) {*/
    /*printf("IDX: %d, OC: %d, arg[0]: %d, arg[1]: %d\n", i-1,ampi_tape[i-1].oc, ampi_tape[i-1].arg[0], ampi_tape[i-1].arg[1]);*/
    /*}*/
}
void ampi_print_tape_entry(int *i) {
    /*printf("  -----------------------------------------\n");*/
    printf("             AMPI CALL: ");
    switch(ampi_tape[*i].oc){
	case SEND : {
			printf("SEND");
			break;
		    }
	case RECV : {
			printf("RECV");
			break;
		    }
	case IRECV : {
			printf("IRECV");
			break;
		    }
	case ISEND : {
			printf("ISEND");
			break;
		    }
	case WAIT : {
			printf("WAIT");
			break;
		    }
	case REDUCE : {
			printf("REDUCE");
			break;
		    }
	case ALLREDUCE : {
			printf("ALLREDUCE");
			break;
		    }
    }
    printf("\n");
    /*printf("  -----------------------------------------\n");*/
}

void ampi_check_tape_size(long int size) {
    if(ampi_vac>=ampi_chunks*AMPI_CHUNK_SIZE-size) {
	ampi_tape_entry *tmp;
        tmp=(ampi_tape_entry*) realloc(ampi_tape,(ampi_chunks+1)*AMPI_CHUNK_SIZE*sizeof(ampi_tape_entry));
	if(tmp != NULL) {
	    ampi_tape=tmp;
	    ampi_chunks=ampi_chunks+1;
	}
	else {
	    printf("AMPI tape allocation error.\n");
	}
    }
}
