// Project 2: Shashank Shekhar, sshekha4; Pranav Gaikwad, pmgaikwa
//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "file_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

// defines a task
typedef struct task_node {
    __u64 id;
    struct task_struct *task_pointer;
    struct list_head task_list;
} TaskNode;

// defines a container
// contains list of tasks and list of allocated memory objects
typedef struct container_node {
    __u64 id;
    int num_tasks;
    int num_objects;
    TaskNode t_list;
    struct mutex task_lock;  // local lock for operations on tasks' list
    struct list_head c_list;
} ContainerNode;

// a global lock on list of containers
static DEFINE_MUTEX(container_lock);

// pointer to head of container list
// similar to initializing a linked list
struct list_head container_list_head = LIST_HEAD_INIT(container_list_head);

/**
 * Returns 'container id' in kernel mode from user mode struct
 * @param  user_cmd Command from user mode
 * @return          Container id as an llu
 */
__u64 _get_container_id_in_kernel(struct file_container_cmd __user *user_cmd) {
    __u64 cid;
    struct file_container_cmd *buf = (struct file_container_cmd*)kmalloc(sizeof(*user_cmd), GFP_KERNEL);
    copy_from_user(buf, user_cmd, sizeof(*user_cmd));
    cid = buf->cid;
    kfree(buf);
    return cid;
}

/**
 * Returns 'container id' in kernel mode from user mode struct
 * @param  user_cmd Command from user mode
 * @return          Container id as an llu
 */
int _get_pid_in_kernel(struct file_container_cmd __user *user_cmd) {
    int pid;
    struct file_container_cmd *buf = (struct file_container_cmd*)kmalloc(sizeof(*user_cmd), GFP_KERNEL);
    copy_from_user(buf, user_cmd, sizeof(*user_cmd));
    pid = buf->pid;
    kfree(buf);
    return pid;
}

/**
 * Returns container with given id
 * @param cid Container id
 */      
void* _get_container(__u64 cid) {
    struct list_head *c_pos, *c_q;
    list_for_each_safe(c_pos, c_q, &container_list_head) {
        ContainerNode *temp_container = list_entry(c_pos, ContainerNode, c_list);
        if (temp_container->id == cid) {
            return temp_container;
        }
    }
    return NULL;
}

/**
 * Checks whether given container exists
 * @param  cid Container Id
 * @return     1 if exists, 0 if does not exist
 */
int _container_exists(__u64 cid) {
    ContainerNode* container = (ContainerNode*)_get_container(cid);
    if(container != NULL) {
        return 1;
    }
    return 0;
}

/**
 * Adds new container with given container id to list of containers
 * @param cid Container Id
 */
void _add_new_container(__u64 cid) {
    ContainerNode *new_container;
    new_container = (ContainerNode*)kmalloc(sizeof(ContainerNode), GFP_KERNEL);
    new_container->id = cid;
    new_container->num_tasks = 0;
    // initialize task list and lock 
    mutex_init(&new_container->task_lock);
    INIT_LIST_HEAD(&((new_container->t_list).task_list));
    // add new container to the container list
    list_add(&(new_container->c_list), &container_list_head);
}

/**
 * Registers given container in container list
 * Checks whether given container already exists, 
 * if not, creates a new container & adds it to list
 * @param cid Container Id
 */
void _register_container(__u64 cid) {    
    // Do not create new container if it exists already
    if (_container_exists(cid)) {
        return;
    }    

    mutex_lock(&container_lock);
    _add_new_container(cid);
    mutex_unlock(&container_lock);
}

/**
 * Checks whether given task exists in given container
 * @param  tid task id
 * @param  cid Container id
 * @return     1 if exists, 0 if does not exist
 */
int _task_exists(__u64 cid, __u64 tid) {
    struct list_head *t_pos, *t_q;
    ContainerNode *temp_container = (ContainerNode*)_get_container(cid);

    if (temp_container != NULL) {
        list_for_each_safe(t_pos, t_q, &(temp_container->t_list).task_list) {
            TaskNode *temp_task = list_entry(t_pos, TaskNode, task_list);
            if (temp_task->id == tid) {
                return 1;
            }
        }
    }
    return 0;
}

/**
 * Adds new task to given container
 * @param cid Container id 
 * @param tid task id
 */
