/*
 * CSFileHandle.m
 *
 * Copyright (c) 2017-present, MacPaw Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */
#import "CSFileHandle.h"

#include <sys/stat.h>



NSString *CSCannotOpenFileException=@"CSCannotOpenFileException";
NSString *CSFileErrorException=@"CSFileErrorException";




@implementation CSFileHandle

+(CSFileHandle *)fileHandleForReadingAtPath:(NSString *)path
{ return [self fileHandleForPath:path modes:@"rb"]; }

+(CSFileHandle *)fileHandleForWritingAtPath:(NSString *)path
{ return [self fileHandleForPath:path modes:@"wb"]; }

+(CSFileHandle *)fileHandleForPath:(NSString *)path modes:(NSString *)modes
{
	if(!path) return nil;

	#if defined(__COCOTRON__) // Cocotron
	FILE *fileh=_wfopen([path fileSystemRepresentationW],
	(const wchar_t *)[modes cStringUsingEncoding:NSUnicodeStringEncoding]);
	#elif defined(__MINGW32__) // GNUstep under mingw32 - sort of untested
	FILE *fileh=_wfopen((const wchar_t *)[path fileSystemRepresentation],
	(const wchar_t *)[modes cStringUsingEncoding:NSUnicodeStringEncoding]);
	#else // Cocoa or GNUstep under Linux
	FILE *fileh=fopen([path fileSystemRepresentation],[modes UTF8String]);
	#endif

	if(!fileh) [NSException raise:CSCannotOpenFileException
	format:@"Error attempting to open file \"%@\" in mode \"%@\".",path,modes];

	CSFileHandle *handle=[[[CSFileHandle alloc] initWithFilePointer:fileh closeOnDealloc:YES path:path] autorelease];
	if(handle) return handle;

	fclose(fileh);
	return nil;
}

+(CSFileHandle *)fileHandleForStandardInput
{
	static CSFileHandle *handle=nil;
	if(!handle) handle=[[CSFileHandle alloc] initWithFilePointer:stdin closeOnDealloc:NO path:@"/dev/stdin"];
	return handle;
}

+(CSFileHandle *)fileHandleForStandardOutput
{
	static CSFileHandle *handle=nil;
	if(!handle) handle=[[CSFileHandle alloc] initWithFilePointer:stdout closeOnDealloc:NO path:@"/dev/stdout"];
	return handle;
}

+(CSFileHandle *)fileHandleForStandardError
{
	static CSFileHandle *handle=nil;
	if(!handle) handle=[[CSFileHandle alloc] initWithFilePointer:stderr closeOnDealloc:NO path:@"/dev/stderr"];
	return handle;
}




-(id)initWithFilePointer:(FILE *)file closeOnDealloc:(BOOL)closeondealloc path:(NSString *)filepath
{
	if(self=[super init])
	{
		fh=file;
		path=[filepath retain];
 		close=closeondealloc;
		multilock=nil;
		fhowner=nil;
	}
	return self;
}

-(id)initAsCopyOf:(CSFileHandle *)other
{
	if(self=[super initAsCopyOf:other])
	{
		fh=other->fh;
		path=[other->path retain];
 		close=NO;
		if(other->fhowner) fhowner=[other->fhowner retain];
		else fhowner=[other retain];

		if(!other->multilock) [other _setMultiMode];

		multilock=[other->multilock retain];
		[multilock lock];
		pos=other->pos;
		[multilock unlock];
	}
	return self;
}

-(void)dealloc
{
	if(fh && close) fclose(fh);
	[path release];
	[fhowner release];
	[multilock release];
	[super dealloc];
}

-(void)close
{
	if(fh && close) fclose(fh);
	fh=NULL;
}




-(FILE *)filePointer { return fh; }




-(off_t)fileSize
{
	#if defined(__MINGW32__)
	struct _stati64 s;
	if(_fstati64(fileno(fh),&s)) [self _raiseError];
	#else
	struct stat s;
	if(fstat(fileno(fh),&s)) [self _raiseError];
	#endif

	return s.st_size;
}

-(off_t)offsetInFile
{
	if(multilock) return pos;
	else return ftello(fh);
}

-(BOOL)atEndOfFile
{
	return [self offsetInFile]==[self fileSize];
/*	if(multi) return pos==[self fileSize];
	else return feof(fh);*/ // feof() only returns true after trying to read past the end
}



-(void)seekToFileOffset:(off_t)offs
{
	if(multilock) { [multilock lock]; }
	//if(offs>[self fileSize]) [self _raiseEOF];
	if(fseeko(fh,offs,SEEK_SET)) [self _raiseError];
	if(multilock) { pos=ftello(fh); [multilock unlock]; }
}

-(void)seekToEndOfFile
{
	if(multilock) { [multilock lock]; }
	if(fseeko(fh,0,SEEK_END)) [self _raiseError];
	if(multilock) { pos=ftello(fh); [multilock unlock]; }
}

-(void)pushBackByte:(int)byte
{
	if(multilock) [self _raiseNotSupported:_cmd];
	if(ungetc(byte,fh)==EOF) [self _raiseError];
}

-(int)readAtMost:(int)num toBuffer:(void *)buffer
{
	if(num==0) return 0;
	if(multilock) { [multilock lock]; fseeko(fh,pos,SEEK_SET); }
	int n=(int)fread(buffer,1,num,fh);
	if(n<=0&&!feof(fh)) [self _raiseError];
	if(multilock) { pos=ftello(fh); [multilock unlock]; }
	return n;
}

-(void)writeBytes:(int)num fromBuffer:(const void *)buffer
{
	if(multilock) { [multilock lock]; fseeko(fh,pos,SEEK_SET); }
	if(fwrite(buffer,1,num,fh)!=num) [self _raiseError];
	if(multilock) { pos=ftello(fh); [multilock unlock]; }
}

-(NSString *)name
{
	return path;
}




-(void)_raiseError
{
	if(feof(fh)) [self _raiseEOF];
	else [[[[NSException alloc] initWithName:CSFileErrorException
	reason:[NSString stringWithFormat:@"Error while attempting to read file \"%@\": %s.",[self name],strerror(errno)]
	userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:errno] forKey:@"ErrNo"]] autorelease] raise];
}

-(void)_setMultiMode
{
	if(!multilock)
	{
		multilock=[NSLock new];
		pos=ftello(fh);
	}
}

@end
