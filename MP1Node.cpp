/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions. (Revised 2020)
 *
 *  Starter code template
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node( Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = new Member;
    this->shouldDeleteMember = true;
	memberNode->inited = false;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member* member, Params *params, EmulNet *emul, Log *log, Address *address) {
    for( int i = 0; i < 6; i++ ) {
        NULLADDR[i] = 0;
    }
    this->memberNode = member;
    this->shouldDeleteMember = false;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {
    if (shouldDeleteMember) {
        delete this->memberNode;
    }
}

/**
* FUNCTION NAME: recvLoop
*
* DESCRIPTION: This function receives message from the network and pushes into the queue
*                 This function is called by a node to receive messages currently waiting for it
*/
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
* FUNCTION NAME: nodeStart
*
* DESCRIPTION: This function bootstraps the node
*                 All initializations routines for a member.
*                 Called by the application layer.
*/
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
    /*
    * This function is partially implemented and may require changes
    */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
	// node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
	initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == strcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
* FUNCTION NAME: finishUpThisNode
*
* DESCRIPTION: Wind up this node and clean up state
*/
int MP1Node::finishUpThisNode(){
    /*
     * Your code goes here
     */
}

/**
* FUNCTION NAME: nodeLoop
*
* DESCRIPTION: Executed periodically at each member
*                 Check your messages in queue and perform membership protocol duties
*/
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
    /*
    * Your code goes here
    */
   //TODO: where to add the introducer node to its membership list?
    Member * node = (Member *)env;
    MessageHdr * msg = (MessageHdr *)data;
    MsgTypes msgType = msg->msgType;

    switch (msgType) {
        case JOINREQ: handleJOINREQ(node, msg); break;
        case JOINREP: handleJOINREP(node, msg, size); break;
    }
}

void MP1Node::handleJOINREQ(Member *introducer, MessageHdr *msg) {
    // Add the requesting process to the introducer's membership list

    Address clientAddr = Address();
    char addr [6]; // array of 6 bytes
    long heartbeat;
    int id;
    short port;
    long timestamp;

    // If this node isn't part of its own list, add it to the list
    if (!selfIsPartOfMemberList()) {
        addSelfToMemberList();
    }

    // Copy data from message body to vars
    memcpy(&heartbeat, (char *)(msg+1) + 1 + sizeof(introducer->addr.addr), sizeof(long)); // get heartbeat from message
    memcpy(&addr, (char *)(msg+1), sizeof(introducer->addr.addr));
    memcpy(&clientAddr.addr, &addr, sizeof(addr));
    timestamp = time(NULL);
    addrToIDPort((char *)clientAddr.addr, &id, &port);
    // printf("id: %d\n", id);
    // printf("port: %d\n", port);
    // printf("%d.%d.%d.%d:%d\n", clientAddr.addr[0], clientAddr.addr[1], clientAddr.addr[2], clientAddr.addr[3], *(short *)&clientAddr.addr[4]);

    // Create new member list entry
    cout << id << " " << port << " " << heartbeat << " " << timestamp << "\n";
    MemberListEntry newMemberEntry = MemberListEntry(id, port, heartbeat, timestamp);

    // Add new member list entry to this process' membership list
    introducer->memberList.push_back(newMemberEntry);

    // Create the response message to let the requester know they have joined
    size_t msgsize = sizeof(MessageHdr) + sizeof(introducer->memberList);
    MessageHdr *outMsg = (MessageHdr *) malloc(msgsize * sizeof(char));
    outMsg->msgType = JOINREP;
    memcpy((char *)(outMsg+1), &introducer->memberList, sizeof(introducer->memberList));
    emulNet->ENsend(&introducer->addr, &clientAddr, (char *)outMsg, msgsize);
}

void MP1Node::handleJOINREP(Member *node, MessageHdr *msg, int size) {
    // Get membership list from message
    vector<MemberListEntry> *memberList = (vector<MemberListEntry> *) malloc(size - sizeof(MessageHdr));
    char *data = (char *) msg;
    memcpy(memberList, data + sizeof(MessageHdr), size - sizeof(MessageHdr));

    // Set this node's membership list
    node->memberList.insert(end(node->memberList), begin(*memberList), end(*memberList));

    // Let this node know it's in the membership group
    node->inGroup = true;

    // Find position of self in membeship list
    vector<MemberListEntry>::iterator iter;
    int nodeId;
    short nodePort;
    for (iter = node->memberList.begin(); iter != node->memberList.end(); iter++) {
        MemberListEntry entry = *iter;
        addrToIDPort((char *)node->addr.addr, &nodeId, &nodePort);
        if (entry.id == nodeId && entry.port == nodePort) {
            node->myPos = iter;
            break;
        }
    }

    // for (int i = 0; i < node->memberList.size(); i++) {
    //     MemberListEntry entry = memberList->at(i);
    //     log->LOG(&node->addr, "%dth entry; id:%d; port:%d; heartbeat:%d; timestamp:%d",
    //             i, entry.id, entry.port, entry.heartbeat, entry.timestamp);
    // }
}

bool MP1Node::selfIsPartOfMemberList() {
    if (memberNode->memberList.size() == 0) {
        return false;
    }

    vector<MemberListEntry>::iterator itr;

    for (itr = memberNode->memberList.begin(); itr != memberNode->memberList.end(); itr++) {
        MemberListEntry entry = *itr;
        int id; short port;
        addrToIDPort((char *) memberNode->addr.addr, &id, &port);
        if (
            id == entry.id && port == entry.port
        ) {
            return true;
        }
    }

    return false;
}

void MP1Node::addSelfToMemberList() {
    int id; short port; 
    addrToIDPort((char *) memberNode->addr.addr, &id, &port);
    MemberListEntry newMemberEntry = MemberListEntry(id, port, memberNode->heartbeat, time(NULL));
    memberNode->memberList.push_back(newMemberEntry);
    memberNode->myPos = memberNode->memberList.end();
}

void MP1Node::addrToIDPort(char *addr, int *id, short *port){
    memcpy(id, addr + 0, sizeof(int));
    memcpy(port, addr + 4, sizeof(short));
}


/**
* FUNCTION NAME: nodeLoopOps
*
* DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
*                 the nodes
*                 Propagate your membership list
*/
void MP1Node::nodeLoopOps() {
    
    /*
     * Your code goes here
     */

    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
