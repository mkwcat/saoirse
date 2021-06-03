#include <util.h>
#include <types.h>

#include <ios.h>
#include <es.h>

#define IOCTL_SHA1_INIT 0
#define IOCTL_SHA1_UPDATE 1
#define IOCTL_SHA1_FINAL 2

typedef struct _SHA1Context
{
	u32 state[5];
	u32 count[2];
} SHA1Context;

static s32 __sha1Fd = -1;

s32 SHA1_Open()
{
	__sha1Fd = IOS_Open("/dev/sha", IOS_OPEN_NONE);
	return __sha1Fd;
}

void SHA1_Close()
{
	IOS_Close(__sha1Fd);
	__sha1Fd = -1;
}

s32 SHA1_Init(SHA1Context* context)
{
	IOVector vec[3] ATTRIBUTE_ALIGN(32);

	if (__sha1Fd < 0)
		return __sha1Fd;
	if ((u32) context & 3)
		return -4;
	
	vec[0].data = NULL;
	vec[0].len = 0;
	vec[1].data = (void*) context;
	vec[1].len = sizeof(SHA1Context);
	vec[2].data = NULL;
	vec[2].len = 0;

	return IOS_Ioctlv(__sha1Fd, IOCTL_SHA1_INIT, 1, 2, vec);
}

s32 SHA1_Update(SHA1Context* context, void* data, u32 len)
{
	IOVector vec[3] ATTRIBUTE_ALIGN(32);

	if (__sha1Fd < 0)
		return __sha1Fd;
	if ((u32) context & 3 || (u32) data & 0x3F || len & 0x3F)
		return -4;
	
	vec[0].data = data;
	vec[0].len = len;
	vec[1].data = (void*) context;
	vec[1].len = sizeof(SHA1Context);
	vec[2].data = NULL;
	vec[2].len = 0;
	
	return IOS_Ioctlv(__sha1Fd, IOCTL_SHA1_UPDATE, 1, 2, vec);
}

s32 SHA1_Final(SHA1Context* context, u8 digest[20])
{
	IOVector vec[3] ATTRIBUTE_ALIGN(32);

	if (__sha1Fd < 0)
		return __sha1Fd;
	if ((u32) context & 3 || (u32) digest & 3)
		return -4;
	
	vec[0].data = NULL;
	vec[0].len = 0;
	vec[1].data = (void*) context;
	vec[1].len = sizeof(SHA1Context);
	vec[2].data = (void*) digest;
	vec[2].len = 20;

	return IOS_Ioctlv(__sha1Fd, IOCTL_SHA1_FINAL, 1, 2, vec);
}

s32 SHA1(u8 digest[20], void* data, u32 len)
{
	s32 ret;
	SHA1Context context;

	if (__sha1Fd < 0 && SHA1_Open() < 0)
		return __sha1Fd;

	ret = SHA1_Init(&context);
	if (ret != 0)
		return ret;
	
	ret = SHA1_Update(&context, data, len);
	if (ret != 0)
		return ret;
	
	return SHA1_Final(&context, digest);
}