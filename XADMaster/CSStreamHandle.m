/*
 * CSStreamHandle.m
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
#import "CSStreamHandle.h"

@implementation CSStreamHandle

/*-(id)initWithName:(NSString *)descname
{
	return [self initWithName:descname length:CSHandleMaxLength];
}

-(id)initWithName:(NSString *)descname length:(off_t)length
{
	if(self=[super initWithName:descname])
	{
		streampos=0;
		streamlength=length;
		endofstream=NO;
		needsreset=YES;
		nextstreambyte=-1;

		input=NULL;
	}
	return self;
}*/

-(id)initWithParentHandle:(CSHandle *)handle
{
	return [self initWithParentHandle:handle length:CSHandleMaxLength];
}

-(id)initWithParentHandle:(CSHandle *)handle length:(off_t)length
{
	if(self=[super initWithParentHandle:handle])
	{
		streampos=0;
		streamlength=length;
		endofstream=NO;
		needsreset=YES;
		nextstreambyte=-1;

		input=NULL;
	}
	return self;
}

-(id)initWithInputBufferForHandle:(CSHandle *)handle
{
	return [self initWithInputBufferForHandle:handle length:CSHandleMaxLength bufferSize:4096];
}

-(id)initWithInputBufferForHandle:(CSHandle *)handle length:(off_t)length
{
	return [self initWithInputBufferForHandle:handle length:length bufferSize:4096];
}

-(id)initWithInputBufferForHandle:(CSHandle *)handle bufferSize:(int)buffersize
{
	return [self initWithInputBufferForHandle:handle length:CSHandleMaxLength bufferSize:buffersize];
}

-(id)initWithInputBufferForHandle:(CSHandle *)handle length:(off_t)length bufferSize:(int)buffersize;
{
	if(self=[super initWithParentHandle:handle])
	{
		streampos=0;
		streamlength=length;
		endofstream=NO;
		needsreset=YES;
		nextstreambyte=-1;

		input=CSInputBufferAlloc(handle,buffersize);
	}
	return self;
}

-(id)initAsCopyOf:(CSStreamHandle *)other
{
	[self _raiseNotSupported:_cmd];
	return nil;
}

-(void)dealloc
{
	CSInputBufferFree(input);
	[super dealloc];
}



-(off_t)fileSize { return streamlength; }

-(off_t)offsetInFile { return streampos; }

-(BOOL)atEndOfFile
{
	if(needsreset) { [self resetStream]; needsreset=NO; }

	if(endofstream) return YES;
	if(streampos==streamlength) return YES;
	if(nextstreambyte>=0) return NO;

	uint8_t b[1];
	@try
	{
		if([self streamAtMost:1 toBuffer:b]==1)
		{
			nextstreambyte=b[0];
			return NO;
		}
	}
	@catch(id e) {}

	endofstream=YES;
	return YES;
}

-(void)seekToFileOffset:(off_t)offs
{
	if(![self _prepareStreamSeekTo:offs]) return;

	if(offs<streampos)
	{
		streampos=0;
		endofstream=NO;
		//nextstreambyte=-1;
		if(input) CSInputRestart(input);
		[self resetStream];
	}

	if(offs==0) return;

	[self readAndDiscardBytes:offs-streampos];
}

-(void)seekToEndOfFile { [self readAndDiscardAtMost:CSHandleMaxLength]; }

-(int)readAtMost:(int)num toBuffer:(void *)buffer
{
	if(needsreset) { [self resetStream]; needsreset=NO; }

	if(endofstream) return 0;
	if(streampos+num>streamlength) num=(int)(streamlength-streampos);
	if(!num) return 0;

	int offs=0;
	if(nextstreambyte>=0)
	{
		((uint8_t *)buffer)[0]=nextstreambyte;
		streampos++;
		nextstreambyte=-1;
		offs=1;
		if(num==1) return 1;
	}

	int actual=[self streamAtMost:num-offs toBuffer:((uint8_t *)buffer)+offs];

	if(actual==0) endofstream=YES;

	streampos+=actual;

	return actual+offs;
}



-(void)resetStream {}

-(int)streamAtMost:(int)num toBuffer:(void *)buffer { return 0; }




-(void)endStream
{
	endofstream=YES;
}

-(BOOL)_prepareStreamSeekTo:(off_t)offs
{
	if(needsreset) { [self resetStream]; needsreset=NO; }

	if(offs==streampos) return NO;
	if(endofstream&&offs>streampos) [self _raiseEOF];
	if(offs>streamlength) [self _raiseEOF];
	if(nextstreambyte>=0)
	{
		nextstreambyte=-1;
		streampos+=1;
		if(offs==streampos) return NO;
	}

	return YES;
}

-(void)setStreamLength:(off_t)length { streamlength=length; }

-(void)setInputBuffer:(CSInputBuffer *)inputbuffer
{
	CSInputBufferFree(input);
	input=inputbuffer;
}

@end
