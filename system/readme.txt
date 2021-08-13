 Oct. 21, 2001

 set up IMAGO strategy:
 1) Posix threads for system threads.

 2) user-threads for imagoes.

 3) Memory: all-in-one block.

 Oct. 30, 2001 

 1) Design memory expansion algorithm.

 Feb. 28, 2002

 1) Discuss with Guillium about the architecture of MLVM
    and possible communication protocols.


 Mar. 14, 2002

 1) Discuss with Liang about the architecture of IMAGO-Prolog Compiler.

 June 17, 2002

 1) start to review and modify MLVM virtual machine.

 2) finished the pototypying of MLVM multithreading.

 June 30, 2002

 1) Multi imagoes are running, and memory expansion and GC worked.

 July 14 2002
 
 1) gcc on laptop can not use sizeof(f) where f is a global array.
 
 2) a problem should be considered later: during stack allocation,
    stack cells for vars are not initialized. Some of them will be
    set during head unification, however, some of them may be set
    later in successive calls. Thus, if memory expansion occurs,
    these uninitialized cells could hold any kind of values. There
    is no harm if values are CON, REF, or LIS, however, if a value
    is FVL (floating structure name), the memory manager will skip
    the next 2 cells without doing adjustment. A solution is to 
    initialize all cells before making a call.
    
 3) we should create another queue to hold imagoes which were failed
    in memory allocation, i.e., malloc or realloc can not find enough
    memory for use. Imagoes in this queue will be released to compete
    for memory when an imago is disposed or moved out (where memory
    block occupied by the dead imago will be freed).
    
 4) now, we should consider passing initial arguments to a newly
    created imago. Only one parameter can be passed through create/3,
    however, this argument could be anything of any type. A simple
    solution is to simulate garbage collection against only one root -
    the argument. The root is traversed and copied onto the stack of
    the new imago, and finally, this root is set on the top of the stack.
    The only problem is that the parent imago and the child imago are
    not sharing the same name database. If the parameter being passed 
    involves complicated structures and symbolic atoms, functors as well
    as symbolic names must be searched against the child's name database
    in order to make the constant (functor) terms uniquely indexed.
    The same function possibly will be adopted during handling messengers.
    Since parameters (or later symbolic messages) are not too huge in size,
    we use linear search at this stage. If later we found this could be
    inefficient, we change the search algorithm by that time.
    
    Another possible case in handling passing initial parameter (or later
    message) is that the symbol table (name database) overflow. If this
    happens, we need another memory expansion function. This function
    similar to the stack expansion function, should not be too hard to
    implement.

July 25, 2002

 1) Memory expansion and garbage collection have been tested by two
    benchmarks - qsort and serialise. 

 2) Imago creation and cloning have been tested and worked properly,
    however, messenger queue handling (upon clone) has not been implemented
    yet.

 3) Initial parameter passing has been implemented and newly created
    imagoes could access the parameter correctly.

 4) Now we are going to consider messenger dispatching. Two kinds of 
    messengers should be considered: system messenger which could be 
    invoked by stationary or worker, and user-defined messenger which
    could be created by stationary only.

 5) After the dispatching mechanism being implemented, we should consider
    the "attach" function, which includes how to find the receiving imago,
    how to access msg-queue of the receiver (mutual exclusive?), how to
    release a waiting imago, and how to deliver message, etc.

 6) Searching for a receiver is the key issue: we must maintain a log to
    remember all imagoes (workers + stationary) which are running or had 
    worked on this server. A data structure should include the following
    fields:
		application_id;
		imago_name;
		living_status (alive, moved, disposed);
		time_stamp  (for clear up );
		queen_host (server_name, ip_addr, port, ...);
		moved_host (if the imago has moved);
		icb_pointer (NULL if moved or disposed);
		next;		(chain pointer)


    A new entry is inserted into the log list when an imago is created,
    cloned, or moved-in. The corresponding entry will be modified if its
    logged imago is moved out or disposed, for the later case, the 
    icb_pointer field becomes NULL.
		

    Correspondly, we should add a log_pointer field to the ICB such that
    and imago could be able to modify its log status when something happen.
    
 7) A problem should be carefully invesgated is whether a lock is required
    for each imago. In IMAGO system, each imago is in fact a block of shared
    data among a set of system threads, such as engine, memory_manager, 
    communication_handler, etc. Without considering messengers, the pointer
    to an imago control block is hold solely by one of these system thread,
    and thus the ICB holder (the system thread) is free to access the imago
    data. This is already mutual-exclusive, because no more than one system
    thread can hold an ICB at the same time (Note that ICB's are handled
    mutual exclusively among different queues).

    However, when a messenger issues an 'attach' primitive, its receiver's
    state is unknown: it might be moved/disposed, or it might still alive
    (either in the hand of one of the system threads, or waiting to be
    scheduled in a system queue).  For the later case, the receiver's
    msg-queue must be protected.

