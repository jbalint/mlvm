/** $RCSfile: queue.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: queue.c,v 1.13 2003/03/21 13:12:03 gautran Exp $ 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/************************************************/
/*	 	  queue.c                       */
/************************************************/

#include "system.h"

LOGPtr log_block = NULL;

void log_init(){  // initialize log pool 
    int i;
    LOGPtr temp;

    log_block = temp = free_logs->head = (LOGPtr) malloc(MAX_IMAGOES*sizeof(LOG));
    if (!temp){
	system_error("System LOG initialization", E_MEMORY_OUT);
	exit(-1);
    }
    for (i = 0; i < MAX_IMAGOES - 1; i++){
	temp->next = temp + 1;
	temp++;
    }
    temp->next = NULL;
    free_logs->tail = temp;
}
void log_cleanup() {
    if( log_block )
	free( log_block );
    log_block = 0;
}

LOGPtr log_new(){  // get a new log 
    int i;
    LOGPtr temp;
    pthread_mutex_lock(&(free_logs->lock));                   
    temp = free_logs->head;
    if (temp){
	if (temp->next){
	    free_logs->head = temp->next;
	}
	else {
	    free_logs->head = free_logs->tail = NULL;
	}
    }
    else { // expand free logs
	temp = free_logs->head = (LOGPtr) malloc(MAX_IMAGOES*sizeof(LOG));
	if (!temp){
	    system_error("System LOG initialization", E_MEMORY_OUT);
	    exit(-1);
	}
	for (i = 0; i < MAX_IMAGOES - 1; i++){
	    temp->next = temp + 1;
	    temp++;
	}
	temp->next = NULL;
	free_logs->tail = temp;	
	temp = free_logs->head;
	free_logs->head = temp->next;
    }
    pthread_mutex_unlock(&(free_logs->lock));                   
    return temp;
}

void log_free(LOGPtr ip){    // insert the log to the free pool 
    pthread_mutex_lock(&(free_logs->lock));                   
    ip->next = free_logs->head;
    if (free_logs->head == NULL){
	free_logs->tail = free_logs->head = ip;
    }
    else {
	free_logs->head = ip;
    }
    pthread_mutex_unlock(&(free_logs->lock));                   
}

void log_append(LOGPtr ip){    // append the log to live_logs 
    ip->next = NULL;
    if (live_logs->head == NULL){
	live_logs->head = ip;
    }
    else {
	live_logs->tail->next = ip;
    }
    live_logs->tail = ip;
}

LOGPtr log_find(long host_id, int application, char* name) {
    LOGPtr temp;
    temp = live_logs->head;
    while (temp){
	if( (temp->server.host_id == host_id) && 
	    (temp->application == application) &&
	    ((name == NULL) || (strcmp(temp->name, name) == 0)) ) 
	    break;
	temp = temp->next;
    }
    return temp;
}

void log_moved( LOGPtr log, int jumps, const char *to ) {
    log->status = LOG_MOVED;
    log->ip = NULL;
    strcpy(log->server.name, to);
    log->jumps = jumps;
    log->time = time(NULL);
}

LOGPtr log_alive( LOGPtr log, ICBPtr ip ) {
    LOGPtr llog;
    if( log == NULL ) llog = log_new();
    else llog = log;

    ip->log = llog;
    llog->application = ip->application;
    strcpy(llog->name, ip->name);
    llog->status = LOG_ALIVE;
    llog->server = ip->queen;
    llog->ip = ip;
    llog->jumps = ip->jumps;
    llog->time = time(NULL);
    if( log == NULL ) {
	log_append(llog);
    }
    return llog;
}
void log_disposed( LOGPtr log ) {
    log->status = LOG_DISPOSED;
    log->ip = NULL;
}
void log_blocked( LOGPtr log ) {
    log->status = LOG_BLOCKED;
}

void print_log( FILE *fp ) { // a temporary function for printing log records
    LOGPtr log;
  
    log = live_logs->head;
    fprintf( fp, "### LOG records begin:\n" );
    while (log) {
	fprintf( fp, "Application: %d, Imago: %s, Jumps: %d, Status: ", 
		 log->application, log->name, log->jumps );
	switch( log->status ) {
	case LOG_MOVED:
	    fprintf( fp, "Moved('%s')", log->server.name );
	    break;
	case LOG_ALIVE:
	    fprintf( fp, "Alive" );
	    break;
	case LOG_DISPOSED:
	    fprintf( fp, "Disposed" );
	    break;
	case LOG_BLOCKED:
	    fprintf( fp, "Blocked" );
	    break;
	case LOG_SLEEPING:
	    fprintf( fp, "Sleeping" );
	    break;
	default:
	    fprintf( fp, "(Unknown)" );
	    break;
	}
	fprintf( fp, "\n" );
	log = log->next;
    }
    fprintf( fp, "### LOG records end:\n" );
}


