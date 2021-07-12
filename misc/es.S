/* 
 * IOS Module Loader
 *   Written by TheLordScruffy
 *   Copyright (c) 2021 TheLordScruffy
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define syscall .word 0xE6000010 | 0x20 *

#define IOS_CreateThread             0x00
#define IOS_GetThreadId              0x03
#define IOS_StartThread              0x05
#define IOS_SuspendThread            0x06
#define IOS_CreateMessageQueue       0x0A
#define IOS_DestroyMessageQueue      0x0B
#define IOS_SendMessage              0x0C
#define IOS_ReceiveMessage           0x0E
#define IOS_RegisterResourceManager  0x1B
#define IOS_ResourceReply            0x2A
#define IOS_InvalidateDCache         0x3F
#define IOS_FlushDCache              0x40
#define IOS_LaunchElf                0x41
#define IOS_LaunchRM                 0x5A

#define VFILE_BASE                   0x11000000
#define VFILE_MAGIC                  0x46494C45
#define VFILE_MAXSIZE                0x01000000
#define FILE_MAGIC                   0x00
#define FILE_SIZE                    0x04
#define FILE_OPEN                    0x08
#define FILE_POS                     0x0C
#define FILE_DATA                    0x20

#define MSG_COMMAND                  0x00
#define MSG_RESULT                   0x04
#define MSG_FD                       0x08
#define MSG_OPEN_PATH                0x0C
#define MSG_OPEN_MODE                0x10
#define MSG_OPEN_UID                 0x14
#define MSG_OPEN_GID                 0x18
#define MSG_RW_DATA                  0x0C
#define MSG_RW_LENGTH                0x10
#define MSG_SEEK_WHERE               0x0C
#define MSG_SEEK_WHENCE              0x10
#define MSG_IOCTL_COMMAND            0x0C
#define MSG_IOCTL_IN                 0x10
#define MSG_IOCTL_IN_LEN             0x14
#define MSG_IOCTL_OUT                0x18
#define MSG_IOCTL_OUT_LEN            0x1C

#define IOS_OPEN                     1
#define IOS_CLOSE                    2
#define IOS_READ                     3
#define IOS_WRITE                    4
#define IOS_SEEK                     5
#define IOS_IOCTL                    6
#define IOS_IOCTLV                   7

#define IOS_OPEN_NONE                0
#define IOS_OPEN_READ                1
#define IOS_OPEN_WRITE               2
#define IOS_OPEN_RW                  (IOS_OPEN_READ | IOS_OPEN_WRITE)

#define SEEK_SET                     0
#define SEEK_CUR                     1
#define SEEK_END                     2

#define ISFS_STATS_SIZE              0x00
#define ISFS_STATS_POS               0x04

#define IOS_SUCCESS                  0
#define IOS_ENOENT                   -6
#define ISFS_EINVAL                  -101
#define ISFS_EACCESS                 -102

/* ---- ENTRY START ---- */
		.arm
		.text
		.global entryProc
		.fnstart