July 26, 2002

 1) Added a lock in ICB structure. The logic of using the lock is as follows:

	messenger attach(receiver):
		lock log_queue;
		  search for receiver, r;
		  if (found){
			if (alive){
				lock r.msg_queue;
				insert the messenger;
				unlock r.msg_queue;
				if (receiver_waiting_for_msg)
					release the receiver;
				unlock log_queue;
				messenger becomes blocked;
			}
			else if (moved){
				bind result to moved(Where);
 			}
			else {  // disposed
				bind result to disposed;
 			}
		  else { // not found
				bind result to moved(queen);
 		  }
		unlock log_queue;
		return succeed;

	worker move(new_host):
		lock log_queue;
			change log_status to moved;
			change log_host to the new_host;
			lock msg_queue;
			for (each messenger in the queue){
				bind result to moved(new_host);
				set the messenger READY and insert to engine_queue;
			}
			unlock msg_queue;
			change log_ICBPtr to NULL;
		unlock log_queue;
		marshling and send the worker out;
		release resources;

	worker accept(receiver, msg):
		lock msg_queue;
			search for matching (receiver, msg);
		unlock msg_queue;
		if (found) return;
		else if (need_wait){
			insert this to waiting_queue;
		}
		else {
			return fail;
		}

  2) other functions such as create, clone, dispose, etc should also
     consider the maintainence of the log_queue.

  3) entries in log_queue will be removed if a system message is received
     requesting to delete entries of certain application, or out-dated
     entries will be eliminated (when)? When an entry is to be eliminated,
     we must check if the corresponding imago is still alive. If yes, we
     have to dispose it.

  4) each IPC holds a pointer to its log record, and each log record has
     a pointer to its recorded ICB (possibly NULL if moved or disposed).


July 29, 2002  

  1) log_queue facility has been added. Log entries could be appended
     during creation and cloning. Log status of a disposed imago could be
     charged by imago_out.

  2) all system queue related operations must be protected by the queue-lock,
     which have been done in existing functions. New functions should try to use
     existing macros or functions. 

  3) we do not log messengers, so log-related functions must check imago
     type. (has been done in imago_out on dispose).

  4) in our later version of imago remote server, we might consider to
     built up a log queue for each application. In this case, we search
     an imago by application first (no lock) and then aim to that application
     for other operations. This should be more efficient for remote server
     when thousands of imagoes of different applications are running. 

  5) now we shall consider messenger dispatching function. There are two
     kind of messengers: system-defined, and user-defined, where latter
     can be dispatched by stationary imago only. Each server should have
     a pool of system-defined messengers. As the size messenger code is 
     very small, we might use memory to preload these messengers and make
     the dispatching more fast (than loding from disk).

July 30, 2002

  1) Try to install a Linux server in office.

  2) Cross machine execution (nomoving) has been tested.

July 31, 2002

  1) Messenger dispatching takes three arguments: messenger_name, receiver,
     and message. Messenger_name is a file name which either refers to a
     system-defined messenger or a user-defined messenger. Here, we suppose
     that a system-defined messenger has a name of the form:

		$xxx  // starting with a $ and followed by a symbolic string
			  // such as $oneway_messenger

     where a user-defined messenger has a name of the form:

		yyy.lo // a symbolic string followed by .lo

  2) a messenger imago is anonymous. However, it will automatically carry
     the sender's name (the imago who dispatched it). Thus, we use the 'name'
     field of ICB structure to store the sender's name.

  3) builtin primitive 'dispatch' checks the legality of messenger to be
     created and inserts parent imago into creating_queue with three
     arguments in its stack (msgr_name, revr_name, msg).

  4) the dispatcher shall load the messenger into a memory block,
     allocate an ICB, copy sender's name , copy initial argument, and 
     make both parent imago and the newly created messenger imago ready
     to run.

  5) ** NOTE: copy argument should be careful. The dispatcher must combine
     the last two arguments into one list, that is [recr_name, msg], because
     we have defined that the arity of any imago is 1.
     In fact, this is very easy to implement:

	*(ip->st + 1) = makelist(ip->st - 1);  // make a new arg on stack top
	ip->st++;   // which groups two args as a list [recr_name, msg]
	copy_arg(ip, cip); // then copy the arg to the stack of messenger