void _add_new_task(__u64 cid, struct task_struct *task_ptr) {
    TaskNode *new_task_node;
    ContainerNode *new_container;

    new_container = (ContainerNode*)_get_container(cid);
    
    if (new_container != NULL) {
        new_task_node = (TaskNode*)kmalloc(sizeof(TaskNode), GFP_KERNEL);
        new_task_node->id = task_ptr->pid;
        new_task_node->task_pointer = task_ptr;
        list_add_tail(&(new_task_node->task_list), &((new_container->t_list).task_list));
        new_container->num_tasks = new_container->num_tasks + 1;    
    }
}

/**
 * Finds the container node which contains given task
 * @param  tid Task id
 * @return     Container Node
 */
void* _find_container_containing_task(int tid) {
    ContainerNode *temp_container;
    struct list_head *c_pos, *c_q, *t_pos, *t_q;

    list_for_each_safe(c_pos, c_q, &container_list_head){
        temp_container = list_entry(c_pos, ContainerNode, c_list);
        list_for_each_safe(t_pos, t_q, &(temp_container->t_list).task_list) {
            TaskNode *temp_task = list_entry(t_pos, TaskNode, task_list);
            if (temp_task->id == tid) {
                return temp_container;
            }
        }
    }
    return NULL;
}

/**
 * Associates given task with given container
 * Checks whether given task already exists in given container, 
 * if not, creates a new entry for given task in the container
 * @param cid Container id 
 * @param tid Task id
 */
void _register_task(__u64 cid, struct task_struct *task_ptr) {
    ContainerNode *container;
    // Do not add new task if it already exists
    if (_task_exists(cid, task_ptr->pid)) {
        return;
    }

    container = (ContainerNode*)_get_container(cid);

    if (container != NULL) {
        mutex_lock(&container->task_lock);
        _add_new_task(cid, task_ptr);
        mutex_unlock(&container->task_lock);
    }
}

/**
 * Removes task from given container
 */
void _deregister_task_from_container(pid_t tid) {
    ContainerNode *temp_container;
    struct list_head *c_pos, *c_q, *t_pos, *t_q;

    list_for_each_safe(c_pos, c_q, &container_list_head){
        temp_container = list_entry(c_pos, ContainerNode, c_list);
        list_for_each_safe(t_pos, t_q, &(temp_container->t_list).task_list) {
            TaskNode *temp_task = list_entry(t_pos, TaskNode, task_list);
            if (temp_task->id == tid) {
                temp_container->num_tasks = temp_container->num_tasks - 1;
                list_del(t_pos);
                kfree(temp_task);
            }
        }
    }
}

/**
 * Prints all containers
 */
void print_containers(void) {
    struct list_head *c_pos, *c_q;
    list_for_each_safe(c_pos, c_q, &container_list_head) {
        struct list_head *t_pos, *t_q;
        ContainerNode *temp_container = list_entry(c_pos, ContainerNode, c_list);
        printk("------ Container %llu ------\n", temp_container->id);
        list_for_each_safe(t_pos, t_q, &(temp_container->t_list).task_list) {
            TaskNode *temp_task = list_entry(t_pos, TaskNode, task_list);
            printk("task %llu\n", temp_task->id);
        }
        printk("-------------------------\n");
    }
}


int file_container_delete(struct file_container_cmd __user *user_cmd)
{
    _deregister_task_from_container(current->pid);
    
    return 0;
}

int file_container_create(struct file_container_cmd __user *user_cmd)
{
    __u64 cid = _get_container_id_in_kernel(user_cmd);
    
    _register_container(cid);

    _register_task(cid, current);

    return 0;
}

int file_container_get_container_id(struct file_container_cmd __user *user_cmd)
{
    int pid = _get_pid_in_kernel(user_cmd);

    ContainerNode *container = (ContainerNode*)_find_container_containing_task(pid);

    print_containers();

    if (container != NULL) return container->id;
 
    return -1;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int file_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case FCONTAINER_IOCTL_CREATE:
        return file_container_create((void __user *)arg);
    case FCONTAINER_IOCTL_GETCID:
        return file_container_get_container_id((void __user *)arg);
    case FCONTAINER_IOCTL_DELETE:
        return file_container_delete((void __user *)arg);
    default:
        return -ENOTTY;
    }
}