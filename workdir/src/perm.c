#include "perm.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <AclAPI.h>

bool get_perms(const wchar_t* path, bool is_dir, uint32_t* flags, WString* owner_name, WString* group_name) {
    PSID owner = NULL, group = NULL;
    PACL dacl = NULL;
    SECURITY_DESCRIPTOR sd[10];
    PSECURITY_DESCRIPTOR psd_alloc = NULL;
    PSECURITY_DESCRIPTOR psd = &sd[0];

    SECURITY_INFORMATION in = OWNER_SECURITY_INFORMATION | 
                              GROUP_SECURITY_INFORMATION | 
                              DACL_SECURITY_INFORMATION;
    DWORD size;
    if (!GetFileSecurityW(path, in, &sd[0], sizeof(sd), &size)) {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            psd_alloc = HeapAlloc(GetProcessHeap(), 0, size);
            if (!GetFileSecurityW(path, in, psd_alloc, size, &size)) {
                HeapFree(GetProcessHeap(), 0, psd_alloc);
                return false;
            }
            psd = psd_alloc;
        } else {
            goto fail;
        }
    }
    BOOL has_dacl, deflt;
    if (!GetSecurityDescriptorDacl(psd, &has_dacl, &dacl, &deflt) || !has_dacl) {
        goto fail;
    }
    if (!GetSecurityDescriptorGroup(psd, &group, &deflt)) {
        goto fail;
    }
    if (!GetSecurityDescriptorOwner(psd, &owner, &deflt)) {
        goto fail;
    }

    unsigned char* ace;
    PSID sid;
    SID_NAME_USE name_use;

    wchar_t domaim_buf[1024];
    DWORD name_size = owner_name->capacity;
    DWORD domain_size = 1024;
    WString_clear(owner_name);
    if (owner == NULL) {
        WString_extend(owner_name, L"unkown");
    } else if (LookupAccountSidW(NULL, owner, owner_name->buffer, &name_size, domaim_buf, &domain_size, &name_use)) {
        owner_name->length = name_size;
    } else {
        WString_reserve(owner_name, name_size);
        domain_size = 1024;
        if (!LookupAccountSidW(NULL, owner, owner_name->buffer, &name_size, domaim_buf, &domain_size, &name_use)) {
            WString_extend(owner_name, L"unkown");
        }
    }

    name_size = group_name->capacity;
    domain_size = 1024;
    WString_clear(group_name);
    if (owner == NULL) {
        WString_extend(group_name, L"unkown");
    } else if (LookupAccountSidW(NULL, group, group_name->buffer, &name_size, domaim_buf, &domain_size, &name_use)) {
        owner_name->length = name_size;
    } else {
        WString_reserve(group_name, name_size);
        domain_size = 1024;
        if (!LookupAccountSidW(NULL, group, group_name->buffer, &name_size, domaim_buf, &domain_size, &name_use)) {
            WString_extend(group_name, L"unkown");
        }
    }

    if (dacl == NULL) {
        *flags = OWNER_ALL | GROUP_ALL | USER_ALL;
        goto end;
    }

    PSID user_sid = HeapAlloc(GetProcessHeap(), 0, 100);
    int a;
    DWORD ssize = 100;
    if (!CreateWellKnownSid(WinBuiltinUsersSid, NULL, user_sid, &ssize)) {
        goto fail;
    }

    uint32_t allow_mask = 0;

    TRUSTEEW t;
    t.pMultipleTrustee = NULL;
    t.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    t.TrusteeForm = TRUSTEE_IS_SID;
    t.TrusteeType = TRUSTEE_IS_USER;
    t.ptstrName = owner;

    DWORD mask;
    if (GetEffectiveRightsFromAclW(dacl, &t, &mask) == ERROR_SUCCESS) {
        bool read = (mask & FILE_GENERIC_READ) == FILE_GENERIC_READ;
        bool write = (mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE;
        bool execute = (mask & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE;
        allow_mask |= OWNER_MASK(read, write, execute);
    }
    t.TrusteeType = TRUSTEE_IS_GROUP;
    t.ptstrName = group;
    if (GetEffectiveRightsFromAclW(dacl, &t, &mask) == ERROR_SUCCESS) {
        bool read = (mask & FILE_GENERIC_READ) == FILE_GENERIC_READ;
        bool write = (mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE;
        bool execute = (mask & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE;
        allow_mask |= GROUP_MASK(read, write, execute);
    }
    t.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    t.ptstrName = user_sid;
    if (GetEffectiveRightsFromAclW(dacl, &t, &mask) == ERROR_SUCCESS) {
        bool read = (mask & FILE_GENERIC_READ) == FILE_GENERIC_READ;
        bool write = (mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE;
        bool execute = (mask & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE;
        allow_mask |= USER_MASK(read, write, execute);
    }

    *flags = allow_mask;
    HeapFree(GetProcessHeap(), 0, user_sid);
end:
    if (psd_alloc != NULL) {
        HeapFree(GetProcessHeap(), 0, psd_alloc);
    }
    return true;
fail:
    if (psd_alloc != NULL) {
        HeapFree(GetProcessHeap(), 0, psd_alloc);
    }
    return false;
}