Aug. 3, 2002

  1) Assembler has been modified from generating (type, code) pair to (code)
     directly and therefore the size of an .lo file could be reduced.
     
  2) What we need to do is to use instruction tab to load each instruction and
     based on the instruction type to load and do necessary address adjustment
     for each operand. In fact, the similar program has been used in clone and
     memory expansion.
     
  3) Another important problem is how to expand name_table. A name_table has 
     a small chunk of free space during loading an imago. However, this space
     could possibly be exhausted if too many dynamic constants being constructed
     during execution. For example, a messenger could carry a huge structured
     message which involves many distinct symbolic strings (constants); or
     a worker could receive such a huge message and the symbolic strings in
     the message were not in its name_table. To solve name_table expansion problem,
     we need re-layout the segmentations of the memory block of imago:
     
     -------------- <- block_ceiling
     | taril      |
     |------------|
     |            |
     |-----^------| <- stack_top
     |            |
     | stack      |
     |            |
     |------------| <- stack_base
     |            |
     |-----^------| <- name_top
     | name_table |
     |------------| <- name_base
     | code       |
     |------------|
     | proc_table |
     -------------- <- block_base
     
     New constants will be saved from name_top upto stack_base. If the space in 
     between has been exhausted, we should invoke a function to make an expansion.
     Instead of dynamically allocating a new block, we simply move the stack upward
     for a certain size (such as 256 bytes), whereas leave other segments, such as
     trail, code, proc_table, even the name_table unchanged. Advantages are:
     
     a) generally speaking, the free space above the current stack are large enough
     to allow such an expansion, this is because the whole block will be expanded
     whenever the free stack is found smaller than 1/3 of cache size, which is far
     larger than the expansion size of name_table. In other words, there is always
     enough free space to accomondate name_table expansion.
     
     b) for messenger imagoes, a common case where name_table should be expanded
     is the construction of a huge message. Fortunately, the stack to be moved at
     this time in almost empty, therefore no extra overhead for such an expansion.
     
     c) for other imagoes, name_table expansion happens mostly during receiving
     a huge message or accessing database. However, it is argued that no very many
     new names will be constructed in the above mentioned cases. For example, if a
     worker received a huge message, it must make an interpretation or analysis
     of the message. If so, most symbolic constants must have being used in its
     program, and therefore have already in its associated name_table. In other
     words, for a worker, its name_table will be expanded only if it inputs dynamic
     raw data (through messages or database queries) which carry a big amount
     distinct symbolic constants not found in its name_table. 
     
     d) during name_table expansion, no thread switch is required, only an in-line
     function will do the job. The complexity of this function is linear to the
     size of the active stack. Let newp and oldp be two pointers,
     
     	oldp = stack_top;
     	newp = stack_top = stack_top + NAME_EXP_SIZE;
     	
     	while (oldp >= stack_base){
     	
     		if (oldp - 2 > stack_base && is_fvl(tag(*(oldp - 2)))){
     			*newp-- = *oldp--;
     			*newp-- = *oldp--;
     			*newp-- = *oldp--;
     			continue;
     		}
     		else {
     			if (is_con(*oldp)){
     			  *newp-- = *oldp--;
     			  continue;
     			}
     			else{
     			  temp = *oldp--;
     			  *newp-- = (int)(addr(temp) + NAME_EXP_SIZE) | tag(temp);
     			  continue;
     			}
     		}
     	}
     	
     	// following this code, we need a few assignments to make sure that
     	// the imago's ICB has updated stack-related addresses.
     	
     	// Note: the following also must be done:
     	// 1) we need to scan the trail to adjust stack-pointers.
     	// 2) we have to scan cp-chain and bb-chain to re-adjust these code-pointers.
     	// the reason is very simple: these pointers are not stack pointers, so they
     	// should not be changed. However, during stack moving, the were modified
     	// by a simple algorithm without checking if they are code-pointers or
     	// stack-pointers, therefore after such a stack move, we has to restore
     	// their original value. Fortunately, a simple scan following CP chain
     	// and BB chain will do the job. ** this kind of readjustment is different
     	// from the CP and BB chain readjustment in memory_expansion algorithm.
     	
     	
     	As we are moving the stack from top to bottom, we have to look-ahead to
     	check whether the term pointed by oldp is a word of a floating value. If
     	yes, we must copy three successive words to destination directly.
     
Aug. 8, 2002

1) Have finished modification of assembler and loader such that byte code will
   be loaded based on instruction-type instead of a prefix type in .lo file.

2) new block loyout has been completed.

Aug. 9, 2002

1) Finished the name_table_expansion algorithm, some modifications have
   been made wrt previous thinking: instead of adjusting CP,TT's, we use
   a stack range to control the adjustment: any address beyond the range will
   not get adjusted. More details can be found in the creator package.

2) One testing has been made for the new name_table_expansion algorithm:
   when a new imago is created, we do not allocate extra space for name_table,
   and thus the creator invokes the name_table_expansion function. It is working.

Aug. 14, 2002

1) Search for quotes for IMAGO Lab Servers.

