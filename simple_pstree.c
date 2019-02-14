#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include "simple_pstree.h"
#define NETLINK_TEST 25
#define STRING_MAX 20
#define KMAX 1000000
struct sockaddr_nl user_addr, dest_addr;
// iovec buffer
/* struct iovec{
    void* iov_base;  : starting address
    size_t iov_len;  : number of bytes to transfer
}*/
struct iovec iov;
// netlink msg requires the existence of this header
// an application must supply this header in each netlink message
struct nlmsghdr* nl;
char para[STRING_MAX];
int CreateSocket(void)
{
    return socket(AF_NETLINK,SOCK_RAW,NETLINK_TEST);
}

char* ReadFromUser(int argc,char* input)
{
    //// Original input length
    //// printf("Length of input:%ld\n",strlen(input));
    // init of global para
    memset(para,0,STRING_MAX*sizeof(char));
    if(argc==1) return strcat(para,"c 1");
    else {
        // c or p or s?
        strncpy(para,input+1,1);
        ////printf("para after copy c or p or s: %s\n",para);
        if(strlen(input)>2) {
            char pidstr[STRING_MAX];
            memset(pidstr,0,sizeof(pidstr));
            memmove(pidstr,input+2,strlen(input)-2);
            ////printf("pidstr: %s\n",pidstr);
            strcat(para," ");
            strcat(para,pidstr);
        } else {
            strcat(para," ");
            char tmp[10];
            if(para[0]=='c') sprintf(tmp,"%d",1); // pid omitted: init
            else sprintf(tmp,"%d",getpid());      // pid omitted: simple_pstree
            ////printf("%s\n",tmp);
            strcat(para,tmp);
        }
    }
    //// printf("Result string before leaving ProcessMsg: %s\n",para);
    return para;
}
int main(int argc, char const *argv[])
{
    // Create a user socket
    int sock=CreateSocket();
    if(sock<0) printf("socket create error\n");
    // initializd to 0 for default
    memset(&user_addr,0,sizeof(struct sockaddr_nl));
    user_addr.nl_family=AF_NETLINK;
    user_addr.nl_pid=getpid(); // get pid
    user_addr.nl_groups=0; // not in multicast group, only unicast
    memset(&dest_addr,0,sizeof(struct sockaddr_nl));
    dest_addr.nl_family=AF_NETLINK;
    dest_addr.nl_pid=0; // if destination is kernel, this should be 0
    dest_addr.nl_groups=0; // not in multicast group, only unicast
    // bind(int sockfd,const struct sockaddr_nl*,socklen_t addrlen)
    // bind() assigns the address specified by addr to the sockfd.
    // addrlen specifies the size, in bytes, of the address structure pointed to by addr
    bind(sock,(struct sockaddr*)&user_addr,sizeof(struct sockaddr_nl));
    // Kernel requires each netlink message to include Netlink message header
    // Netlink message is combined of header and message payload
    // An application allocates a buffer long enough to store both header and payload
    // The starting of the buffer holds the netlink message and it's followed by the payload
    char tmp[STRING_MAX];
    memset(tmp,0,sizeof(tmp));
    if(argc>1) strcpy(tmp,argv[1]);
    ReadFromUser(argc,tmp);
    //// FinalResult length
    //// printf("Message with length: %s with strlen() %ld\n",para,strlen(para));
    // msgSize= the size of message in bytes
    int msgSize=sizeof(para);
    // NLMSG_SPACE returns the number of bytes that a netlink message with payload of len will occupy
    nl=(struct nlmsghdr*)malloc(NLMSG_SPACE(msgSize));
    // Fill the netlink message header
    nl->nlmsg_len=NLMSG_SPACE(msgSize); // Length of message, including header and payload
    nl->nlmsg_pid=getpid();
    nl->nlmsg_flags=0;
    memcpy(NLMSG_DATA(nl),para,msgSize); // copy actual message into the payload
    //// What is inside actual message
    //// printf("What is inside NLMSG_DATA(nl): %s\n",(char*)NLMSG_DATA(nl));
    // The buffer is passed to netlink core by iovec structure
    // Buffer starting point, which is nlmsghd
    iov.iov_base=(void*) nl; // iov_base holds address of the netlink message buffer
    iov.iov_len=nl->nlmsg_len; // iov_len holds length of the buffer, which is header+payload
    // Supply netlink address to the struct msghdr msg for the sendmsg()
    struct msghdr msg;
    // memset(&msg,0,sizeof(struct msghdr));
    /* msg_control and msg_controllen must be initialized; otherwise sendmsg() won't work */
    /* Either initialized them manually or memset() whole msghdr first */
    msg.msg_control=NULL;
    msg.msg_controllen=0;
    msg.msg_name=(void*)&dest_addr; // Information of dest address
    msg.msg_namelen=sizeof(struct sockaddr_nl); // Length of which is pointed by msg_name
    msg.msg_iov=&iov; // The address of iovec structure which holds address of the buffer
    msg.msg_iovlen=1; // Only one buffer


    if(sendmsg(sock,&msg,0)<0) printf("Not works!\n");
    ////printf("WAIT FOR KERNEL\n");

    // Enlarge msg buffer for kernel response
    free(nl);
    nl=(struct nlmsghdr*)malloc(KMAX*sizeof(struct nlmsghdr));
    nl->nlmsg_len=NLMSG_SPACE(KMAX*sizeof(struct nlmsghdr)); // Length of message, including header and payload
    iov.iov_base=(void*)nl;
    iov.iov_len=nl->nlmsg_len;
    recvmsg(sock,&msg,0);
    printf("%s",(char*)NLMSG_DATA(msg.msg_iov->iov_base));
    free(nl);
    close(sock);
}
