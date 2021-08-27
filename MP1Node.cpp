/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
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
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
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
 * 				All initializations routines for a member.
 * 				Called by the application layer.
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
	char *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(JOINREQ) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (char*) malloc(msgsize * sizeof(char));
        if (msg == nullptr) {
            cerr << 1000 << endl;
        }

        // create JOINREQ message: format of data is {messageType address heartbeat}
        msg[0] = JOINREQ;
        memcpy((msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif
        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
void MP1Node::finishUpThisNode() {
   delete(emulNet);
   delete(log);
   delete(par);
   delete(memberNode);
   failedMembers.clear();
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
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
}

vector<string> MP1Node::splitString(string str, char delim) {
    vector<string> pieces;
    string piece;
    stringstream sd(str);

    while(getline(sd, piece, delim)) {
        pieces.push_back(piece);
    }

    return pieces;
}

/**
 * FUNCTION NAME: getMemberListEntry
 *
 * DESCRIPTION: Returns a pointer to the MemberListEntry with corresponding id and port
 */
MemberListEntry* MP1Node::getMemberListEntry(int id, int port) {
    vector<MemberListEntry>::iterator position;
    for(position = memberNode->memberList.begin(); position < memberNode->memberList.end(); position++) {
        if (position->id == id && position->port == port) {
            return &(*position);
        }
    }
    return nullptr;
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
}

/**
 * FUNCTION NAME: clear
 *
 * DESCRIPTION: Assigns all elements of an array to NULL
 */
void MP1Node::clear(char* arr, size_t len) {
    for (int i = 0; i < len; i++) {
        arr[i] = '\0';
    }
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
void MP1Node::recvCallBack(void *env, char *msg, int size) {
    if (msg[0] == JOINREQ) {
        char *addr = (char*) malloc(sizeof(memberNode->addr.addr) + 1);
//        if (addr == NULL) {
//            cerr << 2000 << endl;
//        }
//        clear(addr, sizeof(memberNode->addr.addr) + 1);
        memcpy(addr, msg + 1, sizeof(memberNode->addr.addr));
        int id;
        short port;
        memcpy(&id, &addr[0], sizeof(int));
		memcpy(&port, &addr[4], sizeof(short));

        long heartbeat;
        memcpy(&heartbeat, msg + 1 + sizeof(memberNode->addr.addr) + 1, sizeof(long));
        
        memberNode->memberList.push_back(MemberListEntry(id, port, heartbeat, par->getcurrtime()));
        sendMembershipList(to_string(id) + ":" + to_string(port));
//        free(addr);
    } 
    else if(msg[0] == JOINREP || msg[0] == GOSSIP) {
        size_t contentSize = strlen(msg) - 1;
        char *content = (char*)malloc(contentSize * sizeof(char) + 1);
//        if (content == NULL) {
//            cerr << 1000 << endl;
//        }
//         clear(content, contentSize * sizeof(char) + 2);
        memcpy(content, msg + 1, contentSize * sizeof(char));

        string contentString = content;
        vector<string> pieces = splitString(contentString, ',');

        for(int i = 0; i < pieces.size(); i++) {
            vector<string> pieceInfo = splitString(pieces[i], ':');
            cerr << "hhh:"  << endl;

            int id = stoi(pieceInfo[0]);
            cerr << "here" << endl;
            short port = stoi(pieceInfo[1]);
            cerr << "dee" << endl;
            int heartbeat = stoi(pieceInfo[2]);
            cerr << "heefefre" << endl;
            // int id, port, heartbeat;


            MemberListEntry* memberListEntry = getMemberListEntry(id, port);
            if (memberListEntry == nullptr) {
                memberNode->memberList.push_back(MemberListEntry(id, port, heartbeat, par->getcurrtime()));
            }
            else if (memberListEntry->getheartbeat() < heartbeat) {
                memberListEntry->setheartbeat(heartbeat);
                memberListEntry->timestamp = par->getcurrtime();
            }
//            free(content);
        }
    }
}

/**
 * FUNCTION NAME: isMemberFailed
 *
 * DESCRIPTION: Check if the member list with given index in membership list is failed
 */
bool MP1Node::isMemberFailed(int index) {
    for(int i = 0; i < failedMembers.size(); i++) {
        if(index == failedMembers[i]) {
            return true;
        }
    }
    return false;
}

/**
 * FUNCTION NAME: getMembershipListString
 *
 * DESCRIPTION: Produces the string of the entire membership list
 */
string MP1Node::getMembershipListString() {
    string membershipString = "";

    int id;
    short port;
    memcpy(&id, &memberNode->addr.addr[0], sizeof(int));
    memcpy(&port, &memberNode->addr.addr[4], sizeof(short));
    membershipString += to_string(id) + ":" + to_string(port) + ":" + to_string(memberNode->heartbeat);
    if (memberNode->memberList.size() > 0) membershipString += ",";

    vector<MemberListEntry>::iterator position;
    for(position = memberNode->memberList.begin(); position < memberNode->memberList.end(); position++) {
        string memberString = "";
        memberString += to_string(position->getid()) + ":";
        memberString += to_string(position->getport()) + ":";
        memberString += to_string(position->getheartbeat());

        membershipString += memberString += ((position != memberNode->memberList.end() - 1) ? "," : "");
    }
    return membershipString;
}

/**
 * FUNCTION NAME: sendMembershipList
 *
 * DESCRIPTION: Sends the entire membership list to other nodes
 */
void MP1Node::sendMembershipList(string destination) {
    string membershipList = getMembershipListString();
//    cerr << "wewqq---" << membershipList << endl;
//    size_t messageSize = 1 + membershipList.size();
    string msg = "0" + membershipList;
//    clear((char*)msg, (messageSize) * sizeof(char));
//    memcpy(msg + 1 + 1, membershipList.c_str(), messageSize - 1);

    if (destination.empty()) {
        vector<MemberListEntry>::iterator position;
        for(position = memberNode->memberList.begin(); position < memberNode->memberList.end(); position++) {
            string address = to_string(position->getid());
            address += ":";
            address += to_string(position->getport());

            // size_t messageSize = sizeof(MessageHdr) + sizeof(membershipList.c_str());
            // MessageHdr* msg = (MessageHdr*) malloc((messageSize + 2) * sizeof(char));
            // clear((char*)msg, (messageSize + 2) * sizeof(char));
            msg[0] = GOSSIP;
            // memcpy((char*)(msg+1) + 1, membershipList.c_str(), sizeof(membershipList.c_str()));
        
            emulNet->ENsend(&(memberNode->addr), new Address(address), msg);
        }
    }
    else {
            // size_t messageSize = sizeof(MessageHdr) + sizeof(membershipList.c_str());
            // MessageHdr* msg = (MessageHdr*) malloc((messageSize + 2) * sizeof(char));
            msg[0] = JOINREP;
            // memcpy((char*)(msg+1) + 1, membershipList.c_str(), sizeof(membershipList.c_str()));
            

            emulNet->ENsend(&(memberNode->addr), new Address(destination), msg);
    }
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
    this->memberNode->heartbeat++;
	
    for (int i = 0; i < failedMembers.size(); i++) {
        int failedMemberIndex = failedMembers[i];
        if ((par->getcurrtime() - memberNode->memberList[failedMemberIndex].gettimestamp()) > (TFAIL + TREMOVE)) {
            memberNode->memberList.erase(memberNode->memberList.begin() + failedMemberIndex);
            failedMembers.erase(failedMembers.begin() + i);
            i--;
        }
    }

    vector<MemberListEntry>::iterator position;
    for(position = memberNode->memberList.begin(); position < memberNode->memberList.end(); position++) {
        int index = distance(memberNode->memberList.begin(), position);
        if ( (par->getcurrtime() - position->gettimestamp()) > TFAIL && !isMemberFailed(index) ) {
            // Mark them as failed
            failedMembers.push_back(index);
        }
    }

    sendMembershipList("");
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

    memset(&joinaddr, 0, sizeof(Address));
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