2) Finished functions for initializing builtin messengers. A builtin_msger
   table is initialized which include entries of each messenger, where each
   entry contains [file_name, messenger_name, ICBPtr]. If the entry file is 
   found, it is loaded and an ICB is allocated. Note: the block for every
   preloaded messenger does not include stack space. Because we only need
   the initial data to clone a messenger. 

Aug. 20, 2002

1) finished dispatching/3 as well as dispatcher function.

2) Oneway_messenger starts to execute and its initial arguments seem correct.

3) These parameters have successfully passed to attach/3 predicate.

4) IMAGO lab purchase order has been send out.

Aug. 21, 2002

1) Now I am considering how to implement "attach/3"

2) Installing licq and messenger on laptop

Aug. 22, 2002

1) when a messenger issues an attach/3, there are several possibles:

	a) the receiver can not be found from local log: bind moved(queen)

	b) found, however

	i) moved, bind moved(where),
	ii) disposed, bind deceased,
	iii) alive, insert the messenger into its msq,
	iv) blocked, insert the messenger into its msq, and set it ready

2) the attach/3 takes the following algorithm:

	lock_log;
	r = search for receiver;
	if (r == alive || r == blocked)
		lock_msq;
		insert this messenger into msq;
		unlock_msq;
		if (r == blocked) insert_receiver into ready_queue;
	}
	else {
		make binding;
 	}
	unlock_log;
	return;

Aug. 26, 2002

1) finished the first version of attach

2) tested a case: receiver not found at local host, binding to 'moved(queen)'

3) drafted 'wait_accept/2', without two space unification yet

4) to be tested: stationary imago creates a worker, and then dispatches a messenger
	worker: issues wait_accept/2

5) a problem: during testing 2), an error occurs. As the move/1 not implemented
yet, it makes the messenger recursively 'attach', and invokes memory-expansion,
for some unknown reason, memory not properly expanded, segmentation error.
This is to be investigated. *******

Aug. 27, 2002

1) working on wait_accept/2 

2) A case to be cared is that trailling operations have not be done on some bindings 
in imago predicates. A solution can be found in bp_arg/3. *******

3) A special case of the above problem is how to handle trailing operations against
two imago spaces. It is possible that bindings occurs in either side of the receiver
and the messenger. *******

Aug. 29, 2002

1) simple cases of wait_accept/2 have been implemented, that is, if either
receiver or messenger has a variable msg, then msg is copied to the corresponder's
space.

2) in order to solve problem 3) above, a simple solution is :
suppose R be message carried by the receiver, and M the message carried by the
messenger, and neither of them is a variable (a compound term), the algorithm
is:
	i) copy M to R's stack as M', invoke full unification algorithm to
	unify R and M', if succeed, goto ii), else, eliminate side effects
	during unification, and return error message.
	ii) copy R to M's space as R', unify M and R (this must succeed)
	return E_SUCCEED.

The advantage is that full unification is done within one imago's working
space. The shortcoming is that we have to do the second copy/unification if
the first succeeds.

However, the message mechanism of IMAGO is very powerful, users have the freedom
to accept a messenger selectively (based on the binding of the sender's name and
the message).

Example 1 (First-Come-First-Serve receiving):

		w(...) :- wait_accept(Who, Msg),
			// process Msg,
			w(...).


Example 2 (selective receiving):

		w(...) :- accept(Who, first_priority(M)), !,
			// select and process the first priority message,
			w(...).
		w(...) :- accept(my_buddy, Msg), !,
			// sellect and process Msg from my_buddy,
			w(...).
		w(...) :- wait_accept(Who, Msg),
			// accept any Msg from any imago Who,
			// process Msg,
			w(...).

Example 3 (priority receiving):

		w(...) :- wait_accept(Who, first_priority(M)), 
			// first select and process the first priority message,
			wait_accept(my_buddy, Msg),
			// then sellect and process Msg from my_buddy,
			other_processing(...).

A brief example of the Game of Life:

		:-worker(cell).

		cell(Neighbor) :-
			send_status(alive, 0, Neighbor),
			game(0, Neighbor, alive),
			dispose.

		game(_, _, dead).
		game(I, N, _) :-
			accept_neighbors(I, Status, 8),
			determine_my_status(My_status, Status),
			I1 is I + 1,
			send_status(My_status, N),
			game(I1, N, My_status).

		accept_neighbors(_, [], 0).
		accept_neighbors(I, [S|SL], N):-
			wait_accept(_, generation(I, S)),
			N1 is N - 1,
			accept_neighbors(I, SL, N1).

		send_status(_, _, []).
		send_status(S, I, [N|NL]):-
			dispatch("$oneway_messenger", N, generation(I, S)),
			send_status(S, I, NL).

		determine_my_status(Ms, Status):-
			// determine my status based on Status,
			// ommitted.

		:- end_worker(cell).
			
Aug. 30, 2002

1) finished wait_accept/2 and accept/2. they work beautifully.

