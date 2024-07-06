/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     jxy-s   2022
 *
 */

#pragma once

#include "../../Library/IPC/PortMessage.h"

//#include <kphmsgdefs.h>

#pragma warning(push)
#pragma warning(disable : 4201)
#pragma warning(disable : 4200)

typedef enum _KPH_MESSAGE_ID
{
    InvalidKphMsg,

    //
    // PH -> KPH
    //

    // ...

    MaxKphMsgClient,
    MaxKphMsgClientAllowed = 0x40000000,

    //
    // KPH -> PH
    //

    KphMsgProcessCreate = 0x40000001,
    KphMsgProcessExit   = 0x40000002,
    KphMsgImageLoad     = 0x40000004,
    KphMsgProcAccess    = 0x40000008,
    KphMsgUntrustedImage= 0x40000011,
    KphMsgAccessFile    = 0x40000012,
    KphMsgAccessReg     = 0x40000014,

    KphMsgProgramRules  = 0x40000101,
    KphMsgAccessRules   = 0x40000102,

    MaxKphMsg
} KPH_MESSAGE_ID, *PKPH_MESSAGE_ID;

C_ASSERT(sizeof(KPH_MESSAGE_ID) == 4);
C_ASSERT(sizeof(KPH_MESSAGE_ID) == sizeof(ULONG));
C_ASSERT(MaxKphMsg > 0);

typedef struct _KPH_MESSAGE
{
    MSG_HEADER Header;

    //MSG_HEADER_EX HeaderEx;

    CHAR Data[0x1000];
} KPH_MESSAGE, *PKPH_MESSAGE;

typedef const KPH_MESSAGE* PCKPH_MESSAGE;

//#define KPH_MESSAGE_MIN_SIZE RTL_SIZEOF_THROUGH_FIELD(KPH_MESSAGE, Data)
#define KPH_MESSAGE_MIN_SIZE UFIELD_OFFSET(KPH_MESSAGE, Data)

#pragma warning(pop)


