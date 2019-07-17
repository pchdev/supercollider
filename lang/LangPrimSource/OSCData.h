#pragma once

#include "PyrPrimitive.h"
#include "scsynthsend.h"

int makeSynthMsgWithTags(big_scpacket *packet, PyrSlot *slots, int size);
int makeSynthBundle(big_scpacket *packet, PyrSlot *slots, int size, bool useElapsed);

void PerformOSCBundle(int inSize, char *inData, PyrObject *inReply, int inPortNum);
void PerformOSCMessage(int inSize, char *inData, PyrObject *inReply, int inPortNum, double time);

PyrObject* ConvertOSCMessage(int inSize, char *inData);