2) finished trailing operation in all existing builtins.

3) finished sender/1, reset_sender/1.

4) coded a new system messenger cod_messenger, which carries a partially
instantiated msg to the receiver, and returns to the sender with a possibly
bound msg. the usage pattern is:

	imagoA :- Msg = msg(foo, X),
		dispatch($cod_messenger, imagoB, Msg),
		wait_accept(imagoB, Msg),
		show(X).

	imagoB :- Msg = msg(Y, bar),
		wait_accept(imagoA, Msg),
		show(Y).

the behavior of the pattern looks like a remote unification/synchronization.

The cod_messenger is working.

5) an interesting example is coded and working:

// :- stationary(m5).
//
// m1(M) :- create("m5_worker.lo", my_worker, 1000),
//	dispatch($oneway_messenger, my_worker, msg1),
//	dispatch($oneway_messenger, my_worker, msg2),
//	dispatch($oneway_messenger, my_worker, msg3),
//	dispatch($oneway_messenger, my_worker, msg4),
//	dispatch($oneway_messenger, my_worker, msg5),
//	dispatch($cod_messenger, my_worker, msg(hollo, buddy, [X|L])),
//	wait_accept(my_worker, M),
// 	write(M), nl,
//	terminate.
// :- end_stationary(m5).

// :- worker(m5_worker).
// m5_worker(_) :-
//	wait_accept(W, msg(X, Y, [a, b, c])),
//	write("X="),
//	write(X), nl,
//	write("Y="),
//	write(Y), nl,
// 	others(L),
//	write("MSGS=");
//	write(L), nl
//	dispose.
// others([M|L]) :- accept(_, M), !,
//	others(L).
// others([]).
// :- end_worker(m5_worker).

In this example, the queen creates my_worker, and then dispatches 5
oneway_messengers with messages msg1, ..., msg5, after that, it dispatches
a cod_messenger carring a msg(hollo, buddy, [X|L]), and then waiting
to receive the cod_messenger coming back. The final binding of M is
msg(hollo, buddy, [a, b, c]).

On the other hand, my_worker ingores messages carried by these 
oneway_messengers, eventhough whenever their arriving will trigger
the evaluation of wait_accept/2. my_worker waits until the message
from the cod_messenger arriving, which makes the bindings X = hollo,
and Y = buddy. When this is done, my_worker grabs all the messages
currently in its msg_queue to a list L, the final binding of L is
[msg5, msg4, .., msg1].

This example has been tested.

6) The summer is over. 

Sept. 6 2002

1) IMAGO Lab has been set up. Email from Guillaume:

Ok... I solved the extra processor mystery....
In fact, the Xeon processor is hyperthreading enable. It means the 
the CPU is internally duplicated (2 logical CPU within 1 physical).
The linux kernel 2.4.18 does not support such thing, but the 2.4.19 does ! ! !
So I upgraded the kernel on both machine and they now run great ! !
I thought you would like to know....

2) New students are coming. Topics to be investigated are:

	* system monitoring
	* security module
	* database interface
	* search engine interface
	* application examples
	* performance analysis
	* other languages

3) MLVM to be solved: 

	* waiting for memory queue (max expansion size?)
	* moving function and communication protocols
	* logging and searching (change log queuing and searching policy)
	* internal host-name structure (as a Prolog term)

Sept. 17, 2002

1) implementing memory waiting queue

<<<<<<< readme.txt
2) implementing release messengers: the function needs two parameters:
	a) ICBPtr ip: either a worker or a stationary imago whose msq will
		be released. From ip we can find its current status as well
		as its msq. The release operation will bind the returning var
		of each messenger in msq to different values: such as
		moved(xxx), cloned(xxx), deceased. The binding will depend on
		the current status of the imago.
	b) char* name: this gives the xxx part for set up a binding. For example,
		if a clone imago has a name "child", then the parent imago's (ip)
		msq is released by calling release_msger(ip, "child"), and thus
		all messengers current attached with the parent will be released
		with a binding "cloned(child)".

=======
2) implementing release messengers: the function needs two parameters:
	a) ICBPtr ip: either a worker or a stationary imago whose msq will
		be released. From ip we can find its current status as well
		as its msq. The release operation will bind the returning var
		of each messenger in msq to different values: such as
		moved(xxx), cloned(xxx), deceased. The binding will depend on
		the current status of the imago.
	b) char* name: this gives the xxx part for set up a binding. For example,
		if a clone imago has a name "child", then the parent imago's (ip)
		msq is released by calling release_msger(ip, "child"), and thus
		all messengers current attached with the parent will be released
		with a binding "cloned(child)".


Sept. 18, 2002

1) release_msger() has been coded but not tested yet.

