#ifndef _PH_MISC_H
#define _PH_MISC_H

#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS NTAPI PhCreateKey(
    _Out_ PHANDLE KeyHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ HANDLE RootDirectory,
    _In_ PPH_STRINGREF ObjectName,
    _In_ ULONG Attributes,
    _In_ ULONG CreateOptions,
    _Out_opt_ PULONG Disposition
);

NTSTATUS NTAPI PhOpenKey(
    _Out_ PHANDLE KeyHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ HANDLE RootDirectory,
    _In_ PPH_STRINGREF ObjectName,
    _In_ ULONG Attributes
);

#define PH_KEY_PREDEFINE(Number) ((HANDLE)(LONG_PTR)(-3 - (Number) * 2))
#define PH_KEY_IS_PREDEFINED(Predefine) (((LONG_PTR)(Predefine) < 0) && ((LONG_PTR)(Predefine) & 0x1))
#define PH_KEY_PREDEFINE_TO_NUMBER(Predefine) (ULONG)(((-(LONG_PTR)(Predefine) - 3) >> 1))

#define PH_KEY_LOCAL_MACHINE PH_KEY_PREDEFINE(0) // \Registry\Machine
#define PH_KEY_USERS PH_KEY_PREDEFINE(1) // \Registry\User
#define PH_KEY_CLASSES_ROOT PH_KEY_PREDEFINE(2) // \Registry\Machine\Software\Classes
#define PH_KEY_CURRENT_USER PH_KEY_PREDEFINE(3) // \Registry\User\<SID>
#define PH_KEY_CURRENT_USER_NUMBER 3
#define PH_KEY_MAXIMUM_PREDEFINE 4

NTSTATUS NTAPI PhQueryValueKey(
    _In_ HANDLE KeyHandle,
    _In_opt_ PPH_STRINGREF ValueName,
    _In_ KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    _Out_ PVOID* Buffer
);

NTSTATUS NTAPI PhSetValueKey(
    _In_ HANDLE KeyHandle,
    _In_opt_ PPH_STRINGREF ValueName,
    _In_ ULONG ValueType,
    _In_ PVOID Buffer,
    _In_ ULONG BufferLength
);

NTSTATUS NTAPI PhSetFileCompletionNotificationMode(
    _In_ HANDLE FileHandle,
    _In_ ULONG Flags
    );

VOID NTAPI KphpTpSetPoolThreadBasePriority(
    _Inout_ PTP_POOL Pool,
    _In_ ULONG BasePriority
    );

VOID NTAPI KphSetServiceSecurity(SC_HANDLE ServiceHandle);



FORCEINLINE
PVOID
NTAPI
PhGetLoaderEntryDllBaseZ(
    _In_ PWSTR DllName
    )
{
    return LoadLibraryW(DllName);
}

FORCEINLINE
PVOID
NTAPI
PhGetDllBaseProcedureAddress(
    _In_ PVOID DllBase,
    _In_opt_ PSTR ProcedureName,
    _In_opt_ USHORT ProcedureNumber
    )
{
    return GetProcAddress((HMODULE)DllBase, ProcedureName);
}

FORCEINLINE
PVOID 
NTAPI 
PhLoadLibrary(
    _In_ PCWSTR FileName
    )
{
    return LoadLibraryW(FileName); 
}

// rev from RtlInitializeSid (dmex)
FORCEINLINE
BOOLEAN
NTAPI
PhInitializeSid(
    _Out_ PSID Sid,
    _In_ PSID_IDENTIFIER_AUTHORITY IdentifierAuthority,
    _In_ UCHAR SubAuthorityCount
    )
{
    return NT_SUCCESS(RtlInitializeSid(Sid, IdentifierAuthority, SubAuthorityCount));
}

// rev from RtlLengthSid (dmex)
FORCEINLINE
ULONG
NTAPI
PhLengthSid(
    _In_ PSID Sid
    )
{
    return RtlLengthSid(Sid);
}

// rev from RtlLengthRequiredSid (dmex)
FORCEINLINE
ULONG
NTAPI
PhLengthRequiredSid(
    _In_ ULONG SubAuthorityCount
    )
{
    return RtlLengthRequiredSid(SubAuthorityCount);
}

// rev from RtlEqualSid (dmex)
FORCEINLINE
BOOLEAN
NTAPI
PhEqualSid(
    _In_ PSID Sid1,
    _In_ PSID Sid2
    )
{
    return RtlEqualSid(Sid1, Sid2);

}

// rev from RtlValidSid (dmex)
FORCEINLINE
BOOLEAN
NTAPI
PhValidSid(
    _In_ PSID Sid
    )
{
    return RtlValidSid(Sid);
}

// rev from RtlSubAuthoritySid (dmex)
FORCEINLINE
PULONG
NTAPI
PhSubAuthoritySid(
    _In_ PSID Sid,
    _In_ ULONG SubAuthority
    )
{
    return RtlSubAuthoritySid(Sid, SubAuthority);
}

// rev from RtlSubAuthorityCountSid (dmex)
FORCEINLINE
PUCHAR
NTAPI
PhSubAuthorityCountSid(
    _In_ PSID Sid
    )
{
    return RtlSubAuthorityCountSid(Sid);
}

// rev from RtlIdentifierAuthoritySid (dmex)
FORCEINLINE
PSID_IDENTIFIER_AUTHORITY
NTAPI
PhIdentifierAuthoritySid(
    _In_ PSID Sid
    )
{
    return RtlIdentifierAuthoritySid(Sid);

}

