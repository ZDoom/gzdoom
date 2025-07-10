/*
** i_protocol.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __I_PROTOCOL_H__
#define __I_PROTOCOL_H__

#include "tarray.h"
#include "zstring.h"

uint8_t UncheckedReadInt8(uint8_t** stream);
int16_t UncheckedReadInt16(uint8_t** stream);
int32_t UncheckedReadInt32(uint8_t** stream);
int64_t UncheckedReadInt64(uint8_t** stream);
float UncheckedReadFloat(uint8_t** stream);
double UncheckedReadDouble(uint8_t** stream);
char* UncheckedReadString(uint8_t** stream);
const char* UncheckedReadStringConst(uint8_t** stream);
void UncheckedWriteInt8(uint8_t val, uint8_t** stream);
void UncheckedWriteInt16(int16_t val, uint8_t** stream);
void UncheckedWriteInt32(int32_t val, uint8_t** stream);
void UncheckedWriteInt64(int64_t val, uint8_t** stream);
void UncheckedWriteFloat(float val, uint8_t** stream);
void UncheckedWriteDouble(double val, uint8_t** stream);
void UncheckedWriteString(const char* string, uint8_t** stream);

void AdvanceStream(TArrayView<uint8_t>& stream, size_t bytes);

uint8_t ReadInt8(TArrayView<uint8_t>& stream);
int16_t ReadInt16(TArrayView<uint8_t>& stream);
int32_t ReadInt32(TArrayView<uint8_t>& stream);
int64_t ReadInt64(TArrayView<uint8_t>& stream);
float ReadFloat(TArrayView<uint8_t>& stream);
double ReadDouble(TArrayView<uint8_t>& stream);
char* ReadString(TArrayView<uint8_t>& stream);
const char* ReadStringConst(TArrayView<uint8_t>& stream);
void ReadBytes(TArrayView<uint8_t>& dst, TArrayView<uint8_t>& stream);
void WriteInt8(uint8_t val, TArrayView<uint8_t>& stream);
void WriteInt16(int16_t val, TArrayView<uint8_t>& stream);
void WriteInt32(int32_t val, TArrayView<uint8_t>& stream);
void WriteInt64(int64_t val, TArrayView<uint8_t>& stream);
void WriteFloat(float val, TArrayView<uint8_t>& stream);
void WriteDouble(double val, TArrayView<uint8_t>& stream);
void WriteString(const char* string, TArrayView<uint8_t>& stream);
void WriteBytes(const TArrayView<uint8_t>& source, TArrayView<uint8_t>& stream);
void WriteFString(FString& string, TArrayView<uint8_t>& stream);

#endif //__I_PROTOCOL_H__