2) Some interesting cites:

  CACM Sept. 2002 Vol 45 No 9. pp. 14

	Intelligent agent can carry out numerous decision-making and 
	problem-solving tasks. Any agent is an entity that carries out 
	activitives on behalf of its principal. A mobile agent is purposely
	separated from the location of its principal, in order to carry out
	remote tasks. In that capacity, the agent is best equipped with as
	much intelligence as possible, in order to autonomously represent
	and serve its principal. ... the challenge is between builting agents
	as light and mobile as possible, while making them as intelligent as 
	possible. This is absolutely correct. Unfortunately, developers are
	frequently tempted to add intelligence and thus sacrificing mobility.
	This is one of the reasons for the recent attempts to specicalize
	agents and to built agent coalitions that jointly complete remote tasks.
	Kowalczyk et al("InterMarket - Towards Intelligent Mobile Agent
	e-Market-places", in Proc. of the 9th IEEE conference and Workshop
	on the Engineering of Computer based Systems, Apr. 2002) propose
	a coalition between thin messenger agents and thicher decision-making
	agents, thus balancing "weight" and capability.

	In consideration of the trade-off between risks and rewards, XXX
	concludes that the risks outweigh the rewards. This is too general
	a conclusion. Provision of qualified access rights for different
	classes of users has been a long-standing principle for information
	systems design. Before the widespread use of the Web, few companies
	would have even considered giving remote end users direct access to 
	their transaction processing systems, whether for online banking,
	shopping, or information retrival. Now this is commonplace, dispite
	the risks inherent in this practice. Provision of similar rights to
	mobile agents may be only a matter of time and market demand.

	by Wagner and Turban, who wrote a article "Are Intelligent E-Commence
	Agents Partners or Predators?" ACAM May 2002


Sept. 19, 2002

1) testing release_msger with different cases:
	
	Grneral case: 
		stationsary: mx -> creating a worker, first dispatching
			5 oneway_messenger, and then a cod_messenger,
			waiting to accept the returned cod_messenger.

	a) m6_worker: after wait_accept the cod_messenger, dispose itself,
		which will cause the release of all the queued messengers,
		and they in turn dispose themself.

	b) m7_worker: after wait_accept the cod_messenger, clone itself,
		and then do accept/2, however, as messengers have been released
		during clone operation, it receives nothing ([]), and dispose
		itself. As a result, 10 messengers, 5 original, 5 clones, will
		try to re-attach to the original receiver and the cloned receiver
		respectively, but such attachment will fail (a "deceased" binding)
		thus they all dispose themself.

	c) m8_worker: after wait_accept the cod_messenger, clone itself,
		and then do wait_accept/2 five times. In this case, both
		the original worker and the cloned worker receive 5 messengers
		respectively and they all finish as expected.
>>>>>>> 1.2

Sept. 18, 2002

1) release_msger() has been coded but not tested yet.

2) Some interesting cites:

  CACM Sept. 2002 Vol 45 No 9. pp. 14

	Intelligent agent can carry out numerous decision-making and 
	problem-solving tasks. Any agent is an entity that carries out 
	activitives on behalf of its principal. A mobile agent is purposely
	separated from the location of its principal, in order to carry out
	remote tasks. In that capacity, the agent is best equipped with as
	much intelligence as possible, in order to autonomously represent
	and serve its principal. ... the challenge is between builting agents
	as light and mobile as possible, while making them as intelligent as 
	possible. This is absolutely correct. Unfortunately, developers are
	frequently tempted to add intelligence and thus sacrificing mobility.
	This is one of the reasons for the recent attempts to specicalize
	agents and to built agent coalitions that jointly complete remote tasks.
	Kowalczyk et al("InterMarket - Towards Intelligent Mobile Agent
	e-Market-places", in Proc. of the 9th IEEE conference and Workshop
	on the Engineering of Computer based Systems, Apr. 2002) propose
	a coalition between thin messenger agents and thicher decision-making
	agents, thus balancing "weight" and capability.

	In consideration of the trade-off between risks and rewards, XXX
	concludes that the risks outweigh the rewards. This is too general
	a conclusion. Provision of qualified access rights for different
	classes of users has been a long-standing principle for information
	systems design. Before the widespread use of the Web, few companies
	would have even considered giving remote end users direct access to 
	their transaction processing systems, whether for online banking,
	shopping, or information retrival. Now this is commonplace, dispite
	the risks inherent in this practice. Provision of similar rights to
	mobile agents may be only a matter of time and market demand.

	by Wagner and Turban, who wrote a article "Are Intelligent E-Commence
	Agents Partners or Predators?" ACAM May 2002


Sept. 19, 2002

