#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <linux/pid.h>
#include <linux/list.h>
#include <linux/sched.h>
#define NETLINK_TEST 25
#define STRING_MAX 15
#define KMAX 1000000
static struct sock* kernelSock;
char finalResult[KMAX];
int count;
static void process_name_children(struct task_struct*, struct list_head*,int);
static void PrintProcess(struct task_struct** task)
{
    printk(KERN_INFO "Process name(pid): %s(%d)\n",(*task)->comm,(*task)->pid);
}

static void PrintThreadProcess(struct task_struct** task)
{
    printk(KERN_INFO "Thread process name(pid): %s(%d)\n",(*task)->comm,(*task)->pid);
}

/**
* Strcat() finalResult with a string with the form of process_name(process_pid)
* @param task Reference of pointer pointing to the target process which is a struct task_struct
* @return No return value given
*/
static void process_name(struct task_struct** task)
{
    char tmp[45];
    sprintf(tmp,"%s(%d)\n",(*task)->comm,(*task)->pid);
    strcat(finalResult,tmp);
}



static void process_name_thread_for_children(struct task_struct* leader,int count)
{
    int localCount=0;
    struct list_head* iter=NULL;
    if(!leader) {
        printk(KERN_ERR "group leader is null\n");
        return;
    }
    list_for_each(iter,&leader->thread_group) {
        struct task_struct* p=list_entry(iter,struct task_struct,thread_group);
        while(localCount++ < count)
            strcat(finalResult,"    ");
        localCount=0;
        process_name(&p);
    }
}
/**
*
* Explanation:  If task->pid is 1, it means it is init() system call, so concat it to finalResult directly.
*               If task->pid>0, then we call process_name_parent() recursively until init()
*               If task->pid<0, then return(No such process exsits)
*               Maintain a golbal count as counter for the number of spaces.
*               If the process is nth child of init, it should have nth spaces, using count as n.
*
* @param task Reference of pointer pointing to the target process which is a struct task_struct.
* @return No return value given.
*/

static void process_name_parent(struct task_struct* task)
{
    if(task->pid==1) ;  // init, strcat the string directly without 4 spaces
    else if(task->pid>0) { // keep going
        process_name_parent(task->parent);
        int i=0;
        while(i++<count)
            strcat(finalResult,"    ");
    } else return;
    process_name(&task);
    count++;
}

static void process_name_sibling(struct task_struct* leader,struct list_head* tmphead,int targetPid)
{
    struct list_head* iterator=NULL;
    struct task_struct* siblingptr=NULL;
    struct task_struct* siblingThreadptr=NULL;
    list_for_each(iterator,tmphead) {
        siblingptr=list_entry(iterator,struct task_struct,sibling);
        if(!siblingptr) printk(KERN_ERR "No sibling.\n");
        // Target process is not the sibling of itself.
        if(siblingptr->pid != targetPid) process_name(&siblingptr);
    }
    list_for_each(iterator,&leader->thread_group) {
        siblingThreadptr=list_entry(iterator,struct task_struct,thread_group);
        process_name(&siblingThreadptr);
    }
}

static void process_name_children(struct task_struct* leader,struct list_head* tmphead,int count)
{
    int localcount=0;
    struct list_head* iterator=NULL;
    struct task_struct* childptr=NULL;
    list_for_each(iterator,tmphead) {
        childptr=list_entry(iterator,struct task_struct,sibling);
        while(localcount++<count) // count = how many ansectors
            strcat(finalResult,"    ");
        localcount=0;
        process_name(&childptr);
        process_name_children(childptr->group_leader,&childptr->children,count+1); // Finding children
    }
    process_name_thread_for_children(leader,count); // Parent's thread is my siblings
}
static void FindParent(int targetPid)
{
    count=0; // global count
    struct pid* targetPidStruct=NULL;
    struct task_struct* task=NULL;
    targetPidStruct=find_get_pid(targetPid); // retrun the struct pid of given pid number
    if(!targetPidStruct) {
        printk(KERN_ERR "No such Process with pid: %d\n",targetPid);
        return;
    }
    task=pid_task(targetPidStruct,PIDTYPE_PID); // Find the task_struct of given struct pid
    // >0 means until sysemd
    process_name_parent(task);
}


