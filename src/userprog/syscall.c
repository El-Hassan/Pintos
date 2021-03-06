#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

struct lock filesys_lock;

static void syscall_handler(struct intr_frame *);

void syscall_init(void) {
	lock_init(&filesys_lock);
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");

}

void halt(void) {
//printf ("this is halt!\n");
	shutdown_power_off();
}

void exit(int status) {
	closeAllOpenFiles();
	thread_current()->st = status;
	printf("%s: exit(%d)\n", thread_current()->file_name, status);
	thread_exit();
}

int exec(const char *cmd_line) {
	if (!validate_pointer(cmd_line) || !cmd_line) {
		exit(-1);
	}
	if (thread_current()->depth > 30)
		return -1;

	int child_pid = process_execute(cmd_line);

	if (child_pid == -1)
		return -1;
	struct thread *child = get_thread_from_all(child_pid);

	child->depth = thread_current()->depth + 1;
	child->parent_exec = thread_current();

	sema_down(&thread_current()->wait_exec);

	if (thread_current()->child_sucess)
		return child_pid;
	return -1;
}

int wait(int pid) {
	return process_wait(pid);
}

// Creates a new file called file initially initial_size bytes in size
bool create(const char *file, unsigned initial_size) {
	if (!validate_pointer(file) || !file) {
		exit(-1);
	}
	lock_acquire(&filesys_lock);
	bool s = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return s;

}

// Deletes the file called file
bool remove(const char *file) {
	if (!validate_pointer(file) || !file) {
		exit(-1);
	}
	lock_acquire(&filesys_lock);
	bool s = filesys_remove(file);
	lock_release(&filesys_lock);
	return s;
}

// Opens the file called file
int open(const char *file) {
	if (!validate_pointer(file) || !file) {
		exit(-1);
	}
	lock_acquire(&filesys_lock);
	struct file *spFile = filesys_open(file);
	if (spFile == NULL) {
		lock_release(&filesys_lock);
		return -1;
	}
	//file_deny_write(spFile);
	addNewFd(thread_current()->fileDescriptor, spFile);
	int sp = thread_current()->fileDescriptor;
	(thread_current()->fileDescriptor)++;
	lock_release(&filesys_lock);
	return sp;
}

// Returns the size, in bytes, of the file open as fd
int filesize(int fd) {
	lock_acquire(&filesys_lock);
	if (fd == 1 || fd == 0) {
		//error
		lock_release(&filesys_lock);
	}
	int s = file_length(getFile(fd));
	lock_release(&filesys_lock);
	return s;
}

// Reads size bytes from the file open as fd into buffer
int read(int fd, void *buffer, unsigned size) {
	lock_acquire(&filesys_lock);
	if (!validate_pointer(buffer) || !buffer) {
		lock_release(&filesys_lock);
		exit(-1);
	}

	if (fd == 0) {
		char *sp = buffer;
		int i = 0;
		for (i = 0; i < size; i++) {
			*(sp + i) = input_getc();
		}
		lock_release(&filesys_lock);
		return size;
	}
	if (fd == 1) {
		lock_release(&filesys_lock);
		exit(-1);
	}
	struct file *sp = getFile(fd);
	if (sp == NULL) {
		lock_release(&filesys_lock);
		return -1;
	}
	int s = file_read(sp, buffer, size);
	lock_release(&filesys_lock);
	return s;
}

// Writes size bytes from buffer to the open file fd
int write(int fd, const void *buffer, unsigned size) {
	lock_acquire(&filesys_lock);
	if (!validate_pointer(buffer) || !buffer) {
		lock_release(&filesys_lock);
		exit(-1);
	}
	if (fd == 1) {
		putbuf(buffer, (size_t) size);
		lock_release(&filesys_lock);
		return size;
	}
	if (fd == 0) {
		lock_release(&filesys_lock);

	}
	int s = file_write(getFile(fd), buffer, size);
	lock_release(&filesys_lock);
	return s;
}

// Changes the next byte to be read or written in open file fd to position, expressed in bytes from the beginning of the file
void seek(int fd, unsigned position) {
	lock_acquire(&filesys_lock);
	file_seek(getFile(fd), position);
	lock_release(&filesys_lock);
}

