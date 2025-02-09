#include "perm.h"
#define WIN32_LEAN_AND_MEAN
#include <AclAPI.h>
#include "printf.h"
#include <windows.h>

void get_owner_group(PSID owner, PSID group, PSECURITY_DESCRIPTOR desc,
                     WString *owner_name, WString *group_name) {
    SID_NAME_USE name_use;

    wchar_t domaim_buf[1024];
    DWORD name_size = owner_name->capacity;
    DWORD domain_size = 1024;
    WString_clear(owner_name);
    if (owner == NULL) {
        WString_clear(owner_name);
    } else if (LookupAccountSidW(NULL, owner, owner_name->buffer, &name_size,
                                 domaim_buf, &domain_size, &name_use)) {
        owner_name->length = name_size;
    } else {
        WString_reserve(owner_name, name_size);
        domain_size = 1024;
        if (!LookupAccountSidW(NULL, owner, owner_name->buffer, &name_size,
                               domaim_buf, &domain_size, &name_use)) {
            WString_clear(owner_name);
        }
        owner_name->length = name_size;
    }

    name_size = group_name->capacity;
    domain_size = 1024;
    WString_clear(group_name);
    if (group == NULL) {
        WString_clear(group_name);
    } else if (LookupAccountSidW(NULL, group, group_name->buffer, &name_size,
                                 domaim_buf, &domain_size, &name_use)) {
        group_name->length = name_size;
    } else {
        WString_reserve(group_name, name_size);
        domain_size = 1024;
        if (!LookupAccountSidW(NULL, group, group_name->buffer, &name_size,
                               domaim_buf, &domain_size, &name_use)) {
            WString_clear(group_name);
        }
        group_name->length = name_size;
    }
}

void get_flags(PACL dacl, PSID owner, PSID group, uint32_t *flags) {
    if (dacl == NULL) {
        *flags = OWNER_ALL | GROUP_ALL | USER_ALL;
        return;
    }

    PSID user_sid = Mem_alloc(100);
    int a;
    DWORD ssize = 100;
    if (!CreateWellKnownSid(WinBuiltinUsersSid, NULL, user_sid, &ssize)) {
        Mem_free(user_sid);
        return;
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
    Mem_free(user_sid);
}

bool get_perms_h(HANDLE file, uint32_t* flags, WString* owner_name, WString* group_name) {
    SECURITY_INFORMATION in = OWNER_SECURITY_INFORMATION |
                              GROUP_SECURITY_INFORMATION |
                              DACL_SECURITY_INFORMATION;
    PSID owner = NULL, group = NULL;
    PACL dacl = NULL;
    PSECURITY_DESCRIPTOR desc;
    if (GetSecurityInfo(file, SE_FILE_OBJECT, in, &owner, &group, &dacl, NULL,
                        &desc) != ERROR_SUCCESS) {
        *flags = 0;
        CloseHandle(file);
        return true;
    }

    get_owner_group(owner, group, desc, owner_name, group_name);
    get_flags(dacl, owner, group, flags);

    LocalFree(desc);
    return true;
}

bool get_perms_size_date(const wchar_t *path, bool is_dir, uint32_t *flags,
                         WString *owner_name, WString *group_name,
                         uint64_t *size, FILETIME *time, uint64_t* disk_size) {
    WString_clear(owner_name);
    WString_clear(group_name);
    *flags = 0;

    DWORD attr = FILE_FLAG_OPEN_REPARSE_POINT |
                 (is_dir ? FILE_FLAG_BACKUP_SEMANTICS : 0);
    HANDLE file = CreateFileW(path, READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, attr, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!GetFileTime(file, NULL, NULL, time)) {
        time->dwLowDateTime = 0;
        time->dwHighDateTime = 0;
    }


    LARGE_INTEGER q_size;
    if (GetFileSizeEx(file, &q_size)) {
        *size = q_size.QuadPart;
    } else {
        *size = 0;
    }

    FILE_STANDARD_INFO i;
    if (GetFileInformationByHandleEx(file, FileStandardInfo, &i, sizeof(i))) {
        *disk_size = i.AllocationSize.QuadPart;
    } else {
        *disk_size = *size;
    }


    SECURITY_INFORMATION in = OWNER_SECURITY_INFORMATION |
                              GROUP_SECURITY_INFORMATION |
                              DACL_SECURITY_INFORMATION;
    PSID owner = NULL, group = NULL;
    PACL dacl = NULL;
    PSECURITY_DESCRIPTOR desc;
    if (GetSecurityInfo(file, SE_FILE_OBJECT, in, &owner, &group, &dacl, NULL,
                        &desc) != ERROR_SUCCESS) {
        *flags = 0;
        CloseHandle(file);
        return true;
    }

    get_owner_group(owner, group, desc, owner_name, group_name);
    get_flags(dacl, owner, group, flags);

    LocalFree(desc);
    CloseHandle(file);
    return true;
}

bool get_perms(const wchar_t *path, bool is_dir, uint32_t *flags,
               WString *owner_name, WString *group_name) {
    PSID owner = NULL, group = NULL;
    PACL dacl = NULL;
    SECURITY_DESCRIPTOR sd[10];
    PSECURITY_DESCRIPTOR psd_alloc = NULL;
    PSECURITY_DESCRIPTOR psd = &sd[0];

    SECURITY_INFORMATION in =
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION;
    if (flags != NULL) {
        in |= DACL_SECURITY_INFORMATION;
    }
    DWORD size;
    if (!GetFileSecurityW(path, in, &sd[0], sizeof(sd), &size)) {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            psd_alloc = Mem_alloc(size);
            if (!GetFileSecurityW(path, in, psd_alloc, size, &size)) {
                Mem_free(psd_alloc);
                return false;
            }
            psd = psd_alloc;
        } else {
            goto fail;
        }
    }
    BOOL has_dacl, deflt;
    if (!GetSecurityDescriptorGroup(psd, &group, &deflt)) {
        goto fail;
    }
    if (!GetSecurityDescriptorOwner(psd, &owner, &deflt)) {
        goto fail;
    }

    get_owner_group(owner, group, psd, owner_name, group_name);

    if (flags == NULL) {
        goto end;
    }

    if (!GetSecurityDescriptorDacl(psd, &has_dacl, &dacl, &deflt) ||
        !has_dacl) {
        goto fail;
    }

    get_flags(dacl, owner, group, flags);
end:
    if (psd_alloc != NULL) {
        Mem_free(psd_alloc);
    }
    return true;
fail:
    if (psd_alloc != NULL) {
        Mem_free(psd_alloc);
    }
    return false;
}