1) testing release_msger with different cases:
	
	Grneral case: 
		stationsary: mx -> creating a worker, first dispatching
			5 oneway_messenger, and then a cod_messenger,
			waiting to accept the returned cod_messenger.

	a) m6_worker: after wait_accept the cod_messenger, dispose itself,
		which will cause the release of all the queued messengers,
		and they in turn dispose themself.

	b) m7_worker: after wait_accept the cod_messenger, clone itself,
		and then do accept/2, however, as messengers have been released
		during clone operation, it receives nothing ([]), and dispose
		itself. As a result, 10 messengers, 5 original, 5 clones, will
		try to re-attach to the original receiver and the cloned receiver
		respectively, but such attachment will fail (a "deceased" binding)
		thus they all dispose themself.

	c) m8_worker: after wait_accept the cod_messenger, clone itself,
		and then do wait_accept/2 five times. In this case, both
		the original worker and the cloned worker receive 5 messengers
		respectively and they all finish as expected.


Sept. 20, 2002

1) waiting_mem_queue facilities have been added, that is, whenever a dynamic
	memory request fails (at creation, dispatching, and memory expansion),
	the requesting imago is put into the waiting_mem_queue.
	On the other hand, whenever an imago finishes (dispose, terminate, error,
	or move_out), one of the waiting imago will be released to compete
	for the memory. There is no guaranttee that it will get its required space,
	if not, it will be re-appended to the waiting_mem_queue again.

2) We may set up a quote for each waiting imago, for say, 5. That is, if an imago
	being released 5 times and still not being able to get what it required,
	we beleave that it might ask too much, and change its status to
	IMAGO_ERROR, and dispose it.

3) This facility has not been tested yet, we need a program to generate 1000+
	imagoes to do the testing.

4) Another problem needs to be re-investigated is related with memory expansion.
	Consider the following case: when a new imago is created/dispatched,
	an initial argument will be passed to this imago. The question is how
	about the argument is too huge to fit into the space allocated to
	this imago? This problem mostly happens when a messenger is dispatched
	with a huge amount of message. A possible solution is to check the
	size of the argument and pass the size as one of the parameters to
	determine the initial block size. This function has the form of:

		int arg_size(int term, ICBPtr ip){
		int size = 0, root, i;
		int* stk = ip->st + 1;
		int* btk = stk;

 			*stk++ = term;  // push(term);
			while(stk > btk ){ // !empty_stack()
				term = *(--stk); // term = pop();
				deref(root, term);
				if (is_int(root)) size++;
				else if (is_con(root)) size += 2;
				else if (is_list(root)){
					*stk++ = *addr(root); // push(term.head);
					*stk++ = *(addr(root) + 1); // push(term.tail);
				}
				else if (is_float(root)) size += 4;
				else { // a structure
					for (i = 1; i <= arity(root); i++)
						*stk++ = *(addr(root) + i); // push(term[i])
				}
			}
			return size;
		}

	And then pass the size to load_xxx_file(...) which will allocate
	the initial block plus additional size for the imago.


	The same problem also arises in other cases, such as accept/2 and 
	wait_accept/2. If it happens, the solution is a little bit more 
	complicated.  we not only need to check the receiver side, but also
	need to check the messenger, because the receiving process involves
	full unification, i.e., a huge data could be at the either side.

	Possible solution is:
	
	a) call s1 = arg_size(messenger.arg), 
		s2 = arg_size(receiver.arg),
	and varify whether the current free stacks of both imagoes is big enough 
	to hold the message. If one or both need memory expansion, then
	roll back program pointers and insert them to memory_queue asking for memory
	expansion. After expansion, the accept/wait_accept, or attach will be 
	re-executed, and there is enough space to continue.

5) The current implementation has a bug in dealing with messenger memory expansion
	and GC. We might need a special policy for messengers.

Sept. 23, 2002

1) I have tested waiting_mem_queue by reducing MAX_IMAGOES to a small amount, such as 
	7, and tested with benchmark qsort() which creates 10 workers, and thus results
	in the case of waiting for ICB. It works, that is, a waiting creator will be 
	released upon the termination of a worker. 

	A minor problem happenned during testing. As we initialize the number of log
	records by the MAX_IMAGOES, the logs are not enough, thus a temporary log
	expansion algorithm is added.

2) The testing arises a problem to be revisited. In MLVM, there are two dynamic
	spaces to be manipulated: ICB/LOG and imago blocks.

	Imago blocks are allocated purely dynamically under the requests of
	creation, dispatching, move_in, and cloning. 

	An imago block will be freed in the cases of error, disposing and move_out.

	However, when a new imago is to be generated, we must first get a free ICB
	to control the life of the imago. Therefore, we has the following problems:

	a) ICB's run out.
	b) Block allocation fails.

	When an IMAGO system starts, a constant amount of ICB's is initialized and
	a free_icb queue is formed. The questions are: 
 
		i) Do we insert a creator into the waiting_mem_queue when there is 
		no more ICB? 

		Ans 1: No, instead,  a simple solution is to return an error code 
		to the creator, and thus makes the creator being disposed.

		Ans 2: Yes, so the creator could be released when another imago
		terminates. But this solution bears a risk that the whole system
		deadlocked. For example, imagoes are all waiting for some messengers
		whereas there is no ICB's to let messengers get in.

		ii) Do we create more ICB's when needed? if so, how about the try
		fails?

		Ans: No.
	