// Returns the position of the next byte to be read or written in open file fd, expressed in bytes from the beginning of the file
unsigned tell(int fd) {
	lock_acquire(&filesys_lock);
	if (fd == 0 || fd == 1) {
		//error
		lock_release(&filesys_lock);
		return 0;
	}
	unsigned s = file_tell(getFile(fd));
	lock_release(&filesys_lock);
	return s;
}

// Closes file descriptor fd
void close(int fd) {
	lock_acquire(&filesys_lock);
	file_close(getFile(fd));
	closeFile(fd);
	lock_release(&filesys_lock);

}

// read from the stack then call the right system call and pass to it the arguments required
static void syscall_handler(struct intr_frame *f) {
	int *top_of_stack = f->esp;
	if (!validate_pointer(top_of_stack) || !top_of_stack) {
		exit(-1);
	}

	switch (*top_of_stack) {
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		if (!(validate_pointer(top_of_stack + 1)))
			exit(-1);
		exit(*(top_of_stack + 1));
		break;
	case SYS_EXEC:
		if (!(validate_pointer(top_of_stack + 1)))
			exit(-1);
		f->eax = exec(*(top_of_stack + 1));
		break;
	case SYS_WAIT:
		if (!(validate_pointer(top_of_stack + 1)))
			exit(-1);
		f->eax = wait(*(top_of_stack + 1));
		break;
	case SYS_CREATE:
		if (!(validate_pointer(top_of_stack + 1)
				&& validate_pointer(top_of_stack + 2)))
			exit(-1);
		f->eax = create(*(top_of_stack + 1), *(top_of_stack + 2));
		break;
	case SYS_REMOVE:
		if (!(validate_pointer(top_of_stack + 1)))
			exit(-1);
		f->eax = remove(*(top_of_stack + 1));
		break;
	case SYS_OPEN:
		if (!(validate_pointer(top_of_stack + 1)))
			exit(-1);
		f->eax = open(*(top_of_stack + 1));
		break;
	case SYS_FILESIZE:
		if (!(validate_pointer(top_of_stack + 1)))
			exit(-1);
		f->eax = filesize(*(top_of_stack + 1));
		break;
	case SYS_READ:
		if (!(validate_pointer(top_of_stack + 1)
				&& validate_pointer(top_of_stack + 2)
				&& validate_pointer(top_of_stack + 3)))
			exit(-1);
		f->eax = read(*(top_of_stack + 1), *(top_of_stack + 2),
				*(top_of_stack + 3));
		break;
	case SYS_WRITE:
		if (!(validate_pointer(top_of_stack + 1)
				&& validate_pointer(top_of_stack + 2)
				&& validate_pointer(top_of_stack + 3)))
			exit(-1);
		f->eax = write(*(top_of_stack + 1), *(top_of_stack + 2),
				*(top_of_stack + 3));
		break;
	case SYS_SEEK:
		if (!(validate_pointer(top_of_stack + 1)
				&& validate_pointer(top_of_stack + 2)))
			exit(-1);
		seek(*(top_of_stack + 1), *(top_of_stack + 2));
		break;
	case SYS_TELL:
		if (!(validate_pointer(top_of_stack + 1)))
			exit(-1);
		f->eax = tell(*(top_of_stack + 1));
		break;
	case SYS_CLOSE:
		if (!(validate_pointer(top_of_stack + 1)))
			exit(-1);
		close(*(top_of_stack + 1));
		break;
	default:
		printf("ERROR IN SYSCALL NUMBER!\n");
	}
}
// check the validitiy of the pointer on the stack
int validate_pointer(void *p1) {
	int address = p1;
	bool pass = true;
	int ps = PHYS_BASE;
	if (address == 0 || address == ps)
		return false;
	if ((address <= 0 && ps >= 0)) {
		return false;
	} else if ((address > 0 && ps > 0) || (address < 0 && ps < 0)) {
		if (address >= ps) {
			pass = false;
		}
	}
	return pass;
}