static void FindSibling(int targetPid)
{
    struct pid* targetPidStruct=NULL;
    struct task_struct* task=NULL;
    struct list_head* listptr=NULL;
    targetPidStruct=find_get_pid(targetPid); // retrun the struct pid of given pid number
    if(!targetPidStruct) {
        printk(KERN_ERR "No such Process with pid: %d\n",targetPid);
        return;
    }
    task=pid_task(targetPidStruct,PIDTYPE_PID); // Find the task_struct of given struct pid
    PrintProcess(&task);
    listptr=&task->parent->children;
    // This line will make head point to sibling field of target task
    // But the list will go back to parent't children field
    // listptr=&task->sibling;
    if(!listptr) {
        printk(KERN_ERR "No siblings\n");
        return;
    }
    process_name_sibling(task->parent->group_leader,listptr,targetPid); // target's parent's threads are target's siblings
}
static void FindChildren(int targetPid)
{
    struct pid* targetPidStruct=NULL;
    struct task_struct* task=NULL;
    struct list_head* listptr=NULL;
    targetPidStruct=find_get_pid(targetPid); // retrun the struct pid of given pid number
    if(!targetPidStruct) {
        printk(KERN_ERR "No such Process with pid: %d\n",targetPid);
        return;
    }
    task=pid_task(targetPidStruct,PIDTYPE_PID); // Find the task_struct of given struct pid
    process_name(&task); // Put the target process at the first line
    process_name_children(task->group_leader,&task->children,1);
    printk(KERN_INFO "Final result of children: %s",finalResult);
}
static int ProcessMsgPid(void* message)
{
    char str[STRING_MAX];
    int pid;
    memset(str,0,sizeof(str));
    memcpy(str,message+2,strlen(message)-1);
    //
    sscanf(str,"%d",&pid);
    printk(KERN_INFO "Print pid inside ProcessPidMsg() as an integer: %d\n",pid);
    return pid;
}

static void sendMsg(int len,int pid,void* message)
{
    struct sk_buff* skbtmp;
    struct nlmsghdr* nl;
    // Alloc new empty sk_buff, GFP_KERNEL means allocate normal kernel RAM
    skbtmp=alloc_skb(len,GFP_KERNEL);
    if(!skbtmp) printk(KERN_ERR "alloc sk_buffer error!");
    nl=nlmsg_put(skbtmp,0,0,0,len,0); // Add new message to sk_buff with size len
    if(!nl) printk(KERN_ERR "nlmsg_put error!\n");
    NETLINK_CB(skbtmp).portid=0;
    NETLINK_CB(skbtmp).dst_group=0;
    memcpy(NLMSG_DATA(nl),message,len);
    netlink_unicast(kernelSock,skbtmp,pid,MSG_DONTWAIT);
}

static void receiveMsg(struct sk_buff* skb)
{
    memset(finalResult,0,sizeof(finalResult)); // global finalResult
    struct sk_buff* skbtmp;
    struct nlmsghdr* nl;
    skbtmp=skb_get(skb); // Return the reference of the sk_buffer
    nl=nlmsg_hdr(skb); // nlmsg_hdr() returns the actual netlink message
    // NLMSG_DATA(nl) returns the void pointer points to the payload area of the message
    // memcpy(): copy the sizeof(str) bytes from location pointed to by NLMSG_DATA(nl) to str
    printk(KERN_INFO "wtf is message: %s\n",NLMSG_DATA(nl));
    int targetPid=ProcessMsgPid(NLMSG_DATA(nl));
    char message[nl->nlmsg_len];
    strcpy(message,(char*)NLMSG_DATA(nl));
    if(message[0]=='p') FindParent(targetPid);
    else if(message[0]=='s') FindSibling(targetPid);
    else FindChildren(targetPid);
    printk("Final: %s\n",finalResult);
    sendMsg(KMAX,nl->nlmsg_pid,finalResult);
}


static __init int ksimple_init(void)
{
    struct netlink_kernel_cfg cfg;
    memset(&cfg,0,sizeof(struct netlink_kernel_cfg));
    cfg.input=receiveMsg; // The function binding to the socket
    kernelSock=netlink_kernel_create(&init_net,NETLINK_TEST,&cfg);
    return 0;
}



static __exit void ksimple_exit(void)
{
    netlink_kernel_release(kernelSock);
}

module_init(ksimple_init);
module_exit(ksimple_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wang0630");