Sept. 27, 2002

1) Another problem related with imago creation is that a log record must be allocated
	to remember the trace of the imago. When the imago disposes itself, its ICB is
	returned to the free pool, however, its log is still in the log queue. Logs will
	be discarded only when its corresponding application terminates. Thus we have a
	question, how about there is no free log available upon an imago creation?

	Possible solutions are:

	a) report an error (not good).

	b) allocate more logs dynamically (still not good, how about no more memory?)

	c) free all LOG_DISPOSED logs (no matter which application), except that the
	stationary server can not do so, because it must maintain the complete log
	of its representing application.  If nothing could be freed, then start to
	free all LOG_MOVED logs. It can be proved that there is no harm to free these
	kinds of logs on a remote imago server, however, only efficiency of look_up
	is lower. For example, let A be a worker who has moved to another server.
	A messenger looking for A can immediately move to that server if we have
	A's log, however, if the log is discarded, we have to move the messenger to
	its stationary server first, and then the messenger will be redirected to
	that server. 

	From this discussion, we have to make sure that the stationary server must
	maintain an updated log, that is, whenever a log is freed, it must be registed
	with the stationary server.

	The final question is that how about we still gain nothing after the above
	freeing process? If we initialize #_log_records >= #_ICB's, then we ganranttee
	that an imago gets a log if it has been allocated an ICB, because it is
	impossible that more than number of ICB's imagoes ALIVE at the same time.
	
	
Oct. 2, 2002

1) arg_size() has been added, which is called to get the size of initial argument
	and this size will be used as additional amount for initial block allocation.
	The purpose of this function is to accomendate huge message size when a 
	messenger is dispatched.

2) two tests should be designed for dispatching messengers which carry a big message.

	Test1:
		stationary		worker
		create		->	
					wait_accept
		dispatch	->	qsort(L)
				<-	dispatch(L)
		wait_accept		dispose
		display
		terminate


	Test2:

		stationary		worker
		create		->
					wait_accept
		dispatch	->	random(L)
					clone		-> 	cloned worker
					qsort(L)		serialis(L)
					dispatch(L1)		dispatch(L2)
		wait_accept(LA)		dispose			dispose
		wait_accept(LB)
		display(LA & LB)
		terminate

Oct. 4, 2002

1) Test1 has been done, working good.

2) Test2 seems has a problem: the problem is related with time (or ME?)
	When tested with small msg's, it works. However, when two huge
	msg's are attached, (in different order, both in, one-in-one-not yet)
	a segment fault occurs.

3) As the matter of fact, the above mentioned error is not caused by time
	nor ME, instead, it is a bug in copy_arg() function. During copying,
	we first dereferencing a var, and then check its type, however, I
	did not store the dereferenced value back to the destination, thus
	leave the root as an old address.
<<<<<<< readme.txt

Oct. 15, 2002

1) CVS system has been set to be the version control system of IMAGO.

2) I should continue MLVM in two directions:

	a) try more examples to make sure the engine bug-free

	b) separate versions of Imago-remote-server and Imago-stationary-server.

Oct. 16 - Oct. 30, 2002

1) collecting more papers and conference information.

2) How to implement call/1

	*st: the goal of the form g/n(arg1, arg2, ..., argn)

	a) check the proc_table for g/n for its entry pg
	b) simulate put operation to set up calling arguments
	c) change pp to gp (shall we save pp as cp??)
	d) return to engine

3) IMAGO system can simulate RPC/RMI and still allow both caller and callee
to move as they wish.

	for example: we define:
	
	rpc(Callee, Goal):-
		dispatch($oneway_messenger, Callee, Goal),
		wait_accept(Callee, Goal).


	Thus, from the callers side:

		rpc(stack, push(Item)),

		rpc(stack, pop(Item)),

	and on the callee side:

	on_call(L) :-
		wait_accept(Caller, Goal),
		process(L, L1, Goal),
		dispatch($oneway_messenger, Caller, Goal).
		on_call(L1).

	process([], [Item], push(Item)):-!.
	process(L, [Item|L], push(Item):-!.
	process([], [], pop([])):-!.
	process([Item|L], L,  pop(Item)):-!.

	
=======

Oct. 15, 2002

1) CVS system has been set to be the version control system of IMAGO.

2) I should continue MLVM in two directions:

	a) try more examples to make sure the engine bug-free

	b) separate versions of Imago-remote-server and Imago-stationary-server.
>>>>>>> 1.4