ICBPtr icb_block = NULL;

void icb_init(){   // initialize icb pool 
    int i;
    ICBPtr temp;

    icb_block = temp = free_queue->head = (ICBPtr) malloc(MAX_IMAGOES*sizeof(ICB));
    if (!temp){
	system_error("System ICB initialization", E_MEMORY_OUT);
	exit(-1);
    }
    memset( temp, 0, MAX_IMAGOES*sizeof(ICB) );

    for (i = 0; i < MAX_IMAGOES - 1; i++){
	pthread_mutex_init(&(temp->msq.lock), NULL);
	pthread_cond_init(&(temp->msq.cond), NULL);
	temp->next = temp + 1;
	temp++;
    }
    temp->next = NULL;
    free_queue->tail = temp;
}
void icb_cleanup() {
    if( icb_block )
	free( icb_block );
    icb_block = NULL;
}

ICBPtr icb_new(){
    ICBPtr temp;
    pthread_mutex_lock(&(free_queue->lock));                   
    temp = free_queue->head;
    if (temp){
	if (temp->next){
	    free_queue->head = temp->next;
	}
	else {
	    free_queue->head = free_queue->tail = NULL;
	}
    }
    temp->status = IMAGO_READY;
    temp->cin  = fileno(stdin);
    temp->cout = fileno(stdout);
    temp->cerr = fileno(stderr);
    pthread_mutex_unlock(&(free_queue->lock));
    return temp;
}

void icb_free(ICBPtr ip){
    if( pthread_mutex_trylock(&ip->lock) == 0 )
	debug("%s(%p): ICB mutex not locked.\n", __PRETTY_FUNCTION__, ip );
    ip->status = IMAGO_INVALID;
    /** Unlock the mutex after we changed the status to invalid. */
    pthread_mutex_unlock(&ip->lock);
    /** dealloacte memory block if any. */
    if (ip->block_base) free(ip->block_base);

    /** Clear up the structure for later use
	Do not clean EVERY thing... The ICB lock must remain valid all along. */
    memset( &ip->application, 0,  (char *)&ip->lock - (char *)&ip->application );
    ip->status = IMAGO_INVALID;

    pthread_mutex_lock(&(free_queue->lock));                   
    ip->next = free_queue->head;
    if (que_empty(free_queue)){
	free_queue->tail = free_queue->head = ip;
    }
    else {
	free_queue->head = ip;
    }
    pthread_mutex_unlock(&(free_queue->lock));                   
}

ICBPtr icb_first(icb_queue* iq){   // get the first icb from icb_queue 
    ICBPtr temp;

    temp = iq->head;
    if (temp){
	if (temp->next){
	    iq->head = temp->next;
	}
	else {
	    iq->head = iq->tail = NULL;
	}
    }
    return temp;
}

void icb_insert(icb_queue* iq, ICBPtr ip){ 
    // insert the icb to the front of icb_queue */

    ip->next = iq->head;
    if (iq->head == NULL){
	iq->tail = iq->head = ip;
    }
    else {
	iq->head = ip;
    }
}

void icb_append(icb_queue* iq, ICBPtr ip){    
    // append the icb to the end of icb_queue 

    ip->next = NULL;
    if (que_empty(iq)){
	iq->head = ip;
    }
    else {
	iq->tail->next = ip;
    }
    iq->tail = ip;
}

int icb_length(icb_queue* iq){
    int i = 0;
    ICBPtr temp = iq->head;
    while(temp){ i++; temp = temp->next; }
    return i;
}

void icb_remove(icb_queue* iq, ICBPtr ip){    
    // remove the icb from icb_queue 
    ICBPtr temp;

    if (ip == iq->head){ // the first one in the queue
	if (ip == iq->tail){ // singleton
	    iq->head = iq->tail = ip->next = NULL;
	    return;
	}
	else{ 
	    iq->head = ip->next;
	    ip->next = NULL;
	    return;
	}
    }

    temp = iq->head;
    while (temp && temp->next != ip) temp = temp->next;
    if (temp){
	temp->next = ip->next;
	ip->next = NULL;
	// in case the last one
	if (ip == iq->tail) iq->tail = temp; 
    }
}