entryProc:
		stmfd   sp!, { r4-r6, lr }
		/* Create a simple message queue. */
		adr     r0, entryMsgQueue          @ ptr
		mov     r1, #1                     @ n_msgs
		syscall IOS_CreateMessageQueue
		cmp     r0, #0
		blt     entryEnd
		mov     r6, r0                     @ queueid = ret

		/* Create data thread. */
		mov     r0, #10                    @ priority
		mov     r1, #1                     @ detached
		str     r0, [sp, #0x0]
		str     r1, [sp, #0x4]
		adr     r0, deviceProc             @ proc
		mov     r1, r6                     @ arg
		adr     r2, deviceStack + 0x20     @ stack_top
		mov     r3, #0x20                  @ stack_size
		syscall IOS_CreateThread
		cmp     r0, #0
		blt     entryEndDeleteQueue
		mov     r5, r0                     @ tid = ret

		syscall IOS_StartThread
		cmp     r0, #0
		blt     entryEndDeleteQueue

		mov     r0, r6                     @ queueid
		adr     r1, entryMsg               @ msg
		mov     r2, #0                     @ flags
		syscall IOS_ReceiveMessage
		cmp     r0, #0
		blt     entryEndDeleteQueue

		adr     r1, entryMsg
		ldr     r0, [r1]
		cmp     r0, #0
		blt     entryEndDeleteQueue
		adr     r0, devicePath             @ path
		syscall IOS_LaunchRM
		
entryEndDeleteQueue:
		mov     r4, r0
		mov     r0, r6
		syscall IOS_DestroyMessageQueue
		mov     r0, r4
entryEnd:
		ldmfd   sp!, { r4-r6, lr }
		adr     r1, entryMsgQueue
		str     lr, [r1, #0]
		bx      lr

/* ------------------ */

entryMsgQueue:
		.space  0x4
entryMsg:
		.word   0x00000000

		.cantunwind
		.fnend
		.size   entryProc, . - entryProc
/* ---- ENTRY END ---- */

/* /dev/scruffy
 *
 * A device used as a substitute for a NAND file, since the kernel doesn't read
 * operating system files from anywhere. This is the simple read-only memory
 * implementation.
 * Memory base is 0x11000000. The first 4 characters must be "FILE", then there
 * should be a 32-bit value representing file size, and then extra memory space
 * for device work, until the file data starts 0x20 in.
 */

/* ---- DEVICE START ---- */
		.arm
		.text
		.global deviceProc
		.fnstart
deviceProc:
		stmfd   sp!, { r4-r7, lr }
		mov     r5, r0                     @ entry queueid
		ldr     r7, =VFILE_BASE

		ldr     r1, [r7, #FILE_MAGIC]
		ldr     r0, =VFILE_MAGIC
		cmp     r1, r0
		ldr     r0, =-500                  @ bad magic error (-500)
		bne     deviceFail

		ldr     r1, [r7, #FILE_SIZE]
		ldr     r0, =VFILE_MAXSIZE
		cmp     r1, r0
		ldr     r0, =-501                  @ file too large error (-501)
		bhs     deviceFail

		mov     r0, #0
		str     r0, [r7, #FILE_POS]

		/* Create data message queue. */
		adr     r0, deviceMsgQueue         @ ptr
		mov     r1, #8                     @ n_msgs
		syscall IOS_CreateMessageQueue
		cmp     r0, #0
		blt     deviceFail
		mov     r6, r0                     @ queueid = ret

		/* Register device as resource. */
		adr     r0, devicePath             @ device
		mov     r1, r6                     @ queueid
		syscall IOS_RegisterResourceManager
		mov     r4, r0                     @ err = ret
		cmp     r0, #0
		blt     deviceFailDeleteQueue

		mov     r1, #150
		mov     r0, r5
		mov     r2, #0
		syscall IOS_SendMessage

deviceMsgLoop:
		mov     r0, r6                     @ queueid 
		adr     r1, deviceMsg              @ msg
		mov     r2, #0                     @ flags
		syscall IOS_ReceiveMessage
		mov     r4, r0                     @ err
		cmp     r0, #0
		blt     deviceFailDeleteQueue
		adr     r1, deviceMsg
		ldr     r4, [r1]                   @ r4, msg

		ldr     r0, [r4, #MSG_COMMAND]
		cmp     r0, #IOS_OPEN
		beq     deviceOpen
		cmp     r0, #IOS_CLOSE
		beq     deviceClose
		cmp     r0, #IOS_READ
		beq     deviceRead
		cmp     r0, #IOS_WRITE
		beq     deviceWrite
		cmp     r0, #IOS_SEEK
		beq     deviceSeek
		cmp     r0, #IOS_IOCTL
		beq     deviceIoctl
		ldr     r1, =ISFS_EINVAL
		b       deviceMsgReply

deviceOpen:
		adr     r0, devicePath
		ldr     r1, [r4, #MSG_OPEN_PATH]
		bl      strcmp
		cmp     r0, #0
		ldreq   r1, =IOS_ENOENT
		beq     deviceMsgReply

		ldr     r0, [r4, #MSG_OPEN_MODE]
		cmp     r0, #IOS_OPEN_READ
		ldrne   r1, =ISFS_EACCESS
		bne     deviceMsgReply
		
		ldr     r1, [r4, #MSG_FD]
		b       deviceMsgReply

deviceClose:
		mov     r1, #0                     @ blank reply
		b       deviceMsgReply

deviceRead:
		add     r1, r7, #FILE_DATA
		ldr     r0, [r7, #FILE_POS]
		add     r12, r1, r0                @ read position
		ldr     r2, [r7, #FILE_SIZE]
		ldr     r3, [r4, #MSG_RW_LENGTH]

		add     r0, r3, r0
		cmp     r0, r2
		ldrhi   r1, =ISFS_EINVAL
		bhi     deviceMsgReply
		str     r0, [r7, #FILE_POS]        @ update position

		mov     r1, #0
		ldr     r2, [r4, #MSG_RW_DATA]
		/* Note: Length must be 4 byte aligned if reading to MEM1. */
deviceReadLoop:
		ldrb    r0, [r12, r1]
		strb    r0, [r2, r1]
		add     r1, r1, #1
		cmp     r1, r3
		blo     deviceReadLoop

		ldr     r1, [r4, #MSG_RW_LENGTH]
		b       deviceMsgReply

deviceWrite:
		ldr     r1, =ISFS_EACCESS
		b       deviceMsgReply

deviceSeekWCur:
		ldr     r0, [r7, #FILE_POS]
		add     r1, r1, r0
		b       deviceSeekBreak
deviceSeekWEnd:
		add     r1, r2, r1
		b       deviceSeekBreak
deviceSeek:
		ldr     r1, [r4, #MSG_SEEK_WHERE]
		ldr     r2, [r7, #FILE_SIZE]

		ldr     r0, [r4, #MSG_SEEK_WHENCE]
		cmp     r0, #SEEK_CUR
		beq     deviceSeekWCur
		cmp     r0, #SEEK_END
		beq     deviceSeekWEnd
deviceSeekBreak:
		cmp     r1, r2                     @ position < file size
		strls   r1, [r7, #FILE_POS]
		ldrhi   r1, =ISFS_EINVAL
		b       deviceMsgReply

deviceIoctl:
		ldr     r1, =ISFS_EINVAL
		ldr     r0, [r4, #MSG_IOCTL_COMMAND]
		cmp     r0, #0xB                   @ IOCTL GetFileStats
		bne     deviceMsgReply
		
		ldr     r0, [r4, #MSG_IOCTL_IN_LEN]
		cmp     r0, #0x0
		bne     deviceMsgReply
		ldr     r0, [r4, #MSG_IOCTL_OUT_LEN]
		cmp     r0, #0x8
		bne     deviceMsgReply
		
		mov     r1, #IOS_SUCCESS
		ldr     r2, [r4, #MSG_IOCTL_OUT]
		ldr     r0, [r7, #FILE_SIZE]
		str     r0, [r2, #ISFS_STATS_SIZE]
		ldr     r0, [r7, #FILE_POS]
		str     r0, [r2, #ISFS_STATS_POS]
		/* Fall through to reply. */

deviceMsgReply:
		mov     r0, r4                     @ request
		syscall IOS_ResourceReply
		b       deviceMsgLoop

deviceFailDeleteQueue:
		mov     r0, r6                     @ queueid
		syscall IOS_DestroyMessageQueue
		mov     r0, r4
deviceFail:
		mov     r1, r0                     @ message
		mov     r0, r5                     @ queueid
		mov     r2, #0                     @ flags
		syscall IOS_SendMessage

		/* Return to close the thread. */
		ldmfd   sp!, { r4-r7, lr }
		bx lr

/* Simple true or false strcmp. */
strcmp:
		sub     r2, r0, #1
		sub     r1, r1, #1
		mov     r0, #0
strcmpLoop:
		ldrb    r12, [r2, #1]!
		ldrb    r3,  [r1, #1]!
		cmp     r12, r3
		bxne    lr
		
		cmp     r12, #0
		moveq   r0, #1
		bxeq    lr
		b       strcmpLoop

/* ------------------ */

devicePath:
		.string "/dev/scruffy"
		.align  2
deviceMsgQueue:
		.space  0x20
deviceMsg:
		.word   0x00000000
deviceStack:
		.space  0x20

		.cantunwind
		.fnend
		.size   deviceProc, . - deviceProc
/* ---- DEVICE END ---- */