FORCEINLINE
BOOLEAN
NTAPI
PhEqualIdentifierAuthoritySid(
    _In_ PSID_IDENTIFIER_AUTHORITY IdentifierAuthoritySid1,
    _In_ PSID_IDENTIFIER_AUTHORITY IdentifierAuthoritySid2
    )
{
    return RtlEqualMemory(RtlIdentifierAuthoritySid(IdentifierAuthoritySid1), RtlIdentifierAuthoritySid(IdentifierAuthoritySid2), sizeof(SID_IDENTIFIER_AUTHORITY));
}

// rev from RtlFreeSid (dmex)
FORCEINLINE
BOOLEAN
NTAPI
PhFreeSid(
    _In_ _Post_invalid_ PSID Sid
    )
{
    return !RtlFreeSid(Sid);
}



typedef struct _PH_TOKEN_ATTRIBUTES
{
    HANDLE TokenHandle;
    struct
    {
        ULONG Elevated : 1;
        ULONG ElevationType : 2;
        ULONG ReservedBits : 29;
    };
    ULONG Reserved;
    PSID TokenSid;
} PH_TOKEN_ATTRIBUTES, *PPH_TOKEN_ATTRIBUTES;

typedef enum _MANDATORY_LEVEL_RID {
    MandatoryUntrustedRID = SECURITY_MANDATORY_UNTRUSTED_RID,
    MandatoryLowRID = SECURITY_MANDATORY_LOW_RID,
    MandatoryMediumRID = SECURITY_MANDATORY_MEDIUM_RID,
    MandatoryMediumPlusRID = SECURITY_MANDATORY_MEDIUM_PLUS_RID,
    MandatoryHighRID = SECURITY_MANDATORY_HIGH_RID,
    MandatorySystemRID = SECURITY_MANDATORY_SYSTEM_RID,
    MandatorySecureProcessRID = SECURITY_MANDATORY_PROTECTED_PROCESS_RID
} MANDATORY_LEVEL_RID, *PMANDATORY_LEVEL_RID;

PHLIBAPI
PH_TOKEN_ATTRIBUTES
NTAPI
PhGetOwnTokenAttributes(
    VOID
    );

NTSTATUS PhGetSystemLogicalProcessorInformation(
    _In_ LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType,
    _Out_ PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *Buffer,
    _Out_ PULONG BufferLength
    );


NTSTATUS PhSetThreadBasePriority(
    _In_ HANDLE ThreadHandle,
    _In_ KPRIORITY Increment
    );

NTSTATUS PhSetThreadIoPriority(
    _In_ HANDLE ThreadHandle,
    _In_ IO_PRIORITY_HINT IoPriority
    );

NTSTATUS PhSetThreadPagePriority(
    _In_ HANDLE ThreadHandle,
    _In_ ULONG PagePriority
    );


FORCEINLINE
NTSTATUS
PhGetProcessIsWow64(
    _In_ HANDLE ProcessHandle,
    _Out_ PBOOLEAN IsWow64
    )
{
    NTSTATUS status;
    ULONG_PTR wow64;

    status = NtQueryInformationProcess(
        ProcessHandle,
        ProcessWow64Information,
        &wow64,
        sizeof(ULONG_PTR),
        NULL
        );

    if (NT_SUCCESS(status))
    {
        *IsWow64 = !!wow64;
    }

    return status;
}

// rev from SE_TOKEN_USER (dmex)
typedef struct _PH_TOKEN_USER
{
    union
    {
        TOKEN_USER TokenUser;
        SID_AND_ATTRIBUTES User;
    };
    union
    {
        SID Sid;
        BYTE Buffer[SECURITY_MAX_SID_SIZE];
    };
} PH_TOKEN_USER, *PPH_TOKEN_USER;

C_ASSERT(sizeof(PH_TOKEN_USER) >= TOKEN_USER_MAX_SIZE);

NTSTATUS PhGetTokenUser(
    _In_ HANDLE TokenHandle,
    _Out_ PPH_TOKEN_USER User
);

/** The PID of the idle process. */
#define SYSTEM_IDLE_PROCESS_ID ((HANDLE)0)
/** The PID of the system process. */
#define SYSTEM_PROCESS_ID ((HANDLE)4)

PHLIBAPI
NTSTATUS
NTAPI
PhCreateFile(
    _Out_ PHANDLE FileHandle,
    _In_ PPH_STRINGREF FileName,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ ULONG FileAttributes,
    _In_ ULONG ShareAccess,
    _In_ ULONG CreateDisposition,
    _In_ ULONG CreateOptions
);

PHLIBAPI
NTSTATUS
NTAPI
PhCreateFileWin32(
    _Out_ PHANDLE FileHandle,
    _In_ PWSTR FileName,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ ULONG FileAttributes,
    _In_ ULONG ShareAccess,
    _In_ ULONG CreateDisposition,
    _In_ ULONG CreateOptions
);

PHLIBAPI
NTSTATUS
NTAPI
PhGetFileSize(
    _In_ HANDLE FileHandle,
    _Out_ PLARGE_INTEGER Size
);

PHLIBAPI
NTSTATUS
NTAPI
PhGetFileUsn(
    _In_ HANDLE FileHandle,
    _Out_ PLONGLONG Usn
);

#include "verify.h"

VERIFY_RESULT PhVerifyFileCached(
    _In_ PPH_STRING FileName,
    _In_opt_ PPH_STRING PackageFullName,
    _Out_opt_ PPH_STRING *SignerName,
    _Out_opt_ BYTE* SignerSHA1Hash,
    _Out_opt_ PDWORD SignerSHA1HashSize,
    _Out_opt_ BYTE* SignerSHAXHash,
    _Out_opt_ PDWORD SignerSHAXHashSize,
    _In_ BOOLEAN NativeFileName,
    _In_ BOOLEAN CachedOnly
);

#ifdef __cplusplus
}
#endif

#endif
